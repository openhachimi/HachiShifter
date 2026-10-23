#pragma once
#include "PlaybackRenderPriority.h"
#include <juce_core/juce_core.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_set>

namespace hachi::backend
{
struct RenderSchedule
{
    std::string key;
    double startSeconds = std::numeric_limits<double>::quiet_NaN();
    double endSeconds = std::numeric_limits<double>::quiet_NaN();
    std::function<void()> discarded;
};

// Serial engines must wait in the priority queue, not occupy every worker
// while waiting for the engine's own mutex. DS retains each complete request.
enum class RenderLane { parallel, classicUtau, diffSinger, llsm, neural };

class PlaybackRenderQueue
{
public:
    explicit PlaybackRenderQueue(int workerCount)
    {
        for (int i = 0; i < std::max(1, workerCount); ++i)
            workers.emplace_back([this] { run(); });
    }
    ~PlaybackRenderQueue()
    {
        cancelAll();
        { const std::scoped_lock lock(mutex); stopping = true; }
        changed.notify_all();
        for (auto& worker : workers) worker.join();
    }
    PlaybackRenderQueue(const PlaybackRenderQueue&) = delete;
    PlaybackRenderQueue& operator=(const PlaybackRenderQueue&) = delete;

    // Safe on the audio callback: no queue lock, sorting, allocations or wakeup.
    void setPosition(double seconds) noexcept
    { clock->store(std::isfinite(seconds) ? std::max(0.0, seconds) : 0.0, std::memory_order_relaxed); }
    std::shared_ptr<const std::atomic<double>> positionClock() const { return clock; }

    void beginUpdate()
    { const std::scoped_lock lock(mutex); ++updateDepth; }
    void endUpdate(const std::unordered_set<std::string>& activeKeys)
    {
        std::vector<Item> removed;
        {
            const std::scoped_lock lock(mutex);
            for (auto it = pending.begin(); it != pending.end();)
                if (!it->schedule.key.empty() && !activeKeys.contains(it->schedule.key))
                { removed.push_back(std::move(*it)); it = pending.erase(it); }
                else ++it;
            // Content revisions invalidate running work too. Signal only: the
            // renderer's cancellation checks drain it without blocking the UI.
            // Seeking changes priority, not content, and never comes here.
            for (auto* item : active)
                if (!item->schedule.key.empty() && !activeKeys.contains(item->schedule.key))
                    item->job->signalJobShouldExit();
        }
        for (auto& item : removed) discard(item);
        { const std::scoped_lock lock(mutex); if (updateDepth > 0) --updateDepth; }
        changed.notify_all();
    }
    void add(std::unique_ptr<juce::ThreadPoolJob> job, RenderSchedule schedule = {},
             RenderLane lane = RenderLane::parallel)
    {
        Item item{std::move(job), std::move(schedule), lane};
        {
            const std::scoped_lock lock(mutex);
            if (!stopping && !cancelling)
            { pending.push_back(std::move(item)); changed.notify_all(); return; }
        }
        discard(item);
    }
    // Read-only shutdown diagnostics; never changes scheduling or ownership.
    [[nodiscard]] bool hasActiveJobs()
    { const std::scoped_lock lock(mutex); return !active.empty(); }
    void cancelAll()
    {
        std::vector<Item> removed;
        {
            std::unique_lock lock(mutex);
            cancelling = true;
            removed.swap(pending);
            for (auto* item : active) item->job->signalJobShouldExit();
            changed.notify_all();
            idle.wait(lock, [this] { return active.empty(); });
            cancelling = false;
        }
        for (auto& item : removed) discard(item);
        changed.notify_all();
    }
private:
    struct Item
    {
        std::unique_ptr<juce::ThreadPoolJob> job;
        RenderSchedule schedule;
        RenderLane lane;
    };
    static void discard(Item& item)
    { if (item.schedule.discarded) item.schedule.discarded(); }
    bool eligible(const Item& item) const
    {
        return item.lane == RenderLane::parallel || std::none_of(active.begin(), active.end(),
            [&](const auto* other) { return other->lane == item.lane; });
    }
    void run()
    {
        for (;;)
        {
            std::optional<Item> item;
            {
                std::unique_lock lock(mutex);
                changed.wait(lock, [this]
                {
                    return stopping || (!cancelling && updateDepth == 0
                        && std::any_of(pending.begin(), pending.end(),
                            [this](const auto& work) { return eligible(work); }));
                });
                if (stopping) return;
                auto best = pending.end();
                const auto position = clock->load(std::memory_order_relaxed);
                for (auto it = pending.begin(); it != pending.end(); ++it)
                    if (eligible(*it) && (best == pending.end()
                        || playbackRenderRank(it->schedule.startSeconds, it->schedule.endSeconds, position)
                         < playbackRenderRank(best->schedule.startSeconds, best->schedule.endSeconds, position)))
                        best = it;
                item.emplace(std::move(*best));
                pending.erase(best);
                active.push_back(&*item);
            }
            try { item->job->runJob(); }
            catch (...) { item->job->signalJobShouldExit(); }
            if (item->job->shouldExit()) discard(*item);
            {
                const std::scoped_lock lock(mutex);
                std::erase(active, &*item);
                idle.notify_all();
            }
            changed.notify_all();
        }
    }
    std::shared_ptr<std::atomic<double>> clock = std::make_shared<std::atomic<double>>(0.0);
    std::mutex mutex;
    std::condition_variable changed, idle;
    std::vector<Item> pending;
    std::vector<Item*> active;
    std::vector<std::thread> workers;
    int updateDepth = 0;
    bool stopping = false, cancelling = false;
};
}

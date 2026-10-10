#pragma once
#include <cstddef>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace hachi::backend
{
// Session-owned decoded cores. Keys include every input tensor and its context;
// no project IDs, timestamps or approximate pitch comparisons are sufficient.
// Immutable shared values survive eviction while another render reads them.
class NsfRenderChunkCache final
{
public:
    using Samples = std::shared_ptr<const std::vector<float>>;
    explicit NsfRenderChunkCache(std::size_t maxBytes = 64 * 1024 * 1024,
                                 std::size_t maxEntries = 256)
        : byteLimit(maxBytes), entryLimit(maxEntries) {}

    Samples get(const std::string& key)
    {
        const std::scoped_lock lock(mutex);
        const auto found = entries.find(key);
        if (found == entries.end()) return {};
        recent.splice(recent.begin(), recent, found->second.position);
        return found->second.samples;
    }
    void put(const std::string& key, Samples samples)
    {
        if (!samples || samples->empty()) return;
        const auto bytes = samples->size() * sizeof(float);
        if (bytes > byteLimit || entryLimit == 0) return;
        const std::scoped_lock lock(mutex);
        if (const auto found = entries.find(key); found != entries.end())
        {
            recent.splice(recent.begin(), recent, found->second.position);
            return;
        }
        while (!recent.empty() && (usedBytes + bytes > byteLimit || entries.size() >= entryLimit))
        {
            const auto last = entries.find(recent.back());
            usedBytes -= last->second.samples->size() * sizeof(float);
            entries.erase(last);
            recent.pop_back();
        }
        recent.push_front(key);
        entries.emplace(key, Entry{std::move(samples), recent.begin()});
        usedBytes += bytes;
    }
private:
    struct Entry { Samples samples; std::list<std::string>::iterator position; };
    std::mutex mutex;
    std::list<std::string> recent;
    std::unordered_map<std::string, Entry> entries;
    std::size_t usedBytes = 0;
    const std::size_t byteLimit, entryLimit;
};
}

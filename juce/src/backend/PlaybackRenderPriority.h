#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

namespace hachi::backend
{
// A sounding interval first, then the future from near to far, then the past
// from near to far. Untimed (standalone/headless) work keeps submission order.
inline std::pair<int, double> playbackRenderRank(double start, double end, double position)
{
    if (!std::isfinite(start) || !std::isfinite(end)) return {3, 0};
    if (!std::isfinite(position)) position = 0;
    end = std::max(start, end);
    if (start <= position && end > position) return {0, 0};
    if (start >= position) return {1, start - position};
    return {2, position - end};
}

// Claim indices only: the request, neighbour relationships, cache keys and
// chronological mixing order must never be reordered along with synthesis.
class PlaybackNoteOrder
{
public:
    explicit PlaybackNoteOrder(std::vector<std::pair<double, double>> spansToUse)
        : spans(std::move(spansToUse)), pending(spans.size())
    { std::iota(pending.begin(), pending.end(), std::size_t{0}); }

    std::optional<std::size_t> take(std::optional<double> position)
    {
        const std::scoped_lock lock(mutex);
        if (pending.empty()) return {};
        auto best = pending.begin();
        if (position)
            best = std::min_element(pending.begin(), pending.end(), [&](auto a, auto b)
            {
                return playbackRenderRank(spans[a].first, spans[a].second, *position)
                     < playbackRenderRank(spans[b].first, spans[b].second, *position);
            });
        const auto index = *best;
        pending.erase(best);
        return index;
    }
private:
    std::vector<std::pair<double, double>> spans;
    std::vector<std::size_t> pending;
    std::mutex mutex;
};
}

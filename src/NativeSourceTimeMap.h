#pragma once
#include "ProjectModel.h"
#include <algorithm>
#include <cmath>

namespace hachi
{
// Shared with the native renderer: manual anchors take precedence; otherwise
// consonant timing and velocity are converted into the same monotonic map.
inline std::vector<SourceTimePoint> nativeSourceTimeMap(const ClipData& clip)
{
    std::vector<SourceTimePoint> result;
    const auto sourceDuration = clip.sourceDurationSeconds > 1.0e-9
        ? clip.sourceDurationSeconds : clip.durationSeconds;
    std::vector<SourceTimePoint> timeAnchors;
    if (!clip.sourceTimeMap.empty())
    {
        timeAnchors.reserve(clip.sourceTimeMap.size());
        for (const auto& point : clip.sourceTimeMap)
            timeAnchors.push_back({ juce::jlimit(0.0, clip.durationSeconds, point.targetSeconds),
                                    juce::jlimit(0.0, sourceDuration, point.sourceSeconds) });
    }
    else
    {
        timeAnchors.push_back({ 0.0, 0.0 });
        for (const auto& note : clip.notes)
        {
            const auto noteTargetStart = juce::jlimit(0.0, clip.durationSeconds, note.startSeconds);
            const auto sourceStart = clip.durationSeconds > 1.0e-9
                ? noteTargetStart / clip.durationSeconds * sourceDuration : 0.0;
            timeAnchors.push_back({ noteTargetStart, sourceStart });
            if (note.consonantSeconds <= 1.0e-6 || note.attackSpeed <= 1.0e-6f) continue;
            const auto targetAttack = juce::jlimit(noteTargetStart, clip.durationSeconds,
                noteTargetStart + note.consonantSeconds);
            const auto sourceAttack = juce::jlimit(sourceStart, sourceDuration,
                sourceStart + note.consonantSeconds * static_cast<double>(note.attackSpeed));
            timeAnchors.push_back({ targetAttack, sourceAttack });
        }
    }
    timeAnchors.push_back({ clip.durationSeconds, sourceDuration });
    std::stable_sort(timeAnchors.begin(), timeAnchors.end(), [](const auto& left, const auto& right)
    {
        if (std::abs(left.targetSeconds - right.targetSeconds) > 1.0e-9)
            return left.targetSeconds < right.targetSeconds;
        return left.sourceSeconds < right.sourceSeconds;
    });
    for (const auto& anchor : timeAnchors)
    {
        if (result.empty())
        {
            result.push_back(anchor);
            continue;
        }
        auto& previous = result.back();
        if (std::abs(anchor.targetSeconds - previous.targetSeconds) <= 1.0e-7)
        {
            previous.sourceSeconds = std::max(previous.sourceSeconds, anchor.sourceSeconds);
            continue;
        }
        if (anchor.sourceSeconds > previous.sourceSeconds + 1.0e-7)
            result.push_back(anchor);
    }
    if (result.empty() || result.back().targetSeconds < clip.durationSeconds - 1.0e-7)
        result.push_back({ clip.durationSeconds, sourceDuration });
    return result;
}

inline double nativeSourceTimeAt(const std::vector<SourceTimePoint>& map, double target)
{
    if (map.empty()) return target;
    if (target <= map.front().targetSeconds) return map.front().sourceSeconds;
    if (target >= map.back().targetSeconds) return map.back().sourceSeconds;
    const auto next = std::upper_bound(map.begin(), map.end(), target,
        [](double t, const auto& point) { return t < point.targetSeconds; });
    const auto& previous = *(next - 1);
    const auto span = next->targetSeconds - previous.targetSeconds;
    return span <= 1.0e-9 ? next->sourceSeconds
        : previous.sourceSeconds + (next->sourceSeconds - previous.sourceSeconds)
            * (target - previous.targetSeconds) / span;
}

inline double nativeTargetTimeAt(const std::vector<SourceTimePoint>& map, double source)
{
    if (map.empty()) return source;
    if (source <= map.front().sourceSeconds) return map.front().targetSeconds;
    if (source >= map.back().sourceSeconds) return map.back().targetSeconds;
    const auto next = std::upper_bound(map.begin(), map.end(), source,
        [](double t, const auto& point) { return t < point.sourceSeconds; });
    const auto& previous = *(next - 1);
    const auto span = next->sourceSeconds - previous.sourceSeconds;
    return span <= 1.0e-9 ? next->targetSeconds
        : previous.targetSeconds + (next->targetSeconds - previous.targetSeconds)
            * (source - previous.sourceSeconds) / span;
}

// The renderer removes the empty padding introduced by region edge extension.
// forView merged children also place their audio window inside the parent.
inline ClipData nativeAudioPreviewClip(ClipData clip)
{
    if (clip.audioDurationSeconds >= 0.0)
    {
        const auto offset = clip.audioStartSeconds;
        clip.startSeconds += offset;
        clip.durationSeconds = clip.audioDurationSeconds;
        for (auto& note : clip.notes) note.startSeconds -= offset;
        for (auto& point : clip.sourceTimeMap) point.targetSeconds -= offset;
        clip.audioStartSeconds = 0.0;
        clip.audioDurationSeconds = -1.0;
    }
    return clip;
}
}

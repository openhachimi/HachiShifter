#pragma once
#include "ClipParts.h"
#include "backend/AmplitudeEnvelopeCurve.h"

namespace hachi
{
inline float nativeEnvelopeDbAt(const std::vector<AmplitudeEnvelopePoint>& points, double time)
{
    if (points.empty()) return 0;
    const auto right = std::upper_bound(points.begin(), points.end(), time,
        [](double t, const auto& p) { return t < p.timeSeconds; });
    if (right == points.begin()) return right->gainDb;
    if (right == points.end()) return points.back().gainDb;
    const auto& left = *std::prev(right);
    const auto span = right->timeSeconds - left.timeSeconds;
    const auto fraction = span > 1.e-9 ? static_cast<float>((time - left.timeSeconds) / span) : 0.f;
    return backend::envelopeDbBetween(left.gainDb, right->gainDb, fraction, left.linearToNext);
}

struct NativeSharedEnvelopeMember
{
    double noteStart = 0, noteEnd = 0;
    std::shared_ptr<const std::vector<AmplitudeEnvelopePoint>> points; // absolute time, base included
    std::vector<AmplitudeEnvelopePoint> relativePoints() const
    {
        auto result = *points;
        for (auto& point : result) point.timeSeconds -= noteStart;
        return result;
    }
};
using NativeSharedEnvelopes = std::map<juce::String, NativeSharedEnvelopeMember>;

// Use exactly the pitch-line membership rules: same recording splits and
// explicit linked material, with independent overlapping voices excluded.
inline NativeSharedEnvelopes nativeSharedEnvelopes(const TrackData& track)
{
    if (!trackShowsAllNativeRegions(track)) return {};
    // Pitch independence must not cut the shared gain curve or change levels.
    const auto lines = sharedPitchLines(track, {}, nullptr, true);
    struct Entry { NoteData note; double start, end; };
    std::map<const SharedPitchLine*, std::vector<Entry>> groups;
    for (const auto& parent : track.clips)
        for (const auto& clip : expandedClipParts(parent))
            for (const auto& note : clip.notes)
                if (const auto* member = lines.memberFor(note.id); member && member->nativeSharedCurve)
                    groups[member->line.get()].push_back({note, clip.startSeconds + note.startSeconds,
                        clip.startSeconds + note.startSeconds + note.durationSeconds});
    NativeSharedEnvelopes result;
    for (auto& [key, notes] : groups)
    {
        juce::ignoreUnused(key);
        std::stable_sort(notes.begin(), notes.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
        auto curve = std::make_shared<std::vector<AmplitudeEnvelopePoint>>();
        for (std::size_t i = 0; i < notes.size(); ++i)
        {
            const auto& entry = notes[i];
            auto own = entry.note.amplitudeEnvelope;
            if (own.empty()) own = {{0, 0, true}, {entry.note.durationSeconds, 0, true}};
            own = scaledAmplitudeEnvelope(own, entry.note.amplitudeEnvelopeBasePercent);
            for (auto point : own)
            {
                // Nominal cut endpoints are not independent attacks/releases.
                // A deliberately inserted control point on a seam is retained.
                if (!point.nativeSeamAnchor && ((i > 0 && std::abs(point.timeSeconds) < 1.e-7)
                    || (i + 1 < notes.size() && std::abs(point.timeSeconds - entry.note.durationSeconds) < 1.e-7))) continue;
                point.timeSeconds += entry.start; curve->push_back(point);
            }
        }
        std::stable_sort(curve->begin(), curve->end(), [](const auto& a, const auto& b) { return a.timeSeconds < b.timeSeconds; });
        std::vector<AmplitudeEnvelopePoint> unique;
        for (const auto& point : *curve)
            if (!unique.empty() && std::abs(unique.back().timeSeconds - point.timeSeconds) < 1.e-7)
                unique.back() = point;
            else unique.push_back(point);
        *curve = std::move(unique);
        if (curve->size() < 2) continue;
        for (const auto& entry : notes) result[entry.note.id] = {entry.start, entry.end, curve};
    }
    return result;
}

// Store the portion heard by each fragment, so disconnect/copy/reopen can
// preserve its level. Synthesized cut endpoints are not new group handles.
inline std::vector<AmplitudeEnvelopePoint> sliceNativeSharedEnvelope(
    const std::vector<AmplitudeEnvelopePoint>& absolute, double start, double end)
{
    const auto boundary = [&](double time)
    {
        for (auto point : absolute)
            if (std::abs(point.timeSeconds - time) < 1.e-7)
            { point.timeSeconds = time - start; point.nativeSeamAnchor = true; return point; }
        const auto right = std::upper_bound(absolute.begin(), absolute.end(), time,
            [](double t, const auto& p) { return t < p.timeSeconds; });
        const auto linear = right != absolute.begin() && std::prev(right)->linearToNext;
        return AmplitudeEnvelopePoint {time - start, nativeEnvelopeDbAt(absolute, time), linear, false};
    };
    std::vector<AmplitudeEnvelopePoint> result {boundary(start)};
    for (auto point : absolute)
        if (point.timeSeconds > start + 1.e-7 && point.timeSeconds < end - 1.e-7)
        { point.timeSeconds -= start; result.push_back(point); }
    result.push_back(boundary(end));
    return result;
}

inline void materializeNativeSharedEnvelope(NoteData& note, const NativeSharedEnvelopes& groups)
{
    const auto found = groups.find(note.id);
    if (found == groups.end()) return;
    const auto& member = found->second;
    note.amplitudeEnvelope = unscaledAmplitudeEnvelope(
        sliceNativeSharedEnvelope(*member.points, member.noteStart, member.noteEnd), note.amplitudeEnvelopeBasePercent);
}
}

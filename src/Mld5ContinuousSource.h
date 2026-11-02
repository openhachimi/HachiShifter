#pragma once
#include "NativeSourceTimeMap.h"
#include "backend/RenderService.h"
#include <array>
#include <map>
#include <numeric>

namespace hachi
{
struct Mld5ContinuousSourcePlan
{
    // Chronological, corresponding pairs; these point at the caller's immutable clips.
    std::vector<const ClipData*> clips;
    std::vector<backend::Mld5FileRenderRequest> requests;
    backend::Mld5FileRenderRequest merged;
    double startSeconds = 0, endSeconds = 0;
    bool ready = false;
    juce::String error;
};

namespace mld5_continuous_detail
{
using Request = backend::Mld5FileRenderRequest;
using Curve = std::vector<float> Request::*;
inline constexpr std::array<Curve, 9> curves {{ &Request::sourceMidi, &Request::targetMidi,
    &Request::formantSemitones, &Request::noteGain, &Request::tension,
    &Request::breath, &Request::robustPitchCurve,&Request::mld5SourceMidiHint,&Request::mld5TargetMidiIntent }};
inline float control(const Request& r, std::size_t lane, double local)
{
    const auto& v = r.*curves[lane];
    if (v.empty()) return lane == 3 ? 1.f : lane == 1 ? control(r, 0, local) : 0.f;
    const double x = std::clamp(local / (r.framePeriodMs * .001), 0., double(v.size() - 1));
    const auto a = std::size_t(x), b = std::min(a + 1, v.size() - 1);
    // Match Mld5Renderer's target-grid UV rule, never interpolate a fake F0.
    if ((lane < 2||lane>=7) && (v[a] <= 0 || v[b] <= 0)) return v[a];
    return v[a] + float(x - double(a)) * (v[b] - v[a]);
}
inline double sourceAt(const Request& r, double local)
{
    const auto& m = r.timeMap;
    if (m.empty()) return r.sourceOffsetSeconds + local / r.targetDurationSeconds * r.sourceDurationSeconds;
    auto hi = std::upper_bound(m.begin(), m.end(), local,
        [](double x, const auto& p) { return x < p.targetSeconds; });
    if (hi == m.begin()) ++hi;
    if (hi == m.end()) --hi;
    const auto& a = *(hi - 1); const auto& b = *hi;
    return r.sourceOffsetSeconds + a.sourceSeconds + (local - a.targetSeconds)
        * (b.sourceSeconds - a.sourceSeconds) / (b.targetSeconds - a.targetSeconds);
}
inline bool optionsMatch(const Request& a, const Request& b)
{
    return a.pitchBackend == b.pitchBackend && a.mld5SpliceBeforePitch == b.mld5SpliceBeforePitch && a.stretchAlgorithm == b.stretchAlgorithm
        && a.framePeriodMs == b.framePeriodMs && a.sourceRate == b.sourceRate
        && a.sourceBuffer == b.sourceBuffer && a.sourceAssembly == b.sourceAssembly
        && a.normalizeVolume == b.normalizeVolume && a.nsfSmoothPitchTransitions == b.nsfSmoothPitchTransitions
        && a.nsfNoiseProtection == b.nsfNoiseProtection && a.matchNsfSourceLevel == b.matchNsfSourceLevel
        && a.hifiganModelDirectory == b.hifiganModelDirectory && a.exportComponent == b.exportComponent
        && a.inference.requested == b.inference.requested && a.inference.deviceIndex == b.inference.deviceIndex
        && a.inference.intraOpThreads == b.inference.intraOpThreads
        && a.nsfIncrementalRender == b.nsfIncrementalRender
        && a.neuralGuardStartSeconds == b.neuralGuardStartSeconds && a.neuralGuardEndSeconds == b.neuralGuardEndSeconds
        && a.neighborEdgeF0Start == b.neighborEdgeF0Start && a.neighborEdgeF0End == b.neighborEdgeF0End;
}
inline void uniqueTimes(std::vector<double>& times)
{
    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end(), [](double a, double b) { return std::abs(a-b) < 1.e-10; }), times.end());
}
}

// No media reads, cache changes, curve editing or DSP. The caller owns grouping,
// one synthesis, slicing back to owners, and preventing double contribution.
[[nodiscard]] inline Mld5ContinuousSourcePlan planMld5ContinuousSource(
    const std::vector<const ClipData*>& clips,
    const std::vector<backend::Mld5FileRenderRequest>& requests,
    const std::map<juce::String, juce::String>& ownerKeys, double sourceSampleRate = 48000.)
{
    using namespace mld5_continuous_detail;
    Mld5ContinuousSourcePlan result;
    const auto fail = [&](const char* why) { Mld5ContinuousSourcePlan p; p.error = why; return p; };
    if (clips.size() < 2 || clips.size() != requests.size()) return fail("Need at least two corresponding clips/requests");
    if (!std::isfinite(sourceSampleRate) || sourceSampleRate <= 0) return fail("Invalid source sample rate");
    const double sampleTolerance = .5 / sourceSampleRate, epsilon = 1.e-9;
    if (!std::isfinite(sampleTolerance)) return fail("Invalid source sample tolerance");
    std::vector<std::size_t> order(clips.size()); std::iota(order.begin(), order.end(), 0);
    for (const auto* c : clips)
        if (!c || !std::isfinite(c->startSeconds) || !std::isfinite(c->durationSeconds)
            || !std::isfinite(c->sourceOffsetSeconds) || !std::isfinite(c->sourceDurationSeconds)
            || !std::isfinite(c->startSeconds + c->durationSeconds)) return fail("Null clip or non-finite clip geometry");
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) { return clips[a]->startSeconds < clips[b]->startSeconds; });
    juce::String materialId, fingerprint;
    for (const auto i : order)
    {
        const auto& c = *clips[i]; const auto& r = requests[i];
        const double sourceDuration = c.sourceDurationSeconds > epsilon ? c.sourceDurationSeconds : c.durationSeconds;
        if (!std::isfinite(c.startSeconds) || !std::isfinite(c.durationSeconds) || c.durationSeconds <= 0
            || !std::isfinite(r.targetDurationSeconds) || !std::isfinite(r.sourceDurationSeconds)
            || !std::isfinite(r.sourceOffsetSeconds) || !std::isfinite(r.sourceOffsetSeconds + r.sourceDurationSeconds)
            || r.sourceOffsetSeconds < 0 || r.sourceDurationSeconds <= 0
            || r.sourceFile == juce::File{} || r.sourceFile != c.sourceFile || !c.parts.empty()
            || std::abs(r.targetDurationSeconds - c.durationSeconds) > epsilon
            || std::abs(r.sourceOffsetSeconds - c.sourceOffsetSeconds) > epsilon
            || std::abs(r.sourceDurationSeconds - sourceDuration) > epsilon)
            return fail("Clip/request source or duration mismatch");
        if (r.pitchBackend != backend::PitchRenderBackend::mld5 || r.sourceAssembly
            || !std::isfinite(r.framePeriodMs) || r.framePeriodMs <= 0
            || !std::isfinite(r.sourceRate) || r.sourceRate < 0 || !optionsMatch(requests[order.front()], r))
            return fail("Incompatible engine/options/frame periods");
        if (c.sourceFile != clips[order.front()]->sourceFile) return fail("Different physical source file");
        if (c.materialSourceFingerprint.isNotEmpty())
        {
            if (fingerprint.isNotEmpty() && fingerprint != c.materialSourceFingerprint) return fail("Different source fingerprints");
            fingerprint = c.materialSourceFingerprint;
        }
        for (const auto& note : c.notes) if (note.materialAudioId.isNotEmpty())
        {
            if (materialId.isNotEmpty() && materialId != note.materialAudioId) return fail("Different annotation variants");
            materialId = note.materialAudioId;
        }
        // Validate RAW anchors before any native helper could sort/max-repair them.
        const auto validMap = [&](const auto& map)
        {
            if (map.empty()) return true;
            if (map.size() < 2 || std::abs(map.front().targetSeconds) > epsilon
                || std::abs(map.back().targetSeconds - c.durationSeconds) > epsilon) return false;
            for (std::size_t k = 0; k < map.size(); ++k)
            {
                const auto& p = map[k];
                if (!std::isfinite(p.targetSeconds) || !std::isfinite(p.sourceSeconds)
                    || p.targetSeconds < -epsilon || p.targetSeconds > c.durationSeconds + epsilon
                    || p.sourceSeconds < -epsilon || p.sourceSeconds > r.sourceDurationSeconds + sampleTolerance
                    || (k && (p.targetSeconds <= map[k-1].targetSeconds || p.sourceSeconds <= map[k-1].sourceSeconds))) return false;
            }
            return true;
        };
        if (!validMap(c.sourceTimeMap) || !validMap(r.timeMap)) return fail("Non-forward or invalid source clock");
        const auto nativeMap = nativeSourceTimeMap(c);
        // Request map is already the LOCAL read-source clock: never apply it to F0 again.
        for (const auto& p : nativeMap)
            if (std::abs(sourceAt(r, p.targetSeconds) - c.sourceOffsetSeconds - p.sourceSeconds) > sampleTolerance)
                return fail("Request/native source clock mismatch");
        for (const auto& p : r.timeMap)
            if (std::abs(sourceAt(r, p.targetSeconds) - c.sourceOffsetSeconds - nativeSourceTimeAt(nativeMap, p.targetSeconds)) > sampleTolerance)
                return fail("Request/native middle source clock mismatch");
        for (const auto member : curves) for (const float value : r.*member)
            if (!std::isfinite(value)) return fail("Non-finite authored control");
        if (r.targetDurationSeconds / (r.framePeriodMs * .001) > 10000000.) return fail("Excessive control grid");
        result.clips.push_back(&c); result.requests.push_back(r);
    }
    const auto physicalOwner = [&](const ClipData& c)
    {
        const auto found = ownerKeys.find(c.id);
        return found == ownerKeys.end() ? c.id : found->second;
    };
    result.startSeconds = result.clips.front()->startSeconds;
    result.endSeconds = result.startSeconds;
    double sourceBegin = result.requests.front().sourceOffsetSeconds, sourceEnd = sourceBegin;
    for (std::size_t i = 0; i < result.clips.size(); ++i)
    {
        const auto& c = *result.clips[i]; const auto& r = result.requests[i];
        if (i)
        {
            const auto& previous = *result.clips[i-1];
            const auto owner = physicalOwner(c), previousOwner = physicalOwner(previous);
            const bool sameOwner = owner.isNotEmpty() && owner == previousOwner;
            const bool sameUsage = c.materialSourceUsageGroupId.isNotEmpty()
                && c.materialSourceUsageGroupId == previous.materialSourceUsageGroupId;
            if (!sameOwner && !sameUsage && !(previous.glideConnectedToNext && c.glideConnectedFromPrevious))
                return fail("No physical-owner/source-use/bidirectional-glide identity");
            if (c.startSeconds > result.endSeconds + epsilon) return fail("Target gap");
            if (r.sourceOffsetSeconds <= result.requests[i-1].sourceOffsetSeconds + epsilon) return fail("Source offset does not advance to a new physical slice");
            if(c.startSeconds+c.durationSeconds<=result.endSeconds+epsilon)return fail("Contained or duplicated owner is a layer, not source continuation");
        }
        for (std::size_t j = 0; j < i; ++j)
        {
            const auto& p = *result.clips[j]; const auto& q = result.requests[j];
            const double from = c.startSeconds, to = std::min(c.startSeconds + c.durationSeconds, p.startSeconds + p.durationSeconds);
            if (to < from - epsilon) continue;
            std::vector<double> knots { from, std::max(from, to) };
            const auto add = [&](double t) { if (t > from && t < to) knots.push_back(t); };
            for (const auto& point : r.timeMap) add(c.startSeconds + point.targetSeconds);
            for (const auto& point : q.timeMap) add(p.startSeconds + point.targetSeconds);
            for (const auto pair : { std::make_pair(&c, &r), std::make_pair(&p, &q) })
                for (std::size_t k = 0; double(k) * pair.second->framePeriodMs * .001 < pair.first->durationSeconds; ++k)
                    add(pair.first->startSeconds + double(k) * pair.second->framePeriodMs * .001);
            uniqueTimes(knots);
            // Also sample interval interiors: UV control interpolation has one-sided steps.
            const auto nodes = knots;
            for (std::size_t k = 1; k < nodes.size(); ++k)
            {
                knots.push_back(nodes[k-1] + (nodes[k] - nodes[k-1]) * .25);
                knots.push_back(nodes[k-1] + (nodes[k] - nodes[k-1]) * .75);
            }
            for (const double t : knots)
            {
                if (std::abs(sourceAt(r, t-c.startSeconds) - sourceAt(q, t-p.startSeconds)) > sampleTolerance)
                    return fail("Conflicting overlapping/boundary source clock");
                // A mere touch may join different notes; no positive overlap to reconcile.
                if (to <= from + epsilon) continue;
                for (std::size_t lane = 0; lane < curves.size(); ++lane)
                    if (std::abs(control(r,lane,t-c.startSeconds) - control(q,lane,t-p.startSeconds)) > 1.e-5f)
                        return fail("Conflicting authored controls in overlap");
            }
        }
        result.endSeconds = std::max(result.endSeconds, c.startSeconds + c.durationSeconds);
        sourceEnd = std::max(sourceEnd, r.sourceOffsetSeconds + r.sourceDurationSeconds);
    }
    result.merged = result.requests.front(); auto& merged = result.merged;
    merged.sourceOffsetSeconds = sourceBegin; merged.sourceDurationSeconds = sourceEnd - sourceBegin;
    merged.targetDurationSeconds = result.endSeconds - result.startSeconds;
    merged.pitchBackend = backend::PitchRenderBackend::mld5; merged.isGlideMerged = true;
    merged.preserveUneditedSource = std::all_of(result.requests.begin(),result.requests.end(),[](const auto& r){ return r.preserveUneditedSource; });
    merged.timeMap.clear();
    std::vector<double> times;
    for (std::size_t i = 0; i < result.clips.size(); ++i)
    {
        const auto& c = *result.clips[i]; times.push_back(c.startSeconds); times.push_back(c.startSeconds + c.durationSeconds);
        for (const auto& p : result.requests[i].timeMap) times.push_back(c.startSeconds + p.targetSeconds);
    }
    uniqueTimes(times);
    const auto ownerAt = [&](double t)
    {
        std::size_t i = 0;
        while (i + 1 < result.clips.size() && t > result.clips[i]->startSeconds + result.clips[i]->durationSeconds + epsilon) ++i;
        return i; // All overlaps were proved equal; choose, never average.
    };
    for (const double t : times)
    {
        const auto i = ownerAt(t);
        backend::TimeMapPoint point { t-result.startSeconds, sourceAt(result.requests[i], t-result.clips[i]->startSeconds)-sourceBegin };
        if (!merged.timeMap.empty() && (point.targetSeconds <= merged.timeMap.back().targetSeconds
            || point.sourceSeconds <= merged.timeMap.back().sourceSeconds)) return fail("Merged clock is not strictly forward");
        merged.timeMap.push_back(point);
    }
    const double step = merged.framePeriodMs * .001;
    const double frames = std::ceil(merged.targetDurationSeconds / step) + 1;
    if (!std::isfinite(frames) || frames > 10000000.) return fail("Excessive merged control grid");
    for (const auto member : curves) (merged.*member).clear();
    for (std::size_t frame = 0; frame < std::size_t(frames); ++frame)
    {
        const double t = result.startSeconds + std::min(merged.targetDurationSeconds, double(frame)*step);
        const auto i = ownerAt(t);
        for (std::size_t lane = 0; lane < curves.size(); ++lane)
            (merged.*curves[lane]).push_back(control(result.requests[i],lane,t-result.clips[i]->startSeconds));
    }
    result.ready = true; return result;
}
}

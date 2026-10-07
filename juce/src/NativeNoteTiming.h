#pragma once
#include "NativeSourceTimeMap.h"
#include "ClipParts.h"
#include <optional>
#include <limits>

namespace hachi
{
inline std::vector<SourceTimePoint> nativeClipClock(const ClipData& clip)
{
    auto map = nativeSourceTimeMap(nativeAudioPreviewClip(clip));
    for (auto& point : map) point.targetSeconds += clip.audioStartSeconds;
    return map;
}

// Bind each split to its own source interval. Older splits copied the whole
// syllable's segment metadata into both halves; the shared audio clock is truth.
inline void bindNativeNoteSource(NoteData& note, const ClipData& clip,
                                 const std::vector<SourceTimePoint>* clock = nullptr)
{
    const auto localMap = clock ? std::vector<SourceTimePoint>{} : nativeClipClock(clip);
    const auto& map = clock ? *clock : localMap;
    const auto start = clip.sourceOffsetSeconds + nativeSourceTimeAt(map, note.startSeconds);
    const auto end = clip.sourceOffsetSeconds + nativeSourceTimeAt(map, note.startSeconds + note.durationSeconds);
    const auto oldStart = note.nativeSourceStartSeconds >= 0 ? note.nativeSourceStartSeconds : start;
    std::vector<NativeSegment> segments;
    for (auto segment : note.nativeSegments)
    {
        const auto first = std::max(start, oldStart + segment.sourceStartSeconds);
        const auto last = std::min(end, oldStart + segment.sourceEndSeconds);
        if (last <= first + 1.0e-9) continue;
        segment.sourceStartSeconds = first - start;
        segment.sourceEndSeconds = last - start;
        segment.alignmentSeconds = juce::jlimit(first, last, oldStart + segment.alignmentSeconds) - start;
        segments.push_back(std::move(segment));
    }
    note.nativeSourceStartSeconds = start;
    note.nativeSourceEndSeconds = end;
    note.nativeSegments = std::move(segments);
}

enum class NativeNoteTimeEdit { move, leftEdge, rightEdge };

struct NativeNoteMovePlan
{
    ClipData clip;
    double delta = 0;
};

inline std::optional<NativeNoteMovePlan> planNativeNoteMove(
    const ClipData& original, const std::vector<juce::String>& ids,
    double requested, float pitch, NativeNoteTimeEdit edit = NativeNoteTimeEdit::move)
{
    if (!std::isfinite(requested) || !std::isfinite(pitch)) return {};
    const auto selected = [&](const NoteData& note)
    { return std::find(ids.begin(), ids.end(), note.id) != ids.end(); };
    if (std::none_of(original.notes.begin(), original.notes.end(), selected)) return {};
    if (std::abs(requested)<1.e-9 && std::abs(pitch)<1.e-6f) return NativeNoteMovePlan {original,0};
    if (!original.parts.empty() && original.nativeAudioLinked)
    {
        // An identity clock describes the common editor time axis. Its warped
        // result maps NEW time back to OLD time, independently of source files.
        auto proxy=original;proxy.parts.clear();proxy.nativeAudioLinked=false;
        proxy.audioStartSeconds=0;proxy.audioDurationSeconds=-1;
        proxy.sourceOffsetSeconds=0;proxy.sourceDurationSeconds=proxy.durationSeconds;
        proxy.sourceTimeMap={{0,0},{proxy.durationSeconds,proxy.durationSeconds}};
        auto plan=planNativeNoteMove(proxy,ids,requested,pitch,edit);if(!plan)return {};
        const auto commonClock=plan->clip.sourceTimeMap;
        const auto warp=[&](double t){return nativeTargetTimeAt(commonClock,t);};
        auto result=std::move(plan->clip);result.nativeAudioLinked=true;
        result.sourceOffsetSeconds=original.sourceOffsetSeconds;result.sourceDurationSeconds=original.sourceDurationSeconds;
        result.sourceTimeMap=original.sourceTimeMap;result.parts=original.parts;
        for(auto& part:result.parts)
        {
            const auto old=std::find_if(original.parts.begin(),original.parts.end(),[&](const auto& p){return p.id==part.id;});
            auto source=*old;
            for(auto note:original.notes)if(note.clipPartId==part.id)
            {note.startSeconds-=old->startSeconds;source.notes.push_back(std::move(note));}
            const auto clock=nativeClipClock(source);const auto offset=old->startSeconds;
            const auto newStart=warp(offset);
            part.startSeconds=newStart;part.durationSeconds=warp(offset+old->durationSeconds)-newStart;
            part.audioStartSeconds=warp(offset+old->audioStartSeconds)-newStart;
            part.audioDurationSeconds=warp(offset+old->audioStartSeconds+old->audioLength())-newStart-part.audioStartSeconds;
            auto anchors=clock;
            // Include every common warp breakpoint inside this child's audio.
            for(const auto& point:commonClock)
                if(point.sourceSeconds>offset+old->audioStartSeconds && point.sourceSeconds<offset+old->audioStartSeconds+old->audioLength())
                    anchors.push_back({point.sourceSeconds-offset,nativeSourceTimeAt(clock,point.sourceSeconds-offset)});
            std::stable_sort(anchors.begin(),anchors.end(),[](const auto& a,const auto& b){return a.targetSeconds<b.targetSeconds;});
            part.sourceTimeMap.clear();
            for(auto point:anchors)
            {
                point.targetSeconds=warp(offset+point.targetSeconds)-newStart;
                if(part.sourceTimeMap.empty()||point.targetSeconds>part.sourceTimeMap.back().targetSeconds+1.e-7)
                    part.sourceTimeMap.push_back(point);
            }
            const auto gains=[&](auto& curve){for(auto& point:curve)point.timeSeconds=warp(offset+point.timeSeconds)-newStart;};
            gains(part.gainEnvelope);for(auto& layer:part.inheritedGainEnvelopes)gains(layer);
            for(auto& note:result.notes)if(note.clipPartId==part.id)
            {
                auto bound=std::find_if(source.notes.begin(),source.notes.end(),[&](const auto& n){return n.id==note.id;});
                auto originalNote=*bound;bindNativeNoteSource(originalNote,source,&clock);
                note.nativeSourceStartSeconds=originalNote.nativeSourceStartSeconds;
                note.nativeSourceEndSeconds=originalNote.nativeSourceEndSeconds;note.nativeSegments=std::move(originalNote.nativeSegments);
            }
        }
        return NativeNoteMovePlan{std::move(result),plan->delta};
    }
    // Merged regions keep independent recordings and clocks. Edit each owned
    // source child, then put its notes back into the flat parent editor.
    if (!original.parts.empty())
    {
        auto result = original;
        std::vector<NativeNoteMovePlan> plans;
        double delta = requested;
        for (const auto& child : expandedClipParts(original))
            if (auto plan = planNativeNoteMove(child, ids, delta, pitch, edit))
            { delta = requested < 0 ? std::max(delta, plan->delta) : std::min(delta, plan->delta); }
        double start = original.startSeconds, end = original.startSeconds + original.durationSeconds;
        for (const auto& child : expandedClipParts(original))
            if (auto plan = planNativeNoteMove(child, ids, delta, pitch, edit))
            {
                start = std::min(start, plan->clip.startSeconds);
                end = std::max(end, plan->clip.startSeconds + plan->clip.durationSeconds);
                plans.push_back(std::move(*plan));
            }
        const auto shift = original.startSeconds - start;
        result.startSeconds = start; result.durationSeconds = end - start;
        for (auto& note : result.notes) note.startSeconds += shift;
        for (auto& part : result.parts) part.startSeconds += shift;
        shiftClipGainEnvelopes(result, shift);
        for (auto& plan : plans)
        {
            const auto part = std::find_if(result.parts.begin(), result.parts.end(),
                [&](const auto& child) { return original.id + ":" + child.id == plan.clip.id; });
            if (part == result.parts.end()) return {};
            const auto id = part->id;
            const auto offset = plan.clip.startSeconds - start;
            for (auto note : plan.clip.notes)
            {
                note.startSeconds += offset;
                const auto target = std::find_if(result.notes.begin(), result.notes.end(),
                    [&](const auto& value) { return value.id == note.id; });
                if (target != result.notes.end()) *target = std::move(note);
            }
            plan.clip.id = id; plan.clip.startSeconds = offset; plan.clip.notes.clear();
            // Parent gain layers are inherited during expansion, not stored
            // a second time in the source child.
            plan.clip.inheritedGainEnvelopes = part->inheritedGainEnvelopes;
            plan.clip.gain = part->gain; plan.clip.muted = part->muted;
            plan.clip.fadeInSeconds = part->fadeInSeconds; plan.clip.fadeOutSeconds = part->fadeOutSeconds;
            *part = std::move(plan.clip);
        }
        return NativeNoteMovePlan { std::move(result), delta };
    }

    // An edge stretch changes only the edited note's length. Propagate the
    // boundary displacement outwards through touching notes so its neighbors
    // move rigidly instead of losing part of their recording.
    std::vector<double> movingEdges;
    const auto edgeMoves = [&](double time)
    {
        return std::any_of(movingEdges.begin(), movingEdges.end(),
            [&](double value) { return std::abs(value - time) < 1.0e-7; });
    };
    if (edit != NativeNoteTimeEdit::move)
    {
        for (const auto& note : original.notes)
            if (selected(note))
                movingEdges.push_back(edit == NativeNoteTimeEdit::leftEdge ? note.startSeconds
                    : note.startSeconds + note.durationSeconds);
        bool changed = true;
        while (changed)
        {
            changed = false;
            for (const auto& note : original.notes)
            {
                if (selected(note)) continue;
                const auto start = note.startSeconds;
                const auto end = start + note.durationSeconds;
                const auto attached = edit == NativeNoteTimeEdit::leftEdge ? end : start;
                const auto outer = edit == NativeNoteTimeEdit::leftEdge ? start : end;
                if (edgeMoves(attached) && !edgeMoves(outer))
                { movingEdges.push_back(outer); changed = true; }
            }
        }
    }
    struct Boundary { double time; bool moves; };
    std::vector<Boundary> boundaries { {original.audioStartSeconds, false},
        {original.audioStartSeconds + original.audioLength(), false} };
    for (const auto& note : original.notes)
    {
        boundaries.push_back({note.startSeconds, selected(note) && edit != NativeNoteTimeEdit::rightEdge});
        boundaries.push_back({note.startSeconds + note.durationSeconds, selected(note) && edit != NativeNoteTimeEdit::leftEdge});
    }
    if (edit != NativeNoteTimeEdit::move)
        for (auto& boundary : boundaries) boundary.moves = edgeMoves(boundary.time);
    // Analysis normally leaves a short piece of original audio outside the
    // first/last note. Those outer margins travel with their nearest edge;
    // treating them as fixed neighbors locks the whole recording in place.
    const auto first = std::min_element(original.notes.begin(), original.notes.end(),
        [](const auto& a, const auto& b) { return a.startSeconds < b.startSeconds; });
    const auto last = std::max_element(original.notes.begin(), original.notes.end(),
        [](const auto& a, const auto& b)
        { return a.startSeconds + a.durationSeconds < b.startSeconds + b.durationSeconds; });
    if (original.audioStartSeconds <= first->startSeconds + 1.0e-7)
        boundaries[0].moves = edit == NativeNoteTimeEdit::move ? selected(*first) : edgeMoves(first->startSeconds);
    if (original.audioStartSeconds + original.audioLength() >= last->startSeconds + last->durationSeconds - 1.0e-7)
        boundaries[1].moves = edit == NativeNoteTimeEdit::move ? selected(*last)
            : edgeMoves(last->startSeconds + last->durationSeconds);
    std::stable_sort(boundaries.begin(), boundaries.end(),
        [](const auto& a, const auto& b) { return a.time < b.time; });
    std::vector<Boundary> edges;
    for (const auto& edge : boundaries)
        if (!edges.empty() && std::abs(edges.back().time - edge.time) < 1.0e-7)
            edges.back().moves |= edge.moves;
        else edges.push_back(edge);
    double low = -std::numeric_limits<double>::infinity(), high = -low;
    for (const auto& edge : edges)
        if (edge.moves) low = std::max(low, -original.startSeconds - edge.time);
    for (std::size_t i = 1; i < edges.size(); ++i)
    {
        const auto length = edges[i].time - edges[i-1].time;
        const auto minimum = std::min(.01, length);
        const auto slope = int(edges[i].moves) - int(edges[i-1].moves);
        if (slope > 0) low = std::max(low, minimum - length);
        if (slope < 0) high = std::min(high, length - minimum);
    }
    const auto delta = juce::jlimit(low, high, requested);
    const auto warp = [&](double time)
    {
        const auto next = std::upper_bound(edges.begin(), edges.end(), time,
            [](double t, const auto& edge) { return t < edge.time; });
        if (next == edges.begin()) return time + (edges.front().moves ? delta : 0);
        if (next == edges.end()) return time + (edges.back().moves ? delta : 0);
        const auto& prev = *(next-1);
        const auto amount = (time-prev.time)/(next->time-prev.time);
        return time + delta * ((prev.moves ? 1.0 : 0.0)*(1-amount) + (next->moves ? 1.0 : 0.0)*amount);
    };
    auto result = original;
    const auto audioStart = warp(original.audioStartSeconds);
    const auto audioEnd = warp(original.audioStartSeconds + original.audioLength());
    const auto rigid = std::all_of(edges.begin(), edges.end(), [](const auto& edge) { return edge.moves; });
    const auto origin = rigid ? delta : std::min(0.0, audioStart);
    result.startSeconds += origin;
    result.audioStartSeconds = audioStart - origin;
    result.audioDurationSeconds = audioEnd - audioStart;
    result.durationSeconds = std::max(.01, warp(original.durationSeconds) - origin);
    const auto clock = nativeClipClock(original);
    auto map = clock;
    for (const auto& edge : edges)
        if (edge.time >= original.audioStartSeconds - 1.0e-7
            && edge.time <= original.audioStartSeconds + original.audioLength() + 1.0e-7)
            map.push_back({edge.time, nativeSourceTimeAt(clock, edge.time)});
    std::stable_sort(map.begin(), map.end(), [](const auto& a, const auto& b) { return a.targetSeconds < b.targetSeconds; });
    result.sourceTimeMap.clear();
    for (auto point : map)
    {
        point.targetSeconds = warp(point.targetSeconds) - origin;
        if (result.sourceTimeMap.empty() || point.targetSeconds > result.sourceTimeMap.back().targetSeconds + 1.0e-7)
            result.sourceTimeMap.push_back(point);
    }
    for (auto& note : result.notes)
    {
        bindNativeNoteSource(note, original, &clock);
        const auto start = warp(note.startSeconds);
        const auto duration = warp(note.startSeconds + note.durationSeconds) - start;
        const auto ratio = duration / note.durationSeconds;
        const auto local = [&](double t) { return t < 0 ? t : t * ratio; };
        for (auto& point : note.contour) point.timeSeconds = local(point.timeSeconds);
        for (auto& point : note.pitchControlPoints) point.timeSeconds = local(point.timeSeconds);
        for (auto& point : note.diffSingerPitchReference) point.timeSeconds = local(point.timeSeconds);
        for (auto& point : note.diffSingerPitchOffset) point.timeSeconds = local(point.timeSeconds);
        for (auto& curve : note.utauFlagCurves) for (auto& point : curve.points) point.timeSeconds = local(point.timeSeconds);
        for (auto& point : note.amplitudeEnvelope) point.timeSeconds = local(point.timeSeconds);
        for (auto& marker : note.sibilantMarkers) marker = local(marker);
        note.consonantSeconds *= ratio;
        note.startSeconds = start - origin; note.durationSeconds = duration;
        if (selected(note))
        {
            note.midiNote = juce::jlimit(0.f, 127.f, note.midiNote + pitch);
            for (auto& point : note.pitchControlPoints) point.targetMidi = juce::jlimit(0.f,127.f,point.targetMidi + pitch);
        }
        result.durationSeconds = std::max(result.durationSeconds, note.startSeconds + note.durationSeconds);
    }
    const auto gainTime = [&](auto& points) { for (auto& point : points) point.timeSeconds = warp(point.timeSeconds) - origin; };
    gainTime(result.gainEnvelope);
    for (auto& layer : result.inheritedGainEnvelopes) gainTime(layer);
    return NativeNoteMovePlan { std::move(result), delta };
}
}

#pragma once
#include "ProjectModel.h"
#include "TrackGainEnvelope.h"
#include <algorithm>
#include <cmath>

namespace hachi
{
inline std::vector<const ClipData*> clipSourceRegions(const ClipData& clip)
{
    if(clip.parts.empty())return {&clip};
    std::vector<const ClipData*> sources;
    for(const auto& part:clip.parts)sources.push_back(&part);
    return sources;
}
// Keep source data separate while presenting one editable timeline rectangle.
inline std::vector<ClipData> expandedClipParts(const ClipData& parent, bool forView = false)
{
    if (parent.parts.empty()) return {parent};
    auto result = parent.parts;
    for (auto& part : result) part.notes.clear();
    for (const auto& note : parent.notes)
    {
        auto owner = std::find_if(result.begin(), result.end(), [&](const auto& part) { return part.id == note.clipPartId; });
        if (owner == result.end())
            owner = std::min_element(result.begin(), result.end(), [&](const auto& a, const auto& b)
            {
                const auto distance = [&](const auto& p)
                { return std::max({p.startSeconds-note.startSeconds, note.startSeconds-p.startSeconds-p.durationSeconds, 0.0}); };
                return distance(a) < distance(b);
            });
        owner->notes.push_back(note);
    }
    for (auto& part : result)
    {
        const auto offset = part.startSeconds;
        part.showNoteHints = parent.showNoteHints;
        part.showNormalDisplay = parent.showNormalDisplay;
        part.gain *= parent.gain; part.muted = part.muted || parent.muted;
        if (offset < 1.0e-8) part.fadeInSeconds = std::max(part.fadeInSeconds, parent.fadeInSeconds);
        if (std::abs(offset+part.durationSeconds-parent.durationSeconds)<1.0e-8)
            part.fadeOutSeconds = std::max(part.fadeOutSeconds, parent.fadeOutSeconds);
        if (forView)
        {
            shiftClipGainEnvelopes(part,offset);
            if(!parent.gainEnvelope.empty())
            {
                if(!part.gainEnvelope.empty())part.inheritedGainEnvelopes.push_back(std::move(part.gainEnvelope));
                part.gainEnvelope=parent.gainEnvelope;
            }
            part.inheritedGainEnvelopes.insert(part.inheritedGainEnvelopes.end(),parent.inheritedGainEnvelopes.begin(),parent.inheritedGainEnvelopes.end());
            part.id = parent.id;
            part.audioDurationSeconds = part.audioLength();
            part.audioStartSeconds += offset;
            for (auto& point : part.sourceTimeMap) point.targetSeconds += offset;
            for (auto& point : part.nativeTrimClock) point.targetSeconds += offset;
            part.startSeconds = parent.startSeconds;
            part.durationSeconds = parent.durationSeconds;
        }
        else
        {
            const auto inherit=[&](const auto& points)
            {if(!points.empty()){auto local=points;shiftGainEnvelope(local,-offset);part.inheritedGainEnvelopes.push_back(std::move(local));}};
            inherit(parent.gainEnvelope);for(const auto& layer:parent.inheritedGainEnvelopes)inherit(layer);
            part.id = parent.id + ":" + part.id;
            part.startSeconds += parent.startSeconds;
            for (auto& note : part.notes) note.startSeconds -= offset;
            double earliest=0, latest=part.durationSeconds;
            for(const auto& note:part.notes)
            { earliest=std::min(earliest,note.startSeconds);latest=std::max(latest,note.startSeconds+note.durationSeconds); }
            part.audioDurationSeconds=part.audioLength();
            part.audioStartSeconds-=earliest;part.startSeconds+=earliest;part.durationSeconds=latest-earliest;
            for(auto& note:part.notes)note.startSeconds-=earliest;
            for(auto& point:part.sourceTimeMap)point.targetSeconds-=earliest;
            for(auto& point:part.nativeTrimClock)point.targetSeconds-=earliest;
            shiftClipGainEnvelopes(part,-earliest);
        }
    }
    return result;
}

inline void expandProjectClipParts(ProjectData& data, bool forView = false)
{
    for (auto& track : data.tracks)
    {
        if(std::none_of(track.clips.begin(),track.clips.end(),[](const auto& c){return !c.parts.empty();}))continue;
        std::vector<ClipData> expanded;
        for (const auto& clip : track.clips)
        {
            auto parts = expandedClipParts(clip, forView);
            // Only the read-only expanded project carries this metadata.
            // Material assembled through expandedClipParts during real edits
            // must not retain a stale group after disconnecting or copying.
            if (!clip.parts.empty())
                for (std::size_t i = 0; i < parts.size(); ++i)
                    parts[i].nativePitchGroupId = clip.nativeAudioLinked ? clip.id : clip.id + ":" + clip.parts[i].id;
            expanded.insert(expanded.end(), std::make_move_iterator(parts.begin()), std::make_move_iterator(parts.end()));
        }
        track.clips = std::move(expanded);
    }
}

// Crop a source child without changing the speed of the part that remains.
inline std::vector<ClipData> slicedClipParts(const std::vector<ClipData>& parts, double begin, double end, bool backing, bool compose)
{
    std::vector<ClipData> result;
    for (const auto& original : parts)
    {
        const auto first = std::max(begin, original.startSeconds);
        const auto last = std::min(end, original.startSeconds+original.durationSeconds);
        if (last-first < 1.0e-9) continue;
        auto part = original;
        const auto audioBegin = original.startSeconds+original.audioStartSeconds;
        const auto audioLength = original.audioLength();
        const auto cutA = juce::jlimit(0.0,audioLength,first-audioBegin);
        const auto cutB = juce::jlimit(0.0,audioLength,last-audioBegin);
        const auto sourceLength = backing ? audioLength
            : original.sourceDurationSeconds>1.0e-9 ? original.sourceDurationSeconds : audioLength;
        const auto sourceAt = [&](double audioTime)
        {
            if (audioTime<=0) return 0.0;
            if (audioTime>=audioLength) return sourceLength;
            const auto target = original.audioStartSeconds+audioTime;
            if (original.sourceTimeMap.size()>=2 && !backing && compose)
            {
                const auto next = std::upper_bound(original.sourceTimeMap.begin(),original.sourceTimeMap.end(),target,
                    [](double t,const auto& p){return t<p.targetSeconds;});
                if(next==original.sourceTimeMap.begin())return next->sourceSeconds;
                if(next==original.sourceTimeMap.end())return original.sourceTimeMap.back().sourceSeconds;
                const auto& prev=*(next-1);const auto span=next->targetSeconds-prev.targetSeconds;
                return prev.sourceSeconds+(next->sourceSeconds-prev.sourceSeconds)*(span>1.0e-9?(target-prev.targetSeconds)/span:0.0);
            }
            return audioLength>1.0e-9 ? audioTime*sourceLength/audioLength : 0.0;
        };
        const auto sourceA=sourceAt(cutA), sourceB=sourceAt(cutB);
        part.startSeconds=first-begin;part.durationSeconds=last-first;
        for(auto& point:part.nativeTrimClock)point.targetSeconds+=original.startSeconds-first;
        shiftClipGainEnvelopes(part,original.startSeconds-first);anchorClipGainEnvelope(part);
        part.audioStartSeconds=std::min(part.durationSeconds,std::max(0.0,audioBegin-first));
        part.audioDurationSeconds=cutB-cutA;
        part.sourceOffsetSeconds+=sourceA;part.sourceDurationSeconds=sourceB-sourceA;
        part.sourceTimeMap.clear();
        if(!original.sourceTimeMap.empty() && part.audioDurationSeconds>0)
        {
            part.sourceTimeMap.push_back({part.audioStartSeconds,0});
            for(const auto& point:original.sourceTimeMap)
                if(point.targetSeconds>first-original.startSeconds && point.targetSeconds<last-original.startSeconds)
                    part.sourceTimeMap.push_back({point.targetSeconds+original.startSeconds-first,point.sourceSeconds-sourceA});
            part.sourceTimeMap.push_back({part.audioStartSeconds+part.audioLength(),sourceB-sourceA});
        }
        if(cutA>0){part.fadeInSeconds=part.crossfadeInSeconds=0;part.glideConnectedFromPrevious=false;}
        if(cutB<audioLength){part.fadeOutSeconds=part.crossfadeOutSeconds=0;part.glideConnectedToNext=false;}
        part.notes.clear();result.push_back(std::move(part));
    }
    return result;
}
}

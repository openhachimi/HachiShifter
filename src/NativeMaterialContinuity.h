#pragma once
#include "NativeNoteTiming.h"
#include <juce_graphics/juce_graphics.h>
#include <map>

namespace hachi
{
// Derived view/render boundary, never another stored clock or a pitch join.
struct NativeMaterialSeam
{
    juce::String leftNoteId,rightNoteId,parentClipId,trackId;
    double sourceSeconds=0,targetSeconds=0;
    float leftMidi=60,rightMidi=60;
};
inline bool nativeMaterialSeams(const TrackData& track,std::vector<NativeMaterialSeam>& output,juce::String& error)
{
    error.clear();std::vector<NativeMaterialSeam> result;constexpr double eps=1.e-7;
    if(!trackShowsAllNativeRegions(track)){output.clear();return true;}
    struct Node{const NoteData* note;const ClipData* parent;const ClipData* owner;double offset,first,last;};
    std::vector<Node> nodes;std::map<const ClipData*,std::vector<SourceTimePoint>> clocks;
    for(const auto& parent:track.clips)for(const auto& note:parent.notes)
    {
        if(note.materialAudioId.isEmpty()||note.materialRegionId.isEmpty()||note.pronunciationKind==PronunciationKind::transition)continue;
        const ClipData* owner=parent.parts.empty()?&parent:nullptr;
        if(!parent.parts.empty())for(const auto& part:parent.parts)if(part.id==note.clipPartId){if(owner){error="Ambiguous continuous-material owner";return false;}owner=&part;}
        if(!owner||owner->sourceFile==juce::File{})continue;
        auto [at,inserted]=clocks.try_emplace(owner);if(inserted)at->second=nativeClipClock(*owner);
        const auto offset=parent.parts.empty()?0.0:owner->startSeconds;
        auto local=note;local.startSeconds-=offset;
        const auto row=nativeNoteSourceTiming(local,*owner,at->second);
        nodes.push_back({&note,&parent,owner,offset,row.regionStartSeconds,row.regionEndSeconds});
    }
    for(const auto& a:nodes)
    {
        if(a.note->materialConnectedNextRegionId.isEmpty())continue;
        std::optional<NativeMaterialSeam> seam;
        for(const auto& b:nodes)
        {
            if(a.note==b.note||a.parent!=b.parent||a.owner->sourceFile!=b.owner->sourceFile
                ||a.note->materialAudioId!=b.note->materialAudioId||a.note->materialConnectedNextRegionId!=b.note->materialRegionId
                ||b.first<=a.first+eps||b.first>a.last+eps||b.last<=a.last+eps)continue;
            if(a.owner->materialSourceFingerprint.isNotEmpty()&&b.owner->materialSourceFingerprint.isNotEmpty()
                &&a.owner->materialSourceFingerprint!=b.owner->materialSourceFingerprint)continue;
            const auto& ac=clocks.at(a.owner);const auto& bc=clocks.at(b.owner);
            if(ac.size()<2||bc.size()<2)continue;
            // The registered incoming row BEGIN is the cut; its P is a later
            // alignment anchor. Confusing these moves the cut onto the beat.
            const auto at=a.parent->startSeconds+a.offset+nativeTargetTimeAt(ac,b.first-a.owner->sourceOffsetSeconds);
            const auto bt=b.parent->startSeconds+b.offset+nativeTargetTimeAt(bc,b.first-b.owner->sourceOffsetSeconds);
            if(!std::isfinite(at)||std::abs(at-bt)>eps)continue;
            if(seam){error="Ambiguous placed continuous-material boundary";return false;}
            seam=NativeMaterialSeam{a.note->id,b.note->id,a.parent->id,track.id,b.first,bt,a.note->midiNote,b.note->midiNote};
        }
        if(seam)result.push_back(*seam);
    }
    output=std::move(result);return true;
}

struct NativeNoteVisualSpan
{
    double scoreFrom=0,scoreTo=0,from=0,p=0,to=0;
    bool transition=false;
};
inline NativeNoteVisualSpan nativeNoteVisualSpan(const NoteData& note,const ClipData& clip,const std::vector<SourceTimePoint>* sharedClock=nullptr)
{
    const auto score=clip.startSeconds+note.startSeconds;
    NativeNoteVisualSpan result{score,score+note.durationSeconds,score,score,score+note.durationSeconds,note.pronunciationKind==PronunciationKind::transition};
    const bool scoreOnly=!note.nativeScoreMaterialAligned&&(clip.sourceFile==juce::File{}||clip.sourceFile.hasFileExtension("ust;ustx;mid;midi"));
    const auto ownClock=sharedClock||scoreOnly?std::vector<SourceTimePoint>{}:nativeClipClock(clip);
    const auto& clock=sharedClock?*sharedClock:ownClock;
    if(note.nativeScoreMaterialAligned)
    {
        NativeScoreMaterialSupport support;juce::String error;
        if(nativeScoreMaterialAudioSupport(note,clip,clock,support,error))
        {result.from=clip.startSeconds+support.targetBegin;result.p=score;result.to=clip.startSeconds+support.targetEnd;}
    }
    else if(scoreOnly)
        result.from=score-(result.transition?0:note.utauPreutteranceOverrideEnabled?std::max(0.0,note.utauPreutteranceSeconds):0.0);
    else
    {
        const auto row=nativeNoteSourceTiming(note,clip,clock);
        result.p=clip.startSeconds+nativeTargetTimeAt(clock,pronunciationTimingFor(row).alignmentSeconds-clip.sourceOffsetSeconds);
    }
    if(result.transition)result.p=result.from;
    // A Melodyne pitchless onset element paired with its vowel is that vowel's
    // lead-in: the whole element reads as preutterance, never a body.
    else if(note.melodyneConsonantCandidate&&note.melodyneVowelNoteId.isNotEmpty())result.p=std::max(result.to,result.scoreTo);
    return result;
}
inline juce::Colour nativeNotePhaseColour(bool preutterance,bool transition,bool selected)
{
    const juce::Colour base(transition?0xff32936f:preutterance?0xfff4c000:0xff7f69ca);
    return selected?base.interpolatedWith(juce::Colours::white,.32f):base;
}
}

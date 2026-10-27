#pragma once
#include "NativeNoteTiming.h"
#include <algorithm>
#include <cmath>

namespace hachi
{
struct ResolvedNativeOverlapBoundary
{
    juce::String parentClipId,leftNoteId,rightNoteId,leftOwnerId,rightOwnerId;
    double overlapFromTargetSeconds=0,overlapToTargetSeconds=0,boundaryTargetSeconds=0;
};
// Explicit user assembly proof, NOT a same-recording identity inference. Pure
// snapshot geometry: no file IO, PCM access, source annotation writes or guesses.
inline bool resolveNativeOverlapBoundary(const TrackData& track,const NativeConnection& edge,
    ResolvedNativeOverlapBoundary& output,juce::String& error)
{
    constexpr double epsilon=1.e-7;error.clear();
    const auto fail=[&](const juce::String& text){error=text;return false;};
    if(edge.type!="native-overlap"||!trackShowsAllNativeRegions(track)||!track.compose||track.accompaniment)
        return fail("Not an explicit native overlap assembly");
    struct Endpoint {const NoteData* note;const ClipData* parent;double start,end;};
    std::vector<Endpoint> all;
    for(const auto& parent:track.clips)for(const auto& note:parent.notes)
        all.push_back({&note,&parent,parent.startSeconds+note.startSeconds,parent.startSeconds+note.startSeconds+note.durationSeconds});
    std::stable_sort(all.begin(),all.end(),[](const auto& a,const auto& b){return a.start<b.start;});
    std::size_t a=all.size(),b=all.size();int countA=0,countB=0;
    for(std::size_t i=0;i<all.size();++i)
    {if(all[i].note->id==edge.leftNoteId){a=i;++countA;}if(all[i].note->id==edge.rightNoteId){b=i;++countB;}}
    if(countA!=1||countB!=1||b!=a+1)return fail("Overlap assembly needs exact consecutive note identities");
    const auto& left=all[a];const auto& right=all[b];
    if(left.note->pronunciationKind==PronunciationKind::transition||right.note->pronunciationKind==PronunciationKind::transition)
        return fail("Transition pronunciations use complete cuts and overlap/fade, not partial source joins");
    if(left.parent!=right.parent||!left.parent->nativeAudioLinked||left.parent->parts.size()<2)
        return fail("Overlap assembly needs one explicit linked source compound");
    const auto& parent=*left.parent;
    const auto owner=[&](const NoteData& note)->const ClipData*
    {const ClipData* found=nullptr;for(const auto& part:parent.parts)if(part.id==note.clipPartId){if(found)return nullptr;found=&part;}return found;};
    const auto* x=owner(*left.note);const auto* y=owner(*right.note);
    if(!x||!y||x==y||x->id.isEmpty()||y->id.isEmpty()||x->sourceFile==juce::File{}||y->sourceFile==juce::File{}
        ||x->sourceFile.hasFileExtension("ust;mid;midi;mpd;hjpx;hspx")||y->sourceFile.hasFileExtension("ust;mid;midi;mpd;hjpx;hspx"))
        return fail("Overlap assembly needs two exact distinct physical child owners");
    for(const auto* part:{x,y})
    {
        if(!std::isfinite(part->sourceOffsetSeconds)||part->sourceOffsetSeconds<0)
            return fail("Invalid physical child source offset");
        const auto clock=nativeClipClock(*part);
        if(clock.size()<2)return fail("Missing physical child source clock");
        for(std::size_t i=0;i<clock.size();++i)
            if(!std::isfinite(clock[i].sourceSeconds)||!std::isfinite(clock[i].targetSeconds)
                ||(i&&(clock[i].sourceSeconds<=clock[i-1].sourceSeconds||clock[i].targetSeconds<=clock[i-1].targetSeconds)))
                return fail("Invalid physical child source clock");
    }
    const auto start=[&](const ClipData& part){return parent.startSeconds+part.startSeconds+part.audioStartSeconds;};
    const auto end=[&](const ClipData& part){return start(part)+part.audioLength();};
    const double from=std::max(start(*x),start(*y)),to=std::min(end(*x),end(*y));
    const double values[]={left.start,left.end,right.start,right.end,from,to,edge.boundarySeconds};
    for(const auto value:values)if(!std::isfinite(value))return fail("Non-finite overlap assembly geometry");
    if(right.start<=left.start+epsilon||right.end<=left.end+epsilon||left.end<=right.start+epsilon
        ||start(*y)<=start(*x)+epsilon||end(*y)<=end(*x)+epsilon||to-from<.001)
        return fail("Nested, reversed or empty overlap assembly geometry");
    // This type declares the automatic overlap-centre policy, not a saved pixel
    // anchor. Retain finite initial provenance on the edge; recompute the live
    // line after move/stretch/reopen from exact current physical owners.
    const auto boundary=(from+to)*.5;
    if(boundary<right.start-epsilon||boundary>left.end+epsilon)
        return fail("Live assembly midpoint lies outside its real note overlap");
    const auto p=parent.startSeconds+nativeNotePreutteranceTarget(*right.note,parent);
    if(!std::isfinite(p)||p<right.start-epsilon||p>right.end+epsilon)
        return fail("Invalid live incoming P anchor");
    for(std::size_t i=0;i<all.size();++i)if(i!=a&&i!=b&&all[i].start<to-epsilon&&all[i].end>from+epsilon)
        return fail("A third voice makes the overlap assembly ambiguous");
    // Other physical owners may carry support without a separate score note.
    for(const auto& part:parent.parts)if(&part!=x&&&part!=y&&start(part)<to-epsilon&&end(part)>from+epsilon)
        return fail("A third physical source crosses the overlap assembly");
    output={parent.id,left.note->id,right.note->id,x->id,y->id,from,to,boundary};return true;
}
}

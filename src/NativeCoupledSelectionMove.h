#pragma once
#include "NativeAudioLink.h"
#include "NativeNoteTiming.h"
#include "NativeVirtualGroupPlan.h"
#include "NativeAudioOverlap.h"
#include <set>
namespace hachi
{
inline bool nativeSelectionHasConnectedSeams(const TrackData& track,const std::vector<NativeVirtualGroup>& groups,const std::vector<juce::String>& ids)
{
    const auto selected=[&](const auto& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();};
    for(const auto& group:groups)if(group.trackId==track.id&&std::any_of(group.orderedNoteIds.begin(),group.orderedNoteIds.end(),selected))return true;
    for(const auto& clip:track.clips)for(const auto& note:clip.notes)if(selected(note.id)&&(clip.nativeAudioLinked||note.connectedToPrevious||note.connectedToNext))return true;
    return false;
}
struct NativeCoupledSelectionMove
{
    bool applicable=false;
    double delta=0;
    std::vector<std::pair<std::size_t,ClipData>> replacements;
};
// Editing geometry only: reuse the existing shared-boundary/weighted-zone warp
// via an ephemeral linked proxy, then restore original physical owner IDs. This
// neither creates cross-source connections nor commits a legacy compound/edge.
inline NativeCoupledSelectionMove planNativeCoupledSelectionMove(const TrackData& track,
    const std::vector<NativeVirtualGroup>& groups,const std::vector<juce::String>& selected,
    double delta,float semitones,juce::String& error,NativeNoteTimeEdit edit=NativeNoteTimeEdit::move)
{
    NativeCoupledSelectionMove result;error.clear();if(!trackShowsAllNativeRegions(track)||selected.empty())return result;
    std::set<juce::String> moving(selected.begin(),selected.end()),affected=moving;
    std::map<juce::String,std::size_t> owners;std::size_t found=0;
    for(std::size_t i=0;i<track.clips.size();++i)for(const auto& note:track.clips[i].notes)
    {if(!owners.emplace(note.id,i).second){error="Ambiguous coupled-move note identity";return result;}if(moving.count(note.id))++found;}
    if(found!=moving.size())return result;
    struct Link{juce::String left,right;};std::vector<Link> links;
    struct SourceSpan{const ClipData* clip;const NoteData* note;double from,to,start,end;};
    std::map<juce::String,std::vector<SourceSpan>> usages;
    for(const auto& clip:track.clips)
    {
        if(!clip.parts.empty()||clip.notes.size()!=1||clip.materialSourceUsageGroupId.isEmpty())continue;
        const auto& note=clip.notes.front();const auto clock=nativeClipClock(clip);if(clock.size()<2)continue;
        usages[clip.materialSourceUsageGroupId].push_back({&clip,&note,clip.sourceOffsetSeconds+nativeSourceTimeAt(clock,note.startSeconds),
            clip.sourceOffsetSeconds+nativeSourceTimeAt(clock,note.startSeconds+note.durationSeconds),clip.startSeconds+note.startSeconds,clip.startSeconds+note.startSeconds+note.durationSeconds});
    }
    for(const auto& [usage,members]:usages)
    {
        juce::ignoreUnused(usage);
        for(const auto& a:members)for(const auto& b:members)
        {
            if(a.note==b.note||a.clip->sourceFile!=b.clip->sourceFile||!a.note->connectedToNext||!b.note->connectedToPrevious
                ||!a.clip->glideConnectedToNext||!b.clip->glideConnectedFromPrevious)continue;
            if(std::abs(a.to-b.from)<=1.e-6&&std::abs(a.end-b.start)<=1.e-6)links.push_back({a.note->id,b.note->id});
        }
    }
    bool virtualWarp=false;
    // A virtual route uses the SAME editor-time warp while its actual source
    // supports still meet. This is an ephemeral coordinate projection only;
    // every returned owner keeps its original source and independent clock.
    // Already separated/reversed routes retain their free geometry, never fill
    // a gap. The group itself is never dissolved or re-ordered by this choice.
    for(const auto& group:groups)
    {
        if(group.trackId!=track.id)continue;
        const bool touches=std::any_of(group.orderedNoteIds.begin(),group.orderedNoteIds.end(),[&](const auto& id){return moving.count(id)>0;});
        if(touches&&std::abs(delta)>1.e-9)
        {
            NativeVirtualConnectionPlan geometry;juce::String issue;
            if(!resolveNativeVirtualGroup(track,group,geometry,issue)){error=issue;return result;}
            // A retained connection never silently becomes independent when
            // a gesture reaches its time limits. Existing geometry is warped
            // as stored; only explicit disconnect can remove this constraint.
            if(!geometry.members.empty())
            {
                const std::set<juce::String> members(group.orderedNoteIds.begin(),group.orderedNoteIds.end());
                for(const auto& id:members)
                {
                    if(!owners.count(id))return result;const auto& owner=track.clips[owners.at(id)];
                    if(!owner.parts.empty()||std::any_of(owner.notes.begin(),owner.notes.end(),[&](const auto& n){return !members.count(n.id)&&!moving.count(n.id);}))
                    {error="Coupled virtual movement would affect an unrelated note or nested owner; choose independent movement explicitly or separate the owner first";return result;}
                }
                for(std::size_t i=1;i<group.orderedNoteIds.size();++i)links.push_back({group.orderedNoteIds[i-1],group.orderedNoteIds[i]});
                virtualWarp=true;
            }
        }
        const auto addBridge=[&](const NativeSplitTransitionEvidence& bridge)
        {
            if(!owners.count(bridge.frontNoteId)||!owners.count(bridge.backNoteId))return;
            const auto& front=track.clips[owners.at(bridge.frontNoteId)];const auto& back=track.clips[owners.at(bridge.backNoteId)];
            if(front.sourceFile==back.sourceFile&&front.sourceFile!=juce::File{})links.push_back({bridge.frontNoteId,bridge.backNoteId});
        };
        if(group.splitTransition)addBridge(*group.splitTransition);
        for(const auto& bridge:group.extraSplitTransitions)addBridge(bridge);
    }
    bool expanded=true;while(expanded)
    {
        expanded=false;for(const auto& edge:links)if(affected.count(edge.left)||affected.count(edge.right))
        {result.applicable=true;expanded|=affected.insert(edge.left).second;expanded|=affected.insert(edge.right).second;}
    }
    if(!result.applicable)return result;
    std::set<std::size_t> chosenIndices;for(const auto& id:affected)chosenIndices.insert(owners.at(id));
    if(chosenIndices.size()<2){result.applicable=false;return result;} // existing common owner already warps together
    std::vector<ClipData> chosen;
    for(const auto index:chosenIndices)
    {
        if(!track.clips[index].parts.empty()){error="Coupled movement across nested physical owners needs an explicit common source projection";return result;}
        chosen.push_back(track.clips[index]);
    }
    float low=-127,high=127;for(const auto& clip:chosen)for(const auto& note:clip.notes)if(moving.count(note.id)){low=std::max(low,-note.midiNote);high=std::min(high,127-note.midiNote);}
    const auto pitch=juce::jlimit(low,high,semitones);
    auto proxy=assembledLinkedAudio(chosen);
    if(virtualWarp){proxy.id="ephemeral-virtual-editor-time-axis";proxy.sourceFile=juce::File{};}
    const auto plan=planNativeNoteMove(proxy,selected,delta,pitch,edit,true);
    if(!plan)
    {error="Linked movement cannot preserve a positive shared source clock";return result;}
    // Accept the SAME clamped delta for all members. Falling back to an
    // independent owner edit here used to tear a/b/c apart at the limit.
    if(std::abs(plan->delta)<1.e-9&&std::abs(pitch)<1.e-6)return result;
    auto pieces=expandedClipParts(plan->clip);
    if(pieces.size()!=chosen.size()){error="Coupled source projection lost a physical owner";return result;}
    std::size_t index=0;
    for(const auto originalIndex:chosenIndices)
    {
        auto piece=std::move(pieces[index]);const auto& original=chosen[index++];
        piece.id=original.id;piece.nativeAudioLinked=original.nativeAudioLinked;piece.nativePitchGroupId=original.nativePitchGroupId;
        for(auto& note:piece.notes)note.clipPartId.clear();
        if(original.audioDurationSeconds<0&&std::abs(piece.audioStartSeconds)<1.e-8&&std::abs(piece.audioLength()-piece.durationSeconds)<1.e-8)
        {piece.audioStartSeconds=0;piece.audioDurationSeconds=-1;}
        result.replacements.emplace_back(originalIndex,std::move(piece));
    }
    // Unbound external sources are independent voices and may overlap this
    // route. Internal seam topology is maintained by the common warp itself.
    result.delta=plan->delta;return result;
}
}

#pragma once
#include "NativeVirtualConnectionPlan.h"

namespace hachi
{
// Raw join facts and an independent identified-transition decision. A pitch
// join alone, a short duration, or an e_g filename is NOT this proof.
struct NativeSplitTransitionPlan
{
    NativeVirtualConnectionPlan logical; // exactly 3 roles: left / ONE bridge / right
    std::array<NativeVirtualMember,4> physical;
    double middleSourceSeconds=0,middleTargetSeconds=0;
};
// Four explicit physical notes with one verified source-continuous split bridge.
// Pure validation/projection only, not a detector or audio merger. New source
// evidence must be retained from the original graph before this can be automatic.
inline bool planNativeSplitTransition(const TrackData& track,const juce::String& leftId,
    const NativeSplitTransitionEvidence& evidence,const juce::String& rightId,
    NativeSplitTransitionPlan& output,juce::String& error,
    NativeVirtualResolveMode mode=NativeVirtualResolveMode::newSelection,
    const std::set<juce::String>* knownRouteMembers=nullptr)
{
    error.clear();const auto fail=[&](const juce::String& why){error=why;return false;};
    if(!trackShowsAllNativeRegions(track)||!track.compose||track.accompaniment)return fail("Split transition requires one native melodic track");
    const std::array<juce::String,4> ids{leftId,evidence.frontNoteId,evidence.backNoteId,rightId};
    if(std::set<juce::String>(ids.begin(),ids.end()).size()!=4||std::any_of(ids.begin(),ids.end(),[](const auto& id){return id.isEmpty();}))return fail("Split transition needs four exact distinct physical notes");
    if(!evidence.transitionIdentified||evidence.logicalId.isEmpty())return fail("A joined pair is not automatically an identified transition");
    if(!evidence.joinsPitches||!evidence.joinsAmplitudes||!evidence.joinsPhases
        ||evidence.frontElementIdentity.isEmpty()||evidence.backElementIdentity.isEmpty()
        ||evidence.precedingElementIdentity!=evidence.frontElementIdentity||evidence.followingElementIdentity!=evidence.backElementIdentity)
        return fail("Missing reciprocal explicit source-continuous join evidence");
    if(evidence.sourceObjectIdentity.isEmpty()||evidence.frontSourceObjectIdentity!=evidence.sourceObjectIdentity||evidence.backSourceObjectIdentity!=evidence.sourceObjectIdentity)
        return fail("Bridge halves do not share the original physical graph source identity");
    struct Endpoint{const NoteData* note=nullptr;const ClipData* parent=nullptr;const ClipData* owner=nullptr;double offset=0,sourceFrom=0,sourceTo=0;};
    std::array<Endpoint,4> endpoints;NativeSplitTransitionPlan proposed;
    for(std::size_t i=0;i<ids.size();++i)
    {
        auto& endpoint=endpoints[i];
        for(const auto& parent:track.clips)for(const auto& note:parent.notes)if(note.id==ids[i])
        {if(endpoint.note)return fail("Ambiguous split transition endpoint");endpoint.note=&note;endpoint.parent=&parent;}
        if(!endpoint.note)return fail("Missing split transition member");
        endpoint.owner=endpoint.parent->parts.empty()?endpoint.parent:nullptr;
        for(const auto& part:endpoint.parent->parts)if(part.id==endpoint.note->clipPartId)
        {if(endpoint.owner)return fail("Ambiguous split transition owner");endpoint.owner=&part;endpoint.offset=part.startSeconds;}
        if(!endpoint.owner||endpoint.owner->sourceFile==juce::File{}||endpoint.owner->sourceFile.hasFileExtension("ust;ustx;mid;midi;mpd;hjpx;hspx"))return fail("Split bridge needs exact physical source owners");
        const auto clock=nativeClipClock(*endpoint.owner);
        if(clock.size()<2)return fail("Missing split transition source clock");
        for(std::size_t j=0;j<clock.size();++j)if(!std::isfinite(clock[j].sourceSeconds)||!std::isfinite(clock[j].targetSeconds)
            ||(j&&(clock[j].sourceSeconds<=clock[j-1].sourceSeconds||clock[j].targetSeconds<=clock[j-1].targetSeconds)))return fail("Invalid split transition source clock");
        auto local=*endpoint.note;local.startSeconds-=endpoint.offset;
        double from=local.startSeconds,to=from+local.durationSeconds;
        if(local.nativeScoreMaterialAligned)
        {NativeScoreMaterialSupport support;if(!nativeScoreMaterialAudioSupport(local,*endpoint.owner,clock,support,error))return false;from=support.targetBegin;to=support.targetEnd;}
        if(!std::isfinite(from)||!std::isfinite(to)||to-from<.001||from<clock.front().targetSeconds-1.e-7||to>clock.back().targetSeconds+1.e-7)return fail("Split member outside actual source support");
        endpoint.sourceFrom=endpoint.owner->sourceOffsetSeconds+nativeSourceTimeAt(clock,from);
        endpoint.sourceTo=endpoint.owner->sourceOffsetSeconds+nativeSourceTimeAt(clock,to);
        const auto origin=endpoint.parent->startSeconds+endpoint.offset;
        proposed.physical[i]={ids[i],endpoint.parent->id,endpoint.owner->id,endpoint.note->materialAudioId,endpoint.note->materialRegionId,endpoint.owner->sourceFile,origin+from,origin+to,0,i==1||i==2};
        proposed.physical[i].sourceFromSeconds=endpoint.sourceFrom;proposed.physical[i].sourceToSeconds=endpoint.sourceTo;
    }
    if(endpoints[0].note->pronunciationKind!=PronunciationKind::normal||endpoints[3].note->pronunciationKind!=PronunciationKind::normal)return fail("Bridge requires ordinary outer pronunciation endpoints");
    if(endpoints[1].note->pronunciationKind==PronunciationKind::breath||endpoints[2].note->pronunciationKind==PronunciationKind::breath)return fail("A breath is not a split transition half");
    const auto& a=proposed.physical[1];const auto& b=proposed.physical[2];
    if(a.source!=b.source)return fail("Bridge halves must be continuous slices of the same recording");
    if(std::abs(endpoints[1].sourceTo-endpoints[2].sourceFrom)>1.e-6)return fail("Target adjacency does not prove a continuous physical source cut");
    const bool retained=mode==NativeVirtualResolveMode::retainedConnection;
    if(!retained&&std::abs(a.to-b.from)>1.e-6)return fail("The two split halves do not meet at one target cut");
    proposed.middleSourceSeconds=endpoints[1].sourceTo;proposed.middleTargetSeconds=a.to;
    NativeTransitionShape shape{1,proposed.middleSourceSeconds,"melodyne-explicit-two-piece-transition"};
    if(!validateNativeTransitionShape(shape,endpoints[1].sourceFrom,endpoints[2].sourceTo,error))return false;
    auto bridge=a;bridge.noteId=evidence.logicalId;bridge.from=std::min(a.from,b.from);bridge.to=std::max(a.to,b.to);bridge.middle=proposed.middleTargetSeconds;bridge.transition=true;
    bridge.sourceToSeconds=endpoints[2].sourceTo;
    auto& logical=proposed.logical;logical.throughTransition=true;logical.retainedTopology=retained;logical.members={proposed.physical[0],bridge,proposed.physical[3]};
    const auto& left=logical.members.front();const auto& right=logical.members.back();
    if(!retained&&(right.from<=left.from||right.to<=left.to))return fail("Ambiguous outer pronunciation order");
    const auto handover=[&](const NativeVirtualMember& first,const NativeVirtualMember& second,juce::String fromId,juce::String toId)
    {
        const bool reverse=(first.from+first.to)>(second.from+second.to);
        auto from=std::max(first.from,second.from),to=std::min(first.to,second.to);
        if(from>to&&retained){from=to=reverse?(second.to+first.from)*.5:(first.to+second.from)*.5;logical.geometryNotice="Moved split bridge keeps its original members across separated support";}
        return NativeVirtualHandover{fromId,toId,from,std::max(from,to),retained&&reverse};
    };
    if(!retained&&(std::max(left.from,a.from)>std::min(left.to,a.to)+1.e-7||std::max(b.from,right.from)>std::min(b.to,right.to)+1.e-7))
        return fail("Missing actual support on one side of the logical bridge");
    logical.handovers={handover(left,a,left.noteId,bridge.noteId),handover(b,right,bridge.noteId,right.noteId)};
    // The two known halves are ONE bridge, not a fourth independent voice.
    // Unselected fifth voices still must not cross either handover.
    if(!retained)for(const auto& parent:track.clips)for(const auto& note:parent.notes)
    {
        if(std::find(ids.begin(),ids.end(),note.id)!=ids.end()||(knownRouteMembers&&knownRouteMembers->count(note.id)))continue;
        const auto from=parent.startSeconds+note.startSeconds,to=from+note.durationSeconds;
        for(const auto& h:logical.handovers)if(from<h.to-1.e-7&&to>h.from+1.e-7)return fail("Another independent voice crosses the split bridge handover");
    }
    output=std::move(proposed);return true;
}
// Logical bridge contribution belongs to only ONE source half at the real cut.
// Never crossfade the two source-continuous halves a second time.
inline std::array<double,4> nativeSplitTransitionContributions(const NativeSplitTransitionPlan& plan,double time)
{
    const auto logical=nativeVirtualContributions(plan.logical,time);std::array<double,4> result{logical[0],0,0,logical[2]};
    const auto half=time<plan.middleTargetSeconds?1u:2u;
    if(time>=plan.physical[half].from&&time<plan.physical[half].to)result[half]=logical[1];
    return result;
}
}

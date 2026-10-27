#pragma once
#include "NativeNoteTiming.h"
#include <array>
#include <set>

namespace hachi
{
// Derived target geometry for an EXPLICIT ordered selection. This planner never
// rewrites source identity, C/P, source clocks, pitch voicing or clip envelopes.
// Automatic candidate discovery must supply the same exact identities and must
// not infer ordering from labels (a transition may have any alias).
struct NativeVirtualMember
{
    juce::String noteId,parentId,ownerId,audioId,regionId;
    juce::File source;
    double from=0,to=0,middle=0;
    bool transition=false;
    double sourceFromSeconds=0,sourceToSeconds=0; // exact physical source support, not target adjacency
};
struct NativeVirtualHandover
{
    juce::String leftNoteId,rightNoteId;
    double from=0,to=0; // shared audible window, or a zero-width touching cut
    bool reverseTime=false; // spatial reversal does not reverse endpoint IDs
};
struct NativeVirtualConnectionPlan
{
    std::vector<NativeVirtualMember> members;
    std::vector<NativeVirtualHandover> handovers;
    // Logical roles differ from physical slices when a bridge has two halves.
    // Ranges are inclusive physical indices, precomputed outside the callback.
    std::vector<NativeVirtualMember> contributionMembers;
    std::vector<std::pair<std::size_t,std::size_t>> contributionRanges;
    double splitMiddleTargetSeconds=0;
    // Two members may be direct or an explicitly selected ordinary/transition
    // half-route; three can span an identified bridge with an explicit source
    // middle. No alias/phoneme/midpoint inference is used.
    bool throughTransition=false;
    bool retainedTopology=false;
    juce::String geometryNotice;
};
enum class NativeVirtualResolveMode { newSelection, retainedConnection };
inline bool resolveNativeVirtualMember(const TrackData& track,const juce::String& id,NativeVirtualMember& output,juce::String& error,bool requireTransitionShape=true)
{
    constexpr double epsilon=1.e-7;
    const auto fail=[&](const juce::String& text){error=text;return false;};
        const NoteData* note=nullptr;const ClipData* parent=nullptr;
        for(const auto& p:track.clips)for(const auto& n:p.notes)if(n.id==id)
        {if(note)return fail("Ambiguous virtual endpoint identity");note=&n;parent=&p;}
        if(!note)return fail("Missing virtual endpoint: "+id);
        const ClipData* owner=parent->parts.empty()?parent:nullptr;
        for(const auto& part:parent->parts)if(part.id==note->clipPartId)
        {if(owner)return fail("Ambiguous virtual source owner");owner=&part;}
        if(!owner||owner->sourceFile==juce::File{}||owner->sourceFile.hasFileExtension("ust;ustx;mid;midi;mpd;hjpx;hspx"))
            return fail("Virtual connection requires exact physical audio owners");
        const auto offset=parent->parts.empty()?0.:owner->startSeconds;
        const auto origin=parent->startSeconds+offset;
        const auto clock=nativeClipClock(*owner);
        if(clock.size()<2)return fail("Missing source clock for virtual endpoint");
        for(std::size_t i=0;i<clock.size();++i)
            if(!std::isfinite(clock[i].sourceSeconds)||!std::isfinite(clock[i].targetSeconds)
                ||(i&&(clock[i].sourceSeconds<=clock[i-1].sourceSeconds||clock[i].targetSeconds<=clock[i-1].targetSeconds)))
                return fail("Non-monotonic virtual endpoint source clock");
        auto local=*note;local.startSeconds-=offset;
        double from=local.startSeconds,to=from+local.durationSeconds;
        if(local.nativeScoreMaterialAligned)
        {NativeScoreMaterialSupport support;if(!nativeScoreMaterialAudioSupport(local,*owner,clock,support,error))return false;from=support.targetBegin;to=support.targetEnd;}
        if(!std::isfinite(from)||!std::isfinite(to)||to-from<.001
            ||from<clock.front().targetSeconds-epsilon||to>clock.back().targetSeconds+epsilon)
            return fail("Virtual endpoint lies outside its actual source support");
        NativeVirtualMember member{id,parent->id,owner->id,note->materialAudioId,note->materialRegionId,owner->sourceFile,
            origin+from,origin+to,0,note->pronunciationKind==PronunciationKind::transition};
        member.sourceFromSeconds=owner->sourceOffsetSeconds+nativeSourceTimeAt(clock,from);
        member.sourceToSeconds=owner->sourceOffsetSeconds+nativeSourceTimeAt(clock,to);
        if(member.transition&&requireTransitionShape)
        {
            if(!note->nativeTransition)return fail("Transition identity needs an explicit middle boundary; no midpoint guess");
            const auto first=owner->sourceOffsetSeconds+nativeSourceTimeAt(clock,from);
            const auto last=owner->sourceOffsetSeconds+nativeSourceTimeAt(clock,to);
            if(!validateNativeTransitionShape(*note->nativeTransition,first,last,error))return false;
            member.middle=origin+nativeTargetTimeAt(clock,note->nativeTransition->boundarySourceSeconds-owner->sourceOffsetSeconds);
        }
        output=std::move(member);return true;
    
}
inline bool nativeVirtualMembersSourceContinuous(const NativeVirtualMember& left,const NativeVirtualMember& right)
{
    return left.source==right.source&&left.sourceToSeconds>left.sourceFromSeconds&&right.sourceToSeconds>right.sourceFromSeconds
        &&std::abs(left.sourceToSeconds-right.sourceFromSeconds)<=1.e-6;
}
inline bool planNativeVirtualHandovers(const TrackData& track,const std::vector<juce::String>& orderedIds,NativeVirtualConnectionPlan& result,juce::String& error,const std::set<juce::String>* knownRouteMembers=nullptr)
{
    constexpr double epsilon=1.e-7;
    const auto fail=[&](const juce::String& text){error=text;return false;};
    if(std::none_of(result.members.begin(),result.members.end(),[](const auto& member){return !member.transition;}))
        return fail("Select at least one ordinary note together with the transition material");
    // A user may connect a->bridge first and extend bridge->c afterwards.
    // The partial route holds the ordinary endpoint's target through the
    // bridge; transition source pitch never becomes another musical target.
    result.throughTransition=std::any_of(result.members.begin(),result.members.end(),
        [](const auto& member){return member.transition;});
    // Source identity is not source continuity: a/c slices from the same WAV
    // may skip b, repeat a range, or reverse source order. Those are virtual
    // assembly too. Only a REAL adjoining source cut uses the partial authority.
    for(std::size_t i=1;i<result.members.size();++i)
        if(!result.retainedTopology&&nativeVirtualMembersSourceContinuous(result.members[i-1],result.members[i]))
            return fail("Physically continuous source slices use their existing source-continuity authority");
    const auto& left=result.members.front();const auto& right=result.members.back();
    if(right.from<=left.from+epsilon||right.to<=left.to+epsilon)
    {
        if(!result.retainedTopology)return fail("Reversed or nested endpoint order is ambiguous");
        result.geometryNotice="Moved endpoints retain their original virtual direction";
    }
    for(std::size_t i=1;i<result.members.size();++i)
    {
        const auto& a=result.members[i-1];const auto& b=result.members[i];
        const bool reversed=(b.from+b.to)<(a.from+a.to);
        double from=std::max(a.from,b.from),to=std::min(a.to,b.to);
        if(from>to+epsilon)
        {
            if(!result.retainedTopology)return fail("Virtual connection does not invent support across a gap");
            // No fabricated samples: both real source spans stay audible on
            // their own support and the reference connection crosses the gap.
            from=to=reversed?(b.to+a.from)*.5:(a.to+b.from)*.5;
            result.geometryNotice="Virtual connection retained across separated source support";
        }
        else if(result.throughTransition)
        {
            const auto cutFrom=a.transition?std::max(from,a.middle):from;
            const auto cutTo=b.transition?std::min(to,b.middle):to;
            if(cutFrom>cutTo+epsilon)
            {
                if(!result.retainedTopology)return fail("Transition side has no real overlap with its designated endpoint");
                result.geometryNotice="Moved transition keeps its designated neighbours; using their real overlap";
            }
            else{from=cutFrom;to=cutTo;}
        }
        result.handovers.push_back({a.noteId,b.noteId,from,std::max(from,to),result.retainedTopology&&reversed});
    }
    // An explicit group must not swallow another unselected voice. Only reject
    // when it crosses a handover, not when it is elsewhere on the same track.
    if(!result.retainedTopology)for(const auto& p:track.clips)for(const auto& n:p.notes)
    {
        if(std::find(orderedIds.begin(),orderedIds.end(),n.id)!=orderedIds.end()||(knownRouteMembers&&knownRouteMembers->count(n.id)))continue;
        const auto first=p.startSeconds+n.startSeconds,last=first+n.durationSeconds;
        for(const auto& h:result.handovers)
            if(first<h.to-epsilon&&last>h.from+epsilon)
                return fail("Another voice crosses the virtual handover; choose the intended voice explicitly");
    }
    return true;
}
inline bool planNativeVirtualConnection(const TrackData& track,
    const std::vector<juce::String>& orderedIds,NativeVirtualConnectionPlan& output,juce::String& error,
    NativeVirtualResolveMode mode=NativeVirtualResolveMode::newSelection,
    const std::set<juce::String>* knownRouteMembers=nullptr)
{
    error.clear();constexpr double epsilon=1.e-7;
    const auto fail=[&](const juce::String& text){error=text;return false;};
    if(!trackShowsAllNativeRegions(track)||!track.compose||track.accompaniment)
        return fail("Virtual connection requires one native note track");
    // A chain of any length: each consecutive pair is one cross-source hop.
    // An end may be an identified transition while the user builds a route
    // in two pairwise steps; at least one ordinary pitch reference is required.
    if(orderedIds.size()<2)
        return fail("Choose at least two endpoints, optionally with identified transitions between them");
    if(std::set<juce::String>(orderedIds.begin(),orderedIds.end()).size()!=orderedIds.size())return fail("Duplicate virtual endpoint identity");
    NativeVirtualConnectionPlan result;result.retainedTopology=mode==NativeVirtualResolveMode::retainedConnection;
    for(const auto& id:orderedIds)
    {NativeVirtualMember member;if(!resolveNativeVirtualMember(track,id,member,error))return false;result.members.push_back(std::move(member));}
    if(!planNativeVirtualHandovers(track,orderedIds,result,error,knownRouteMembers))return false;
    output=std::move(result);return true;
}
// Gain ownership preflight shared by commits and playback. An owner may hold
// multiple consecutive syllables: group weights apply ONLY within the member's
// actual support. Simultaneous notes in that same rendered owner cannot be
// independently weighted, so ambiguous overlapping support is still refused.
inline bool nativeVirtualOwnersIndependentlyRenderable(const TrackData& track,
    const NativeVirtualConnectionPlan& plan,juce::String& error)
{
    for(const auto& member:plan.members)
    {
        const ClipData* parent=nullptr;
        for(const auto& clip:track.clips)if(clip.id==member.parentId)
        {if(parent){error="Ambiguous virtual parent identity";return false;}parent=&clip;}
        if(!parent){error="Missing virtual source parent";return false;}
        const ClipData* owner=parent->parts.empty()?parent:nullptr;
        for(const auto& part:parent->parts)if(part.id==member.ownerId)owner=&part;
        if(!owner){error="Missing virtual source owner";return false;}
        const auto offset=parent->parts.empty()?0.:owner->startSeconds;
        const auto clock=nativeClipClock(*owner);
        for(const auto& note:parent->notes)
        {
            if(note.id==member.noteId||(!parent->parts.empty()&&note.clipPartId!=member.ownerId))continue;
            auto local=note;local.startSeconds-=offset;
            double from=local.startSeconds,to=from+local.durationSeconds;
            if(local.nativeScoreMaterialAligned)
            {NativeScoreMaterialSupport support;if(!nativeScoreMaterialAudioSupport(local,*owner,clock,support,error))return false;from=support.targetBegin;to=support.targetEnd;}
            from+=parent->startSeconds+offset;to+=parent->startSeconds+offset;
            if(from<member.to-1.e-7&&to>member.from+1.e-7)
            {error="Virtual connection needs independent source support; another note overlaps inside the same rendered owner";return false;}
        }
    }
    return true;
}

// Planning/verification utility: a partition of contribution weights, separate
// from author envelopes. At most one route is active. Never emit a contributor
// outside its real source support, and never turn a silent gap into sound.
inline std::array<double,4> nativeVirtualContributions(const NativeVirtualConnectionPlan&,double);
// Scalar realtime authority: no allocation, no truncation, arbitrary chain
// length. The product partition extends the exact existing two/three-role law.
inline double nativeVirtualMemberContribution(const NativeVirtualConnectionPlan& plan,std::size_t member,double time)
{
    if(member>=plan.members.size()||!std::isfinite(time))return 0;
    const bool mapped=!plan.contributionRanges.empty();
    if(!mapped&&(!plan.contributionMembers.empty()||plan.members.size()<=3))
        return member<4?nativeVirtualContributions(plan,time)[member]:0.;
    const auto roleCount=mapped?plan.contributionMembers.size():plan.members.size();
    if(plan.handovers.size()+1!=roleCount||(mapped&&plan.contributionRanges.size()!=roleCount))return 0;
    double remainder=1,selected=0,sum=0;int active=0;bool present=false;
    for(std::size_t i=0;i<roleCount;++i)
    {
        double value=remainder;
        if(i<plan.handovers.size())
        {
            const auto& h=plan.handovers[i];double u=h.to<=h.from?(time<h.from?0.:1.):std::clamp((time-h.from)/(h.to-h.from),0.,1.);
            if(h.reverseTime)u=1-u;u=u*u*(3-2*u);value*=1-u;remainder*=u;
        }
        const auto range=mapped?plan.contributionRanges[i]:std::pair<std::size_t,std::size_t>{i,i};
        if(range.first>range.second||range.second>=plan.members.size())return 0;
        bool available=false;std::size_t chosen=range.first;
        // Incoming physical half wins in an overlap, at its actual source-cut
        // projection. A logical role never counts as two independent voices.
        for(std::size_t p=range.first;p<=range.second;++p)
            if(time>=plan.members[p].from&&time<plan.members[p].to){available=true;chosen=p;}
        if(available){++active;sum+=value;if(chosen==member){selected=value;present=true;}}
    }
    if(!present)return 0;
    if(sum>1.e-12)return selected/sum;
    return plan.retainedTopology&&active>0?1./active:0.;
}
inline std::array<double,4> nativeVirtualContributions(const NativeVirtualConnectionPlan& plan,double time)
{
    std::array<double,4> weights{};
    if(!plan.contributionRanges.empty())
    {for(std::size_t i=0;i<weights.size()&&i<plan.members.size();++i)weights[i]=nativeVirtualMemberContribution(plan,i,time);return weights;}
    const bool split=!plan.contributionMembers.empty();
    const auto& members=split?plan.contributionMembers:plan.members;
    if(!std::isfinite(time)||members.size()<2||plan.handovers.size()+1!=members.size()
        ||(split&&plan.members.size()!=4))return weights;
    if(!split&&members.size()>3)
    {
        // Compatibility utility for old small-plan callers only; production
        // consumers use nativeVirtualMemberContribution and never truncate.
        for(std::size_t i=0;i<weights.size()&&i<members.size();++i)
            weights[i]=nativeVirtualMemberContribution(plan,i,time);
        return weights;
    }
    const auto progress=[&](const NativeVirtualHandover& h)
    {
        double u=h.to<=h.from?(time<h.from?0.:1.):std::clamp((time-h.from)/(h.to-h.from),0.,1.);
        if(h.reverseTime)u=1.-u;
        return u*u*(3.-2.*u);
    };
    const auto a=progress(plan.handovers[0]);weights[0]=1.-a;
    if(members.size()==2)weights[1]=a;
    else{const auto b=progress(plan.handovers[1]);weights[1]=a*(1.-b);weights[2]=a*b;}
    for(std::size_t i=0;i<members.size();++i)if(time<members[i].from||time>=members[i].to)weights[i]=0;
    if(split)
    {
        const auto bridge=weights[1],right=weights[2];weights[1]=weights[2]=0;weights[3]=right;
        const bool front=time>=plan.members[1].from&&time<plan.members[1].to;
        const bool back=time>=plan.members[2].from&&time<plan.members[2].to;
        // Exact source cut: if moving produces an overlap, the incoming half's
        // actual source-cut projection wins, not a fabricated average seam.
        if(back)weights[2]=bridge;else if(front)weights[1]=bridge;
    }
    if(plan.retainedTopology)
    {
        // Partition only REAL available contributions, never normalize PCM.
        // A logical bridge's union may contain a gap after moving one half;
        // that absent half must not steal an ordinary endpoint's audible weight.
        const auto& physical=plan.members;double sum=0;int active=0;
        for(std::size_t i=0;i<physical.size();++i)
        {sum+=weights[i];if(time>=physical[i].from&&time<physical[i].to)++active;}
        if(sum>1.e-12)for(auto& value:weights)value/=sum;
        else if(active>0)
        {
            // Preserve one logical bridge role, not two independent voices,
            // even when both source-continuous halves were dragged to overlap.
            const bool both=split&&time>=physical[1].from&&time<physical[1].to&&time>=physical[2].from&&time<physical[2].to;
            if(both)--active;
            for(std::size_t i=0;i<physical.size();++i)
                if(!(both&&i==1)&&time>=physical[i].from&&time<physical[i].to)weights[i]=1./active;
        }
    }
    return weights;
}
}

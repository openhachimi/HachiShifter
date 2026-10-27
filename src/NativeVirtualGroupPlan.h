#pragma once
#include "NativeSplitTransitionPlan.h"
#include "NativeVirtualHandoverWindow.h"
#include <map>
namespace hachi
{
inline std::vector<const NativeSplitTransitionEvidence*> nativeVirtualSplitProofs(const NativeVirtualGroup& group)
{
    std::vector<const NativeSplitTransitionEvidence*> proofs;
    if(group.splitTransition)proofs.push_back(&*group.splitTransition);
    for(const auto& proof:group.extraSplitTransitions)proofs.push_back(&proof);
    return proofs;
}
inline bool validateNativeVirtualHopTargets(const NativeVirtualGroup& group,const NativeVirtualConnectionPlan& plan,juce::String& error)
{
    const bool mapped=!plan.contributionRanges.empty();const auto& roles=mapped?plan.contributionMembers:plan.members;
    std::set<std::pair<juce::String,juce::String>> seen;
    for(const auto& manual:group.hopPitchCurves)
    {
        if(!seen.emplace(manual.leftNoteId,manual.rightNoteId).second){error="Duplicate virtual hop curve";return false;}
        bool found=false;
        for(std::size_t i=0;i+1<roles.size();)
        {
            const auto end=roles[i+1].transition&&i+2<roles.size()?i+2:i+1;
            const auto first=mapped?plan.contributionRanges[i].first:i,last=mapped?plan.contributionRanges[end].second:end;
            found|=plan.members[first].noteId==manual.leftNoteId&&plan.members[last].noteId==manual.rightNoteId;i=end;
        }
        if(!found){error="Virtual manual hop no longer matches its exact endpoint identities";return false;}
    }
    return true;
}
// One geometry authority for model, render and display. Physical source owners
// remain independent while each validated two-half bridge is ONE logical role.
inline bool resolveNativeVirtualGroup(const TrackData& track,const NativeVirtualGroup& group,
    NativeVirtualConnectionPlan& output,juce::String& error,
    NativeVirtualResolveMode mode=NativeVirtualResolveMode::retainedConnection,
    const std::set<juce::String>* knownRouteMembers=nullptr)
{
    const auto proofs=nativeVirtualSplitProofs(group);
    if(proofs.empty())return planNativeVirtualConnection(track,group.orderedNoteIds,output,error,mode,knownRouteMembers)
        &&validateNativeVirtualHopTargets(group,output,error)&&applyNativeVirtualHandoverWindows(group,output,error);
    const auto& ids=group.orderedNoteIds;std::set<juce::String> selected(ids.begin(),ids.end());
    if(ids.size()<4||selected.size()!=ids.size()){error="Split bridge needs distinct physical members";return false;}
    if(knownRouteMembers)selected.insert(knownRouteMembers->begin(),knownRouteMembers->end());
    std::set<juce::String> halfIds,logicalIds;std::map<juce::String,NativeSplitTransitionPlan> splitPlans;
    std::map<juce::String,NativeVirtualMember> physical;
    for(const auto* proof:proofs)
    {
        const auto front=std::find(ids.begin(),ids.end(),proof->frontNoteId);
        if(front==ids.end()||front==ids.begin()||std::distance(front,ids.end())<3||front[1]!=proof->backNoteId
            ||!halfIds.insert(proof->frontNoteId).second||!halfIds.insert(proof->backNoteId).second||!logicalIds.insert(proof->logicalId).second)
        {error="Split bridge identities or physical adjacency disagree";return false;}
        NativeSplitTransitionPlan split;
        if(!planNativeSplitTransition(track,front[-1],*proof,front[2],split,error,mode,&selected))return false;
        for(const auto& member:split.physical)physical[member.noteId]=member;
        splitPlans.emplace(proof->frontNoteId,std::move(split));
    }
    NativeVirtualConnectionPlan projected;projected.retainedTopology=mode==NativeVirtualResolveMode::retainedConnection;
    for(const auto& id:ids)
    {
        if(const auto member=physical.find(id);member!=physical.end())projected.members.push_back(member->second);
        else{NativeVirtualMember resolved;if(!resolveNativeVirtualMember(track,id,resolved,error))return false;projected.members.push_back(std::move(resolved));}
    }
    NativeVirtualConnectionPlan logical;logical.retainedTopology=projected.retainedTopology;
    for(std::size_t i=0;i<ids.size();)
    {
        if(const auto split=splitPlans.find(ids[i]);split!=splitPlans.end())
        {
            logical.members.push_back(split->second.logical.members[1]);
            projected.contributionRanges.emplace_back(i,i+1);
            if(projected.contributionRanges.size()==2)projected.splitMiddleTargetSeconds=split->second.middleTargetSeconds;
            i+=2;
        }
        else{logical.members.push_back(projected.members[i]);projected.contributionRanges.emplace_back(i,i);++i;}
    }
    if(!planNativeVirtualHandovers(track,ids,logical,error,knownRouteMembers))return false;
    projected.contributionMembers=std::move(logical.members);projected.handovers=std::move(logical.handovers);
    projected.throughTransition=logical.throughTransition;projected.geometryNotice=logical.geometryNotice;
    for(std::size_t i=0;i<projected.handovers.size();++i)
    {
        // A moved half does not lend the OTHER half's union support to its
        // external neighbour. Reuse the split planner's exact physical edge.
        for(const auto& [frontId,split]:splitPlans)
        {
            juce::ignoreUnused(frontId);
            if(projected.contributionMembers[i].noteId==split.logical.members[1].noteId)projected.handovers[i]=split.logical.handovers[1];
            if(projected.contributionMembers[i+1].noteId==split.logical.members[1].noteId)projected.handovers[i]=split.logical.handovers[0];
        }
        projected.handovers[i].leftNoteId=ids[projected.contributionRanges[i].second];
        projected.handovers[i].rightNoteId=ids[projected.contributionRanges[i+1].first];
    }
    if(!validateNativeVirtualHopTargets(group,projected,error)||!applyNativeVirtualHandoverWindows(group,projected,error))return false;
    output=std::move(projected);error.clear();return true;
}
// Pure prepare authority for UI drafts and the single Model commit. The input
// geometry is always rebuilt WITHOUT overrides to prevent cumulative offsets.
inline bool prepareNativeVirtualHandoverWindow(const TrackData& track,const NativeVirtualGroup& group,
    const juce::String& left,const juce::String& right,double absoluteFrom,double absoluteTo,
    NativeVirtualGroup& output,juce::String& error)
{
    error.clear();
    if(!std::isfinite(absoluteFrom)||!std::isfinite(absoluteTo)||absoluteTo<=absoluteFrom)
    {error="Handover window needs finite, increasing boundaries";return false;}
    if(!validateNativeVirtualHandoverWindows(group,error))return false;
    auto canonicalGroup=group;canonicalGroup.handoverWindows.clear();NativeVirtualConnectionPlan canonical;
    if(!resolveNativeVirtualGroup(track,canonicalGroup,canonical,error))return false;
    const auto at=std::find_if(canonical.handovers.begin(),canonical.handovers.end(),[&](const auto& h){return h.leftNoteId==left&&h.rightNoteId==right;});
    if(at==canonical.handovers.end()){error="Unknown or non-adjacent physical handover identity";return false;}
    double low=0,high=0;
    if(!nativeVirtualHandoverWindowSupport(canonical,std::size_t(std::distance(canonical.handovers.begin(),at)),low,high))
    {error="A gap or touching cut has no positive real handover support";return false;}
    if(absoluteFrom<low||absoluteTo>high)
    {error="Handover window cannot expand canonical overlap or cross a typed middle";return false;}
    NativeVirtualHandoverWindow window{left,right,absoluteFrom-at->from,absoluteTo-at->to};
    auto candidate=group;
    const auto old=std::find_if(candidate.handoverWindows.begin(),candidate.handoverWindows.end(),[&](const auto& w){return w.leftNoteId==left&&w.rightNoteId==right;});
    if(old==candidate.handoverWindows.end())candidate.handoverWindows.push_back(window);else *old=window;
    NativeVirtualConnectionPlan preview;if(!resolveNativeVirtualGroup(track,candidate,preview,error))return false;
    output=std::move(candidate);return true;
}
inline juce::String nativeVirtualHopPitchId(const juce::String& group,const juce::String& left,const juce::String& right)
{return group+"/hop/"+juce::String(left.length())+":"+left+right;}
inline bool nativeVirtualCurveTarget(const NativeVirtualGroup& group,const juce::String& id)
{return group.id==id||id.startsWith(group.id+"/hop/");}
struct NativeVirtualPitchHop
{
    NativeVirtualGroup group;
    NativeVirtualConnectionPlan geometry;
};
inline std::vector<NativeVirtualPitchHop> nativeVirtualPitchHops(const NativeVirtualGroup& group,const NativeVirtualConnectionPlan& geometry)
{
    std::vector<NativeVirtualPitchHop> hops;const bool mapped=!geometry.contributionRanges.empty();
    const auto& roles=mapped?geometry.contributionMembers:geometry.members;
    for(std::size_t i=0;i+1<roles.size();)
    {
        const auto end=(roles[i+1].transition&&i+2<roles.size())?i+2:i+1;
        const auto first=mapped?geometry.contributionRanges[i].first:i,last=mapped?geometry.contributionRanges[end].second:end;
        NativeVirtualPitchHop hop;hop.group=group;hop.group.hopPitchCurves.clear();hop.group.pitchCurve.clear();hop.group.sharedPitch=false;
        hop.group.orderedNoteIds.assign(group.orderedNoteIds.begin()+std::ptrdiff_t(first),group.orderedNoteIds.begin()+std::ptrdiff_t(last+1));
        hop.group.handoverWindows.erase(std::remove_if(hop.group.handoverWindows.begin(),hop.group.handoverWindows.end(),[&](const auto& w)
            {return std::find(hop.group.orderedNoteIds.begin(),hop.group.orderedNoteIds.end(),w.leftNoteId)==hop.group.orderedNoteIds.end()
                ||std::find(hop.group.orderedNoteIds.begin(),hop.group.orderedNoteIds.end(),w.rightNoteId)==hop.group.orderedNoteIds.end();}),hop.group.handoverWindows.end());
        hop.group.id=nativeVirtualHopPitchId(group.id,hop.group.orderedNoteIds.front(),hop.group.orderedNoteIds.back());
        hop.geometry.retainedTopology=geometry.retainedTopology;hop.geometry.throughTransition=end-i==2;
        hop.geometry.members.assign(geometry.members.begin()+std::ptrdiff_t(first),geometry.members.begin()+std::ptrdiff_t(last+1));
        hop.geometry.handovers.assign(geometry.handovers.begin()+std::ptrdiff_t(i),geometry.handovers.begin()+std::ptrdiff_t(end));
        if(mapped)
        {
            hop.geometry.contributionMembers.assign(roles.begin()+std::ptrdiff_t(i),roles.begin()+std::ptrdiff_t(end+1));
            for(std::size_t r=i;r<=end;++r)hop.geometry.contributionRanges.emplace_back(geometry.contributionRanges[r].first-first,geometry.contributionRanges[r].second-first);
        }
        hop.group.pitchDurationSeconds=std::max(.002,std::min({group.pitchDurationSeconds,
            (hop.geometry.members.front().to-hop.geometry.members.front().from)*.4,(hop.geometry.members.back().to-hop.geometry.members.back().from)*.4}));
        for(const auto& manual:group.hopPitchCurves)if(manual.leftNoteId==hop.group.orderedNoteIds.front()&&manual.rightNoteId==hop.group.orderedNoteIds.back())
        {hop.group.pitchCurve=manual.pitchCurve;hop.group.pitchCurveEncoding=manual.pitchCurveEncoding;hop.group.pitchFromOffsetSeconds=manual.pitchFromOffsetSeconds;hop.group.pitchToOffsetSeconds=manual.pitchToOffsetSeconds;}
        hops.push_back(std::move(hop));i=end;
    }
    return hops;
}
}

#pragma once
#include "NativeVirtualGroupPlan.h"
#include <map>
#include <set>
namespace hachi
{
struct NativeVirtualChainExtension
{
    NativeVirtualGroup group;
    std::vector<std::size_t> consumedGroups;
};
// Topology only. Existing directed paths are never sorted by their current
// horizontal position; the newly requested edge joins their exact endpoints.
inline bool planNativeVirtualChainExtension(const ProjectData& project,const std::vector<juce::String>& requested,
    NativeVirtualChainExtension& output,juce::String& error,
    const NativeSplitTransitionEvidence* incomingSplit=nullptr)
{
    error.clear();const auto fail=[&](const juce::String& why){error=why;return false;};
    const std::set<juce::String> selected(requested.begin(),requested.end());
    NativeVirtualChainExtension plan;std::map<juce::String,juce::String> next,previous;std::set<juce::String> nodes;
    std::map<juce::String,NativeSplitTransitionEvidence> proofs;
    const auto addProof=[&](const NativeSplitTransitionEvidence& proof)
    {
        if(const auto old=proofs.find(proof.logicalId);old!=proofs.end())
            return old->second.frontNoteId==proof.frontNoteId&&old->second.backNoteId==proof.backNoteId
                &&old->second.sourceObjectIdentity==proof.sourceObjectIdentity&&old->second.frontElementIdentity==proof.frontElementIdentity&&old->second.backElementIdentity==proof.backElementIdentity;
        proofs.emplace(proof.logicalId,proof);return true;
    };
    const auto addPath=[&](const std::vector<juce::String>& path)
    {
        for(const auto& id:path)nodes.insert(id);
        for(std::size_t i=1;i<path.size();++i)
        {
            const auto& a=path[i-1];const auto& b=path[i];
            if(a==b||(next.count(a)&&next.at(a)!=b)||(previous.count(b)&&previous.at(b)!=a))return false;
            next[a]=b;previous[b]=a;
        }
        return true;
    };
    for(std::size_t i=0;i<project.nativeVirtualGroups.size();++i)
    {
        const auto& group=project.nativeVirtualGroups[i];
        if(std::none_of(group.orderedNoteIds.begin(),group.orderedNoteIds.end(),[&](const auto& id){return selected.count(id);}))continue;
        for(const auto* proof:nativeVirtualSplitProofs(group))if(!addProof(*proof))return fail("Conflicting logical bridge identities");
        if(!group.pitchCurve.empty())return fail("Existing manual group curves are preserved; reset them explicitly before merging route components");
        if(plan.consumedGroups.empty())plan.group=group;
        else
        {
            if(plan.group.trackId!=group.trackId)return fail("Virtual chains on different tracks cannot be merged");
            plan.group.sharedPitch|=group.sharedPitch;plan.group.automaticPitch|=group.automaticPitch;plan.group.coupledMovement&=group.coupledMovement;
            plan.group.automaticallyCreated&=group.automaticallyCreated;
            plan.group.hopPitchCurves.insert(plan.group.hopPitchCurves.end(),group.hopPitchCurves.begin(),group.hopPitchCurves.end());
            plan.group.handoverWindows.insert(plan.group.handoverWindows.end(),group.handoverWindows.begin(),group.handoverWindows.end());
        }
        if(!addPath(group.orderedNoteIds))return fail("Existing virtual topology has a branch");
        plan.consumedGroups.push_back(i);
    }
    if(plan.consumedGroups.empty())return fail("No existing virtual component touches the requested connection");
    if(!addPath(requested))return fail("The requested connection would branch or reverse an existing directed path");
    std::vector<juce::String> roots;for(const auto& id:nodes)if(!previous.count(id))roots.push_back(id);
    if(roots.size()!=1)return fail("The requested virtual path is disconnected or cyclic");
    std::vector<juce::String> ordered;auto current=roots.front();std::set<juce::String> visited;
    for(;;)
    {
        if(!visited.insert(current).second)return fail("The requested virtual connection would create a cycle");
        ordered.push_back(current);const auto edge=next.find(current);if(edge==next.end())break;current=edge->second;
    }
    if(ordered.size()!=nodes.size())return fail("The requested connection leaves a disconnected or cyclic component");
    const auto track=std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t){return t.id==plan.group.trackId;});
    if(track==project.tracks.end())return fail("The existing virtual track is missing");
    // Validate the NEW selection conservatively, while existing links retain
    // their geometry freedoms. Explicit old members need not still overlap.
    NativeVirtualConnectionPlan newGeometry;
    NativeVirtualGroup requestedGroup;requestedGroup.trackId=plan.group.trackId;requestedGroup.orderedNoteIds=requested;
    if(incomingSplit)requestedGroup.splitTransition=*incomingSplit;
    if(!resolveNativeVirtualGroup(*track,requestedGroup,newGeometry,error,NativeVirtualResolveMode::newSelection,&nodes))return false;
    if(incomingSplit&&!addProof(*incomingSplit))return fail("Incoming bridge conflicts with the existing logical source proof");
    plan.group.splitTransition.reset();plan.group.extraSplitTransitions.clear();
    for(const auto& [id,proof]:proofs)
    {juce::ignoreUnused(id);if(!plan.group.splitTransition)plan.group.splitTransition=proof;else plan.group.extraSplitTransitions.push_back(proof);}
    plan.group.orderedNoteIds=std::move(ordered);plan.group.enabled=true;
    NativeVirtualConnectionPlan whole;
    if(!resolveNativeVirtualGroup(*track,plan.group,whole,error)||!nativeVirtualOwnersIndependentlyRenderable(*track,whole,error))return false;
    const auto included=[&](const juce::String& id){return nodes.count(id)>0;};
    for(const auto& edge:project.nativeConnections)
        if(included(edge.leftNoteId)!=included(edge.rightNoteId))return fail("An external legacy connection branches from the proposed chain");
    output=std::move(plan);return true;
}
}

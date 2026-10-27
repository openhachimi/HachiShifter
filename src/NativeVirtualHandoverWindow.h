#pragma once
#include "NativeVirtualConnectionPlan.h"

namespace hachi
{
// Structural validation also works while source geometry is temporarily absent.
inline bool validateNativeVirtualHandoverWindows(const NativeVirtualGroup& group,juce::String& error)
{
    std::set<std::pair<juce::String,juce::String>> seen;
    for(const auto& window:group.handoverWindows)
    {
        const auto left=std::find(group.orderedNoteIds.begin(),group.orderedNoteIds.end(),window.leftNoteId);
        if(window.leftNoteId.isEmpty()||window.rightNoteId.isEmpty()||left==group.orderedNoteIds.end()
            ||std::distance(left,group.orderedNoteIds.end())<2||left[1]!=window.rightNoteId
            ||!seen.emplace(window.leftNoteId,window.rightNoteId).second)
        {error="Duplicate, orphaned or non-adjacent virtual handover window";return false;}
        if(!std::isfinite(window.fromOffsetSeconds)||!std::isfinite(window.toOffsetSeconds)
            ||window.fromOffsetSeconds<0||window.toOffsetSeconds>0)
        {error="Invalid virtual handover offsets (windows cannot expand automatic support)";return false;}
        if(group.splitTransition&&group.splitTransition->frontNoteId==window.leftNoteId&&group.splitTransition->backNoteId==window.rightNoteId)
        {error="A logical bridge internal source cut is not a handover";return false;}
        for(const auto& proof:group.extraSplitTransitions)
            if(proof.frontNoteId==window.leftNoteId&&proof.backNoteId==window.rightNoteId)
            {error="A logical bridge internal source cut is not a handover";return false;}
    }
    return true;
}
// The automatic planner may retain a moved typed bridge by falling back to its
// real overlap. An override must still respect the designated typed middle.
inline bool nativeVirtualHandoverWindowSupport(const NativeVirtualConnectionPlan& plan,std::size_t index,double& from,double& to)
{
    if(index>=plan.handovers.size())return false;
    const auto& h=plan.handovers[index];from=h.from;to=h.to;
    const auto left=std::find_if(plan.members.begin(),plan.members.end(),[&](const auto& m){return m.noteId==h.leftNoteId;});
    const auto right=std::find_if(plan.members.begin(),plan.members.end(),[&](const auto& m){return m.noteId==h.rightNoteId;});
    if(left==plan.members.end()||right==plan.members.end())return false;
    from=std::max({from,left->from,right->from});to=std::min({to,left->to,right->to});
    const auto& roles=plan.contributionRanges.empty()?plan.members:plan.contributionMembers;
    if(index+1>=roles.size())return false;
    if(roles[index].transition)from=std::max(from,roles[index].middle);
    if(roles[index+1].transition)to=std::min(to,roles[index+1].middle);
    return std::isfinite(from)&&std::isfinite(to)&&to>from;
}
// Apply LAST, after canonical physical/split geometry. Unsupported edits remain
// stored for a future move back; the automatic route stays audible and warns.
inline bool applyNativeVirtualHandoverWindows(const NativeVirtualGroup& group,NativeVirtualConnectionPlan& plan,juce::String& error)
{
    if(!validateNativeVirtualHandoverWindows(group,error))return false;
    for(const auto& window:group.handoverWindows)
    {
        const auto at=std::find_if(plan.handovers.begin(),plan.handovers.end(),[&](const auto& h)
            {return h.leftNoteId==window.leftNoteId&&h.rightNoteId==window.rightNoteId;});
        if(at==plan.handovers.end()){error="Virtual handover window does not match a physical handover";return false;}
        double low=0,high=0;const auto index=std::size_t(std::distance(plan.handovers.begin(),at));
        const double requestedFrom=at->from+window.fromOffsetSeconds,requestedTo=at->to+window.toOffsetSeconds;
        if(!nativeVirtualHandoverWindowSupport(plan,index,low,high)||!std::isfinite(requestedFrom)||!std::isfinite(requestedTo)
            ||std::min(high,requestedTo)<=std::max(low,requestedFrom))
        {
            if(plan.geometryNotice.isNotEmpty())plan.geometryNotice+="; ";
            plan.geometryNotice+="Stored handover window not applied: no positive real support ("+window.leftNoteId+" -> "+window.rightNoteId+")";
            continue;
        }
        at->from=std::max(low,requestedFrom);at->to=std::min(high,requestedTo);
    }
    return true;
}
}

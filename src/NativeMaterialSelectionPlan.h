#pragma once
#include "SampleSettings.h"
#include <algorithm>
#include <cmath>

namespace hachi
{
// One physical recording and one exact annotation variant per call. Support is
// immutable annotation geometry; ownership is the nonduplicated heard body.
struct NativeMaterialSelectionMember
{
    std::size_t rowIndex = 0;
    double sourceBegin = 0, sourceEnd = 0;
    bool retainedNext = false; // actual chosen overlap relation, not an OTTO link
    std::optional<NativePronunciationEdges> usageEdges; // project-only legacy opt-in
};
struct NativeMaterialSelectionPlan
{
    std::vector<NativeMaterialSelectionMember> members;
    double sourceBegin = 0, sourceEnd = 0;
};
inline bool planNativeMaterialSelection(const std::vector<SampleRegionSetting>& rows,
    const std::vector<juce::String>& ids, NativeMaterialSelectionPlan& output, juce::String& error)
{
    constexpr double epsilon=1.e-8;
    error.clear(); NativeMaterialSelectionPlan result;
    const auto fail=[&](const juce::String& text){error=text;return false;};
    if(rows.empty()||ids.empty())return fail("No material pronunciations selected.");
    for(const auto& row:rows)
    {
        if(row.materialAudioId.isEmpty()||row.materialAudioId!=rows.front().materialAudioId)
            return fail("Material selection requires one exact audio variant.");
        if(!std::isfinite(row.regionStartSeconds)||!std::isfinite(row.regionEndSeconds)
            ||row.regionStartSeconds<0||row.regionEndSeconds<row.regionStartSeconds)
            return fail("Invalid pronunciation support coordinates.");
    }
    std::vector<std::size_t> order;
    for(const auto& id:ids)
    {
        const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.materialRegionId==id;});
        if(found==rows.end())return fail("Unknown selected pronunciation identity: "+id);
        const auto index=std::size_t(found-rows.begin());
        if(std::find(order.begin(),order.end(),index)!=order.end())return fail("Duplicate selected pronunciation identity: "+id);
        if(!std::isfinite(found->regionStartSeconds)||!std::isfinite(found->regionEndSeconds)
            ||found->regionEndSeconds-found->regionStartSeconds<.001)
            return fail("Pronunciation shorter than the native 1ms minimum: "+id);
        order.push_back(index);
    }
    // Preserve the historical unordered-ID convenience for disjoint legacy
    // rows, but never reinterpret reversed overlapping actual selection intent.
    for(std::size_t i=0;i<order.size();++i)for(std::size_t j=i+1;j<order.size();++j)
    {
        const auto& a=rows[order[i]];const auto& b=rows[order[j]];
        if(std::max(a.regionStartSeconds,b.regionStartSeconds)<std::min(a.regionEndSeconds,b.regionEndSeconds)-epsilon
            &&a.regionStartSeconds>=b.regionStartSeconds)
            return fail("Reversed or ambiguous overlapping pronunciation selection.");
    }
    std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return rows[a].regionStartSeconds<rows[b].regionStartSeconds;});
    for(const auto index:order)result.members.push_back({index,rows[index].regionStartSeconds,rows[index].regionEndSeconds,false});
    // Source evidence and actual activation are deliberately separate. A raw-O
    // legacy row in a proven complete-variant overlap chain also needs its own
    // two outer edges when dragged ALONE. No overlap in the registered variant
    // means no opt-in and unchanged legacy NONE behaviour.
    std::vector<std::size_t> supportOrder;
    for(std::size_t i=0;i<rows.size();++i)supportOrder.push_back(i);
    std::stable_sort(supportOrder.begin(),supportOrder.end(),[&](auto a,auto b){return rows[a].regionStartSeconds<rows[b].regionStartSeconds;});
    std::vector<bool> overlapEvidence(rows.size(),false);
    for(std::size_t first=0;first<supportOrder.size();)
    {
        auto last=first;double end=rows[supportOrder[first]].regionEndSeconds;
        bool monotonic=true,rawOverlap=false;
        for(;;)
        {
            const auto& row=rows[supportOrder[last]];
            rawOverlap|=std::isfinite(row.overlapSeconds)&&std::abs(row.overlapSeconds)>1.e-9;
            if(last+1==supportOrder.size()||rows[supportOrder[last+1]].regionStartSeconds>=end-epsilon)break;
            const auto& next=rows[supportOrder[last+1]];
            monotonic&=next.regionStartSeconds>row.regionStartSeconds+epsilon&&next.regionEndSeconds>row.regionEndSeconds+epsilon;
            ++last;end=std::max(end,next.regionEndSeconds);
        }
        if(last>first&&monotonic&&rawOverlap)
            for(auto i=first;i<=last;++i)overlapEvidence[supportOrder[i]]=true;
        first=last+1;
    }
    for(auto& member:result.members)if(overlapEvidence[member.rowIndex]&&!rows[member.rowIndex].nativePronunciationEdges)
    {
        NativePronunciationEdges edges;
        if(!defaultNativePronunciationEdges(rows[member.rowIndex],edges,error))return false;
        edges.provenance="selected-overlap-usage-v1";member.usageEdges=std::move(edges);
    }
    for(std::size_t i=1;i<result.members.size();++i)
    {
        auto& left=result.members[i-1];auto& right=result.members[i];
        const auto& a=rows[left.rowIndex];const auto& b=rows[right.rowIndex];
        if(b.regionStartSeconds>=a.regionEndSeconds-epsilon)continue;
        if(b.regionStartSeconds<=a.regionStartSeconds+epsilon||b.regionEndSeconds<=a.regionEndSeconds+epsilon)
            return fail("Nested or equal overlapping pronunciation supports are ambiguous.");
        const bool explicitOverlap=(std::isfinite(a.overlapSeconds)&&std::abs(a.overlapSeconds)>1.e-9)
            ||(std::isfinite(b.overlapSeconds)&&std::abs(b.overlapSeconds)>1.e-9);
        if(((!a.nativePronunciationEdges&&!left.usageEdges)||(!b.nativePronunciationEdges&&!right.usageEdges))&&!explicitOverlap)
            return fail("Overlapping pronunciation selections require source-edge profiles or explicit overlap support.");
        const auto prepareEdges=[&](const SampleRegionSetting& row,NativeMaterialSelectionMember& member)
        {
            if(row.nativePronunciationEdges)
                return validateNativePronunciationEdges(*row.nativePronunciationEdges,row.regionStartSeconds,row.regionEndSeconds,error);
            NativePronunciationEdges edges;
            if(!defaultNativePronunciationEdges(row,edges,error))return false;
            edges.provenance="selected-overlap-usage-v1";member.usageEdges=std::move(edges);return true;
        };
        if(!prepareEdges(a,left)||!prepareEdges(b,right))return false;
        // Incoming P is an authored source anchor, not a pitch/filename guess.
        // If P is outside shared support, incoming annotation begin is the
        // documented exact source-begin anchor. Never invent a midpoint seam.
        const auto seam=a.connectedNextRegionId==b.materialRegionId?b.regionStartSeconds
            :std::isfinite(b.alignmentSeconds)&&b.alignmentSeconds>=b.regionStartSeconds
                &&b.alignmentSeconds<=a.regionEndSeconds?b.alignmentSeconds:b.regionStartSeconds;
        left.sourceEnd=right.sourceBegin=seam;left.retainedNext=true;
    }
    for(const auto& member:result.members)
    {
        const auto& row=rows[member.rowIndex];
        if(member.sourceBegin<row.regionStartSeconds||member.sourceEnd>row.regionEndSeconds
            ||member.sourceEnd-member.sourceBegin<.001-epsilon)
            return fail("Selected source anchors do not define monotonic 1ms ownership bodies.");
    }
    result.sourceBegin=result.members.front().sourceBegin;
    result.sourceEnd=result.members.front().sourceEnd;
    for(const auto& member:result.members)result.sourceEnd=std::max(result.sourceEnd,member.sourceEnd);
    output=std::move(result);return true;
}
}

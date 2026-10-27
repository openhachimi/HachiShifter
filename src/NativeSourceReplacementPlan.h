#pragma once
#include "ProjectModel.h"
#include "MaterialPronunciationCatalog.h"
#include "PronunciationTiming.h"
#include "backend/UtauRenderer.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace hachi
{
// Pure source-clock planning for explicit score-to-material replacement. No
// project/editor/writer/player exists here; ProjectModel remains the committer.
struct NativeScoreMaterialSource
{
    juce::String noteId,audioId,regionId;
    juce::File audio;
    SampleRegionSetting row;
    std::shared_ptr<const std::vector<SampleRegionSetting>> completeRows;
    double scoreStart=0,scoreDuration=0;
    bool preutteranceKnown=false;
    double targetPreutterance=0;
    int consonantVelocity=100;
    juce::String retainedNextRegion;
};
struct NativeScoreMaterialGroup
{
    std::vector<std::size_t> members;
    double sourceBegin=0,sourceEnd=0,targetBegin=0,targetEnd=0;
    // Absolute source coordinates and parent-local target coordinates.
    std::vector<SourceTimePoint> clock;
};
inline bool nativeScoreMaterialContinuous(const NativeScoreMaterialSource& left,const NativeScoreMaterialSource& right)
{
    constexpr double epsilon=1.e-8;
    if(left.row.pronunciationKind==PronunciationKind::transition||right.row.pronunciationKind==PronunciationKind::transition)return false;
    if(left.audio!=right.audio||left.audioId!=right.audioId||left.audioId.isEmpty()
        ||std::abs(left.scoreStart+left.scoreDuration-right.scoreStart)>epsilon)return false;
    juce::String error;
    if(left.completeRows&&right.completeRows&&left.completeRows==right.completeRows)
        return MaterialPronunciationCatalog::continuousRows(*left.completeRows,left.regionId,right.regionId,error);
    if(left.completeRows&&MaterialPronunciationCatalog::continuousRows(*left.completeRows,left.regionId,right.regionId,error))return true;
    if(right.completeRows&&MaterialPronunciationCatalog::continuousRows(*right.completeRows,left.regionId,right.regionId,error))return true;
    // Existing actual-use relations are usable evidence, not a filename guess.
    if(left.retainedNextRegion!=right.regionId)return false;
    const std::vector<SampleRegionSetting> retained{left.row,right.row};
    return MaterialPronunciationCatalog::continuousRows(retained,left.regionId,right.regionId,error);
}
inline bool planNativeScoreMaterialClocks(const std::vector<NativeScoreMaterialSource>& nodes,
    std::vector<NativeScoreMaterialGroup>& output,juce::String& error)
{
    error.clear();std::vector<NativeScoreMaterialGroup> result;constexpr double epsilon=1.e-9;
    const auto fail=[&](const juce::String& why){error=why;return false;};
    std::vector<std::size_t> order;for(std::size_t i=0;i<nodes.size();++i)order.push_back(i);
    std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return nodes[a].scoreStart<nodes[b].scoreStart;});
    for(std::size_t begin=0;begin<order.size();)
    {
        std::size_t end=begin;while(end+1<order.size()&&nativeScoreMaterialContinuous(nodes[order[end]],nodes[order[end+1]]))++end;
        NativeScoreMaterialGroup group;for(auto i=begin;i<=end;++i)group.members.push_back(order[i]);
        const auto& first=nodes[group.members.front()];const auto& last=nodes[group.members.back()];
        group.sourceBegin=first.row.regionStartSeconds;group.sourceEnd=last.row.regionEndSeconds;
        const auto firstTiming=pronunciationTimingFor(first.row);
        const auto nativeLead=backend::scaledUtauPreutterance(firstTiming.alignmentSeconds-firstTiming.sourceStartSeconds,
            firstTiming.consonantEndSeconds-firstTiming.sourceStartSeconds,firstTiming.sourceEndSeconds-firstTiming.sourceStartSeconds,first.consonantVelocity);
        const auto lead=first.row.pronunciationKind==PronunciationKind::transition?0.0:first.preutteranceKnown?first.targetPreutterance:nativeLead;
        if(!std::isfinite(lead)||lead<0||!std::isfinite(first.scoreStart)||!std::isfinite(last.scoreDuration)||last.scoreDuration<=0)
            return fail("Invalid explicit score/preutterance timing");
        group.targetBegin=first.scoreStart-lead;group.targetEnd=last.scoreStart+last.scoreDuration;
        struct Anchor{double source,target;};std::vector<Anchor> anchors{{group.sourceBegin,group.targetBegin},{group.sourceEnd,group.targetEnd}};
        struct Zone{double begin,end,scale,weight;bool fixed;};std::vector<Zone> zones;
        for(const auto index:group.members)
        {
            const auto& node=nodes[index];const auto timing=pronunciationTimingFor(node.row);
            if(!std::isfinite(node.scoreStart)||!std::isfinite(node.scoreDuration)||node.scoreDuration<.001
                ||node.row.regionStartSeconds<group.sourceBegin-epsilon||node.row.regionEndSeconds>group.sourceEnd+epsilon)
                return fail("Invalid ordered score/source support");
            anchors.push_back({timing.alignmentSeconds,node.scoreStart});
            if(node.preutteranceKnown&&node.row.pronunciationKind!=PronunciationKind::transition)anchors.push_back({timing.sourceStartSeconds,node.scoreStart-node.targetPreutterance});
            const auto segments=pronunciationSegmentsFor(node.row);
            for(const auto& segment:segments)
            {
                const bool consonant=segment.role==NativeSegmentRole::consonant;
                const auto scale=consonant?backend::consonantVelocityScale(node.consonantVelocity):1.0;
                if(!std::isfinite(scale)||scale<=0)return fail("Consonant velocity cannot produce a finite monotonic source clock");
                zones.push_back({timing.sourceStartSeconds+segment.sourceStartSeconds,timing.sourceStartSeconds+segment.sourceEndSeconds,
                    scale,segment.stretchWeight,!((segment.role==NativeSegmentRole::vowel||node.row.pronunciationKind==PronunciationKind::transition)&&segment.stretchable)});
            }
        }
        std::stable_sort(anchors.begin(),anchors.end(),[](const auto& a,const auto& b){return a.source<b.source;});
        std::vector<Anchor> unique;
        for(const auto& anchor:anchors)
        {
            if(!std::isfinite(anchor.source)||!std::isfinite(anchor.target))return fail("Non-finite source/score anchor");
            if(!unique.empty()&&std::abs(anchor.source-unique.back().source)<=epsilon)
            {if(std::abs(anchor.target-unique.back().target)>epsilon)return fail("Explicit preutterance conflicts with the shared physical-source P anchors");continue;}
            if(!unique.empty()&&anchor.target<=unique.back().target+epsilon)
                return fail("The full pronunciation/context cannot fit the requested score/P anchors without dropping source audio");
            unique.push_back(anchor);
        }
        for(std::size_t interval=1;interval<unique.size();++interval)
        {
            const auto a=unique[interval-1],b=unique[interval];const auto targetLength=b.target-a.target;
            std::vector<double> cuts{a.source,b.source};for(const auto& zone:zones)for(const auto cut:{zone.begin,zone.end})if(cut>a.source+epsilon&&cut<b.source-epsilon)cuts.push_back(cut);
            std::sort(cuts.begin(),cuts.end());cuts.erase(std::unique(cuts.begin(),cuts.end(),[&](double x,double y){return std::abs(x-y)<=epsilon;}),cuts.end());
            struct Piece{double end,length,weight;bool fixed;};std::vector<Piece> pieces;double fixedLength=0,weight=0;
            for(std::size_t i=1;i<cuts.size();++i)
            {
                const auto middle=(cuts[i-1]+cuts[i])*.5;bool fixed=false,found=false;double scale=1,stretchWeight=1;
                for(const auto& zone:zones)if(middle>=zone.begin-epsilon&&middle<=zone.end+epsilon)
                {
                    if(zone.fixed){if(fixed&&std::abs(scale-zone.scale)>epsilon)return fail("Conflicting consonant-velocity ownership in overlapping author support");fixed=true;scale=zone.scale;}
                    if(!found)stretchWeight=zone.weight;else if(!fixed&&std::abs(stretchWeight-zone.weight)>epsilon)return fail("Conflicting elastic source weights");found=true;
                }
                if(!found)fixed=true;const auto length=(cuts[i]-cuts[i-1])*(fixed?scale:1.0);const auto ownWeight=(cuts[i]-cuts[i-1])*stretchWeight;
                if(!std::isfinite(length)||!std::isfinite(ownWeight)||ownWeight<=0)return fail("Invalid protected/elastic source interval");
                pieces.push_back({cuts[i],length,ownWeight,fixed});if(fixed)fixedLength+=length;else weight+=ownWeight;
            }
            // Explicit UST preutterance is already the final TARGET lead-in;
            // do not apply consonant velocity to that final override a second
            // time. Every pre-P source sample remains inside the mapped window.
            const bool prefix=(interval==1&&std::abs(b.source-firstTiming.alignmentSeconds)<=epsilon)
                ||std::any_of(group.members.begin(),group.members.end(),[&](const auto member){const auto& node=nodes[member];return node.preutteranceKnown&&node.row.pronunciationKind!=PronunciationKind::transition&&a.source>=node.row.regionStartSeconds-epsilon&&b.source<=node.row.alignmentSeconds+epsilon;});
            if(prefix&&weight<=epsilon)
            {
                if(fixedLength<=epsilon)return fail("A positive source consonant cannot map to zero target preutterance");
                for(auto& piece:pieces)piece.length*=targetLength/fixedLength;
            }
            else
            {
                const auto budget=targetLength-fixedLength;
                if(weight<=epsilon){if(std::abs(budget)>1.e-7)return fail("Requested duration changes a protected pronunciation with no elastic vowel");}
                else{if(budget<=epsilon)return fail("Requested score interval is shorter than its full protected consonant context");for(auto& piece:pieces)if(!piece.fixed)piece.length=budget*piece.weight/weight;}
            }
            if(group.clock.empty())group.clock.push_back({a.target,a.source});double target=a.target;
            for(std::size_t i=0;i<pieces.size();++i){target+=pieces[i].length;if(i+1==pieces.size())target=b.target;group.clock.push_back({target,pieces[i].end});}
        }
        for(std::size_t i=1;i<group.clock.size();++i)if(group.clock[i].sourceSeconds<=group.clock[i-1].sourceSeconds||group.clock[i].targetSeconds<=group.clock[i-1].targetSeconds)
            return fail("Source replacement produced a non-monotonic clock");
        result.push_back(std::move(group));begin=end+1;
    }
    output=std::move(result);return true;
}
}

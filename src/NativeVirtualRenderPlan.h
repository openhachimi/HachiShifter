#pragma once
#include "NativeVirtualPitchPlan.h"
#include "NativeVirtualGroupPlan.h"
#include "NativeVirtualSinglePitch.h"
#include <map>

namespace hachi
{
struct NativeVirtualRenderGroup
{
    juce::String id,trackId;
    NativeVirtualConnectionPlan plan;
    NativeVirtualPitchPlan pitch;
};
inline std::vector<ResolvedConnectionPitchCurve> nativeVirtualResolvedPitchCurves(
    const std::vector<NativeVirtualRenderGroup>& groups,const juce::String& trackId)
{
    std::vector<ResolvedConnectionPitchCurve> curves;
    for(const auto& group:groups)if(group.trackId==trackId&&group.pitch.active)
    {
        if(group.pitch.segments.empty()||group.pitch.includeBaseCurve)curves.push_back(resolvedNativeVirtualPitch(group.pitch));
        for(const auto& segment:group.pitch.segments)if(segment.active)curves.push_back(resolvedNativeVirtualPitch(segment));
    }
    return curves;
}
// Editor/display entry: the SAME overlays as rendering (all active group
// curves, then the continuity line), so the roll draws exactly one line.
inline SharedPitchLines overlayNativeVirtualSingleLine(const TrackData& track,SharedPitchLines lines,
    const std::vector<NativeVirtualGroup>& groups,const juce::String& replacedNoteId={},
    const std::vector<PitchCurveEditPoint>* replacedPoints=nullptr)
{
    overlayNativeVirtualSharedPitchLines(lines,track,groups,replacedNoteId,replacedPoints);
    auto graphCurves=nativeVirtualPitchCurvesForTrack(track,groups,false);
    // A raw shared base is already resolved above with the LIVE stroke. Do not
    // cover it again with a base sampled from the unedited authoring snapshot.
    std::erase_if(graphCurves,[&](const auto& curve){return std::any_of(groups.begin(),groups.end(),[&](const auto& group){return group.sharedPitch&&group.pitchCurve.empty()&&curve.connectionId==group.id;});});
    lines=overlayResolvedConnectionPitchCurves(track,std::move(lines),std::move(graphCurves));
    std::vector<ResolvedConnectionPitchCurve> continuity;
    for(const auto& group:groups)if(group.enabled&&group.trackId==track.id)
    {
        NativeVirtualConnectionPlan plan;juce::String issue;if(!resolveNativeVirtualGroup(track,group,plan,issue))continue;
        auto curves=nativeVirtualContinuityCurves(track,group.id,plan,lines,replacedNoteId,replacedPoints);continuity.insert(continuity.end(),curves.begin(),curves.end());
    }
    if(!continuity.empty())lines=overlayResolvedConnectionPitchCurves(track,std::move(lines),continuity);
    for(const auto& group:groups)if(group.enabled&&group.trackId==track.id)
    {NativeVirtualConnectionPlan plan;juce::String issue;if(resolveNativeVirtualGroup(track,group,plan,issue))bindNativeVirtualSinglePitch(lines,track,group.id,plan);}
    return lines;
}
// Opt-in render-only projection. Original sources, explicit author fades,
// note gains/envelopes and graph data stay intact in the ProjectModel. Active
// group internal legacy joins are temporarily suppressed so the group route
// cannot also render the old direct path. Disabled/absent groups do nothing.
inline bool prepareNativeVirtualRender(const ProjectData& input,ProjectData& output,
    std::vector<NativeVirtualRenderGroup>& groups,juce::String& error,
    juce::StringArray* unavailableGroups=nullptr)
{
    error.clear();std::vector<NativeVirtualRenderGroup> proposed;
    std::map<juce::String,std::size_t> memberGroup;
    for(const auto& group:input.nativeVirtualGroups)if(group.enabled)
    {
        NativeVirtualRenderGroup resolved;resolved.id=group.id;resolved.trackId=group.trackId;
        const auto validate=[&]
        {
            const TrackData* track=nullptr;
            for(const auto& t:input.tracks)if(t.id==group.trackId){if(track){error="Ambiguous virtual track identity";return false;}track=&t;}
            if(!track){error="Virtual group track disappeared: "+group.id;return false;}
            if(!resolveNativeVirtualGroup(*track,group,resolved.plan,error)||!nativeVirtualOwnersIndependentlyRenderable(*track,resolved.plan,error))return false;
            for(const auto& member:resolved.plan.members)
            {
                if(memberGroup.count(member.noteId)){error="Overlapping virtual group membership";return false;}
                if(unavailableGroups&&!member.source.existsAsFile()){error="Virtual source recording is unavailable: "+member.source.getFullPathName();return false;}
            }
            resolved.pitch=planNativeVirtualPitch(*track,group,resolved.plan);return true;
        };
        if(!validate())
        {
            if(!unavailableGroups)return false; // authoring/validation callers stay strict
            unavailableGroups->add("Virtual group ["+group.id+"] is unavailable: "+error);
            error.clear();continue;
        }
        if(unavailableGroups&&resolved.plan.geometryNotice.contains("Stored handover window not applied"))
            unavailableGroups->add("Virtual handover ["+group.id+"]: "+resolved.plan.geometryNotice);
        if(unavailableGroups&&resolved.pitch.conflictingWindows)
            unavailableGroups->add("Virtual pitch ["+group.id+"]: "+resolved.pitch.issue);
        for(const auto& member:resolved.plan.members)memberGroup.emplace(member.noteId,proposed.size());
        proposed.push_back(std::move(resolved));
    }
    for(const auto& edge:input.nativeConnections)
    {
        const auto a=memberGroup.find(edge.leftNoteId),b=memberGroup.find(edge.rightNoteId);
        if((a==memberGroup.end())!=(b==memberGroup.end())
            ||(a!=memberGroup.end()&&b!=memberGroup.end()&&a->second!=b->second))
        {error="External connection now branches from a virtual group";return false;}
    }
    auto render=input;
    std::erase_if(render.nativeConnections,[&](const auto& edge)
    {return memberGroup.count(edge.leftNoteId)&&memberGroup.count(edge.rightNoteId);});
    for(const auto& group:proposed)
        for(std::size_t i=0;i<group.plan.members.size();++i)
        {
            const auto& member=group.plan.members[i];
            for(auto& track:render.tracks)if(track.id==group.trackId)
            {
                // Only member owners are projected; unrelated track-wide
                // synthesis and overlap policies must remain unchanged.
                for(auto& parent:track.clips)if(parent.id==member.parentId)
                {
                    ClipData* owner=parent.parts.empty()?&parent:nullptr;
                    for(auto& part:parent.parts)if(part.id==member.ownerId)owner=&part;
                    if(!owner){error="Virtual owner changed during projection";return false;}
                    const auto origin=parent.startSeconds+(parent.parts.empty()?0.:owner->startSeconds);
                    const auto audibleFrom=origin+owner->audioStartSeconds;
                    const auto audibleTo=audibleFrom+owner->audioLength();
                    // A member inside da_re must not rewrite da's outer fade.
                    if(i>0&&member.from<=audibleFrom+1.e-7){owner->crossfadeInSeconds=0;owner->glideConnectedFromPrevious=false;}
                    if(i+1<group.plan.members.size()&&member.to>=audibleTo-1.e-7){owner->crossfadeOutSeconds=0;owner->glideConnectedToNext=false;}
                    for(auto& note:parent.notes)if(note.id==member.noteId)
                    {if(i>0)note.connectedToPrevious=false;if(i+1<group.plan.members.size())note.connectedToNext=false;}
                }
            }
        }
    output=std::move(render);groups=std::move(proposed);return true;
}
}

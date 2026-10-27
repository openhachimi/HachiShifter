#pragma once
#include "NativeAudioDisconnect.h"
namespace hachi
{
inline bool nativeAudioLinkAvailable(const ProjectData& data,const std::vector<juce::String>& ids)
{
    if(ids.size()<2)return false;
    std::size_t count=0;juce::String owner,region;bool separate=false;
    for(const auto& track:data.tracks)for(const auto& clip:track.clips)for(const auto& note:clip.notes)
        if(std::find(ids.begin(),ids.end(),note.id)!=ids.end())
        {
            if(!trackShowsAllNativeRegions(track)||(owner.isNotEmpty()&&owner!=track.id))return false;
            owner=track.id;++count;
            if(region.isEmpty())region=clip.id;else separate |= region!=clip.id;
            separate |= !clip.parts.empty()&&!clip.nativeAudioLinked;
        }
    return count==ids.size()&&separate;
}
inline ClipData assembledLinkedAudio(const std::vector<ClipData>& clips)
{
    ClipData result;result.id=clips.front().id;result.startSeconds=clips.front().startSeconds;
    for(const auto& clip:clips)result.startSeconds=std::min(result.startSeconds,clip.startSeconds);
    result.sourceFile=clips.front().sourceFile;result.durationSeconds=0;result.nativeAudioLinked=true;
    for(const auto& clip:clips)
    {
        result.showNoteHints |= clip.showNoteHints;result.showNormalDisplay |= clip.showNormalDisplay;
        result.durationSeconds=std::max(result.durationSeconds,clip.startSeconds+clip.durationSeconds-result.startSeconds);
        for(auto part:expandedClipParts(clip))
        {
            part.id="audio-part-"+juce::String(static_cast<int>(result.parts.size()));
            part.startSeconds-=result.startSeconds;part.nativeAudioLinked=false;
            for(auto note:part.notes)
            {note.startSeconds+=part.startSeconds;note.clipPartId=part.id;result.notes.push_back(std::move(note));}
            part.notes.clear();result.parts.push_back(std::move(part));
        }
    }
    std::stable_sort(result.notes.begin(),result.notes.end(),[](const auto& a,const auto& b){return a.startSeconds<b.startSeconds;});
    return result;
}
// Stages only the caller's touched track. False + empty error means already one
// shared owner; failure never changes that staged track or any live project.
inline bool stageNativeAudioLink(TrackData& track,const std::vector<juce::String>& ids,juce::String& error)
{
    error.clear();const auto selected=[&](const auto& n){return std::find(ids.begin(),ids.end(),n.id)!=ids.end();};
    std::size_t owners=0;const ClipData* only=nullptr;
    for(const auto& clip:track.clips)if(std::any_of(clip.notes.begin(),clip.notes.end(),selected)){++owners;only=&clip;}
    if(owners==1&&(only->parts.empty()||only->nativeAudioLinked))return false;
    std::vector<ClipData> kept,chosen;
    for(const auto& clip:track.clips)
    {
        if(std::none_of(clip.notes.begin(),clip.notes.end(),selected)){kept.push_back(clip);continue;}
        auto parts=disconnectedNativeClip(clip,ids);
        if(!parts){error="Source ownership cannot be detached without trimming overlapping notes";return false;}
        for(auto& part:*parts)
        {
            part.id="clip-"+juce::Uuid().toString();
            if(std::any_of(part.notes.begin(),part.notes.end(),selected))chosen.push_back(std::move(part));else kept.push_back(std::move(part));
        }
    }
    if(chosen.size()<2){error="Select at least two distinct native source owners";return false;}
    auto linked=assembledLinkedAudio(chosen);linked.id="clip-"+juce::Uuid().toString();kept.push_back(std::move(linked));
    track.clips=std::move(kept);return true;
}
}

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
}

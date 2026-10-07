#pragma once
#include "NativeAudioLink.h"
namespace hachi
{
struct NativeAudioClipboard
{
    std::vector<ClipData> clips;
    std::vector<NativeConnection> connections;
    double originSeconds = 0;
};
// Store source audio and its edited clock, not just target notes. Separate
// selected runs keep unselected audio out of noncontiguous selections.
inline NativeAudioClipboard copyNativeAudioNotes(const ProjectData& data,
                                                const std::vector<juce::String>& ids)
{
    NativeAudioClipboard result;
    const auto selected=[&](const auto& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();};
    for(const auto& track:data.tracks)
    {
        if(!trackShowsAllNativeRegions(track))continue;
        struct Placed { const NoteData* note;double start; };
        std::vector<Placed> ordered;
        for(const auto& clip:track.clips)for(const auto& note:clip.notes)
            ordered.push_back({&note,clip.startSeconds+note.startSeconds});
        std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.start<b.start;});
        const auto keepJoins=[&](NoteData& note){
            const auto at=std::find_if(ordered.begin(),ordered.end(),[&](const auto& p){return p.note->id==note.id;});
            if(at==ordered.end())return;
            note.connectedToPrevious &= at!=ordered.begin()&&selected(std::prev(at)->note->id);
            note.connectedToNext &= std::next(at)!=ordered.end()&&selected(std::next(at)->note->id);
        };
        for(const auto& parent:track.clips)
        {
        const auto groupStart=result.clips.size();
        for(auto source:expandedClipParts(parent))
        {
            if(!source.sourceFile.existsAsFile())continue;
            std::stable_sort(source.notes.begin(),source.notes.end(),[](const auto& a,const auto& b){return a.startSeconds<b.startSeconds;});
            const auto absoluteStart=source.startSeconds;
            source.startSeconds=0;
            source.sourceTimeMap=nativeClipClock(source);
            std::vector<NoteData> run;
            const auto flush=[&]{
                if(run.empty())return;
                auto begin=run.front().startSeconds,end=begin;
                for(const auto& n:run)end=std::max(end,n.startSeconds+n.durationSeconds);
                if(run.front().id==source.notes.front().id)begin=std::min(begin,0.0);
                if(run.back().id==source.notes.back().id)end=std::max(end,source.durationSeconds);
                const auto cropped=slicedClipParts({source},begin,end,false,true);
                if(!cropped.empty())
                {
                    auto copy=cropped.front();copy.startSeconds=absoluteStart+begin;copy.parts.clear();
                    for(auto note:run)
                    {
                        // Source interval and dense pitch are already bound to
                        // this note; only its position within the new region changes.
                        note.startSeconds-=begin;note.clipPartId.clear();keepJoins(note);
                        copy.notes.push_back(std::move(note));
                    }
                    copy.glideConnectedFromPrevious &= copy.notes.front().connectedToPrevious;
                    copy.glideConnectedToNext &= copy.notes.back().connectedToNext;
                    result.clips.push_back(std::move(copy));
                }
                run.clear();
            };
            for(const auto& note:source.notes)
            {
                if(!selected(note.id)){flush();continue;}
                if(!run.empty()&&note.startSeconds>run.back().startSeconds+run.back().durationSeconds+.002)flush();
                run.push_back(note);
            }
            flush();
        }
        if(parent.nativeAudioLinked && result.clips.size()>groupStart+1)
        {
            std::vector<ClipData> group(result.clips.begin()+static_cast<std::ptrdiff_t>(groupStart),result.clips.end());
            result.clips.erase(result.clips.begin()+static_cast<std::ptrdiff_t>(groupStart),result.clips.end());
            result.clips.push_back(assembledLinkedAudio(group));
        }
        }
    }
    if(result.clips.empty())return result;
    std::stable_sort(result.clips.begin(),result.clips.end(),[](const auto& a,const auto& b){return a.startSeconds<b.startSeconds;});
    result.originSeconds=result.clips.front().startSeconds;
    std::vector<juce::String> copiedIds;
    for(auto& clip:result.clips)
    {clip.startSeconds-=result.originSeconds;for(const auto& note:clip.notes)copiedIds.push_back(note.id);}
    const auto copied=[&](const auto& id){return std::find(copiedIds.begin(),copiedIds.end(),id)!=copiedIds.end();};
    for(auto connection:data.nativeConnections)if(copied(connection.leftNoteId)&&copied(connection.rightNoteId))
    {connection.boundarySeconds-=result.originSeconds;result.connections.push_back(std::move(connection));}
    return result;
}
}

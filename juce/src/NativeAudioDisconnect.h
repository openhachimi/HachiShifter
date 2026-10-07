#pragma once
#include "NativeNoteTiming.h"
namespace hachi
{
inline bool nativeAudioDisconnectAvailable(const ProjectData& data,
                                           const std::vector<juce::String>& ids)
{
    if (ids.size() < 2) return false;
    const auto selected = [&](const auto& id) { return std::find(ids.begin(),ids.end(),id)!=ids.end(); };
    std::size_t found = 0; bool linked = false;
    for (const auto& track : data.tracks) for (const auto& clip : track.clips)
        for (const auto& note : clip.notes) if (selected(note.id))
        {
            if (!trackShowsAllNativeRegions(track)) return false;
            ++found;
            linked |= clip.notes.size()>1 || note.connectedToPrevious || note.connectedToNext
                || clip.glideConnectedFromPrevious || clip.glideConnectedToNext;
        }
    for (const auto& connection : data.nativeConnections)
        linked |= selected(connection.leftNoteId) || selected(connection.rightNoteId);
    return found == ids.size() && linked;
}

// Break the shared audio clock at selected note boundaries. Untouched runs
// stay together, and all source audio (including margins/gaps) is retained.
inline std::optional<std::vector<ClipData>> disconnectedNativeClip(
    const ClipData& original, const std::vector<juce::String>& ids)
{
    const auto selected = [&](const auto& id) { return std::find(ids.begin(),ids.end(),id)!=ids.end(); };
    std::vector<ClipData> result;
    for (auto source : expandedClipParts(original))
    {
        source.nativeAudioLinked=false;
        std::stable_sort(source.notes.begin(),source.notes.end(),
            [](const auto& a,const auto& b){return a.startSeconds<b.startSeconds;});
        if (source.notes.size()<2 || std::none_of(source.notes.begin(),source.notes.end(),
            [&](const auto& n){return selected(n.id);}))
        { result.push_back(std::move(source)); continue; }
        if (!source.sourceFile.existsAsFile()) return {};
        const auto absoluteStart=source.startSeconds; source.startSeconds=0;
        source.sourceTimeMap=nativeClipClock(source);
        const auto emit=[&](std::size_t first,std::size_t last,double begin,double end)
        {
            auto cropped=slicedClipParts({source},begin,end,false,true);
            if (cropped.empty()) return false;
            auto part=std::move(cropped.front()); part.startSeconds=absoluteStart+begin;
            for (auto i=first;i<last;++i)
            {
                auto note=source.notes[i]; bindNativeNoteSource(note,source,&source.sourceTimeMap);
                note.startSeconds-=begin; note.clipPartId.clear(); part.notes.push_back(std::move(note));
            }
            result.push_back(std::move(part)); return true;
        };
        std::size_t first=0; double begin=0;
        for (std::size_t i=1;i<source.notes.size();++i)
        {
            if (!selected(source.notes[i-1].id) && !selected(source.notes[i].id)) continue;
            const auto cut=source.notes[i].startSeconds;
            // Overlapping independently imported notes have no unambiguous
            // common cut. Leave the project intact instead of truncating one.
            if (source.notes[i-1].startSeconds+source.notes[i-1].durationSeconds>cut+1.e-7
                || !emit(first,i,begin,cut)) return {};
            first=i; begin=cut;
        }
        if (!emit(first,source.notes.size(),begin,source.durationSeconds)) return {};
    }
    return result;
}
}

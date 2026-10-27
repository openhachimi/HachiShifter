#pragma once
#include "NativeAudioLink.h"
namespace hachi
{
// Reuse the established sharedPitchLines authority on a temporary editing
// projection. No persistent source owner, path, note or legacy edge is written.
inline SharedPitchLines nativeVirtualSharedPitchLines(const TrackData& track,const NativeVirtualGroup& group,
    const juce::String& replacedNoteId={},const std::vector<PitchCurveEditPoint>* replacedPoints=nullptr)
{
    if(!group.enabled||!group.sharedPitch||group.trackId!=track.id)return {};
    std::vector<ClipData> chosen;
    for(const auto& parent:track.clips)for(auto owner:expandedClipParts(parent))
    {
        std::erase_if(owner.notes,[&](const auto& note){return std::find(group.orderedNoteIds.begin(),group.orderedNoteIds.end(),note.id)==group.orderedNoteIds.end();});
        if(!owner.notes.empty())chosen.push_back(std::move(owner));
    }
    if(chosen.empty())return {};
    TrackData projected;projected.id=track.id;projected.trackType=effectiveTrackType(track);
    projected.pitchAlgorithm=track.pitchAlgorithm;projected.compose=track.compose;projected.accompaniment=track.accompaniment;
    projected.clips={assembledLinkedAudio(chosen)};
    auto lines=sharedPitchLines(projected,replacedNoteId,replacedPoints);
    std::shared_ptr<const SharedPitchLine> common;
    for(const auto& note:projected.clips[0].notes)if(!note.nativeUnpitched)
    {
        const auto* member=lines.memberFor(note.id);
        // A partial monophonic subchain must not lend its F0 to an independent
        // overlapping voice elsewhere in the group. UV members need no F0.
        if(!member||!member->line||(common&&member->line!=common))return {};
        common=member->line;
    }
    return lines;
}
inline void overlayNativeVirtualSharedPitchLines(SharedPitchLines& destination,const TrackData& track,
    const std::vector<NativeVirtualGroup>& groups,const juce::String& replacedNoteId={},
    const std::vector<PitchCurveEditPoint>* replacedPoints=nullptr)
{
    for(const auto& group:groups)if(group.enabled&&group.sharedPitch&&group.trackId==track.id)
    {
        const auto shared=nativeVirtualSharedPitchLines(track,group,replacedNoteId,replacedPoints);
        for(const auto& id:group.orderedNoteIds)
            if(const auto* member=shared.memberFor(id))destination.byNote[id]=*member;
    }
}
}

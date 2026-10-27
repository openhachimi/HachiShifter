#pragma once
#include "../ProjectModel.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualEntryPointSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id=i?"owner-right":"owner-left";clip.sourceFile=output.getChildFile(i?"other-recording.wav":"first-recording.wav");
        clip.startSeconds=i?1.:0.;clip.durationSeconds=clip.sourceDurationSeconds=i?1.:1.2;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.sourceDurationSeconds}};
        clip.fadeInSeconds=.013;clip.fadeOutSeconds=.027;clip.gain=.8f;
        NoteData note;note.id=i?"right":"left";note.label="-";note.durationSeconds=clip.durationSeconds;clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};ProjectModel model;
    const auto routed=[&](bool pitch)
    {
        const auto data=model.snapshot();if(data.nativeVirtualGroups.size()!=1||!data.nativeConnections.empty())return false;
        const auto& group=data.nativeVirtualGroups[0];
        if(group.orderedNoteIds!=std::vector<juce::String>{"left","right"}||!group.enabled||group.automaticPitch!=pitch)return false;
        for(int i=0;i<2;++i)
        {
            const auto& a=seed.tracks[0].clips[i];const auto& b=data.tracks[0].clips[i];
            if(a.id!=b.id||a.sourceFile!=b.sourceFile||a.startSeconds!=b.startSeconds||a.durationSeconds!=b.durationSeconds||a.gain!=b.gain||a.fadeInSeconds!=b.fadeInSeconds||a.fadeOutSeconds!=b.fadeOutSeconds
                ||!b.parts.empty()||b.nativeAudioLinked||b.notes[0].connectedToNext||b.notes[0].connectedToPrevious||b.glideConnectedToNext||b.glideConnectedFromPrevious)return false;
        }
        return true;
    };
    model.resetDocument(seed);const auto original=model.contentFingerprint();
    check("former_force_API_writes_only_virtual_group",model.forceNativeConnection({"right","left"},true,error,.25)&&routed(true)&&model.snapshot().nativeVirtualGroups[0].pitchDurationSeconds==.25);
    const auto connected=model.contentFingerprint();const auto revision=model.revisionNumber();
    check("repeated_force_is_noop_not_parallel_legacy_join",!model.forceNativeConnection({"left","right"},true,error,.25)&&error.isEmpty()&&model.contentFingerprint()==connected&&model.revisionNumber()==revision);
    model.undo();check("former_force_one_undo_restores_exact_source_state",model.contentFingerprint()==original);
    model.redo();check("former_force_disable_routes_to_same_group",model.forceNativeConnection({"left","right"},false,error)&&model.snapshot().nativeVirtualGroups.size()==1&&!model.snapshot().nativeVirtualGroups[0].enabled&&model.snapshot().nativeConnections.empty());
    model.resetDocument(seed);check("portamento_API_used_by_MCP_cannot_bypass_virtual_group",model.setNotesPortamento({"right","left"},true,error,.2)&&routed(true)&&model.snapshot().nativeVirtualGroups[0].pitchDurationSeconds==.2);
    model.resetDocument(seed);check("link_audio_API_cannot_create_cross_source_compound",model.linkNativeAudio({"right","left"})&&routed(false));
    model.resetDocument(seed);model.setNotesConnection({"right","left"},true);check("generic_connection_API_writes_same_virtual_model",routed(false));
    model.setNotesConnection({"left","right"},false);check("generic_disconnect_disables_same_group_without_source_mutation",!model.snapshot().nativeVirtualGroups[0].enabled&&model.snapshot().nativeConnections.empty());
    model.resetDocument(seed);model.toggleNoteConnection("right");check("single_note_connection_toggle_routes_to_virtual_group",routed(false));
    model.toggleNoteConnection("right");check("second_toggle_disables_virtual_not_legacy_flags",model.snapshot().nativeVirtualGroups.size()==1&&!model.snapshot().nativeVirtualGroups[0].enabled&&model.snapshot().nativeConnections.empty());
    model.resetDocument(seed);model.setNotesConnection({"right","left"},true);const auto group=model.snapshot().nativeVirtualGroups[0].id;
    check("force_upgrades_same_group_without_second_route",model.forceNativeConnection({"left","right"},true,error)&&routed(true)&&model.snapshot().nativeVirtualGroups[0].id==group);
    const auto beforeBad=model.contentFingerprint();check("invalid_duration_does_not_create_or_edit_a_route",!model.forceNativeConnection({"left","right"},true,error,-1)&&model.contentFingerprint()==beforeBad);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

#pragma once
#include "../NativeVirtualConnectionPlan.h"
#include "../NativeVirtualGroupIO.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualGroupModelSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;
    for(int i=0;i<3;++i)
    {
        const juce::String id=i==0?"left":i==1?"bridge":"right";
        ClipData c;c.id="owner-"+id;c.sourceFile=output.getChildFile(id+".wav");
        {juce::AudioBuffer<float> pcm(1,48000);pcm.clear();auto out=c.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(out.release(),48000,1,16,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;}
        c.startSeconds=i==0?0.:i==1?.7:1.;c.durationSeconds=c.sourceDurationSeconds=i==0?1.:i==1?.6:1.;c.sourceTimeMap={{0,0},{c.durationSeconds,c.sourceDurationSeconds}};
        c.gain=.7f;c.fadeInSeconds=.012;c.fadeOutSeconds=.023;
        NoteData n;n.id=id;n.durationSeconds=c.durationSeconds;n.label=i==1?"unrelated-name":"-";n.midiNote=60+i*2;n.contour={{0,11,7,true},{n.durationSeconds,-8,-3,true}};
        if(i==1){n.pronunciationKind=PronunciationKind::transition;n.nativeTransition=NativeTransitionShape{1,.3,"test-middle"};}
        c.notes={n};track.clips.push_back(c);
    }
    seed.tracks={track};NativeConnection old;old.id="original-direct";old.leftNoteId="left";old.rightNoteId="right";old.type="pitch-portamento";old.pitchCurve={{.9,60},{1.1,64}};seed.nativeConnections={old};
    ProjectModel model;model.resetDocument(seed);const auto initial=model.contentFingerprint();const auto revision=model.revisionNumber();
    const auto groupId=model.createNativeVirtualConnection({"left","bridge","right"},false,error);
    check("create_one_reversible_group_transaction",groupId.isNotEmpty()&&model.revisionNumber()==revision+1&&model.snapshot().nativeVirtualGroups.size()==1);
    auto data=model.snapshot();
    check("old_direct_curve_retained_as_fallback_not_erased",data.nativeConnections.size()==1&&data.nativeConnections[0].id==old.id&&data.nativeConnections[0].pitchCurve.size()==2&&data.nativeConnections[0].pitchCurve[0].targetMidi==60);
    bool untouched=true;for(int i=0;i<3;++i){const auto& a=seed.tracks[0].clips[i];const auto& b=data.tracks[0].clips[i];untouched&=a.sourceFile==b.sourceFile&&a.id==b.id&&a.startSeconds==b.startSeconds&&a.gain==b.gain&&a.fadeInSeconds==b.fadeInSeconds&&a.fadeOutSeconds==b.fadeOutSeconds&&a.notes[0].midiNote==b.notes[0].midiNote&&a.notes[0].contour[0].relativeCents==b.notes[0].contour[0].relativeCents;}
    check("group_does_not_mutate_physical_owners_music_or_author_fades",untouched);
    model.undo();check("one_undo_restores_exact_original",model.contentFingerprint()==initial&&model.snapshot().nativeVirtualGroups.empty());model.redo();
    check("redo_restores_same_group_identity",model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeVirtualGroups[0].id==groupId);
    const auto beforeReject=model.contentFingerprint();check("double_membership_rejected_atomically",model.createNativeVirtualConnection({"left","bridge","right"},true,error).isEmpty()&&error.contains("already belongs")&&model.contentFingerprint()==beforeReject);
    check("disable_for_AB_retains_group_and_original_graph",model.setNativeVirtualConnectionEnabled(groupId,false,error)&&!model.snapshot().nativeVirtualGroups[0].enabled&&model.snapshot().nativeConnections.size()==1);
    const auto file=output.getChildFile("virtual-group.hjpx");ProjectModel cold;
    check("disabled_group_cold_roundtrip",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().nativeVirtualGroups.size()==1&&cold.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"left","bridge","right"}&&!cold.snapshot().nativeVirtualGroups[0].enabled);
    check("reenable_revalidates_current_support",cold.setNativeVirtualConnectionEnabled(groupId,true,error));
    check("dissolve_restores_exact_original_without_audio_unmix",model.removeNativeVirtualConnection(groupId,error)&&model.contentFingerprint()==initial);
    model.undo();check("dissolve_is_undoable",model.snapshot().nativeVirtualGroups.size()==1);
    auto group=cold.snapshot().nativeVirtualGroups[0];auto tree=nativeVirtualGroupTree(group);NativeVirtualGroup restored;
    check("versioned_schema_roundtrip",nativeVirtualGroupFromTree(tree,restored,error)&&restored.id==group.id&&restored.orderedNoteIds==group.orderedNoteIds);
    tree.setProperty("version",2,nullptr);check("future_group_version_refused",!nativeVirtualGroupFromTree(tree,restored,error));
    tree=nativeVirtualGroupTree(group);tree.getChild(1).setProperty("noteId","left",nullptr);check("duplicate_serialized_member_refused",!nativeVirtualGroupFromTree(tree,restored,error));
    tree=nativeVirtualGroupTree(group);tree.setProperty("enabled","yes",nullptr);check("wrong_flag_type_refused",!nativeVirtualGroupFromTree(tree,restored,error));
    const auto stale=cold.snapshot();auto moved=stale;moved.tracks[0].clips[2].startSeconds=5;cold.resetDocument(moved);check("moved_group_can_be_disabled_without_dissolving",cold.setNativeVirtualConnectionEnabled(groupId,false,error));
    check("reenable_moved_gap_retains_original_binding",cold.setNativeVirtualConnectionEnabled(groupId,true,error)&&cold.snapshot().nativeVirtualGroups[0].orderedNoteIds==stale.nativeVirtualGroups[0].orderedNoteIds&&cold.snapshot().tracks[0].clips[2].startSeconds==5);
    auto multi=seed;auto extra=multi.tracks[0].clips[0].notes[0];extra.id="other-syllable";extra.durationSeconds=.1;multi.tracks[0].clips[0].notes.push_back(extra);ProjectModel unsupported;unsupported.resetDocument(multi);const auto untouchedOwner=unsupported.contentFingerprint();
    check("unsupported_multi_note_owner_rejected_before_commit_not_silenced_after",unsupported.createNativeVirtualConnection({"left","bridge","right"},false,error).isEmpty()&&error.contains("independent source support")&&unsupported.contentFingerprint()==untouchedOwner&&!unsupported.canUndo());
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

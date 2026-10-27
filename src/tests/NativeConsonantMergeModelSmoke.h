#pragma once
#include "../NativeConsonantMergePlan.h"
#include "../NativeConsonantMergeIO.h"
#include "../AudioEngine.h"
#include "../OttoMaterialIO.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
namespace hachi
{
inline bool runNativeConsonantMergeModelSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto source=output.getChildFile("source.wav");
    {juce::AudioBuffer<float> pcm(1,48000);for(int s=0;s<48000;++s)pcm.setSample(0,s,.1f*std::sin(juce::MathConstants<double>::twoPi*173*s/48000.));auto stream=source.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;}
    const auto hash=juce::SHA256(source).toHexString();ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;
    ClipData onset;onset.id="onset-owner";onset.sourceFile=source;onset.startSeconds=1.;onset.durationSeconds=.1;onset.sourceOffsetSeconds=.2;onset.sourceDurationSeconds=.08;onset.sourceTimeMap={{0,0},{.04,.02},{.1,.08}};onset.crossfadeOutSeconds=.01;onset.glideConnectedToNext=true;
    NoteData sh;sh.id="onset";sh.durationSeconds=.1;sh.connectedToNext=true;for(int f=0;f<=20;++f)sh.contour.push_back({f*.005,0,0,false});onset.notes={sh};
    ClipData vowel;vowel.id="vowel-owner";vowel.sourceFile=source;vowel.startSeconds=1.1;vowel.durationSeconds=.4;vowel.sourceOffsetSeconds=.28;vowel.sourceDurationSeconds=.3;vowel.sourceTimeMap={{0,0},{.1,.06},{.4,.3}};vowel.crossfadeInSeconds=.01;vowel.glideConnectedFromPrevious=true;
    NoteData i;i.id="vowel";i.durationSeconds=.4;i.midiNote=i.sourceMidiCenter=64;i.connectedToPrevious=true;i.utauPreutteranceOverrideEnabled=true;i.utauPreutteranceSeconds=.03;i.vibratoEnabled=true;i.vibratoLengthPercent=50;for(int f=0;f<=80;++f)i.contour.push_back({f*.005,float(f*.1),float(f*.07),true});vowel.notes={i};track.clips={onset,vowel};data.tracks={track};
    NativeConsonantMergeEvidence evidence;evidence.sibilantIdentified=true;auto& e=evidence.sourceJoin;e.logicalId="original-join";e.frontNoteId="onset";e.backNoteId="vowel";e.sourceObjectIdentity=e.frontSourceObjectIdentity=e.backSourceObjectIdentity="source-17";e.precedingElementIdentity=e.frontElementIdentity="element-21";e.followingElementIdentity=e.backElementIdentity="element-22";e.joinsPitches=e.joinsAmplitudes=e.joinsPhases=true;
    ProjectModel model;model.resetDocument(data);const auto original=model.contentFingerprint();
    const auto mergedId=model.mergeMelodyneConsonant(evidence,error);
    check("one_atomic_merge_keeps_vowel_identity_and_durable_archive",mergedId=="vowel"&&model.snapshot().tracks[0].clips.size()==1&&model.snapshot().consonantMerges.size()==1&&model.snapshot().tracks[0].clips[0].notes[0].consonantMergeId==model.snapshot().consonantMerges[0].id);
    if(mergedId.isEmpty()){output.getChildFile("verification.txt").replaceWithText(log);return false;}
    const auto merged=model.snapshot();const auto mergedFingerprint=model.contentFingerprint();
    const auto& clip=merged.tracks[0].clips[0];const auto timing=nativeNoteSourceTiming(clip.notes[0],clip,nativeClipClock(clip));
    check("merged_PC_are_independent_and_from_original_source_cut",std::abs(timing.alignmentSeconds-.28)<1.e-8&&std::abs(timing.fixedDurationSeconds-.11)<1.e-8);
    model.undo();check("merge_one_undo_restores_entire_original_project",model.contentFingerprint()==original&&model.snapshot().consonantMerges.empty());model.redo();check("merge_redo_preserves_archive_identity",model.contentFingerprint()==mergedFingerprint);
    const auto file=output.getChildFile("merged.hjpx");ProjectModel cold;
    check("merged_note_and_archive_survive_cold_save_load",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().consonantMerges.size()==1&&cold.snapshot().tracks[0].clips[0].notes[0].consonantMergeId==merged.consonantMerges[0].id);
    check("cold_split_back_restores_exact_original_ids_curves_fades_and_clocks",cold.restoreMelodyneConsonant("vowel",false,error)&&cold.contentFingerprint()==original&&cold.snapshot().tracks[0].clips[0].notes[0].id=="onset"&&cold.snapshot().tracks[0].clips[1].crossfadeInSeconds==.01&&cold.snapshot().consonantMerges.empty());
    cold.undo();check("split_back_is_one_undoable_action",cold.snapshot().tracks[0].clips.size()==1&&cold.snapshot().consonantMerges.size()==1);
    model.resetDocument(merged);model.transposeNote("vowel",2);const auto edited=model.contentFingerprint();
    check("edited_merged_note_requires_explicit_discard_confirmation",!model.restoreMelodyneConsonant("vowel",false,error)&&error.contains("explicitly confirm")&&model.contentFingerprint()==edited);
    check("confirmed_discard_restores_original_fragments_not_new_generic_split",model.restoreMelodyneConsonant("vowel",true,error)&&model.contentFingerprint()==original);
    const auto relocated=output.getChildFile("registered-copy.wav");check("source_relocation_fixture",source.copyFileTo(relocated));auto movedSource=merged;movedSource.tracks[0].clips[0].sourceFile=relocated;model.resetDocument(movedSource);
    check("split_back_uses_current_proven_recording_not_stale_archive_paths",model.restoreMelodyneConsonant("vowel",false,error)&&model.snapshot().tracks[0].clips[0].sourceFile==relocated&&model.snapshot().tracks[0].clips[1].sourceFile==relocated);
    struct RestoreRoots{juce::StringArray roots;~RestoreRoots(){SampleSettings::setMaterialRoots(roots);}} restore{SampleSettings::materialRoots()};SampleSettings::setMaterialRoots({output.getFullPathName()});
    auto converted=merged;juce::StringArray warnings;const auto beforeRequest=AudioEngine::diagnosticNativeRequest(converted.tracks[0].clips[0],converted.tracks[0]);
    const auto sidecarReady=SampleSettings::convertMelodyneProject(converted,warnings,false);const auto& current=converted.tracks[0].clips[0];const auto afterTiming=nativeNoteSourceTiming(current.notes[0],current,nativeClipClock(current));
    check("import_conversion_does_not_reset_merged_C_back_to_P",sidecarReady&&warnings.isEmpty()&&std::abs(afterTiming.alignmentSeconds-.28)<1.e-8&&std::abs(afterTiming.fixedDurationSeconds-.11)<1.e-8&&std::abs(current.notes[0].consonantSeconds-.15)<1.e-8);
    std::vector<SampleRegionSetting> rows;
    check("merged_import_OTTO_preserves_distinct_PC_markers",OttoMaterialIO::loadAudio(SampleSettings::annotationFor(source),source,rows,error)&&rows.size()==1&&std::abs(rows[0].alignmentSeconds-.28)<1.e-8&&std::abs(rows[0].fixedDurationSeconds-.11)<1.e-8);
    const auto afterRequest=AudioEngine::diagnosticNativeRequest(current,converted.tracks[0]);check("conversion_preserves_merged_source_and_target_pitch_requests",beforeRequest.sourceMidi==afterRequest.sourceMidi&&beforeRequest.targetMidi==afterRequest.targetMidi&&beforeRequest.noteGain==afterRequest.noteGain);
    auto tree=nativeConsonantMergeTree(merged.consonantMerges[0]);tree.setProperty("originalStateBase64","bad archive data",nullptr);NativeConsonantMergeRecord record;
    check("corrupt_original_fragment_archive_is_rejected",!nativeConsonantMergeFromTree(tree,record,error));
    check("merge_restore_and_sidecar_never_modify_original_PCM",juce::SHA256(source).toHexString()==hash);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

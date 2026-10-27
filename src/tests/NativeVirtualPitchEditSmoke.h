#pragma once
#include "../AudioEngine.h"
#include "../NativeVirtualGroupIO.h"
#include "../NativeVirtualRenderPlan.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualPitchEditSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id=i?"owner-b":"owner-a";clip.sourceFile=output.getChildFile(i?"b.wav":"a.wav");clip.startSeconds=i?1.:0.;clip.durationSeconds=clip.sourceDurationSeconds=i?1.:1.2;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        juce::AudioBuffer<float> pcm(1,57600);pcm.clear();auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,16,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;writer.reset();
        NoteData note;note.id=i?"b":"a";note.durationSeconds=clip.durationSeconds;note.midiNote=note.sourceMidiCenter=i?64:60;note.sourcePitchMeasured=true;
        for(int f=0;f<=int(std::lround(clip.durationSeconds/.005));++f)note.contour.push_back({f*.005,0,0,true});clip.notes={note};track.clips.push_back(clip);
    }
    data.tracks={track};ProjectModel model;model.resetDocument(data);const auto id=model.createNativeVirtualConnection({"a","b"},true,error);check("editable_virtual_route_created",id.isNotEmpty());if(id.isEmpty())return false;
    const auto resolve=[&](const ProjectData& snapshot)
    {ProjectData render;std::vector<NativeVirtualRenderGroup> groups;juce::String issue;if(!prepareNativeVirtualRender(snapshot,render,groups,issue)||groups.empty())return NativeVirtualPitchPlan{};return groups[0].pitch;};
    const auto automatic=resolve(model.snapshot());if(!automatic.active){error=automatic.issue;check("automatic_seed_available",false);return false;}
    auto points=automatic.points;const auto middle=points.size()/2;points[middle].targetMidi+=.7f;
    const auto before=model.contentFingerprint();
    check("legacy_curve_edit_entry_delegates_to_virtual_group_only",model.setConnectionPitchCurve(id,points,error)&&model.snapshot().nativeConnections.empty()&&!model.snapshot().nativeVirtualGroups[0].pitchCurve.empty());
    const auto manual=resolve(model.snapshot());bool exact=manual.active&&manual.points.size()==points.size();for(std::size_t i=0;exact&&i<points.size();++i)exact&=std::abs(points[i].targetMidi-manual.points[i].targetMidi)<1.e-5&&std::abs(points[i].timeSeconds-manual.points[i].timeSeconds)<1.e-8;
    check("manual_controls_reconstruct_exact_live_target",exact);
    check("repeat_manual_edit_is_noop",!model.setConnectionPitchCurve(id,points,error)&&error.isEmpty());
    model.undo();check("manual_edit_one_undo_restores_seed",model.contentFingerprint()==before);model.redo();
    const auto manualState=model.snapshot();const auto file=output.getChildFile("manual-virtual.hjpx");ProjectModel cold;
    check("manual_curve_cold_roundtrip",model.save(file,error)&&cold.load(file,error)&&!cold.snapshot().nativeVirtualGroups[0].pitchCurve.empty());
    const auto coldPitch=resolve(cold.snapshot());check("cold_curve_matches_controlled_target",coldPitch.active&&std::abs(coldPitch.points[middle].targetMidi-points[middle].targetMidi)<1.e-5);
    auto shifted=manualState;for(auto& c:shifted.tracks[0].clips)c.startSeconds+=2.;const auto moved=resolve(shifted);
    check("horizontal_group_move_preserves_manual_shape",moved.active&&std::abs(moved.points[middle].timeSeconds-manual.points[middle].timeSeconds-2.)<1.e-8&&std::abs(moved.points[middle].targetMidi-manual.points[middle].targetMidi)<1.e-5);
    auto transposed=manualState;transposed.tracks[0].clips[1].notes[0].midiNote+=3;const auto up=resolve(transposed);const auto u=manualState.nativeVirtualGroups[0].pitchCurve[middle].timeSeconds;
    check("single_endpoint_transpose_keeps_residual_shape_on_live_chord",up.active&&std::abs(up.points[middle].targetMidi-manual.points[middle].targetMidi-3*u)<1.e-5);
    auto edited=manual.points[middle];edited.targetMidi+=.2f;check("legacy_single_point_edit_uses_same_group_curve",model.setConnectionPitchPoint(id,int(middle),edited,error)&&model.snapshot().nativeConnections.empty());
    const auto safe=model.contentFingerprint();auto invalid=resolve(model.snapshot()).points;invalid[middle].timeSeconds=invalid.front().timeSeconds;
    check("invalid_manual_edit_is_atomic",!model.setConnectionPitchCurve(id,invalid,error)&&model.contentFingerprint()==safe);
    auto schema=nativeVirtualGroupTree(model.snapshot().nativeVirtualGroups[0]);NativeVirtualGroup decoded;
    check("manual_schema_validates",nativeVirtualGroupFromTree(schema,decoded,error));
    for(auto child:schema)if(child.hasType("VirtualPitchPoint")){child.setProperty("u","not a number",nullptr);break;}
    check("malformed_manual_number_refused",!nativeVirtualGroupFromTree(schema,decoded,error));
    check("explicit_reset_restores_automatic_without_touching_sources",model.resetNativeVirtualPitchCurve(id,error)&&model.snapshot().nativeVirtualGroups[0].pitchCurve.empty()&&resolve(model.snapshot()).active&&model.snapshot().tracks[0].clips[0].sourceFile==data.tracks[0].clips[0].sourceFile);
    model.undo();check("reset_is_undoable",!model.snapshot().nativeVirtualGroups[0].pitchCurve.empty());
    auto disabled=model.snapshot();disabled.nativeVirtualGroups[0].enabled=false;
    check("disabled_manual_curve_restores_original_request",AudioEngine::diagnosticNativeTargetMidi(disabled,0,0)==AudioEngine::diagnosticNativeTargetMidi(data,0,0));
    auto oldProject=data;auto& left=oldProject.tracks[0].clips[0];left.durationSeconds=left.sourceDurationSeconds=left.notes[0].durationSeconds=1.;left.sourceTimeMap={{0,0},{1,1}};
    NativeConnection old;old.id="old-cross-source";old.leftNoteId="a";old.rightNoteId="b";old.type="pitch-portamento";old.pitchCurve={{.95,60},{1.05,64}};oldProject.nativeConnections={old};ProjectModel legacy;legacy.resetDocument(oldProject);const auto originalLegacy=legacy.contentFingerprint();
    check("legacy_cross_source_edit_cannot_bypass_virtual_authority",!legacy.setConnectionPitchCurve(old.id,old.pitchCurve,error)&&error.contains("read-only")&&legacy.contentFingerprint()==originalLegacy&&!legacy.canUndo());
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

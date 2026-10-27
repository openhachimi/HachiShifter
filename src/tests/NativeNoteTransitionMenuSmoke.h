#pragma once
#include "../PianoRollComponent.h"
#include "../NativeVirtualRenderPlan.h"
#include "TeioProjectMaterialRegistrationAudioGate.h"
#include <iostream>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#ifdef small
#undef small
#endif
#endif
namespace hachi
{
inline bool runNativeNoteTransitionMenuSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;
    const std::array<juce::String,3> ids{"left","middle","right"};
    for(int i=0;i<3;++i)
    {
        ClipData clip;clip.id="owner-"+ids[std::size_t(i)];clip.sourceFile=folder.getChildFile(ids[std::size_t(i)]+".wav");clip.startSeconds=i==0?0:i==1?.8:1.;clip.durationSeconds=clip.sourceDurationSeconds=i==1?.6:1.2;clip.sourceOffsetSeconds=i==1?.4:0;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        {juce::AudioBuffer<float> pcm(1,144000);for(int s=0;s<144000;++s)pcm.setSample(0,s,float(.1*std::sin(s*juce::MathConstants<double>::twoPi*(220+i*40)/48000.)));auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,144000))return false;}
        NoteData note;note.id=ids[std::size_t(i)];note.durationSeconds=clip.durationSeconds;note.midiNote=note.sourceMidiCenter=i==2?64:60;note.sourcePitchMeasured=true;note.consonantSeconds=.1;note.utauPreutteranceOverrideEnabled=true;note.utauPreutteranceSeconds=.035;
        note.pitchControlPoints={{0,note.midiNote,PitchCurveShape::linear},{note.durationSeconds,note.midiNote,PitchCurveShape::linear}};
        for(int f=0;f<=int(std::round(note.durationSeconds/.005));++f)note.contour.push_back({f*.005,0,0,true});clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};ProjectModel model;model.resetDocument(seed);const auto original=model.contentFingerprint();
    const auto originalRequest=teioRegistrationAudioDetail::requestFingerprint(seed);const auto sourceHash=juce::SHA256(track.clips[1].sourceFile).toHexString();
    check("manual_middle_source_boundary_sets_only_transition_identity",model.setNativeNoteTransition("middle",.7,error)&&model.snapshot().tracks[0].clips[1].notes[0].pronunciationKind==PronunciationKind::transition&&std::abs(model.snapshot().tracks[0].clips[1].notes[0].nativeTransition->boundarySourceSeconds-.7)<1.e-8);
    check("identity_switch_does_not_rewrite_any_render_request_or_original_PC",teioRegistrationAudioDetail::requestFingerprint(model.snapshot())==originalRequest&&model.snapshot().tracks[0].clips[1].notes[0].consonantSeconds==.1&&model.snapshot().tracks[0].clips[1].notes[0].utauPreutteranceSeconds==.035);
    const auto typed=model.contentFingerprint();check("repeating_the_same_boundary_is_noop",!model.setNativeNoteTransition("middle",.7,error)&&error.isEmpty()&&model.contentFingerprint()==typed);
    check("source_boundary_at_or_outside_the_fragment_is_rejected_atomically",!model.setNativeNoteTransition("middle",.4,error)&&model.contentFingerprint()==typed&&!model.setNativeNoteTransition("middle",1.,error)&&model.contentFingerprint()==typed);
    model.undo();check("one_undo_restores_original_kind_PC_clock_and_controls",model.contentFingerprint()==original);model.redo();
    const auto file=folder.getChildFile("manual-transition.hjpx");ProjectModel cold;
    check("manual_kind_and_exact_source_middle_survive_cold_reload",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().tracks[0].clips[1].notes[0].pronunciationKind==PronunciationKind::transition&&std::abs(cold.snapshot().tracks[0].clips[1].notes[0].nativeTransition->boundarySourceSeconds-.7)<1.e-8);
    check("switch_back_to_normal_restores_original_content_exactly",model.setNativeNoteTransition("middle",std::nullopt,error)&&model.contentFingerprint()==original);
    auto endpoint=seed;endpoint.tracks[0].clips.resize(2);ProjectModel bound;bound.resetDocument(endpoint);const auto pair=bound.createNativeVirtualConnection({"left","middle"},false,error);const auto pairFP=bound.contentFingerprint();
    check("retyping_a_group_endpoint_never_leaves_a_dangling_bridge",pair.isNotEmpty()&&!bound.setNativeNoteTransition("middle",.7,error)&&bound.contentFingerprint()==pairFP);
    ProjectModel interior;interior.resetDocument(seed);const auto path=interior.createNativeVirtualConnection({"left","middle","right"},false,error);
    const auto beforeKind=interior.snapshot();const auto oldWave=AudioEngine::nativeClipWaveformHash(beforeKind.tracks[0].clips[0],beforeKind.tracks[0],&beforeKind.nativeVirtualGroups);
    check("normal_interior_member_can_become_one_typed_bridge_without_new_owner",path.isNotEmpty()&&interior.setNativeNoteTransition("middle",.7,error)&&interior.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"left","middle","right"}&&interior.snapshot().tracks[0].clips.size()==3);
    const auto afterKind=interior.snapshot();check("changing_a_peer_transition_middle_invalidates_group_waveform_identity",AudioEngine::nativeClipWaveformHash(afterKind.tracks[0].clips[0],afterKind.tracks[0],&afterKind.nativeVirtualGroups)!=oldWave);
    NativeVirtualConnectionPlan planned;check("new_explicit_middle_is_used_by_existing_virtual_geometry",resolveNativeVirtualGroup(interior.snapshot().tracks[0],interior.snapshot().nativeVirtualGroups[0],planned,error)&&planned.throughTransition&&std::abs(planned.members[1].middle-1.1)<1.e-8);
    auto protectedData=seed;NativeVirtualGroup group;group.id="protected";group.trackId=track.id;group.orderedNoteIds={"left","middle","right"};NativeVirtualHopPitchCurve manual;manual.leftNoteId="left";manual.rightNoteId="middle";manual.pitchCurve={{0,0,PitchCurveShape::linear},{1,0,PitchCurveShape::linear}};manual.pitchFromOffsetSeconds=-.2;manual.pitchToOffsetSeconds=.4;group.hopPitchCurves={manual};protectedData.nativeVirtualGroups={group};
    ProjectModel protectedModel;protectedModel.resetDocument(protectedData);const auto protectedFP=protectedModel.contentFingerprint();
    check("existing_local_curves_are_not_silently_discarded_when_kind_changes",!protectedModel.setNativeNoteTransition("middle",.7,error)&&protectedModel.contentFingerprint()==protectedFP);
    auto logical=seed;NativeLogicalTransition annotation;annotation.evidence.frontNoteId="middle";annotation.evidence.backNoteId="right";logical.logicalTransitions={annotation};ProjectModel halves;halves.resetDocument(logical);const auto halvesFP=halves.contentFingerprint();
    check("one_known_half_cannot_become_a_second_independent_logical_bridge",!halves.setNativeNoteTransition("middle",.7,error)&&halves.contentFingerprint()==halvesFP);
    auto uv=seed;uv.tracks[0].clips[1].notes[0].nativeUnpitched=true;for(auto& point:uv.tracks[0].clips[1].notes[0].contour)point.voiced=false;ProjectModel unvoiced;unvoiced.resetDocument(uv);
    const bool classified=unvoiced.setNativeNoteTransition("middle",.7,error);const auto uvRequest=AudioEngine::diagnosticNativeTargetMidi(unvoiced.snapshot(),0,1);
    check("manual_transition_identity_never_invents_voicing",classified&&!uvRequest.empty()&&std::all_of(uvRequest.begin(),uvRequest.end(),[](float midi){return midi==0;}));
    I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack(track.id);roll.setBounds(0,0,1100,1000);roll.diagnosticRefresh();
    const auto enabled=roll.diagnosticEnabledNoteMenuIds("middle");check("actual_note_context_menu_offers_switch_to_transition",std::find(enabled.begin(),enabled.end(),PianoRollComponent::setTransitionNoteItem)!=enabled.end());
#if JUCE_WINDOWS
    const auto pump=[&]{for(int pass=0;pass<8;++pass){MSG message{};for(int i=0;i<128&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++i){if(message.message==WM_QUIT){PostQuitMessage(int(message.wParam));return;}TranslateMessage(&message);DispatchMessageW(&message);}model.dispatchPendingMessages();roll.diagnosticRefresh();juce::Thread::sleep(2);}};
    roll.applyNoteMenuChoice("middle",PianoRollComponent::setTransitionNoteItem,0,{},1.15);pump();auto* dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check("real_menu_opens_explicit_source_middle_dialog_not_a_PC_guess",dialog&&std::abs(dialog->getTextEditorContents("source_middle_ms").getDoubleValue()-350.)<.001&&model.contentFingerprint()==original);
    if(dialog)dialog->exitModalState(0);pump();check("cancelling_kind_switch_does_not_write_anything",model.contentFingerprint()==original);
    roll.applyNoteMenuChoice("middle",PianoRollComponent::setTransitionNoteItem,0,{},1.15);pump();dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    if(dialog){dialog->getTextEditor("source_middle_ms")->setText("200");dialog->exitModalState(1);}pump();
    check("real_dialog_confirmation_uses_the_user_chosen_source_middle",dialog&&model.snapshot().tracks[0].clips[1].notes[0].nativeTransition&&std::abs(model.snapshot().tracks[0].clips[1].notes[0].nativeTransition->boundarySourceSeconds-.6)<1.e-8);
    const auto typedMenu=roll.diagnosticEnabledNoteMenuIds("middle");check("typed_note_menu_offers_middle_edit_and_normal_restore",std::find(typedMenu.begin(),typedMenu.end(),PianoRollComponent::editTransitionMiddleItem)!=typedMenu.end()&&std::find(typedMenu.begin(),typedMenu.end(),PianoRollComponent::setNormalNoteItem)!=typedMenu.end());
    roll.applyNoteMenuChoice("middle",PianoRollComponent::setNormalNoteItem,0,{});pump();check("real_note_menu_normal_switch_restores_original_source_facts",model.contentFingerprint()==original);
    roll.applyNoteMenuChoice("middle",PianoRollComponent::setTransitionNoteItem,0,{},1.15);pump();dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    model.transposeNotes({"left"},1);const auto changedWhileOpen=model.contentFingerprint();
    if(dialog){dialog->getTextEditor("source_middle_ms")->setText("200");dialog->exitModalState(1);}pump();
    check("stale_source_boundary_dialog_cannot_commit_after_another_edit",model.contentFingerprint()==changedWhileOpen&&model.snapshot().tracks[0].clips[1].notes[0].pronunciationKind==PronunciationKind::normal);
    for(int i=0;i<4;++i){auto* warning=juce::Component::getCurrentlyModalComponent();if(!warning)break;warning->exitModalState(0);pump();}
#endif
    check("manual_identity_never_writes_source_audio_or_material_manifest",juce::SHA256(track.clips[1].sourceFile).toHexString()==sourceHash&&!folder.getChildFile("otto.ini").existsAsFile());
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

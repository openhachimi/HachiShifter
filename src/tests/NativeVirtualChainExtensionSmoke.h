#pragma once
#include "../NativeVirtualChainExtension.h"
#include "../NativeVirtualRenderPlan.h"
#include "../NativeVirtualAutoConnect.h"
#include "../AudioEngine.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualChainExtensionSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;
    const std::vector<juce::String> ids{"A","B","C","D","E"};
    for(int i=0;i<5;++i)
    {
        ClipData clip;clip.id="owner-"+ids[std::size_t(i)];clip.sourceFile=folder.getChildFile(ids[std::size_t(i)]+".wav");clip.startSeconds=i*.5;clip.durationSeconds=clip.sourceDurationSeconds=.6;clip.sourceTimeMap={{0,0},{.6,.6}};
        {juce::AudioBuffer<float> pcm(1,48000);for(int s=0;s<48000;++s)pcm.setSample(0,s,float(.1*std::sin(s*juce::MathConstants<double>::twoPi*(170+i*31)/48000.)));auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,48000))return false;}
        NoteData note;note.id=ids[std::size_t(i)];note.durationSeconds=.6;note.midiNote=note.sourceMidiCenter=60+float(i*3);for(int f=0;f<=120;++f)note.contour.push_back({f*.005,0,0,true});clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};ProjectModel model;model.resetDocument(seed);
    check("initial_component_created",model.setNativeVirtualConnectionForNotes({"A","B"},true,false,error));
    if(model.snapshot().nativeVirtualGroups.empty())return false;
    const auto first=model.snapshot().nativeVirtualGroups[0].id;const auto before=model.contentFingerprint();
    check("existing_tail_extends_through_the_same_public_transaction",model.setNativeVirtualConnectionForNotes({"B","C"},true,false,error)&&model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeVirtualGroups[0].id==first&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"A","B","C"});
    const auto extended=model.snapshot();model.undo();check("extension_is_one_exact_undo",model.contentFingerprint()==before);model.redo();
    check("redo_restores_members_without_replacing_physical_owners",model.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"A","B","C"}&&model.snapshot().tracks[0].clips.size()==5&&model.snapshot().nativeConnections.empty());
    const auto same=model.contentFingerprint();check("repeating_an_existing_edge_is_noop_without_extra_undo",!model.setNativeVirtualConnectionForNotes({"B","C"},true,false,error)&&model.contentFingerprint()==same);
    check("an_interior_branch_does_not_rewrite_the_existing_path",!model.setNativeVirtualConnectionForNotes({"B","D"},true,false,error)&&model.contentFingerprint()==same);
    NativeVirtualChainExtension cycle;check("explicit_cycle_is_rejected_without_guessing_a_new_order",!planNativeVirtualChainExtension(extended,{"C","A"},cycle,error));
    // Existing direction survives a geometric reversal of its first member.
    model.resetDocument(seed);model.setNativeVirtualConnectionForNotes({"A","B"},true,false,error);model.moveClip("owner-A",3.);
    check("extending_a_moved_component_never_resorts_its_old_members",model.setNativeVirtualConnectionForNotes({"B","C"},true,false,error)&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"A","B","C"});
    // Join TWO existing components without duplicate ownership of their ends.
    ProjectModel joined;joined.resetDocument(seed);joined.setNativeVirtualConnectionForNotes({"A","B"},true,false,error);joined.setNativeVirtualConnectionForNotes({"C","D"},true,false,error);
    const auto two=joined.contentFingerprint();const auto surviving=joined.snapshot().nativeVirtualGroups[0].id;
    check("two_chain_endpoints_merge_into_one_path_and_one_identity",joined.setNativeVirtualConnectionForNotes({"B","C"},true,false,error)&&joined.snapshot().nativeVirtualGroups.size()==1&&joined.snapshot().nativeVirtualGroups[0].id==surviving&&joined.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"A","B","C","D"});
    joined.undo();check("component_merge_undo_restores_both_original_groups_exactly",joined.contentFingerprint()==two&&joined.snapshot().nativeVirtualGroups.size()==2);joined.redo();
    const auto file=folder.getChildFile("extended.hjpx");ProjectModel cold;
    check("extended_direction_and_identity_survive_cold_project_reload",joined.save(file,error)&&cold.load(file,error)&&cold.snapshot().nativeVirtualGroups.size()==1&&cold.snapshot().nativeVirtualGroups[0].id==surviving&&cold.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"A","B","C","D"});
    auto harmony=seed;ProjectModel protectedModel;protectedModel.resetDocument(harmony);protectedModel.setNativeVirtualConnectionForNotes({"A","B"},true,false,error);harmony=protectedModel.snapshot();
    auto other=track.clips.back();other.id="harmony-owner";other.startSeconds=1.02;other.durationSeconds=other.sourceDurationSeconds=.06;other.sourceTimeMap={{0,0},{.06,.06}};other.notes[0].id="harmony";other.notes[0].durationSeconds=.06;harmony.tracks[0].clips.push_back(other);protectedModel.resetDocument(harmony);const auto safe=protectedModel.contentFingerprint();
    check("unrelated_voice_is_not_absorbed_when_extending_a_known_route",!protectedModel.setNativeVirtualConnectionForNotes({"B","C"},true,false,error)&&protectedModel.contentFingerprint()==safe);
    // A stored manual group curve is never silently discarded or stretched to
    // unrelated new endpoints. This unsupported merge must be a clear no-op.
    ProjectModel manual;manual.resetDocument(seed);manual.setNativeVirtualConnectionForNotes({"A","B"},true,true,error);ProjectData manualProjection;std::vector<NativeVirtualRenderGroup> manualGroups;
    bool authored=prepareNativeVirtualRender(manual.snapshot(),manualProjection,manualGroups,error)&&!manualGroups.empty()&&manualGroups[0].pitch.active;
    if(authored){auto points=manualGroups[0].pitch.points;points[points.size()/2].targetMidi+=.5f;authored=manual.setNativeVirtualPitchCurve(manual.snapshot().nativeVirtualGroups[0].id,points,error);}
    const auto manualBefore=manual.contentFingerprint();check("manual_curve_is_preserved_with_explicit_merge_rejection",authored&&!manual.setNativeVirtualConnectionForNotes({"B","C"},true,false,error)&&manual.contentFingerprint()==manualBefore&&!error.isEmpty());
    // Auto pitch must remain LOCAL to consecutive hops. A single A->C chord
    // would incorrectly reshape the middle of B and erase its own pitch trend.
    ProjectModel automatic;automatic.resetDocument(seed);automatic.setNativeVirtualConnectionForNotes({"A","B"},true,true,error);
    const auto linked=automatic.setNativeVirtualConnectionForNotes({"B","C"},true,true,error);ProjectData autoProjection;std::vector<NativeVirtualRenderGroup> autoGroups;
    const auto planned=prepareNativeVirtualRender(automatic.snapshot(),autoProjection,autoGroups,error);const auto curves=planned?nativeVirtualResolvedPitchCurves(autoGroups,track.id):std::vector<ResolvedConnectionPitchCurve>{};
    check("extended_automatic_glides_have_two_local_writers_not_one_global_chord",linked&&planned&&autoGroups.size()==1&&autoGroups[0].pitch.segments.size()==2&&curves.size()==2&&curves[0].affectedNoteIds==std::vector<juce::String>{"A","B"}&&curves[1].affectedNoteIds==std::vector<juce::String>{"B","C"});
    bool farUntouched=curves.size()==2;
    for(const auto& curve:curves)farUntouched&=!(curve.fromSeconds<.8&&curve.toSeconds>.8);
    check("middle_note_trend_outside_each_handover_is_not_overwritten",farUntouched);
    const auto middleTarget=AudioEngine::diagnosticNativeTargetMidi(automatic.snapshot(),0,1);
    check("production_request_consumes_both_local_hops_but_preserves_middle_pitch",middleTarget.size()>110&&middleTarget[10]<62.9f&&std::abs(middleTarget[60]-63.f)<1.e-4&&middleTarget[110]>63.1f);
    auto automaticImport=seed;const auto report=autoConnectNativeVirtualImport(automaticImport,{true,true});
    check("automatic_discovery_extends_shared_endpoints_instead_of_disjoint_pairs",report.created==4&&automaticImport.nativeVirtualGroups.size()==1&&automaticImport.nativeVirtualGroups[0].orderedNoteIds==ids&&automaticImport.nativeVirtualGroups[0].automaticallyCreated);
    const auto unchangedImport=automaticImport;const auto repeated=autoConnectNativeVirtualImport(automaticImport,{true,true});
    check("repeated_import_optimization_does_not_overwrite_existing_components",repeated.created==0&&automaticImport.nativeVirtualGroups.size()==1&&automaticImport.nativeVirtualGroups[0].id==unchangedImport.nativeVirtualGroups[0].id);
    auto bridged=seed;
    for(int i:{1,3}){auto& note=bridged.tracks[0].clips[std::size_t(i)].notes[0];note.pronunciationKind=PronunciationKind::transition;note.nativeTransition=NativeTransitionShape{1,.3,"typed-full-bridge-fixture"};note.nativeUnpitched=true;for(auto& p:note.contour)p.voiced=false;}
    const auto bridgeReport=autoConnectNativeVirtualImport(bridged,{true,true});ProjectData bridgeProjection;std::vector<NativeVirtualRenderGroup> bridgeGroups;
    const auto bridgeReady=prepareNativeVirtualRender(bridged,bridgeProjection,bridgeGroups,error);
    check("two_full_typed_bridges_share_one_normal_endpoint_without_a_second_group",bridgeReport.created==2&&bridged.nativeVirtualGroups.size()==1&&bridged.nativeVirtualGroups[0].orderedNoteIds==ids&&bridgeReady&&bridgeGroups.size()==1&&bridgeGroups[0].pitch.segments.size()==2);
    const auto bridgeTarget=AudioEngine::diagnosticNativeTargetMidi(bridged,0,1);
    check("extended_typed_bridge_chain_keeps_unvoiced_source_requests_zero",!bridgeTarget.empty()&&std::all_of(bridgeTarget.begin(),bridgeTarget.end(),[](float midi){return midi==0;}));
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

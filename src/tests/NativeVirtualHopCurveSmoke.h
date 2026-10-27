#pragma once
#include "../NativeVirtualRenderPlan.h"
#include "../NativeVirtualGroupIO.h"
#include "../PianoRollComponent.h"
#include "../AudioEngine.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualHopCurveSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;
    for(int i=0;i<4;++i)
    {
        ClipData clip;clip.id="owner"+juce::String(i);clip.sourceFile=folder.getChildFile("source"+juce::String(i)+".wav");clip.startSeconds=i;clip.durationSeconds=clip.sourceDurationSeconds=1.2;clip.sourceTimeMap={{0,0},{1.2,1.2}};
        {juce::AudioBuffer<float> pcm(1,96000);for(int s=0;s<96000;++s)pcm.setSample(0,s,float(.1*std::sin(s*juce::MathConstants<double>::twoPi*(220+i*40)/48000.)));auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,96000))return false;}
        NoteData note;note.id="note"+juce::String(i);note.durationSeconds=1.2;note.midiNote=note.sourceMidiCenter=float(60+i*4);note.sourcePitchMeasured=true;for(int f=0;f<=240;++f)note.contour.push_back({f*.005,0,0,true});clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};ProjectModel model;model.resetDocument(seed);check("long_virtual_route_created",model.setNativeVirtualConnectionForNotes({"note0","note1","note2"},true,true,error,.2));
    const auto curves=[&](const ProjectData& data){return nativeVirtualPitchCurvesForTrack(data.tracks[0],data.nativeVirtualGroups);};
    const auto before=curves(model.snapshot());if(before.size()!=2)return false;const auto first=before[0].connectionId,second=before[1].connectionId;
    check("hop_identity_uses_exact_anchor_ids_not_numeric_path_index",first==nativeVirtualHopPitchId(model.snapshot().nativeVirtualGroups[0].id,"note0","note1")&&second==nativeVirtualHopPitchId(model.snapshot().nativeVirtualGroups[0].id,"note1","note2"));
    const auto compact=[](const ResolvedConnectionPitchCurve& curve,float rise)
    {std::vector<PitchCurveEditPoint> p{curve.points.front(),{(curve.fromSeconds+curve.toSeconds)*.5f,(curve.points.front().targetMidi+curve.points.back().targetMidi)*.5f+rise,PitchCurveShape::customBezier},curve.points.back()};p[1].bezierX1=.2f;p[1].bezierY1=.1f;p[1].bezierX2=.8f;p[1].bezierY2=.9f;p[1].highPrecisionBezier=true;return p;};
    const auto original=model.contentFingerprint();const auto cBefore=AudioEngine::diagnosticNativeTargetMidi(model.snapshot(),0,2);
    check("first_hop_manual_edit_does_not_create_a_whole_chain_curve",model.setNativeVirtualPitchCurve(first,compact(before[0],.7f),error)&&model.snapshot().nativeVirtualGroups[0].pitchCurve.empty()&&model.snapshot().nativeVirtualGroups[0].hopPitchCurves.size()==1);
    const auto afterFirst=curves(model.snapshot());bool otherSame=afterFirst.size()==2;
    if(otherSame)for(int i=0;i<=1000;++i){const auto t=before[1].fromSeconds+(before[1].toSeconds-before[1].fromSeconds)*i/1000.;otherSame&=std::abs(evaluatePitchCurve(afterFirst[1].points,t)-evaluatePitchCurve(before[1].points,t))<1.e-6;}
    check("unselected_hop_and_third_source_production_target_are_unchanged",otherSame&&AudioEngine::diagnosticNativeTargetMidi(model.snapshot(),0,2)==cBefore);
    model.undo();check("local_hop_edit_is_one_exact_undo",model.contentFingerprint()==original);model.redo();
    I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack(track.id);roll.setTool(PianoRollComponent::Tool::points);roll.setPixelsPerSecond(300);roll.setRowHeight(22);roll.setBounds(0,0,1300,1100);
    const auto refresh=[&]{model.dispatchPendingMessages();roll.diagnosticRefresh();juce::Image image(juce::Image::ARGB,1300,1100,true,juce::SoftwareImageType{});juce::Graphics g(image);roll.paintEntireComponent(g,true);};refresh();
    const auto mouse=[&](juce::Point<float> at,juce::Point<float> down){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
    check("existing_point_tool_exposes_separate_long_chain_hops",roll.diagnosticConnectionPitchCurves().size()==2&&roll.diagnosticConnectionPitchPoint(first,1).has_value());
    const auto point=roll.diagnosticConnectionPitchPoint(first,1);const auto fp=model.contentFingerprint();if(point){const auto moved=point->translated(0,-11);roll.mouseDown(mouse(*point,*point));roll.mouseDrag(mouse(moved,*point));check("local_hop_mouse_preview_does_not_commit_early",model.contentFingerprint()==fp);roll.mouseUp(mouse(moved,*point));refresh();}
    check("local_hop_mouse_commit_does_not_reanchor_to_the_chain_last_note",model.contentFingerprint()!=fp&&model.snapshot().nativeVirtualGroups[0].hopPitchCurves.size()==1&&std::abs(curves(model.snapshot())[0].points.back().targetMidi-64)<.001&&AudioEngine::diagnosticNativeTargetMidi(model.snapshot(),0,2)==cBefore);
    check("second_hop_can_store_its_own_independent_controls",model.setNativeVirtualPitchCurve(second,compact(curves(model.snapshot())[1],-.5f),error)&&model.snapshot().nativeVirtualGroups[0].hopPitchCurves.size()==2);
    const auto beforeOverlap=model.contentFingerprint();auto overlap=compact(curves(model.snapshot())[1],-.5f);overlap.front().timeSeconds=1.15;
    check("overlapping_incident_manual_windows_are_rejected_without_partial_write",!model.setNativeVirtualPitchCurve(second,overlap,error)&&model.contentFingerprint()==beforeOverlap&&!error.isEmpty());
    // A later duration edit may make previously valid adjacent windows overlap.
    // Keep their exact source/path identities and controls, but never let two
    // independent writers compete on the same middle-note time interval.
    const auto beforeResize=model.contentFingerprint();model.resizeNote("note1",0.,.4);
    ProjectData conflictProjection;std::vector<NativeVirtualRenderGroup> conflicts;juce::StringArray notices;
    const bool conflicted=prepareNativeVirtualRender(model.snapshot(),conflictProjection,conflicts,error,&notices);
    check("later_note_resize_retains_groups_but_isolates_conflicting_pitch_windows",conflicted&&conflicts.size()==1&&conflicts[0].pitch.conflictingWindows
        &&!conflicts[0].pitch.active&&nativeVirtualResolvedPitchCurves(conflicts,track.id).empty()&&notices.size()==1
        &&model.snapshot().nativeVirtualGroups[0].hopPitchCurves.size()==2&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"note0","note1","note2"});
    auto longer=model.snapshot();longer.nativeVirtualGroups[0].orderedNoteIds.push_back("note3");
    ProjectData longerProjection;std::vector<NativeVirtualRenderGroup> longerGroups;
    const bool partial=prepareNativeVirtualRender(longer,longerProjection,longerGroups,error);
    const auto remaining=partial?nativeVirtualResolvedPitchCurves(longerGroups,track.id):std::vector<ResolvedConnectionPitchCurve>{};
    check("conflicting_local_windows_do_not_disable_a_remote_valid_hop",partial&&remaining.size()==1&&remaining[0].leftNoteId=="note2"&&remaining[0].rightNoteId=="note3");
    AudioEngine conflictEngine(false);conflictEngine.prepareToPlay(257,48000);conflictEngine.syncProject(model.snapshot());
    const auto warning=conflictEngine.activeRenderWarnings();
    const auto until=juce::Time::getMillisecondCounterHiRes()+10000.;while(juce::Time::getMillisecondCounterHiRes()<until){const auto p=conflictEngine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(2);}
    juce::String exportError;const bool refused=!conflictEngine.exportWav(folder.getChildFile("conflicting-curve-must-not-export.wav"),exportError);
    check("engine_reports_conflict_without_silently_exporting_a_competing_curve",warning.contains("windows overlap")&&warning.contains(model.snapshot().nativeVirtualGroups[0].id)&&refused&&exportError.contains("windows overlap"));
    juce::AudioBuffer<float> audible(2,48000);audible.clear();conflictEngine.setPosition(0);conflictEngine.play();for(int sample=0;sample<48000;sample+=257)conflictEngine.getNextAudioBlock({&audible,sample,std::min(257,48000-sample)});conflictEngine.stop();
    check("pitch_window_conflict_does_not_mute_the_source_route",audible.getMagnitude(0,48000)>.01f);
    model.undo();check("resize_undo_restores_both_local_curve_writers_exactly",model.contentFingerprint()==beforeResize&&curves(model.snapshot()).size()==2);
    const auto saved=folder.getChildFile("manual-hops.hjpx");ProjectModel cold;
    check("both_hop_curves_and_anchor_identities_survive_cold_reload",model.save(saved,error)&&cold.load(saved,error)&&cold.snapshot().nativeVirtualGroups[0].hopPitchCurves.size()==2&&curves(cold.snapshot())[0].connectionId==first&&curves(cold.snapshot())[1].connectionId==second);
    auto schema=nativeVirtualGroupTree(model.snapshot().nativeVirtualGroups[0]);for(const auto child:schema)if(child.hasType("VirtualHopPitchCurve")){schema.addChild(child.createCopy(),-1,nullptr);break;}NativeVirtualGroup decoded;
    check("duplicate_hop_payload_is_strictly_rejected",!nativeVirtualGroupFromTree(schema,decoded,error));
    // Prepend the previously unrelated fourth source. Existing IDs must not
    // change just because each old hop's numeric position moved by one.
    auto prefixed=model.snapshot();prefixed.tracks[0].clips[3].startSeconds=-1.;model.resetDocument(prefixed);
    check("route_extension_preserves_existing_hop_controls",model.setNativeVirtualConnectionForNotes({"note3","note0"},true,true,error,.2)&&model.snapshot().nativeVirtualGroups[0].hopPitchCurves.size()==2);
    const auto extended=curves(model.snapshot());check("prepending_a_member_does_not_rename_an_existing_hop",extended.size()==3&&extended[1].connectionId==first&&extended[2].connectionId==second);
    refresh();const auto beforeReset=model.contentFingerprint();check("reset_one_hop_keeps_the_other_hop_manual",roll.diagnosticRestoreConnectionPitchCurve(first,0)&&model.snapshot().nativeVirtualGroups[0].hopPitchCurves.size()==1&&model.snapshot().nativeVirtualGroups[0].hopPitchCurves[0].leftNoteId=="note1");model.undo();check("local_reset_is_one_exact_undo",model.contentFingerprint()==beforeReset);
    // A monophonic shared line is a base, not a competing writer. A local
    // manual override must be applied AFTER that base, preserving remote F0.
    auto touching=seed;touching.tracks[0].clips.resize(3);for(int i=0;i<3;++i){touching.tracks[0].clips[std::size_t(i)].durationSeconds=touching.tracks[0].clips[std::size_t(i)].sourceDurationSeconds=1.;touching.tracks[0].clips[std::size_t(i)].sourceTimeMap={{0,0},{1,1}};touching.tracks[0].clips[std::size_t(i)].notes[0].durationSeconds=1.;std::erase_if(touching.tracks[0].clips[std::size_t(i)].notes[0].contour,[](const auto& p){return p.timeSeconds>1.;});}
    ProjectModel shared;shared.resetDocument(touching);shared.linkNativeAudio({"note0","note1","note2"});const auto sharedState=shared.snapshot();NativeVirtualConnectionPlan geometry;resolveNativeVirtualGroup(sharedState.tracks[0],sharedState.nativeVirtualGroups[0],geometry,error);
    const auto hops=nativeVirtualPitchHops(sharedState.nativeVirtualGroups[0],geometry);auto local=hops[0].group;local.automaticPitch=true;const auto seedHop=planNativeVirtualPitch(sharedState.tracks[0],local,hops[0].geometry);auto localCurve=resolvedNativeVirtualPitch(seedHop);
    check("shared_continuous_mode_accepts_a_local_manual_override",seedHop.active&&shared.setNativeVirtualPitchCurve(hops[0].group.id,compact(localCurve,.5f),error));
    ProjectData projected;std::vector<NativeVirtualRenderGroup> prepared;const bool ready=prepareNativeVirtualRender(shared.snapshot(),projected,prepared,error);
    check("shared_base_is_rendered_first_and_local_override_second",ready&&prepared.size()==1&&prepared[0].pitch.includeBaseCurve&&nativeVirtualResolvedPitchCurves(prepared,track.id).size()==2);
    const auto target=AudioEngine::diagnosticNativeTargetMidi(shared.snapshot(),0,0);check("local_shared_override_keeps_far_endpoint_pitch_unchanged",target.size()>100&&std::abs(target[100]-60)<.001);
    auto sharedConflict=shared.snapshot();auto conflictPayload=sharedConflict.nativeVirtualGroups[0].hopPitchCurves[0];
    conflictPayload.leftNoteId="note1";conflictPayload.rightNoteId="note2";conflictPayload.pitchFromOffsetSeconds=-.975;sharedConflict.nativeVirtualGroups[0].hopPitchCurves.push_back(conflictPayload);
    ProjectData sharedProjection;std::vector<NativeVirtualRenderGroup> sharedGroups;const bool sharedReady=prepareNativeVirtualRender(sharedConflict,sharedProjection,sharedGroups,error);
    const auto sharedCurves=sharedReady?nativeVirtualResolvedPitchCurves(sharedGroups,track.id):std::vector<ResolvedConnectionPitchCurve>{};
    check("conflicting_local_overrides_do_not_suppress_the_valid_shared_base",sharedReady&&sharedGroups[0].pitch.conflictingWindows&&sharedGroups[0].pitch.includeBaseCurve&&sharedCurves.size()==1&&sharedCurves[0].connectionId==sharedConflict.nativeVirtualGroups[0].id);
    // Exact support spans from the full teio failure near 219.35 seconds:
    // independently chosen automatic margins overlapped by 0.9745 ms.
    auto shortMiddle=seed;shortMiddle.tracks[0].clips.resize(3);
    const std::array<double,3> starts{219.111922021,219.269318967,219.376686507},ends{219.328766449,219.391555396,219.509765323};
    for(int i=0;i<3;++i)
    {
        auto& clip=shortMiddle.tracks[0].clips[std::size_t(i)];clip.startSeconds=starts[std::size_t(i)];clip.durationSeconds=clip.sourceDurationSeconds=ends[std::size_t(i)]-starts[std::size_t(i)];clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        auto& note=clip.notes[0];note.durationSeconds=clip.durationSeconds;note.contour.clear();for(int frame=0;frame<=int(std::ceil(note.durationSeconds/.005));++frame)note.contour.push_back({std::min(note.durationSeconds,frame*.005),0,0,true});
    }
    ProjectModel shortModel;shortModel.resetDocument(shortMiddle);const bool linked=shortModel.setNativeVirtualConnectionForNotes({"note0","note1","note2"},true,true,error,.1);
    const auto adjusted=curves(shortModel.snapshot());
    check("full_teio_short_middle_auto_margins_reanchor_without_overlapping_writers",linked&&adjusted.size()==2&&adjusted[0].toSeconds<=adjusted[1].fromSeconds+1.e-7&&std::abs(adjusted[0].points.back().targetMidi-adjusted[1].points.front().targetMidi)<.001);
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

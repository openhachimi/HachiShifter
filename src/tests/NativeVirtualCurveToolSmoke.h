#pragma once
#include "../PianoRollComponent.h"
#include "../NativeVirtualRenderPlan.h"
#include "../AudioEngine.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualCurveToolSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id=i?"right-owner":"left-owner";clip.sourceFile=folder.getChildFile(i?"right.wav":"left.wav");clip.startSeconds=i?1.:0;clip.durationSeconds=clip.sourceDurationSeconds=1.2;clip.sourceTimeMap={{0,0},{1.2,1.2}};
        {juce::AudioBuffer<float> pcm(1,96000);for(int s=0;s<96000;++s)pcm.setSample(0,s,float(.1*std::sin(s*juce::MathConstants<double>::twoPi*(i?330:260)/48000.)));auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,96000))return false;}
        NoteData note;note.id=i?"right":"left";note.durationSeconds=1.2;note.midiNote=note.sourceMidiCenter=i?64:60;note.sourcePitchMeasured=true;for(int f=0;f<=240;++f)note.contour.push_back({f*.005,0,0,true});clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};ProjectModel model;model.resetDocument(seed);const auto id=model.createNativeVirtualConnection({"left","right"},true,error,.2);
    NativeVirtualConnectionPlan geometry;resolveNativeVirtualGroup(track,model.snapshot().nativeVirtualGroups[0],geometry,error);const auto automatic=planNativeVirtualPitch(track,model.snapshot().nativeVirtualGroups[0],geometry);
    if(!automatic.active)return false;
    const auto from=automatic.points.front().timeSeconds,to=automatic.points.back().timeSeconds;
    std::vector<PitchCurveEditPoint> controls{{from,60,PitchCurveShape::linear},{(from+to)*.5,62,PitchCurveShape::linear},{to,64,PitchCurveShape::linear}};
    check("compact_editable_virtual_curve_created",model.setNativeVirtualPitchCurve(id,controls,error));
    I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack(track.id);roll.setTool(PianoRollComponent::Tool::points);roll.setPixelsPerSecond(300);roll.setRowHeight(22);roll.setBounds(0,0,1100,1000);
    const auto refresh=[&]{model.dispatchPendingMessages();roll.diagnosticRefresh();juce::Image image(juce::Image::ARGB,1100,1000,true,juce::SoftwareImageType{});juce::Graphics g(image);roll.paintEntireComponent(g,true);};refresh();
    const auto curve=[&](bool preview=true){juce::ignoreUnused(preview);for(const auto& c:roll.diagnosticConnectionPitchCurves())if(c.connectionId==id)return c;return ResolvedConnectionPitchCurve{};};
    check("real_point_editor_exposes_virtual_controls_without_legacy_edges",curve().active&&curve().points.size()==3&&model.snapshot().nativeConnections.empty());
    const auto mouse=[&](juce::Point<float> at,juce::Point<float> down){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
    const auto vertex=roll.diagnosticConnectionPitchPoint(id,1);const auto original=model.contentFingerprint();
    if(!vertex)return false;const auto dragged=vertex->translated(0,-22);
    roll.mouseDown(mouse(*vertex,*vertex));roll.mouseDrag(mouse(dragged,*vertex));
    const auto preview=curve();
    check("virtual_vertex_drag_is_real_preview_not_early_commit",model.contentFingerprint()==original&&preview.active&&std::abs(preview.points[1].targetMidi-63)<.02);
    roll.mouseUp(mouse(dragged,*vertex));refresh();const auto afterVertex=model.contentFingerprint();
    check("vertex_mouse_up_commits_only_the_virtual_group",afterVertex!=original&&model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeConnections.empty()&&model.snapshot().tracks[0].clips[0].notes[0].pitchControlPoints.empty()&&std::abs(curve().points[1].targetMidi-63)<.02);
    model.undo();check("vertex_drag_is_one_undoable_transaction",model.contentFingerprint()==original);model.redo();refresh();
    check("existing_curve_shape_action_accepts_virtual_Bezier",roll.diagnosticConnectionPitchShape(id,1,PitchCurveShape::customBezier));refresh();
    const auto grip=roll.diagnosticConnectionPitchBezierHandle(id,1,1);check("selected_virtual_control_offers_real_Bezier_grips",grip.has_value());
    const auto beforeGrip=model.contentFingerprint();const auto oldCurve=curve();
    if(grip)
    {
        const auto moved=grip->translated(10,-10);roll.mouseDown(mouse(*grip,*grip));roll.mouseDrag(mouse(moved,*grip));
        const auto pending=curve();check("Bezier_grip_changes_preview_without_touching_the_document",pending.active&&std::abs(pending.points[1].bezierX1-oldCurve.points[1].bezierX1)>.01&&model.contentFingerprint()==beforeGrip);
        roll.mouseUp(mouse(moved,*grip));refresh();
        check("Bezier_grip_mouse_up_preserves_shape_in_the_same_group",model.contentFingerprint()!=beforeGrip&&model.snapshot().nativeVirtualGroups[0].pitchCurve[1].shape==PitchCurveShape::customBezier&&model.snapshot().nativeVirtualGroups[0].pitchCurveEncoding==2&&std::abs(curve().points[1].bezierX1-oldCurve.points[1].bezierX1)>.01);
    }
    const auto endpoint=roll.diagnosticConnectionPitchPoint(id,0);const auto oldFrom=curve().fromSeconds;
    if(endpoint){const auto moved=endpoint->translated(9,0);roll.mouseDown(mouse(*endpoint,*endpoint));roll.mouseDrag(mouse(moved,*endpoint));roll.mouseUp(mouse(moved,*endpoint));refresh();}
    check("endpoint_time_can_be_dragged_on_actual_voiced_support",endpoint&&std::abs(curve().fromSeconds-oldFrom-.03)<1.e-5);
    const auto beforeCancel=model.contentFingerprint();const auto cancelPoint=roll.diagnosticConnectionPitchPoint(id,1);
    if(cancelPoint){const auto moved=cancelPoint->translated(0,-12);roll.mouseDown(mouse(*cancelPoint,*cancelPoint));roll.mouseDrag(mouse(moved,*cancelPoint));roll.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));roll.mouseUp(mouse(moved,*cancelPoint));refresh();}
    check("escape_discards_the_virtual_curve_preview",model.contentFingerprint()==beforeCancel);
    ProjectModel::PointCurveEdit edit;edit.kind=ProjectModel::PointCurveEdit::connectionPitch;edit.connectionId=id;edit.pitchPoints=curve().points;
    ProjectModel::PointCurvePlanPreview planned;const auto safe=model.contentFingerprint();
    check("stale_curve_gesture_is_rejected_without_mutation",!model.previewPointCurves({edit},model.revisionNumber()-1,planned,error)&&model.contentFingerprint()==safe);
    auto invalid=edit;invalid.connectionId="unknown-late-target";
    check("invalid_late_command_rejects_the_entire_virtual_curve_batch",!model.setPointCurves({edit,invalid},model.revisionNumber(),&error)&&model.contentFingerprint()==safe);
    const auto target=AudioEngine::diagnosticNativeTargetMidi(model.snapshot(),0,0);const auto finalCurve=curve();
    check("mouse_edited_Bezier_is_the_production_pitch_request",target.size()>200&&finalCurve.active&&std::abs(target[200]-evaluatePitchCurve(finalCurve.points,1.0))<.001);
    const auto saved=folder.getChildFile("edited-virtual.hjpx");ProjectModel cold;
    check("mouse_edited_curve_and_handles_survive_cold_reload",model.save(saved,error)&&cold.load(saved,error)&&cold.snapshot().nativeVirtualGroups.size()==1&&cold.snapshot().nativeVirtualGroups[0].pitchCurveEncoding==2&&cold.snapshot().nativeVirtualGroups[0].pitchCurve[1].shape==PitchCurveShape::customBezier);
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

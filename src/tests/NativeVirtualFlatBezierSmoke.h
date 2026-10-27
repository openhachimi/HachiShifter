#pragma once
#include "../PianoRollComponent.h"
#include "../AudioEngine.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualFlatBezierSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id=i?"right-owner":"left-owner";clip.sourceFile=folder.getChildFile(i?"right.wav":"left.wav");clip.startSeconds=i?1.:0;clip.durationSeconds=clip.sourceDurationSeconds=1.2;clip.sourceTimeMap={{0,0},{1.2,1.2}};
        {juce::AudioBuffer<float> pcm(1,96000);for(int s=0;s<96000;++s)pcm.setSample(0,s,float(.1*std::sin(s*juce::MathConstants<double>::twoPi*261.6256/48000.)));auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,96000))return false;}
        NoteData note;note.id=i?"right":"left";note.durationSeconds=1.2;note.midiNote=note.sourceMidiCenter=60;for(int frame=0;frame<=240;++frame)note.contour.push_back({frame*.005,0,0,true});clip.notes={note};track.clips.push_back(clip);
    }
    data.tracks={track};ProjectModel model;model.resetDocument(data);const auto id=model.createNativeVirtualConnection({"left","right"},true,error,.2);
    std::vector<PitchCurveEditPoint> flat{{.9,60,PitchCurveShape::linear},{1.3,60,PitchCurveShape::customBezier}};flat[1].bezierX1=.2f;flat[1].bezierX2=.8f;flat[1].bezierY1=.2f;flat[1].bezierY2=.8f;flat[1].highPrecisionBezier=true;
    check("two_point_equal_pitch_Bezier_is_seeded",!id.isEmpty()&&model.setNativeVirtualPitchCurve(id,flat,error));if(id.isEmpty())return false;
    const auto original=model.snapshot();const auto fingerprint=model.contentFingerprint();I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack(track.id);roll.setTool(PianoRollComponent::Tool::points);roll.setPixelsPerSecond(400);roll.setRowHeight(22);roll.setBounds(0,0,1200,1000);
    const auto refresh=[&]{model.dispatchPendingMessages();roll.diagnosticRefresh();};refresh();
    const auto mouse=[&](juce::Point<float> at,juce::Point<float> down){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
    const auto curve=[&]{for(const auto& c:roll.diagnosticConnectionPitchCurves())if(c.connectionId==id)return c;return ResolvedConnectionPitchCurve{};};
    const auto endpoint=roll.diagnosticConnectionPitchPoint(id,1);if(endpoint){roll.mouseDown(mouse(*endpoint,*endpoint));roll.mouseUp(mouse(*endpoint,*endpoint));}
    const auto grip=roll.diagnosticConnectionPitchBezierHandle(id,1,1);check("flat_segment_has_a_real_selectable_grip",grip.has_value());
    if(grip)
    {
        const auto moved=grip->translated(8,-22);roll.mouseDown(mouse(*grip,*grip));roll.mouseDrag(mouse(moved,*grip));const auto pending=curve();
        check("dragging_flat_grip_creates_an_arch_without_an_early_model_write",pending.active&&pending.points.size()>2&&evaluatePitchCurve(pending.points,1.03)>60.05&&model.contentFingerprint()==fingerprint);
        roll.mouseUp(mouse(moved,*grip));refresh();
    }
    const auto committed=curve();check("flat_grip_commits_exact_subdivided_cubic_in_the_same_group",committed.active&&committed.points.size()>2&&model.contentFingerprint()!=fingerprint&&model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeConnections.empty());
    double maximum=0;std::vector<PitchCurveEditPoint> expected;
    if(grip)nativeVirtualCurveFromAbsoluteGrips(flat.front(),flat.back(),.9+.4*.2+8./400.,61.,.9+.4*.8,60.,expected,error);
    if(!expected.empty()&&committed.active)for(int i=0;i<=1000;++i){const auto time=.9+.4*i/1000.;maximum=std::max(maximum,double(std::abs(evaluatePitchCurve(expected,time)-evaluatePitchCurve(committed.points,time))));}
    check("actual_drag_target_matches_the_exact_absolute_cubic_not_a_polyline",!expected.empty()&&committed.active&&maximum<.0002);
    const auto target=AudioEngine::diagnosticNativeTargetMidi(model.snapshot(),0,0);check("render_request_consumes_the_new_equal_pitch_arch",target.size()>206&&std::abs(target[206]-evaluatePitchCurve(committed.points,1.03))<.001&&target[206]>60.05);
    const auto file=folder.getChildFile("flat-grip.hjpx");ProjectModel cold;check("flat_grip_arch_cold_roundtrip",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().nativeVirtualGroups[0].pitchCurve.size()>2);
    model.undo();check("flat_grip_drag_is_one_exact_undo",model.contentFingerprint()==fingerprint);model.redo();
    model.resetDocument(original);refresh();const auto last=roll.diagnosticConnectionPitchPoint(id,1);if(last){roll.mouseDown(mouse(*last,*last));roll.mouseUp(mouse(*last,*last));}const auto second=roll.diagnosticConnectionPitchBezierHandle(id,1,2);
    if(second){const auto moved=second->translated(-8,18);roll.mouseDown(mouse(*second,*second));roll.mouseDrag(mouse(moved,*second));check("second_flat_grip_can_bend_downward_in_preview",curve().active&&evaluatePitchCurve(curve().points,1.15)<59.95);roll.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));roll.mouseUp(mouse(moved,*second));refresh();}
    check("escape_cancels_flat_grip_preview_without_adding_hidden_controls",second&&model.contentFingerprint()==fingerprint);
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

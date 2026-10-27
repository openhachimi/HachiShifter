#pragma once
#include "../NativeVirtualRenderPlan.h"
#include "../NativeVirtualGroupIO.h"
#include "../PianoRollComponent.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualNonlinearCurveSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id=i?"right-owner":"left-owner";clip.sourceFile=folder.getChildFile(i?"right.wav":"left.wav");clip.startSeconds=i?1.:0.;clip.durationSeconds=clip.sourceDurationSeconds=1.2;clip.sourceTimeMap={{0,0},{1.2,1.2}};
        {juce::AudioBuffer<float> pcm(1,96000);for(int s=0;s<96000;++s)pcm.setSample(0,s,float(.1*std::sin(s*juce::MathConstants<double>::twoPi*(i?330:260)/48000.)));auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,96000))return false;}
        NoteData note;note.id=i?"right":"left";note.durationSeconds=1.2;note.midiNote=note.sourceMidiCenter=i?64:60;note.sourcePitchMeasured=true;for(int f=0;f<=240;++f)note.contour.push_back({f*.005,0,0,true});clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};ProjectModel model;model.resetDocument(seed);const auto id=model.createNativeVirtualConnection({"left","right"},true,error,.2);
    const auto resolve=[&](const ProjectData& data)
    {NativeVirtualConnectionPlan geometry;juce::String why;const auto& group=data.nativeVirtualGroups[0];if(!resolveNativeVirtualGroup(data.tracks[0],group,geometry,why))return NativeVirtualPitchPlan{};return planNativeVirtualPitch(data.tracks[0],group,geometry);};
    check("editable_virtual_seed_available",!id.isEmpty()&&resolve(model.snapshot()).active);if(id.isEmpty())return false;
    const auto base=resolve(model.snapshot());const auto from=base.points.front().timeSeconds,to=base.points.back().timeSeconds;const auto before=model.contentFingerprint();
    const auto make=[&](PitchCurveShape shape)
    {std::vector<PitchCurveEditPoint> points{{from,60,PitchCurveShape::linear},{(from+to)*.5,63,shape},{to,64,shape}};for(std::size_t i=1;i<points.size();++i){points[i].bezierX1=.15f;points[i].bezierY1=-.2f;points[i].bezierX2=.85f;points[i].bezierY2=1.3f;points[i].highPrecisionBezier=true;}return points;};
    bool shapes=true;double maximum=0;
    for(const auto shape:{PitchCurveShape::customBezier,PitchCurveShape::natural,PitchCurveShape::smooth,PitchCurveShape::easeIn,PitchCurveShape::easeOut})
    {
        const auto original=make(shape);const bool committed=model.setNativeVirtualPitchCurve(id,original,error);const auto curve=resolve(model.snapshot());shapes&=committed&&curve.active;
        if(curve.active)for(int i=0;i<=1200;++i){const auto t=from+(to-from)*i/1200.;maximum=std::max(maximum,double(std::abs(evaluatePitchCurve(original,t)-evaluatePitchCurve(curve.points,t))));}
    }
    std::cout<<"all_shape_max_MIDI_error="<<maximum<<std::endl;
    check("all_existing_nonlinear_interpolations_reconstruct_without_polyline_approximation",shapes&&maximum<.0002);
    const auto authored=make(PitchCurveShape::customBezier);check("custom_Bezier_commit_uses_versioned_shape_encoding",model.setNativeVirtualPitchCurve(id,authored,error)&&model.snapshot().nativeVirtualGroups[0].pitchCurveEncoding==2);
    const auto unchanged=model.contentFingerprint();check("repeating_identical_nonlinear_controls_is_noop",!model.setNativeVirtualPitchCurve(id,authored,error)&&model.contentFingerprint()==unchanged);
    auto handles=authored;handles[1].bezierY1+=.1f;check("handle_only_change_is_a_real_undoable_edit",model.setNativeVirtualPitchCurve(id,handles,error)&&model.contentFingerprint()!=unchanged);model.undo();check("undo_restores_exact_control_handles",model.contentFingerprint()==unchanged);
    const auto manual=model.snapshot();const auto file=folder.getChildFile("nonlinear.hjpx");ProjectModel cold;
    check("shape_encoding_and_original_handles_cold_roundtrip",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().nativeVirtualGroups[0].pitchCurveEncoding==2&&cold.snapshot().nativeVirtualGroups[0].pitchCurve[1].bezierY1==authored[1].bezierY1);
    const auto coldCurve=resolve(cold.snapshot());double coldError=0;
    if(coldCurve.active)for(int i=0;i<=1000;++i){const auto t=from+(to-from)*i/1000.;coldError=std::max(coldError,double(std::abs(evaluatePitchCurve(authored,t)-evaluatePitchCurve(coldCurve.points,t))));}
    check("cold_curve_retains_exact_nonlinear_shape",coldCurve.active&&coldError<.0002);
    auto equal=manual;equal.tracks[0].clips[1].notes[0].midiNote-=4;const auto equalCurve=resolve(equal);double equalError=0;bool notFlat=false;
    if(equalCurve.active)for(int i=0;i<=1000;++i){const auto u=i/1000.;const auto t=from+(to-from)*u;const auto expected=evaluatePitchCurve(authored,t)-4*float(u);const auto actual=evaluatePitchCurve(equalCurve.points,t);equalError=std::max(equalError,double(std::abs(expected-actual)));notFlat|=std::abs(actual-60)>.05;}
    check("equal_live_endpoint_pitches_do_not_flatten_Bezier_arch",equalCurve.active&&notFlat&&equalError<.0002);
    auto degenerate=equal;auto& degenerateGroup=degenerate.nativeVirtualGroups[0];
    PitchCurveEditPoint finish{1,4,PitchCurveShape::customBezier};finish.bezierX1=.15f;finish.bezierY1=-.2f;finish.bezierX2=.85f;finish.bezierY2=1.2f;finish.highPrecisionBezier=true;
    degenerateGroup.pitchCurve={{0,0,PitchCurveShape::linear},finish};const auto splitCurve=resolve(degenerate);
    std::vector<PitchCurveEditPoint> two{{from,60,PitchCurveShape::linear},finish};two.back().timeSeconds=to;two.back().targetMidi=64;double splitError=0;
    if(splitCurve.active)for(int i=0;i<=1000;++i){const auto u=i/1000.;const auto t=from+(to-from)*u;splitError=std::max(splitError,double(std::abs(evaluatePitchCurve(splitCurve.points,t)-evaluatePitchCurve(two,t)+4*float(u))));}
    check("degenerate_equal_pitch_Bezier_is_exactly_subdivided_not_flattened",splitCurve.active&&splitCurve.points.size()>2&&splitError<.0002);
    auto moved=manual;for(auto& clip:moved.tracks[0].clips){clip.startSeconds+=2;clip.notes[0].midiNote+=3;}const auto movedCurve=resolve(moved);double movedError=0;
    if(movedCurve.active)for(int i=0;i<=1000;++i){const auto t=from+(to-from)*i/1000.;movedError=std::max(movedError,double(std::abs(evaluatePitchCurve(movedCurve.points,t+2)-evaluatePitchCurve(authored,t)-3)));}
    check("group_move_and_transpose_preserve_nonlinear_shape",movedCurve.active&&movedError<.0002);
    auto crossed=manual;crossed.tracks[0].clips[0].startSeconds+=2;const auto crossCurve=resolve(crossed);double crossError=0;
    if(crossCurve.active)for(int i=0;i<=1000;++i){const auto u=i/1000.;crossError=std::max(crossError,double(std::abs(evaluatePitchCurve(crossCurve.points,(from+2)*(1-u)+to*u)-evaluatePitchCurve(authored,from+(to-from)*u))));}
    check("crossed_members_reverse_the_time_map_without_losing_directed_shape",crossCurve.active&&crossError<.0002&&crossed.nativeVirtualGroups[0].orderedNoteIds==manual.nativeVirtualGroups[0].orderedNoteIds);
    auto endpoints=authored;endpoints.front().timeSeconds=from-.025;endpoints.back().timeSeconds=to+.025;
    check("endpoint_times_are_editable_on_real_supported_voiced_anchors",model.setNativeVirtualPitchCurve(id,endpoints,error)&&std::abs(resolve(model.snapshot()).points.front().timeSeconds-endpoints.front().timeSeconds)<1.e-8);
    const auto safe=model.contentFingerprint();auto unsupported=endpoints;unsupported.front().timeSeconds=-1.;
    check("unsupported_endpoint_edit_is_rejected_atomically",!model.setNativeVirtualPitchCurve(id,unsupported,error)&&model.contentFingerprint()==safe);
    auto extreme=endpoints;extreme[1].bezierY1=100;extreme[1].bezierY2=-100;
    check("Bezier_interior_extrema_outside_MIDI_range_are_rejected_atomically",!model.setNativeVirtualPitchCurve(id,extreme,error)&&model.contentFingerprint()==safe);
    check("shared_mode_can_reuse_the_same_manual_group_curve",model.linkNativeAudio({"left","right"})&&model.snapshot().nativeVirtualGroups[0].sharedPitch);
    I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack(track.id);roll.setBounds(0,0,1200,900);roll.diagnosticRefresh();const auto displayed=roll.diagnosticPitchLineAt("left",(from+to)*.5);const auto active=resolve(model.snapshot());
    check("manual_group_curve_wins_over_shared_source_curve_in_real_display",displayed&&active.active&&std::abs(*displayed-evaluatePitchCurve(active.points,(from+to)*.5))<.001);
    NativeVirtualGroup decoded;auto schema=nativeVirtualGroupTree(model.snapshot().nativeVirtualGroups[0]);schema.setProperty("pitchCurveEncoding",3,nullptr);check("future_curve_encoding_is_not_silently_read_as_linear",!nativeVirtualGroupFromTree(schema,decoded,error));
    auto legacy=manual;auto& old=legacy.nativeVirtualGroups[0];old.pitchCurveEncoding=1;old.pitchCurve={{0,0,PitchCurveShape::linear},{1,0,PitchCurveShape::linear}};const auto oldCurve=resolve(legacy);
    check("legacy_linear_residual_curves_keep_their_original_interpretation",oldCurve.active&&std::abs(evaluatePitchCurve(oldCurve.points,(from+to)*.5)-62)<.0001);
    juce::ignoreUnused(before);folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

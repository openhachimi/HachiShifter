#pragma once
#include "../NativeVirtualPitchPlan.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualPitchPlanSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto member=[&](juce::String id,double from,double to,float midi,bool bridge=false)
    {
        ClipData clip;clip.id="owner-"+id;clip.sourceFile=output.getChildFile(id+".wav");clip.startSeconds=from;clip.durationSeconds=clip.sourceDurationSeconds=to-from;clip.sourceTimeMap={{0,0},{to-from,to-from}};
        NoteData note;note.id=id;note.label="any";note.durationSeconds=to-from;note.midiNote=note.sourceMidiCenter=midi;note.sourcePitchMeasured=true;
        for(int frame=0;frame<=juce::roundToInt(note.durationSeconds/.005);++frame){const auto t=std::min(note.durationSeconds,frame*.005);note.contour.push_back({t,float(t*2),float(t*2),!bridge});}
        if(bridge){note.nativeUnpitched=true;note.pronunciationKind=PronunciationKind::transition;note.nativeTransition=NativeTransitionShape{1,(to-from)*.5,"fixture-middle"};}
        clip.notes={note};return clip;
    };
    TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;track.clips={member("left",0,1.2,60),member("right",1.,2.,64)};
    NativeVirtualGroup group;group.id="virtual";group.trackId=track.id;group.orderedNoteIds={"left","right"};group.pitchDurationSeconds=.1;
    NativeVirtualConnectionPlan geometry;check("direct_geometry_valid",planNativeVirtualConnection(track,group.orderedNoteIds,geometry,error));
    check("automatic_glide_default_off_produces_no_controls",!planNativeVirtualPitch(track,group,geometry).active&&planNativeVirtualPitch(track,group,geometry).points.empty());
    group.automaticPitch=true;const auto pitch=planNativeVirtualPitch(track,group,geometry);
    check("stable_voiced_evidence_generates_one_local_target_bridge",pitch.active&&pitch.points.size()>2&&pitch.points.front().timeSeconds>=.9&&pitch.points.back().timeSeconds<=1.3);
    if(!pitch.active){error=pitch.issue;check("active_pitch_diagnostic",false);return false;}
    const auto& a=track.clips[0].notes[0];const auto& b=track.clips[1].notes[0];
    check("anchors_use_actual_target_contours_not_only_note_centres",std::abs(pitch.points.front().targetMidi-evaluatePitchCurve(ownPitchPoints(a),pitch.points.front().timeSeconds))<1.e-5&&std::abs(pitch.points.back().targetMidi-evaluatePitchCurve(ownPitchPoints(b),pitch.points.back().timeSeconds-1.))<1.e-5);
    check("local_trend_retained_at_both_anchors",pitch.points.front().targetMidi>60&&pitch.points.back().targetMidi>64);
    bool finite=true;for(std::size_t i=0;i<pitch.points.size();++i){const auto& p=pitch.points[i];finite&=std::isfinite(p.targetMidi)&&p.targetMidi>=0&&p.targetMidi<=127&&(i==0||p.timeSeconds>pitch.points[i-1].timeSeconds);}
    check("finite_strictly_monotonic_target_times",finite);
    auto silent=track;silent.clips[0].notes[0].nativeUnpitched=true;
    check("unpitched_endpoint_does_not_gain_invented_pitch",!planNativeVirtualPitch(silent,group,geometry).active);
    auto absent=track;absent.clips[0].notes[0].contour.clear();
    check("empty_contour_is_not_voiced_evidence",!planNativeVirtualPitch(absent,group,geometry).active);
    auto spike=track;for(auto& p:spike.clips[0].notes[0].contour)p.voiced=false;spike.clips[0].notes[0].contour[190].voiced=true;
    check("isolated_voiced_spike_is_not_a_stable_anchor",!planNativeVirtualPitch(spike,group,geometry).active);
    auto manual=track;manual.clips[0].notes[0].pitchControlPoints={{0,60},{1.2,61}};
    check("manual_note_handles_protected_from_auto_glide",!planNativeVirtualPitch(manual,group,geometry).active);
    manual=track;manual.clips[1].notes[0].contour[50].hasManualTarget=true;
    check("manual_dense_target_protected_from_auto_glide",!planNativeVirtualPitch(manual,group,geometry).active);
    auto triple=track;triple.clips.insert(triple.clips.begin()+1,member("random-transition-alias",.8,1.4,62,true));group.orderedNoteIds={"left","random-transition-alias","right"};
    check("unvoiced_identified_transition_has_valid_route",planNativeVirtualConnection(triple,group.orderedNoteIds,geometry,error));
    const auto bridge=planNativeVirtualPitch(triple,group,geometry);
    check("one_reference_curve_crosses_uv_bridge_without_competing_pair_curves",bridge.active&&bridge.noteIds==group.orderedNoteIds&&bridge.points.front().timeSeconds<.8&&bridge.points.back().timeSeconds>1.4);
    check("reference_plan_never_changes_transition_voicing_or_source_contours",triple.clips[1].notes[0].nativeUnpitched&&std::none_of(triple.clips[1].notes[0].contour.begin(),triple.clips[1].notes[0].contour.end(),[](const auto& p){return p.voiced;})&&track.clips[0].notes[0].pitchControlPoints.empty());
    auto moved=triple;for(auto& c:moved.clips)c.startSeconds+=3.;NativeVirtualConnectionPlan shifted;
    const auto valid=planNativeVirtualConnection(moved,group.orderedNoteIds,shifted,error);const auto shiftedPitch=planNativeVirtualPitch(moved,group,shifted);
    check("moving_sources_reprojects_same_pitch_shape",valid&&shiftedPitch.active&&bridge.points.size()==shiftedPitch.points.size()&&std::abs(shiftedPitch.points.front().timeSeconds-bridge.points.front().timeSeconds-3.)<1.e-8&&std::abs(shiftedPitch.points.front().targetMidi-bridge.points.front().targetMidi)<1.e-5);
    group.enabled=false;check("disabled_group_has_no_pitch_effect",!planNativeVirtualPitch(triple,group,geometry).active);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

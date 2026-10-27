#pragma once
#include "../AudioEngine.h"
#include "../NativeVirtualRenderPlan.h"
#include <iostream>
namespace hachi
{
// Production makeRenderRequest/applyNativeSharedPitch through the existing
// project-aware diagnostic. No NN or media decode needed; not a PCM quality test.
inline bool runNativeVirtualPitchRequestSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;track.smoothOverlaps=false;
    for(int i=0;i<3;++i)
    {
        const juce::String id=i==0?"left":i==1?"bridge":"right";
        ClipData clip;clip.id="owner-"+id;clip.sourceFile=output.getChildFile(id+".wav");clip.startSeconds=i==0?0.:i==1?.8:1.;clip.durationSeconds=clip.sourceDurationSeconds=i==0?1.2:i==1?.6:1.;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        NoteData note;note.id=id;note.label="-";note.durationSeconds=clip.durationSeconds;note.midiNote=note.sourceMidiCenter=i==2?64:60;note.sourcePitchMeasured=true;
        for(int f=0;f<=juce::roundToInt(clip.durationSeconds/.005);++f)
        {const auto time=std::min(clip.durationSeconds,f*.005);note.contour.push_back({time,float(time*2),float(time*2),i!=1&&!(i==0&&time>.98&&time<1.02)});}
        if(i==1){note.nativeUnpitched=true;note.pronunciationKind=PronunciationKind::transition;note.nativeTransition=NativeTransitionShape{1,.3,"fixture"};}
        clip.notes={note};track.clips.push_back(clip);
    }
    data.tracks={track};ProjectModel model;model.resetDocument(data);const auto id=model.createNativeVirtualConnection({"left","bridge","right"},false,error);
    check("valid_route_created",id.isNotEmpty());if(id.isEmpty())return false;
    const auto withoutPitch=model.snapshot();auto withPitch=withoutPitch;withPitch.nativeVirtualGroups[0].automaticPitch=true;
    ProjectData projected;std::vector<NativeVirtualRenderGroup> groups;
    check("active_pitch_derived_in_production_projection",prepareNativeVirtualRender(withPitch,projected,groups,error)&&groups.size()==1&&groups[0].pitch.active);
    if(groups.empty()||!groups[0].pitch.active)return false;
    const auto curves=nativeVirtualResolvedPitchCurves(groups,track.id);const auto& pitch=groups[0].pitch;
    int changed=0,voiced=0,unvoiced=0;bool masks=true,outside=true,finite=true;
    for(int clip=0;clip<3;++clip)
    {
        const auto off=AudioEngine::diagnosticNativeTargetMidi(withoutPitch,0,clip),on=AudioEngine::diagnosticNativeTargetMidi(withPitch,0,clip);
        if(off.empty()||off.size()!=on.size()){masks=outside=finite=false;continue;}
        for(std::size_t f=0;f<off.size();++f)
        {
            const auto time=track.clips[std::size_t(clip)].startSeconds+std::min(track.clips[std::size_t(clip)].durationSeconds,f*.005);
            if(off[f]>0)++voiced;else ++unvoiced;
            masks&=(off[f]>0)==(on[f]>0);finite&=std::isfinite(on[f]);
            if(time<pitch.points.front().timeSeconds-1.e-8||time>pitch.points.back().timeSeconds+1.e-8)outside&=std::abs(off[f]-on[f])<1.e-5;
            else if(off[f]>0&&std::abs(off[f]-on[f])>.01)++changed;
        }
        if(clip==1)check("fully_unvoiced_transition_request_stays_zero",std::all_of(on.begin(),on.end(),[](const auto value){return value==0;}));
    }
    check("voiced_target_frames_really_change",changed>10&&voiced>0);
    check("original_UV_mask_retained_including_internal_dropouts",masks&&unvoiced>0);
    check("far_pitch_and_original_trends_not_rewritten",outside&&finite);
    auto offGroup=withPitch;offGroup.nativeVirtualGroups[0].enabled=false;
    bool exact=true;for(int clip=0;clip<3;++clip)exact&=AudioEngine::diagnosticNativeTargetMidi(data,0,clip)==AudioEngine::diagnosticNativeTargetMidi(offGroup,0,clip);
    check("disabled_group_restores_original_request_bit_exact",exact);
    const auto suffix=nativeVirtualPitchCacheSuffix(curves,track.clips[0]);auto edited=curves;edited[0].points[edited[0].points.size()/2].targetMidi+=.5f;
    check("virtual_pitch_changes_invalidate_native_request_cache",!suffix.empty()&&suffix!=nativeVirtualPitchCacheSuffix(edited,track.clips[0]));
    auto unrelated=track.clips[0];unrelated.notes[0].id="not-in-group";
    check("unrelated_or_no_pitch_requests_keep_legacy_cache_key",nativeVirtualPitchCacheSuffix(curves,unrelated).empty()&&nativeVirtualPitchCacheSuffix({},track.clips[0]).empty());
    auto protectedProject=withPitch;protectedProject.tracks[0].clips[0].notes[0].pitchControlPoints={{0,60},{1.2,61}};
    auto protectedOff=protectedProject;protectedOff.nativeVirtualGroups[0].automaticPitch=false;
    check("automatic_glide_does_not_override_manual_note_curve",AudioEngine::diagnosticNativeTargetMidi(protectedProject,0,0)==AudioEngine::diagnosticNativeTargetMidi(protectedOff,0,0));
    auto legacy=data;legacy.tracks[0].clips.erase(legacy.tracks[0].clips.begin()+1);auto& head=legacy.tracks[0].clips[0];head.durationSeconds=head.sourceDurationSeconds=head.notes[0].durationSeconds=1.;head.sourceTimeMap={{0,0},{1,1}};
    NativeConnection graph;graph.id="legacy-host-curve";graph.leftNoteId="left";graph.rightNoteId="right";graph.type="pitch-portamento";graph.pitchCurve={{.95,60},{1.05,64}};legacy.nativeConnections={graph};
    const auto legacyResolved=resolvedConnectionPitchCurves(legacy.tracks[0],legacy.nativeConnections);
    check("legacy_overlay_control_fixture_is_nonvacuously_active",legacyResolved.size()==1&&legacyResolved[0].active);
    const auto base=sharedPitchLines(legacy.tracks[0],legacy.nativeConnections);
    const auto same=overlayResolvedConnectionPitchCurves(legacy.tracks[0],sharedPitchLines(legacy.tracks[0]),resolvedConnectionPitchCurves(legacy.tracks[0],legacy.nativeConnections));
    bool equivalent=base.byNote.size()==same.byNote.size();for(const auto& [note,member]:base.byNote){const auto* other=same.memberFor(note);if(!other){equivalent=false;continue;}for(double time=0;time<2.;time+=.005)equivalent&=member.line->midiAt(time)==other->line->midiAt(time);}
    check("legacy_and_virtual_share_exact_same_overlay_authority",equivalent);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

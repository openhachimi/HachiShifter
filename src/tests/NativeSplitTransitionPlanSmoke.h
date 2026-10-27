#pragma once
#include "../NativeSplitTransitionPlan.h"
#include <iostream>
namespace hachi
{
inline bool runNativeSplitTransitionPlanSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto member=[&](juce::String id,juce::String file,double targetFrom,double duration,double sourceFrom,double sourceDuration)
    {
        ClipData clip;clip.id="owner-"+id;clip.sourceFile=output.getChildFile(file);clip.startSeconds=targetFrom;clip.durationSeconds=duration;clip.sourceOffsetSeconds=sourceFrom;clip.sourceDurationSeconds=sourceDuration;clip.sourceTimeMap={{0,0},{duration,sourceDuration}};
        NoteData note;note.id=id;note.label="unrelated alias";note.durationSeconds=duration;clip.notes={note};return clip;
    };
    TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;
    track.clips={member("left","first.wav",0,1.2,0,1.2),member("bridge-front","bridge.wav",.8,.3,2.,.2),member("bridge-back","bridge.wav",1.1,.3,2.2,.3),member("right","other.wav",1.,1.,0,1.)};
    NativeSplitTransitionEvidence evidence;evidence.logicalId="ONE-logical-transition";evidence.frontNoteId="bridge-front";evidence.backNoteId="bridge-back";
    evidence.sourceObjectIdentity=evidence.frontSourceObjectIdentity=evidence.backSourceObjectIdentity="original-graph-source-17";
    evidence.precedingElementIdentity=evidence.frontElementIdentity="element-21";evidence.followingElementIdentity=evidence.backElementIdentity="element-22";
    evidence.joinsPitches=evidence.joinsAmplitudes=evidence.joinsPhases=evidence.transitionIdentified=true;
    NativeSplitTransitionPlan plan;
    check("four_physical_pieces_resolve_to_one_logical_bridge",planNativeSplitTransition(track,"left",evidence,"right",plan,error)&&plan.logical.members.size()==3&&plan.logical.handovers.size()==2&&plan.physical.size()==4);
    check("outer_normal_sources_really_overlap_in_this_fixture",track.clips[0].startSeconds+track.clips[0].durationSeconds>track.clips[3].startSeconds);
    check("actual_partial_source_cut_is_middle_not_an_energy_midpoint",std::abs(plan.middleSourceSeconds-2.2)<1.e-8&&std::abs(plan.middleTargetSeconds-1.1)<1.e-8&&std::abs(plan.logical.members[1].middle-1.1)<1.e-8);
    check("halves_keep_distinct_physical_identities_and_original_clocks",plan.physical[1].noteId=="bridge-front"&&plan.physical[2].noteId=="bridge-back"&&plan.physical[1].ownerId!=plan.physical[2].ownerId&&track.clips[1].sourceTimeMap.back().sourceSeconds==.2&&track.clips[2].sourceOffsetSeconds==2.2);
    bool sum=true,oneHalf=true;for(double t=0;t<2.;t+=.001){const auto w=nativeSplitTransitionContributions(plan,t);sum&=std::abs(w[0]+w[1]+w[2]+w[3]-1.)<1.e-8;oneHalf&=!(w[1]>1.e-9&&w[2]>1.e-9);}
    check("four_source_overlap_has_one_contribution_partition",sum);
    check("internal_partial_cut_never_creates_second_bridge_crossfade",oneHalf);
    const auto cut=nativeSplitTransitionContributions(plan,1.1);check("source_cut_handover_goes_exactly_to_back_half",cut[1]==0&&cut[2]>.99);
    auto noIdentity=evidence;noIdentity.transitionIdentified=false;auto named=track;named.clips[1].notes[0].label="e_g";
    check("join_plus_example_alias_does_not_prove_transition_identity",!planNativeSplitTransition(named,"left",noIdentity,"right",plan,error));
    auto pitchOnly=evidence;pitchOnly.joinsAmplitudes=pitchOnly.joinsPhases=false;
    check("pitch_join_alone_is_not_source_cut_proof",!planNativeSplitTransition(track,"left",pitchOnly,"right",plan,error));
    auto wrongSource=evidence;wrongSource.backSourceObjectIdentity="another-graph-source";
    check("same_filename_without_same_graph_source_refused",!planNativeSplitTransition(track,"left",wrongSource,"right",plan,error));
    auto wrongEndpoint=evidence;wrongEndpoint.followingElementIdentity="other-element";
    check("explicit_join_endpoints_must_match_both_halves",!planNativeSplitTransition(track,"left",wrongEndpoint,"right",plan,error));
    auto wrongClock=track;wrongClock.clips[2].sourceOffsetSeconds+=.01;
    check("target_adjacent_but_discontinuous_source_cut_refused",!planNativeSplitTransition(wrongClock,"left",evidence,"right",plan,error));
    auto gap=track;gap.clips[2].startSeconds+=.01;
    check("source_continuity_without_common_target_cut_refused",!planNativeSplitTransition(gap,"left",evidence,"right",plan,error));
    auto breath=track;breath.clips[2].notes[0].pronunciationKind=PronunciationKind::breath;
    check("breath_half_never_consumed_as_transition",!planNativeSplitTransition(breath,"left",evidence,"right",plan,error));
    auto fifth=track;fifth.clips.push_back(member("fifth-voice","harmony.wav",.85,.2,0,.2));
    check("independent_fifth_voice_still_protected",!planNativeSplitTransition(fifth,"left",evidence,"right",plan,error));
    auto shifted=track;for(auto& clip:shifted.clips)clip.startSeconds+=3.;
    check("two_half_middle_follows_actual_source_clock_on_move",planNativeSplitTransition(shifted,"left",evidence,"right",plan,error)&&std::abs(plan.middleTargetSeconds-4.1)<1.e-8&&std::abs(plan.middleSourceSeconds-2.2)<1.e-8);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

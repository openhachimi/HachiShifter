#pragma once
#include "../NativeConsonantMergePlan.h"
#include <iostream>
namespace hachi
{
inline bool runNativeConsonantMergePlanSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;
    ClipData onset;onset.id="sh-owner";onset.sourceFile=output.getChildFile("same-recording.wav");onset.startSeconds=1.;onset.durationSeconds=.1;onset.sourceOffsetSeconds=.2;onset.sourceDurationSeconds=.08;onset.sourceTimeMap={{0,0},{.04,.02},{.1,.08}};
    NoteData sh;sh.id="onset";sh.durationSeconds=.1;sh.midiNote=33;sh.sourceMidiCenter=33;for(int f=0;f<=20;++f)sh.contour.push_back({f*.005,0,0,false});sh.connectedToNext=true;onset.notes={sh};onset.glideConnectedToNext=true;onset.crossfadeOutSeconds=.01;
    ClipData vowel;vowel.id="vowel-owner";vowel.sourceFile=onset.sourceFile;vowel.startSeconds=1.1;vowel.durationSeconds=.4;vowel.sourceOffsetSeconds=.28;vowel.sourceDurationSeconds=.3;vowel.sourceTimeMap={{0,0},{.1,.06},{.4,.3}};vowel.glideConnectedFromPrevious=true;vowel.crossfadeInSeconds=.01;
    NoteData i;i.id="vowel";i.label="i";i.durationSeconds=.4;i.midiNote=i.sourceMidiCenter=64;i.drift=.8f;i.modulation=.7f;i.vibratoEnabled=true;i.vibratoLengthPercent=50;i.vibratoDepthCents=20;for(int f=0;f<=80;++f)i.contour.push_back({f*.005,float(f*.1),float(f*.07),true});i.connectedToPrevious=true;vowel.notes={i};track.clips={onset,vowel};data.tracks={track};
    NativeConsonantMergeEvidence proof;proof.sibilantIdentified=true;auto& raw=proof.sourceJoin;raw.logicalId="raw-join";raw.frontNoteId="onset";raw.backNoteId="vowel";raw.sourceObjectIdentity=raw.frontSourceObjectIdentity=raw.backSourceObjectIdentity="source-17";raw.precedingElementIdentity=raw.frontElementIdentity="element-21";raw.followingElementIdentity=raw.backElementIdentity="element-22";raw.joinsPitches=raw.joinsAmplitudes=raw.joinsPhases=true;
    NativeConsonantMergePlan plan;check("explicit_sibilant_plus_continuous_join_plans_one_vowel_owned_note",planNativeConsonantMerge(data,proof,plan,error)&&plan.merged.notes.size()==1&&plan.merged.notes[0].id=="vowel"&&plan.merged.id=="vowel-owner");
    if(plan.merged.notes.empty())return false;const auto& merged=plan.merged;const auto& n=merged.notes[0];const auto timing=nativeNoteSourceTiming(n,merged,nativeClipClock(merged));
    check("real_cut_is_P_and_no_original_vowel_P_gives_C_equals_P",std::abs(timing.alignmentSeconds-.28)<1.e-8&&std::abs(timing.fixedDurationSeconds-.08)<1.e-8&&std::abs(plan.preutteranceSourceSeconds-.28)<1.e-8&&std::abs(plan.consonantEndSourceSeconds-.28)<1.e-8);
    check("P_target_is_original_partial_cut_not_onset",std::abs(merged.startSeconds+nativeNotePreutteranceTarget(n,merged)-1.1)<1.e-8);
    check("vowel_pitch_centre_and_trend_parameters_inherited_not_onset",n.midiNote==64&&n.sourceMidiCenter==64&&n.drift==.8f&&n.modulation==.7f);
    bool clocks=true;for(const auto& point:onset.sourceTimeMap)clocks&=std::abs(merged.sourceOffsetSeconds+nativeSourceTimeAt(merged.sourceTimeMap,point.targetSeconds)-onset.sourceOffsetSeconds-point.sourceSeconds)<1.e-8;
    for(const auto& point:vowel.sourceTimeMap)clocks&=std::abs(merged.sourceOffsetSeconds+nativeSourceTimeAt(merged.sourceTimeMap,.1+point.targetSeconds)-vowel.sourceOffsetSeconds-point.sourceSeconds)<1.e-8;
    check("both_original_nonlinear_source_clocks_preserved_at_every_knot",clocks);
    bool uv=true,contours=true;for(const auto& p:n.contour)if(p.timeSeconds<.1-1.e-8)uv&=!p.voiced;
    for(const auto& p:i.contour){const auto found=std::find_if(n.contour.begin(),n.contour.end(),[&](const auto& q){return std::abs(q.timeSeconds-p.timeSeconds-.1)<1.e-8;});contours&=found!=n.contour.end()&&found->voiced==p.voiced&&found->relativeCents==p.relativeCents&&found->withoutVibratoCents==p.withoutVibratoCents;}
    check("onset_stays_unvoiced_vowel_dense_pitch_is_shifted_not_recomputed",uv&&contours);
    bool vibrato=true;for(double t=0;t<=.4;t+=.003)vibrato&=std::abs(vibratoCentsAt(i,t)-vibratoCentsAt(n,t+.1))<1.e-6;
    check("vowel_vibrato_time_phase_and_fades_preserved",vibrato);
    check("exact_original_fragments_retained_for_future_durable_split_back",plan.onsetOriginal.id==onset.id&&plan.vowelOriginal.id==vowel.id&&plan.onsetOriginal.crossfadeOutSeconds==.01&&plan.vowelOriginal.notes[0].vibratoLengthPercent==50&&data.tracks[0].clips.size()==2);
    auto withP=data;withP.tracks[0].clips[1].notes[0].utauPreutteranceOverrideEnabled=true;withP.tracks[0].clips[1].notes[0].utauPreutteranceSeconds=.03;
    check("existing_vowel_P_becomes_C_while_partial_cut_stays_P",planNativeConsonantMerge(withP,proof,plan,error)&&std::abs(plan.preutteranceSourceSeconds-.28)<1.e-8&&std::abs(plan.consonantEndSourceSeconds-.31)<1.e-8&&std::abs(nativeNoteSourceTiming(plan.merged.notes[0],plan.merged,nativeClipClock(plan.merged)).fixedDurationSeconds-.11)<1.e-8);
    auto unknown=proof;unknown.sibilantIdentified=false;check("no_F0_or_join_alone_never_auto_merges",!planNativeConsonantMerge(data,unknown,plan,error));
    auto transition=proof;transition.sourceJoin.transitionIdentified=true;check("identified_transition_pair_is_not_a_consonant_merge",!planNativeConsonantMerge(data,transition,plan,error));
    auto logical=data;NativeLogicalTransition row;row.evidence.frontNoteId="onset";row.evidence.backNoteId="vowel";logical.logicalTransitions={row};check("logical_transition_halves_are_explicitly_excluded",!planNativeConsonantMerge(logical,proof,plan,error));
    auto missing=data;missing.tracks[0].clips[0].notes[0].contour.clear();check("empty_analysis_is_not_sibilance_evidence",!planNativeConsonantMerge(missing,proof,plan,error));
    auto voicedOnset=data;voicedOnset.tracks[0].clips[0].notes[0].contour[3].voiced=true;check("pitched_onset_not_swallowed",!planNativeConsonantMerge(voicedOnset,proof,plan,error));
    auto sourceGap=data;sourceGap.tracks[0].clips[1].sourceOffsetSeconds=.29;check("target_adjacent_with_different_source_cut_refused",!planNativeConsonantMerge(sourceGap,proof,plan,error));
    auto breath=data;breath.tracks[0].clips[0].notes[0].pronunciationKind=PronunciationKind::breath;check("breath_not_swallowed",!planNativeConsonantMerge(breath,proof,plan,error));
    auto expressive=data;expressive.tracks[0].clips[0].gain=.5f;check("unsupported_expressive_gain_not_silently_lost",!planNativeConsonantMerge(expressive,proof,plan,error));
    auto overlap=data;auto previous=onset;previous.id="previous-other-source";previous.sourceFile=output.getChildFile("other.wav");previous.startSeconds=.95;previous.durationSeconds=.2;previous.sourceDurationSeconds=.2;previous.sourceTimeMap={{0,0},{.2,.2}};previous.notes[0].id="previous-note";previous.notes[0].durationSeconds=.2;overlap.tracks[0].clips.push_back(previous);
    check("unrelated_source_overlap_is_preserved_not_a_reason_to_drop_explicit_join",planNativeConsonantMerge(overlap,proof,plan,error)&&overlap.tracks[0].clips.size()==3&&plan.merged.sourceFile==onset.sourceFile);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

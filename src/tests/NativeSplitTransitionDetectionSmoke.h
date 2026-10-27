#pragma once
#include "../NativeVirtualAutoConnect.h"
#include <iostream>
namespace hachi
{
inline bool runNativeSplitTransitionDetectionSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    for(int fileIndex=0;fileIndex<4;++fileIndex)
    {
        const auto file=output.getChildFile(fileIndex==0?"left.wav":fileIndex==1?"bridge.wav":fileIndex==2?"right.wav":"flat.wav");
        juce::AudioBuffer<float> pcm(1,96000);for(int s=0;s<pcm.getNumSamples();++s)
        {
            const auto time=s/48000.;const auto envelope=fileIndex==1?std::clamp(std::abs(time-.1)/.025,.04,1.):1.;
            pcm.setSample(0,s,float(.1*envelope*std::sin(juce::MathConstants<double>::twoPi*173*time)));
        }
        auto stream=file.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;
    }
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;
    const std::array<juce::String,4> ids{"left","front","back","right"};
    for(int i=0;i<4;++i)
    {
        ClipData clip;clip.id="owner-"+ids[std::size_t(i)];clip.sourceFile=output.getChildFile(i==0?"left.wav":i==3?"right.wav":"bridge.wav");clip.startSeconds=i==0?0.:i==1?.95:i==2?1.05:1.;
        clip.durationSeconds=clip.sourceDurationSeconds=i==0?1.1:i==3?1.:.1;clip.sourceOffsetSeconds=i==2?.1:0.;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        NoteData note;note.id=ids[std::size_t(i)];note.label="unrelated name";note.durationSeconds=clip.durationSeconds;note.midiNote=note.sourceMidiCenter=i==3?64:60;note.sourcePitchMeasured=true;
        for(int f=0;f<=int(std::lround(note.durationSeconds/.005));++f)note.contour.push_back({f*.005,0,0,i!=2});
        if(i==1){note.connectedToNext=true;clip.glideConnectedToNext=true;}
        if(i==2){note.nativeUnpitched=true;note.connectedToPrevious=true;clip.glideConnectedFromPrevious=true;}
        clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};NativeSplitTransitionEvidence raw;raw.logicalId="raw-pair-candidate";raw.frontNoteId="front";raw.backNoteId="back";
    raw.sourceObjectIdentity=raw.frontSourceObjectIdentity=raw.backSourceObjectIdentity="source-17";raw.precedingElementIdentity=raw.frontElementIdentity="element-21";raw.followingElementIdentity=raw.backElementIdentity="element-22";
    raw.joinsPitches=raw.joinsAmplitudes=raw.joinsPhases=true;
    const auto found=detectMelodyneSplitTransitions(seed,{raw});
    check("two_connected_halves_with_outer_overlap_are_detected_as_one_bridge",found.size()==1&&found[0].orderedNoteIds==std::vector<juce::String>{"left","front","back","right"});
    check("raw_join_does_not_start_as_a_classification",!raw.transitionIdentified&&found.size()==1&&found[0].evidence.transitionIdentified);
    check("real_source_cut_becomes_middle_not_per_half_midpoints",found.size()==1&&std::abs(found[0].middleSourceSeconds-.1)<1.e-8);
    auto enabled=seed;const auto report=autoConnectNativeVirtualImport(enabled,{true,false},{raw});
    check("detected_split_bridge_uses_same_four_member_virtual_commit",report.created==1&&enabled.nativeVirtualGroups.size()==1&&enabled.nativeVirtualGroups[0].splitTransition&&enabled.nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"left","front","back","right"}&&enabled.nativeConnections.empty());
    check("automatic_split_bridge_preserves_original_notes_and_source_clocks",enabled.tracks[0].clips.size()==4&&enabled.tracks[0].clips[1].notes[0].connectedToNext&&enabled.tracks[0].clips[2].notes[0].connectedToPrevious&&enabled.tracks[0].clips[2].sourceOffsetSeconds==.1);
    auto disabled=seed;check("default_off_does_not_classify_or_create_group",autoConnectNativeVirtualImport(disabled,{}, {raw}).created==0&&disabled.nativeVirtualGroups.empty()&&!disabled.tracks[0].clips[1].notes[0].nativeTransition);
    auto flat=seed;flat.tracks[0].clips[1].sourceFile=flat.tracks[0].clips[2].sourceFile=output.getChildFile("flat.wav");
    check("continuous_join_without_cut_energy_evidence_not_auto_typed",detectMelodyneSplitTransitions(flat,{raw}).empty());
    auto sameVowels=seed;for(auto& p:sameVowels.tracks[0].clips[2].notes[0].contour)p.voiced=true;sameVowels.tracks[0].clips[2].notes[0].nativeUnpitched=false;
    check("fully_voiced_stable_vowels_not_called_transition",detectMelodyneSplitTransitions(sameVowels,{raw}).empty());
    auto consonant=seed;consonant.tracks[0].clips[1].notes[0].nativeUnpitched=true;
    check("unpitched_front_is_not_guessed_as_vowel_to_consonant_bridge",detectMelodyneSplitTransitions(consonant,{raw}).empty());
    auto breath=seed;breath.tracks[0].clips[2].notes[0].pronunciationKind=PronunciationKind::breath;check("breath_not_swallowed_by_split_bridge_detection",detectMelodyneSplitTransitions(breath,{raw}).empty());
    auto wrongSource=raw;wrongSource.backSourceObjectIdentity="other-source";check("same_filename_without_same_graph_identity_rejected",detectMelodyneSplitTransitions(seed,{wrongSource}).empty());
    auto noPhase=raw;noPhase.joinsPhases=false;check("pitch_join_alone_never_decides_split_transition",detectMelodyneSplitTransitions(seed,{noPhase}).empty());
    auto unjoined=seed;unjoined.tracks[0].clips[1].notes[0].label="e_g";check("example_alias_without_original_graph_join_is_ignored",detectMelodyneSplitTransitions(unjoined,{}).empty());
    check("competing_logical_bridge_hypotheses_are_not_both_accepted",detectMelodyneSplitTransitions(seed,{raw,raw}).empty());
    auto fifth=seed;auto extra=fifth.tracks[0].clips[0];extra.id="extra-owner";extra.startSeconds=.97;extra.durationSeconds=extra.sourceDurationSeconds=.1;extra.sourceTimeMap={{0,0},{.1,.1}};extra.notes[0].id="independent-voice";extra.notes[0].durationSeconds=.1;fifth.tracks[0].clips.push_back(extra);
    check("real_additional_voice_remains_protected",detectMelodyneSplitTransitions(fifth,{raw}).empty());
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

#pragma once
#include "../NativeConsonantMergeDetection.h"
#include "../SettingsComponent.h"
#include "../PianoRollComponent.h"
#include <iostream>
namespace hachi
{
inline bool runNativeConsonantMergeDetectionSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    for(int kind=0;kind<3;++kind)
    {
        const auto file=output.getChildFile(kind==0?"noise-source.wav":kind==1?"low-source.wav":"tone-source.wav");juce::AudioBuffer<float> pcm(1,48000);std::uint32_t random=7;double previous=0;
        for(int s=0;s<48000;++s)
        {
            random=random*1664525u+1013904223u;const auto white=double(random>>8)/16777216.*2-1;const auto high=white-previous;previous=white;const auto t=s/48000.;
            double sample=.09*std::sin(juce::MathConstants<double>::twoPi*330*t);
            if(t>=.2&&t<.28)sample=kind==0?.06*high:.09*std::sin(juce::MathConstants<double>::twoPi*(kind==1?173:7000)*t);
            pcm.setSample(0,s,float(sample));
        }
        auto stream=file.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;
    }
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;
    ClipData front;front.id="onset-owner";front.sourceFile=output.getChildFile("noise-source.wav");front.startSeconds=1.;front.durationSeconds=.1;front.sourceOffsetSeconds=.2;front.sourceDurationSeconds=.08;front.sourceTimeMap={{0,0},{.1,.08}};front.glideConnectedToNext=true;
    NoteData sh;sh.id="onset";sh.durationSeconds=.1;sh.connectedToNext=true;sh.sibilantMarkers={.08};for(int f=0;f<=20;++f)sh.contour.push_back({f*.005,0,0,false});front.notes={sh};
    ClipData back;back.id="vowel-owner";back.sourceFile=front.sourceFile;back.startSeconds=1.1;back.durationSeconds=.4;back.sourceOffsetSeconds=.28;back.sourceDurationSeconds=.3;back.sourceTimeMap={{0,0},{.4,.3}};back.glideConnectedFromPrevious=true;
    NoteData i;i.id="vowel";i.durationSeconds=.4;i.midiNote=i.sourceMidiCenter=64;i.connectedToPrevious=true;for(int f=0;f<=80;++f)i.contour.push_back({f*.005,0,0,true});back.notes={i};track.clips={front,back};seed.tracks={track};
    NativeSplitTransitionEvidence raw;raw.logicalId="original-join";raw.frontNoteId="onset";raw.backNoteId="vowel";raw.sourceObjectIdentity=raw.frontSourceObjectIdentity=raw.backSourceObjectIdentity="source-17";raw.precedingElementIdentity=raw.frontElementIdentity="element-21";raw.followingElementIdentity=raw.backElementIdentity="element-22";raw.joinsPitches=raw.joinsAmplitudes=raw.joinsPhases=true;
    const auto fingerprint=[](const ProjectData& p){ProjectModel m;m.resetDocument(p);return m.contentFingerprint();};
    auto disabled=seed;check("consonant_auto_merge_defaults_off_exact_noop",autoMergeMelodyneConsonants(disabled,{raw}).merged==0&&fingerprint(disabled)==fingerprint(seed));
    const auto candidates=detectMelodyneConsonantMerges(seed,{raw});check("explicit_join_UV_and_nonperiodic_sibilant_noise_yield_candidate",candidates.size()==1&&candidates[0].evidence.sibilantIdentified);
    auto enabled=seed;const auto report=autoMergeMelodyneConsonants(enabled,{raw},true);
    check("enabled_import_uses_same_durable_model_merge",report.merged==1&&enabled.tracks[0].clips.size()==1&&enabled.consonantMerges.size()==1&&enabled.nativeVirtualGroups.empty());
    check("no_virtual_or_transition_identity_is_fabricated_for_consonant",enabled.logicalTransitions.empty()&&enabled.nativeConnections.empty()&&enabled.tracks[0].clips[0].notes[0].pronunciationKind==PronunciationKind::normal);
    auto low=seed;for(auto& clip:low.tracks[0].clips)clip.sourceFile=output.getChildFile("low-source.wav");check("unvoiced_flag_on_low_periodic_audio_not_enough",detectMelodyneConsonantMerges(low,{raw}).empty());
    auto tone=seed;for(auto& clip:tone.tracks[0].clips)clip.sourceFile=output.getChildFile("tone-source.wav");check("high_frequency_whistle_not_a_sibilant",detectMelodyneConsonantMerges(tone,{raw}).empty());
    auto unknown=seed;unknown.tracks[0].clips[0].notes[0].contour.clear();check("missing_F0_analysis_not_positive_noise_proof",detectMelodyneConsonantMerges(unknown,{raw}).empty());
    auto breath=seed;breath.tracks[0].clips[0].notes[0].pronunciationKind=PronunciationKind::breath;check("explicit_breath_not_merged",detectMelodyneConsonantMerges(breath,{raw}).empty());
    check("protected_split_bridge_half_is_not_swallowed",detectMelodyneConsonantMerges(seed,{raw},{"onset"}).empty());
    auto label=seed;label.tracks[0].clips[0].notes[0].label="sh";check("sibilant_name_without_original_join_does_not_trigger",detectMelodyneConsonantMerges(label,{}).empty());
    auto wrong=raw;wrong.backSourceObjectIdentity="different";check("wrong_source_graph_identity_rejected",detectMelodyneConsonantMerges(seed,{wrong}).empty());
    check("competing_consonant_join_candidates_remain_unresolved",detectMelodyneConsonantMerges(seed,{raw,raw}).empty());
    I18n strings;juce::AudioDeviceManager devices;juce::PropertiesFile::Options options;options.applicationName="consonant-settings";juce::PropertiesFile props(output.getChildFile("prefs.xml"),options);props.clear();SettingsComponent settings(strings,devices,props,[]{});settings.selectPage(4);
    const auto find=[&](auto&& self,juce::Component& c,const juce::String& id)->juce::Component*{if(c.getComponentID()==id)return &c;for(int j=0;j<c.getNumChildComponents();++j)if(auto* found=self(self,*c.getChildComponent(j),id))return found;return nullptr;};
    const auto* toggle=dynamic_cast<juce::ToggleButton*>(find(find,settings,"settings-auto-merge-consonants"));check("real_import_checkbox_defaults_off",toggle&&!toggle->getToggleState());
    ProjectModel model;model.resetDocument(enabled);PianoRollComponent roll(model,strings);roll.setSize(1000,600);roll.refreshFromModel();const auto menu=roll.diagnosticEnabledNoteMenuIds("vowel");
    check("merged_note_menu_offers_exact_original_fragment_restore",std::find(menu.begin(),menu.end(),PianoRollComponent::restoreConsonantFragmentsItem)!=menu.end());
    roll.applyNoteMenuChoice("vowel",PianoRollComponent::restoreConsonantFragmentsItem,0,{});
    check("restore_menu_calls_archive_not_generic_split",model.snapshot().consonantMerges.empty()&&fingerprint(model.snapshot())==fingerprint(seed));
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

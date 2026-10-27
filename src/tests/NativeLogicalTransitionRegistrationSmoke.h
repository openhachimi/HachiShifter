#pragma once
#include "../NativeSplitTransitionDetection.h"
#include "../NativeLogicalTransitionIO.h"
#include "../NativeLogicalTransitionSource.h"
#include "../ProjectMaterialRegistrationFacts.h"
#include "../AudioEngine.h"
#include "../OttoMaterialIO.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
namespace hachi
{
inline bool runNativeLogicalTransitionRegistrationSmoke(const juce::File& output)
{
    using R=ProjectMaterialRegistration;using F=ProjectMaterialRegistrationFacts;
    const auto root=output.getChildFile("logical-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto source=root.getChildFile("bridge.wav");juce::AudioBuffer<float> pcm(1,48000);
    for(int i=0;i<pcm.getNumSamples();++i)pcm.setSample(0,i,.1f*std::sin(juce::MathConstants<double>::twoPi*173*i/48000.));
    {auto stream=source.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;}
    const auto hash=juce::SHA256(source).toHexString();ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id=i?"owner-back":"owner-front";clip.sourceFile=source;clip.startSeconds=i?.3:.2;clip.durationSeconds=clip.sourceDurationSeconds=.1;clip.sourceOffsetSeconds=i?.3:.2;clip.sourceTimeMap={{0,0},{.1,.1}};clip.gain=.8f;
        NoteData note;note.id=i?"back":"front";note.durationSeconds=.1;note.label="-";note.nativeUnpitched=true;note.consonantSeconds=.025;clip.notes={note};track.clips.push_back(clip);
    }
    data.tracks={track};NativeSplitTransitionCandidate candidate;auto& e=candidate.evidence;e.logicalId="ONE-bridge";e.frontNoteId="front";e.backNoteId="back";
    e.sourceObjectIdentity=e.frontSourceObjectIdentity=e.backSourceObjectIdentity="source-17";e.precedingElementIdentity=e.frontElementIdentity="element-21";e.followingElementIdentity=e.backElementIdentity="element-22";
    e.joinsPitches=e.joinsAmplitudes=e.joinsPhases=e.transitionIdentified=true;candidate.sourceFromSeconds=.2;candidate.sourceToSeconds=.4;candidate.middleSourceSeconds=.3;candidate.confidence=.9f;
    const auto music=ProjectModel::registrationMusicSourceFingerprint(data);auto annotated=data;
    check("annotation_without_virtual_optimization_creates_one_logical_identity",annotateMelodyneSplitTransitions(annotated,{candidate})==1&&annotated.logicalTransitions.size()==1&&annotated.nativeVirtualGroups.empty());
    check("repeat_annotation_is_noop",annotateMelodyneSplitTransitions(annotated,{candidate})==0);
    check("annotation_is_not_an_acoustic_music_source_edit",ProjectModel::registrationMusicSourceFingerprint(annotated)==music);
    bool requests=true;for(int i=0;i<2;++i){const auto a=AudioEngine::diagnosticNativeRequest(data.tracks[0].clips[std::size_t(i)],data.tracks[0]);const auto b=AudioEngine::diagnosticNativeRequest(annotated.tracks[0].clips[std::size_t(i)],annotated.tracks[0]);requests&=a.sourceMidi==b.sourceMidi&&a.targetMidi==b.targetMidi&&a.noteGain==b.noteGain&&a.targetDurationSeconds==b.targetDurationSeconds;}
    check("both_physical_half_requests_remain_exact",requests&&annotated.tracks[0].clips[0].notes[0].pronunciationKind==PronunciationKind::normal&&annotated.tracks[0].clips[1].notes[0].pronunciationKind==PronunciationKind::normal);
    const auto facts=F::derive(annotated,F::Policy::ConfirmedCarriedPlacements);
    if(!facts.ready)error=facts.diagnostics.joinIntoString(" | ");
    check("confirmed_registration_has_one_source_variant_one_logical_row",facts.ready&&facts.variants.size()==1&&facts.variants[0].rows.size()==1&&facts.variants[0].rows[0].members.size()==2);
    if(!facts.ready||facts.variants.empty()||facts.variants[0].rows.empty())return false;
    const auto& row=facts.variants[0].rows[0].annotation;
    check("logical_row_spans_both_halves_with_real_cut_not_PC",row.pronunciationKind==PronunciationKind::transition&&row.nativeTransition&&row.regionStartSeconds==.2&&row.regionEndSeconds==.4&&row.nativeTransition->boundarySourceSeconds==.3&&row.alignmentSeconds==.2&&row.fixedDurationSeconds==0);
    struct RestoreRoots{juce::StringArray roots;~RestoreRoots(){SampleSettings::setMaterialRoots(roots);}} restore{SampleSettings::materialRoots()};
    SampleSettings::setMaterialRoots({root.getFullPathName()});
    auto sidecar=annotated;juce::StringArray warnings;const auto converted=SampleSettings::convertMelodyneProject(sidecar,warnings,false);
    std::vector<SampleRegionSetting> importedRows;const auto manifest=SampleSettings::annotationFor(source);
    check("import_sidecar_and_explicit_registration_use_one_complete_transition",converted&&warnings.isEmpty()&&OttoMaterialIO::loadAudio(manifest,source,importedRows,error)&&importedRows.size()==1&&importedRows[0].pronunciationKind==PronunciationKind::transition&&importedRows[0].nativeTransition&&importedRows[0].nativeTransition->boundarySourceSeconds==.3&&importedRows[0].regionStartSeconds==.2&&importedRows[0].regionEndSeconds==.4);
    bool clockExact=true;for(int i=0;i<2;++i){const auto& a=annotated.tracks[0].clips[std::size_t(i)];const auto& b=sidecar.tracks[0].clips[std::size_t(i)];const auto beforeRequest=AudioEngine::diagnosticNativeRequest(a,annotated.tracks[0]);const auto afterRequest=AudioEngine::diagnosticNativeRequest(b,sidecar.tracks[0]);clockExact&=a.startSeconds==b.startSeconds&&a.sourceOffsetSeconds==b.sourceOffsetSeconds&&a.durationSeconds==b.durationSeconds&&beforeRequest.sourceMidi==afterRequest.sourceMidi&&beforeRequest.targetMidi==afterRequest.targetMidi&&beforeRequest.noteGain==afterRequest.noteGain&&a.sourceTimeMap.size()==b.sourceTimeMap.size();for(std::size_t j=0;clockExact&&j<a.sourceTimeMap.size();++j)clockExact&=a.sourceTimeMap[j].targetSeconds==b.sourceTimeMap[j].targetSeconds&&a.sourceTimeMap[j].sourceSeconds==b.sourceTimeMap[j].sourceSeconds;}
    check("sidecar_coalescing_leaves_both_original_render_requests_and_clocks_exact",clockExact&&sidecar.tracks[0].clips.size()==2&&sidecar.logicalTransitions.size()==1);
    SampleRegionSetting frontRow;frontRow.regionStartSeconds=.2;frontRow.regionEndSeconds=.3;SampleRegionSetting backRow;backRow.regionStartSeconds=.3;backRow.regionEndSeconds=.4;SampleRegionSetting unrelated;unrelated.regionStartSeconds=.5;unrelated.regionEndSeconds=.7;
    std::map<juce::String,std::vector<SampleRegionSetting>> fresh{{source.getFullPathName(),{frontRow,backRow,unrelated}}};warnings.clear();
    check("sidecar_only_replaces_proven_halves_and_keeps_unrelated_rows",coalesceNativeLogicalTransitionSourceRows(annotated,fresh,warnings)==1&&fresh.begin()->second.size()==2&&std::any_of(fresh.begin()->second.begin(),fresh.begin()->second.end(),[](const auto& r){return r.regionStartSeconds==.5&&r.regionEndSeconds==.7;}));
    check("repeat_sidecar_coalescing_does_not_duplicate_logical_row",coalesceNativeLogicalTransitionSourceRows(annotated,fresh,warnings)==0&&fresh.begin()->second.size()==2);
    auto plan=R::prepare(annotated,1,root,"registered-logical",facts.variants,ProjectModel::registrationMusicSourceFingerprint);
    if(!plan.ready)error=plan.diagnostics.joinIntoString(" | ");
    check("existing_registration_authority_accepts_one_transition_for_two_members",plan.ready&&plan.document.audio.size()==1&&plan.document.audio[0].rows.size()==1&&plan.bindings.size()==2);
    if(plan.ready)
    {
        ProjectData adopted;check("reference_only_candidate_preserves_original_half_music",R::makeCandidate(annotated,plan,ProjectModel::registrationMusicSourceFingerprint,adopted,error)&&ProjectModel::registrationMusicSourceFingerprint(adopted)==music);
        auto lease=R::stage(plan,error);check("ordinary_atomic_publication_writes_logical_OTTO",lease&&lease->publish(error));
        if(lease&&lease->published())
        {
            OttoMaterialIO::Document saved;check("cold_OTTO_has_one_complete_transition_row",OttoMaterialIO::loadDocument(plan.publishedFolder.getChildFile("otto.ini"),saved,error)&&saved.audio.size()==1&&saved.audio[0].rows.size()==1&&saved.audio[0].rows[0].nativeTransition&&saved.audio[0].rows[0].nativeTransition->boundarySourceSeconds==.3&&saved.audio[0].rows[0].pronunciationKind==PronunciationKind::transition);
            lease->retainAfterAdoption();
        }
    }
    ProjectModel model;model.resetDocument(annotated);const auto file=root.getChildFile("logical.hjpx");ProjectModel cold;
    check("logical_identity_cold_project_roundtrip_without_virtual_group",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().logicalTransitions.size()==1&&cold.snapshot().logicalTransitions[0].evidence.frontNoteId=="front"&&cold.snapshot().logicalTransitions[0].middleSourceSeconds==.3&&cold.snapshot().nativeVirtualGroups.empty());
    auto malformed=nativeLogicalTransitionTree(annotated.logicalTransitions[0]);malformed.setProperty("middle",.4,nullptr);NativeLogicalTransition decoded;
    check("logical_middle_cannot_be_outside_complete_source",!nativeLogicalTransitionFromTree(malformed,decoded,error));
    check("source_recording_never_modified_by_annotation_or_registration",juce::SHA256(source).toHexString()==hash);
    root.getChildFile("verification.txt").replaceWithText(log);std::cout<<"logical_fixture="<<root.getFullPathName()<<std::endl;return ok;
}
}

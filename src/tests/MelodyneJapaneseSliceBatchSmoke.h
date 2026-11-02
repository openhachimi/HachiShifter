#pragma once
#include "../MelodyneJapaneseSliceBatch.h"
#include <iostream>
namespace hachi
{
// Deterministic adapter/transaction tests; the injected decoder is not evidence
// of speech quality. The real-model smoke uses the same adapter separately.
inline bool runMelodyneJapaneseSliceBatchSmoke(const juce::File& output)
{
    using Batch=MelodyneJapaneseSliceBatch;using R=ProjectMaterialRegistration;
    const auto root=output.getChildFile("slice-contract-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String error,log;const auto check=[&](const char* name,bool pass){ok&=pass;log<<name<<"="<<(pass?"PASS":"FAIL")<<" | "<<error<<"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto file=root.getChildFile("abcd.wav");
    {juce::AudioBuffer<float> pcm(1,192000);for(int i=0;i<pcm.getNumSamples();++i)pcm.setSample(0,i,float(.05*(1+i/48000)));auto stream=file.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;}
    const auto sourceHash=juce::SHA256(file).toHexString();const auto models=root.getChildFile("models");models.getChildFile("asr").createDirectory();models.getChildFile("tifa").createDirectory();
    models.getChildFile("asr/model.onnx").replaceWithText("fixture only; never passed to ORT");models.getChildFile("asr/model.meta.json").replaceWithText("{}");models.getChildFile("asr/phoneme_vocab.json").replaceWithText("{\"ka\":1,\"a\":2,\"i\":3}");models.getChildFile("tifa/japanese_dict_full.txt").replaceWithText("ka k a\na a\ni i\nN N\n");
    ProjectData project;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;track.pitchAlgorithm=PitchAlgorithm::world;
    for(int owner=0;owner<2;++owner)
    {
        ClipData clip;clip.id=owner?"bcd-owner":"ab-owner";clip.sourceFile=file;clip.startSeconds=owner?5:0;clip.sourceOffsetSeconds=owner?1:0;clip.durationSeconds=clip.sourceDurationSeconds=owner?3:2;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        for(int i=0;i<(owner?3:2);++i){NoteData note;note.id=juce::String(owner)+"-"+juce::String(i);note.label="-";note.startSeconds=i;note.durationSeconds=1;clip.notes.push_back(note);}track.clips.push_back(clip);
    }project.tracks={track};ProjectModel immutable;immutable.resetDocument(project);const auto musicFingerprint=immutable.contentFingerprint();
    const auto facts=ProjectMaterialRegistrationFacts::derive(project,ProjectMaterialRegistrationFacts::Policy::ConfirmedCarriedPlacements);check("carried_ab_and_bcd_are_real_note_rows",facts.ready&&facts.variants.size()==2&&facts.variants[0].rows.size()==2&&facts.variants[1].rows.size()==3);if(!facts.ready)return false;
    Batch::Options options;options.modelRoot=models;options.workers=2;std::atomic_int calls{0};std::atomic_bool exactSlices{true};
    options.recognizeOverride=[&](const backend::AnnotationAudio& slice,backend::JapaneseAsrResult& decoded,juce::String&,const backend::AnnotationCancel& cancel)
    {if(cancel&&cancel())return false;++calls;if(!(slice.mono.getNumChannels()==1&&slice.mono.getNumSamples()==48000&&slice.duration==1.))exactSlices.store(false);decoded.tokens={"k","a"};return true;};
    Batch::Result result;check("note_slice_recognition_succeeds",Batch::recognize(project,facts.variants,options,result,error));
    check("same_b_in_ab_and_bcd_is_recognized_once",result.ready&&result.taskCount==4&&result.asrCalls==4&&result.reused==1&&calls==4);
    check("ASR_never_receives_the_whole_abcd_recording",exactSlices.load());
    bool syllables=true,geometry=true;for(std::size_t v=0;v<result.variants.size();++v)for(std::size_t r=0;r<result.variants[v].rows.size();++r){const auto& a=result.variants[v].rows[r];const auto& b=facts.variants[v].rows[r];syllables&=a.annotation.name=="ka";geometry&=a.key==b.key&&a.nextRowKey==b.nextRowKey&&a.annotation.regionStartSeconds==b.annotation.regionStartSeconds&&a.annotation.regionEndSeconds==b.annotation.regionEndSeconds&&a.annotation.alignmentSeconds==b.annotation.alignmentSeconds&&a.annotation.fixedDurationSeconds==b.annotation.fixedDurationSeconds;}
    check("dictionary_syllables_not_raw_phones_are_written",syllables);check("existing_cut_P_C_and_row_identities_are_unchanged",geometry);
    auto protectedFacts=facts.variants;protectedFacts[0].rows[0].annotation.name="USER_ALIAS";Batch::Result protectedResult;check("existing_authored_alias_is_not_overwritten",Batch::recognize(project,protectedFacts,options,protectedResult,error)&&protectedResult.variants[0].rows[0].annotation.name=="USER_ALIAS");
    const auto beforeCancel=calls.load();Batch::Result cancelled;check("cancel_before_inference_has_no_calls_or_candidate",!Batch::recognize(project,facts.variants,options,cancelled,error,[]{return true;})&&calls==beforeCancel&&!cancelled.ready);
    auto emptyOptions=options;emptyOptions.recognizeOverride=[](const auto&,auto&,juce::String& why,const auto&){why="No Japanese phonemes were recognized; no annotation was generated";return false;};Batch::Result noWords;check("all_unrecognized_slices_do_not_grant_publication",!Batch::recognize(project,facts.variants,emptyOptions,noWords,error)&&!noWords.ready);
    auto mixedOptions=options;mixedOptions.recognizeOverride=[](const backend::AnnotationAudio& slice,backend::JapaneseAsrResult& decoded,juce::String& why,const auto&){if(slice.mono.getSample(0,0)>.07f&&slice.mono.getSample(0,0)<.12f){why="No Japanese phonemes were recognized; no annotation was generated";return false;}decoded.tokens={"k","a"};return true;};Batch::Result mixed;check("one_unrecognized_transition_like_slice_does_not_abort_other_notes",Batch::recognize(project,facts.variants,mixedOptions,mixed,error)&&mixed.ready&&mixed.variants[0].rows[1].annotation.name=="-"&&mixed.variants[1].rows[0].annotation.name=="-"&&mixed.variants[0].rows[0].annotation.name=="ka");
    auto plan=R::prepare(project,0,root,"published-materials",result.variants,ProjectModel::registrationMusicSourceFingerprint);check("recognized_aliases_use_existing_registration_plan",plan.ready);auto lease=plan.ready?R::stage(plan,error):nullptr;check("stage_not_published",lease&&!plan.publishedFolder.exists());
    check("one_registration_publish_and_cold_OTTO_read",lease&&lease->publish(error)&&lease->release(error));OttoMaterialIO::Document cold;check("OTTO_reloads",OttoMaterialIO::loadDocument(plan.publishedFolder.getChildFile("otto.ini"),cold,error));int aliasCount=0;for(const auto& audio:cold.audio)for(const auto& row:audio.rows){aliasCount+=row.name=="ka";}check("published_OTTO_contains_all_five_syllable_occurrences",aliasCount==5);
    auto cancelledPlan=R::prepare(project,0,root,"cancelled-materials",result.variants,ProjectModel::registrationMusicSourceFingerprint);auto cancelledLease=R::stage(cancelledPlan,error);check("cancelled_registration_never_publishes_OTTO",cancelledLease&&!cancelledLease->publish(error,[]{return true;})&&!cancelledPlan.publishedFolder.exists());if(cancelledLease){juce::String why;cancelledLease->rollback(why);}
    check("source_PCM_and_original_project_are_unchanged",juce::SHA256(file).toHexString()==sourceHash&&immutable.contentFingerprint()==musicFingerprint);
    root.getChildFile("verification.txt").replaceWithText(log);output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}

#pragma once
#include "../JapaneseAnnotationBatch.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>

namespace hachi
{
inline bool runJapaneseAnnotationSafetySmoke(const juce::File& output, const juce::File& realAudio, const juce::File& models)
{
    const auto root=output.getChildFile("japanese-safety-"+juce::Uuid().toString());
    if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool passed){ok&=passed;log+=juce::String(name)+"="+(passed?"PASS":"FAIL")+"\n";std::cout<<name<<'='<<passed<<std::endl;};
    const auto original=juce::SHA256(realAudio).toHexString();
    JapaneseAutoAnnotation::PhoneSequence phones;
    const auto dictionary=models.getChildFile("tifa/japanese_dict_full.txt");
    check("exact_recognized_spelling_preserved",JapaneseAutoAnnotation::phonemesToSyllables({"re"},dictionary,phones,error)&&phones.syllables.size()==1&&phones.syllables[0].alias=="re");
    check("atomic_dictionary_cluster_combination",JapaneseAutoAnnotation::phonemesToSyllables({"ky","a"},dictionary,phones,error)&&phones.syllables.size()==1&&phones.syllables[0].alias=="kya");
    check("nasal_case_not_lowercased",JapaneseAutoAnnotation::phonemesToSyllables({"N"},dictionary,phones,error)&&phones.phones[0]=="N"&&phones.syllables[0].alias=="n");
    check("unsupported_fyu_never_invented",!JapaneseAutoAnnotation::phonemesToSyllables({"fyu"},dictionary,phones,error));
    check("atomic_sequence_saves_syllables_not_phones",JapaneseAutoAnnotation::phonemesToSyllables({"s","o","r","e"},dictionary,phones,error)&&phones.syllables.size()==2&&phones.syllables[0].alias=="so"&&phones.syllables[1].alias=="re");
    backend::AnnotationAudio decoded;
    check("cancel_before_decode",!backend::AnnotationAudio::load(realAudio,decoded,error,[]{return true;}));
    check("real_recording_decode",backend::AnnotationAudio::load(realAudio,decoded,error));
    backend::JapaneseAsrResult asr;backend::OrtExecutionConfig execution;execution.requested=backend::InferenceBackend::cpu;
    check("missing_model_pack_explicit_error",!backend::JapanesePhonemeAsr::transcribe(decoded,root.getChildFile("missing"),execution,asr,error)&&error.containsIgnoreCase("incomplete"));
    const auto bank=root.getChildFile("bank");bank.createDirectory();const auto copied=bank.getChildFile(realAudio.getFileName());
    check("isolated_real_copy",realAudio.copyFileTo(copied));
    JapaneseAutoAnnotation::Result generated;generated.audio=copied;generated.sourceFingerprint=original;generated.audioId="ja_safety";
    SampleRegionSetting row;row.name="a";row.materialAudioId=generated.audioId;row.materialRegionId="ja_r1";
    row.regionEndSeconds=juce::jmin(.1,decoded.duration);row.alignmentSeconds=0;row.role=NativeSegmentRole::vowel;
    JapaneseAnnotationBatch::Plan plan;plan.root=bank;plan.manifest=bank.getChildFile("otto.ini");
    check("shared_publication_candidate",OttoMaterialIO::replaceAudio(plan.candidate,plan.manifest,copied,{row},error,generated.audioId));
    plan.results={generated};plan.ready=true;
    check("cancel_before_publication_keeps_manifest_absent",!JapaneseAnnotationBatch::publish(plan,error,[]{return true;})&&!plan.manifest.exists());
    check("actual_shared_writer_publication",JapaneseAnnotationBatch::publish(plan,error)&&plan.manifest.existsAsFile());
    check("repeat_publish_requires_new_preview",!JapaneseAnnotationBatch::publish(plan,error));
    OttoMaterialIO::Document document;check("read_current_revision",OttoMaterialIO::loadDocument(plan.manifest,document,error));
    plan.candidate=document;plan.ready=true;
    plan.manifest.appendText("\n# external edit\n");
    check("changed_manifest_rejected_without_overwrite",!JapaneseAnnotationBatch::publish(plan,error)&&plan.manifest.loadFileAsString().contains("external edit"));
    check("fresh_source_snapshot",OttoMaterialIO::loadDocument(plan.manifest,document,error));
    plan.candidate=document;plan.ready=true;
    auto bytes=juce::MemoryBlock{};copied.loadFileAsData(bytes);auto changed=bytes;changed.append("X",1);copied.replaceWithData(changed.getData(),changed.getSize());
    const auto manifestBefore=juce::SHA256(plan.manifest).toHexString();
    check("changed_recording_rejected_without_manifest_write",!JapaneseAnnotationBatch::publish(plan,error)&&juce::SHA256(plan.manifest).toHexString()==manifestBefore);
    if(juce::SystemStats::getEnvironmentVariable("HACHI_ANNOTATION_BATCH_NN",{})=="1")
    {
        const auto batchRoot=root.getChildFile("actual-folder");batchRoot.createDirectory();batchRoot.getChildFile("nested").createDirectory();
        check("folder_real_copies",realAudio.copyFileTo(batchRoot.getChildFile("first.wav"))&&realAudio.copyFileTo(batchRoot.getChildFile("nested/second.wav")));
        {juce::AudioBuffer<float> silence(1,1600);silence.clear();auto stream=batchRoot.getChildFile("silent.wav").createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(stream?wav.createWriterFor(stream.release(),16000,1,16,{},0):nullptr);check("folder_explicit_silence_failure_fixture",writer&&writer->writeFromAudioSampleBuffer(silence,0,1600));}
        JapaneseAnnotationBatch::Input input;input.root=batchRoot;input.modelRoot=models;input.execution=execution;input.execution.intraOpThreads=2;
        JapaneseAnnotationBatch::Plan batch;
        const bool previewed=JapaneseAnnotationBatch::preview(input,batch,error);
        check("actual_recursive_folder_native_preview",previewed&&batch.results.size()==2&&batch.failures.size()==1);
        check("folder_preview_writes_nothing",!batchRoot.getChildFile("otto.ini").exists());
        if(previewed)
        {
            check("same_PCM_different_physical_paths_keep_distinct_variants",batch.results[0].audioId!=batch.results[1].audioId);
            check("folder_explicit_success_only_publication",JapaneseAnnotationBatch::publish(batch,error));
            OttoMaterialIO::Document cold;
            check("folder_cold_document_only_successful_files",OttoMaterialIO::loadDocument(batchRoot.getChildFile("otto.ini"),cold,error)&&cold.audio.size()==2);
        }
    }
    check("original_real_recording_unchanged",juce::SHA256(realAudio).toHexString()==original);
    const bool written=root.getChildFile("safety.txt").replaceWithText(log+"last_diagnostic="+error+"\n");
    return ok&&written;
}
}

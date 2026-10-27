#pragma once
#include "../JapaneseAnnotationBatch.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>

namespace hachi
{
// Native mode contract. NN cases require actual caller-supplied audio/models;
// no synthetic recording is used to stand in for ASR/TIFA inference acceptance.
inline bool runJapaneseInferenceModeSmoke(const juce::File& output,const juce::File& realAudio={},const juce::File& models={})
{
    using Auto=JapaneseAutoAnnotation;using Mode=Auto::InferenceMode;
    const auto root=output.getChildFile("japanese-modes-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<"|"<<error<<std::endl;error.clear();};
    const auto finish=[&]{return root.getChildFile("japanese-inference-modes.txt").replaceWithText(log)&&ok;};
    Auto::ModePlan stages;
    check("full_mode_always_runs_both_stages",Auto::planMode(Mode::recognizeAndAlign,stages,error)&&stages.runRecognition&&stages.runAlignment&&stages.canPublish);
    check("recognize_only_never_grants_alignment_or_publication",Auto::planMode(Mode::recognizeOnly,stages,error)&&stages.runRecognition&&!stages.runAlignment&&!stages.canPublish);
    check("align_only_never_runs_recognition",Auto::planMode(Mode::alignOnly,stages,error)&&!stages.runRecognition&&stages.runAlignment&&stages.canPublish);
    check("unknown_mode_rejected",!Auto::planMode(static_cast<Mode>(99),stages,error));
    const auto dictionary=root.getChildFile("dict.txt");check("dictionary_fixture",dictionary.replaceWithText("so s o\nre r e\n"));Auto::PhoneSequence normalized;
    check("raw_phones_normalize_to_syllables_so_re",Auto::phonemesToSyllables({"s","o","r","e"},dictionary,normalized,error)
        &&normalized.syllables.size()==2&&normalized.syllables[0].alias=="so"&&normalized.syllables[1].alias=="re");
    Auto::Result sentinel;sentinel.audioId="unchanged";backend::OrtExecutionConfig cpu;cpu.requested=backend::InferenceBackend::cpu;cpu.deviceIndex=-1;cpu.intraOpThreads=2;
    int callbacks=0;
    check("blank_align_only_rejects_before_audio_or_models",!Auto::infer(root.getChildFile("missing.wav"),root,cpu,Mode::alignOnly,{},sentinel,error,{}, {}, {},[&](const auto&){++callbacks;})
        &&callbacks==0&&sentinel.audioId=="unchanged"&&error.containsIgnoreCase("non-empty"));
    JapaneseAnnotationBatch::Plan forged;forged.mode=Mode::recognizeOnly;forged.ready=true;forged.manifest=root.getChildFile("never-written.ini");forged.results.push_back(sentinel);
    check("recognize_only_publish_guard_cannot_be_bypassed_by_ready_flag",!JapaneseAnnotationBatch::publish(forged,error)&&!forged.manifest.exists());
    if(!realAudio.existsAsFile()||!models.isDirectory())
    {log+="NN cases NOT RUN: actual recording/model root not supplied.\n";return finish();}
    const auto originalHash=juce::SHA256(realAudio).toHexString();const auto bank=root.getChildFile("bank");bank.createDirectory();const auto copied=bank.getChildFile(realAudio.getFileName());
    check("actual_audio_isolated_copy",realAudio.copyFileTo(copied));
    const auto asrOnly=root.getChildFile("asr-only-models");asrOnly.createDirectory();asrOnly.getChildFile("tifa").createDirectory();
    check("asr_model_copy_without_tifa_graphs",models.getChildFile("asr").copyDirectoryTo(asrOnly.getChildFile("asr"))
        &&models.getChildFile("tifa/japanese_dict_full.txt").copyFileTo(asrOnly.getChildFile("tifa/japanese_dict_full.txt")));
    Auto::Result textOnly;
    const bool textInferred=Auto::infer(copied,asrOnly,cpu,Mode::recognizeOnly,{"manual-text-must-be-ignored"},textOnly,error);
    check("actual_recognition_succeeds_without_TIFA_graphs",textInferred&&!textOnly.normalizedSyllables.isEmpty()
        &&textOnly.audioLoadCount==1&&textOnly.asrCallCount==1&&textOnly.alignmentCallCount==0
        &&textOnly.alignmentBackend.isEmpty()&&textOnly.alignment.phones.empty()&&textOnly.rows.empty()&&!textOnly.ready
        &&!textOnly.recognizedTokens.isEmpty()&&textOnly.manualTokens.isEmpty());
    if(!textInferred)return finish();
    const auto alignOnly=root.getChildFile("align-only-models");alignOnly.createDirectory();
    check("tifa_model_copy_without_ASR_graphs",models.getChildFile("tifa").copyDirectoryTo(alignOnly.getChildFile("tifa")));
    Auto::Result aligned;
    check("actual_manual_alignment_succeeds_without_ASR_graphs",Auto::infer(copied,alignOnly,cpu,Mode::alignOnly,textOnly.normalizedSyllables,aligned,error)
        &&aligned.ready&&!aligned.rows.empty()&&aligned.audioLoadCount==1&&aligned.asrCallCount==0&&aligned.alignmentCallCount==1
        &&aligned.asrBackend.isEmpty()&&aligned.asrModelFingerprint.isEmpty()&&aligned.recognizedTokens.isEmpty()
        &&aligned.manualTokens==textOnly.normalizedSyllables&&!aligned.alignmentBackend.isEmpty());
    Auto::Result full;
    check("actual_full_mode_ignores_stale_manual_text",Auto::infer(copied,models,cpu,Mode::recognizeAndAlign,{"unsupported-stale-manual"},full,error)
        &&full.ready&&!full.rows.empty()&&full.asrCallCount==1&&full.alignmentCallCount==1&&full.manualTokens.isEmpty()
        &&!full.normalizedSyllables.isEmpty()&&!full.recognizedTokens.isEmpty());
    Auto::Result failedSentinel;failedSentinel.audioId="retained-output";Auto::Result recognizedBeforeFailure,stageCounts;
    const bool missingAlignment=Auto::infer(copied,asrOnly,cpu,Mode::recognizeAndAlign,{},failedSentinel,error,{}, {},
        [&](const auto& stage){recognizedBeforeFailure=stage;},[&](const auto& stage){stageCounts=stage;});
    check("full_TIFA_failure_keeps_normalized_stage_and_false_output_atomic",!missingAlignment&&failedSentinel.audioId=="retained-output"
        &&!recognizedBeforeFailure.normalizedSyllables.isEmpty()&&!recognizedBeforeFailure.ready&&recognizedBeforeFailure.rows.empty()
        &&stageCounts.audioLoadCount==1&&stageCounts.asrCallCount==1&&stageCounts.alignmentCallCount==1);
    JapaneseAnnotationBatch::Input input;input.root=bank;input.modelRoot=asrOnly;input.execution=cpu;input.mode=Mode::recognizeOnly;
    input.correctedTokens={"ignored-recognition-mode-manual"};input.focusAudio=copied;JapaneseAnnotationBatch::Plan plan;
    check("batch_recognition_is_text_only_with_explicit_focus",JapaneseAnnotationBatch::preview(input,plan,error)&&!plan.ready&&plan.results.empty()
        &&plan.recognitions.size()==1&&plan.candidate.audio.empty()&&plan.focusAudio==copied&&plan.asrCallCount==1&&plan.alignmentCallCount==0&&!plan.manifest.exists());
    input.mode=Mode::recognizeAndAlign;input.correctedTokens.clear();
    check("batch_backfills_recognition_despite_alignment_failure",JapaneseAnnotationBatch::preview(input,plan,error)&&!plan.ready&&plan.results.empty()
        &&plan.recognitions.size()==1&&!plan.failures.isEmpty()&&plan.alignmentCallCount==1);
    input.mode=Mode::alignOnly;input.modelRoot=alignOnly;input.recordings={copied};input.manualInputs[copied.getFullPathName()]=textOnly.normalizedSyllables;
    input.manualInputs[bank.getChildFile("not-selected.wav").getFullPathName()]={"must-not-be-reused"};
    check("manual_map_is_exact_selected_file_only",JapaneseAnnotationBatch::preview(input,plan,error)&&plan.ready&&plan.results.size()==1
        &&plan.results[0].audio==copied&&plan.results[0].manualTokens==textOnly.normalizedSyllables&&plan.asrCallCount==0&&plan.alignmentCallCount==1);
    check("preview_does_not_publish_or_touch_audio",!bank.getChildFile("otto.ini").exists()&&juce::SHA256(copied).toHexString()==originalHash&&juce::SHA256(realAudio).toHexString()==originalHash);
    return finish();
}
}

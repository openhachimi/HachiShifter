#pragma once
// Included only by MainJapaneseAutoAnnotation.cpp. This exercises actual
// inference/confirm/modal/human-Save buttons with real ASR+TIFA model packs.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
namespace hachi
{
void MainComponent::diagnosticJapaneseAutoAnnotationUi(const juce::File& output,const juce::File& modelRoot,
    const juce::File& recording,std::function<void(bool)> finished)
{
    struct Run final:juce::Timer,std::enable_shared_from_this<Run>
    {
        std::shared_ptr<Run> self;std::function<void()> tick;std::function<void(bool)> done;
        juce::File base,root,source;juce::String log,sourceHash,initialManifestHash,newId,newRegion;
        juce::Component::SafePointer<OtoWaveformEditorComponent> human,otherAuthor;
        JapaneseAutoAnnotationDialog::Input input;ProjectData seed;SampleRegionSetting humanRow;
        juce::int64 musicHash=0,beforeFolder=0;int stage=0,pendingBefore=0;double deadline=0,newEnd=0;
        std::uint64_t firedSerial=0,awaitSerial=0;juce::String lastFiredButton,awaitButton;
        bool ok=true;
        void check(const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+"\n";}
        static juce::Component* component(juce::Component* parent,const juce::String& id)
        {if(!parent)return nullptr;if(parent->getComponentID()==id)return parent;for(int i=0;i<parent->getNumChildComponents();++i)if(auto* found=component(parent->getChildComponent(i),id))return found;return nullptr;}
        bool queueButton(juce::Button* button,const juce::String& id)
        {
            if(!button||!button->isEnabled())return false;
            awaitButton=id;awaitSerial=firedSerial+1;
            if(!bool(button->getProperties().getWithDefault("mainJapaneseSmokeObserved",false)))
            {
                const auto actual=button->onClick;const auto weak=weak_from_this();
                button->onClick=[weak,actual,id]
                {if(auto run=weak.lock()){++run->firedSerial;run->lastFiredButton=id;run->log+="actual_button_dispatched="+id+" serial="+juce::String(juce::int64(run->firedSerial))+"\n";}if(actual)actual();};
                button->getProperties().set("mainJapaneseSmokeObserved",true);
            }
            button->triggerClick();return true;
        }
        bool press(juce::Component* parent,const juce::String& id)
        {return queueButton(dynamic_cast<juce::Button*>(component(parent,id)),id);}
        bool pressModal(const juce::String& text)
        {
            auto* modal=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());if(!modal)return false;
            for(int i=0;i<modal->getNumChildComponents();++i)if(auto* button=dynamic_cast<juce::TextButton*>(modal->getChildComponent(i));button&&button->getButtonText()==text)
            {return queueButton(button,"modal:"+text);}return false;
        }
        bool type(juce::Component* parent,const juce::String& id,const juce::String& value)
        {auto* field=dynamic_cast<juce::TextEditor*>(component(parent,id));if(!field||!field->isEnabled())return false;field->setText(value,true);return true;}
        void timerCallback()override{auto keep=shared_from_this();if(tick)tick();}
        void finish(){auto keep=shared_from_this();stopTimer();tick={};const bool written=base.getChildFile("verification.txt").replaceWithText(log);std::cout<<"main_japanese_annotation="<<ok<<"|fixture="<<base.getFullPathName()<<std::endl;if(!ok)std::cerr<<log<<std::endl;auto callback=std::move(done);self.reset();if(callback)callback(ok&&written);}
    };
    stopTimer();audio.stop();auto run=std::make_shared<Run>();run->self=run;run->done=std::move(finished);run->pendingBefore=pendingNativeAnalyses;
    run->base=output.getChildFile("main-japanese-annotation-"+juce::Uuid().toString());run->root=run->base.getChildFile("material");run->source=run->root.getChildFile(recording.getFileName());
    juce::String error;
    run->check("real_native_model_recording_fixture",recording.existsAsFile()&&modelRoot.isDirectory()&&run->root.createDirectory().wasOk()&&recording.copyFileTo(run->source));if(!run->ok){run->finish();return;}
    run->sourceHash=juce::SHA256(run->source).toHexString();juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(run->source));
    if(!reader||reader->sampleRate<=0){run->check("recording_header",false);run->finish();return;}const auto duration=reader->lengthInSamples/reader->sampleRate;
    SampleRegionSetting author;author.materialAudioId="existing-authored-variant";author.materialRegionId="existing-authored-row";author.name="manual-keep";author.regionStartSeconds=author.alignmentSeconds=0;author.regionEndSeconds=duration;author.role=NativeSegmentRole::vowel;
    OttoMaterialIO::Document original;original.audio={{author.materialAudioId,run->source.getFileName(),{author}}};run->check("original_author_registration",OttoMaterialIO::saveDocument(run->root.getChildFile("otto.ini"),original,error));run->initialManifestHash=juce::SHA256(run->root.getChildFile("otto.ini")).toHexString();
    run->seed.bpm=137;TrackData track;track.id="jp-existing-music";track.trackType=TrackType::hachi;track.compose=true;
    ClipData clip;clip.id="jp-existing-clip";clip.sourceFile=run->source;clip.durationSeconds=clip.sourceDurationSeconds=duration;clip.sourceTimeMap={{0,0},{duration,duration}};
    NoteData note;note.id="old-bound";note.label="unchanged";note.durationSeconds=duration;note.nativeUnpitched=true;note.materialAudioId=author.materialAudioId;note.materialRegionId=author.materialRegionId;note.materialSourceStartSeconds=note.nativeSourceStartSeconds=0;note.materialSourceEndSeconds=note.nativeSourceEndSeconds=duration;
    auto bare=note;bare.id="bare-native";bare.label="bare-keep";bare.materialAudioId.clear();bare.materialRegionId.clear();clip.notes={note,bare};track.clips={clip};run->seed.tracks={track};project.resetDocument(run->seed);setAssetManagerVisible(true);
    openMaterialEditor(run->source,run->root.getChildFile("otto.ini"),false,{},author.materialAudioId);
    for(const auto& editor:otoPreviews)if(editor&&editor->materialSourceAudio()==run->source&&editor->materialAudioVariantId()==author.materialAudioId)run->human=editor;
    run->check("actual_original_human_source_editor",bool(run->human));if(!run->human){run->finish();return;}
    run->human->selectMaterialPronunciation(author.materialRegionId,false);run->human->diagnosticMaterialAliasText("dirty author");run->check("dirty_origin_refuses_annotation",!run->human->requestJapaneseAutoAnnotation());run->human->diagnosticMaterialAliasText(author.name);
    run->check("clean_origin_opens_Main_wizard",run->human->requestJapaneseAutoAnnotation());auto* ui=japaneseAutoAnnotationState?japaneseAutoAnnotationState->wizard.getComponent():nullptr;
    run->check("exact_origin_pointer_captured",ui&&japaneseAutoAnnotationState->originEditor.getComponent()==run->human.getComponent());if(!ui){run->finish();return;}
    run->input.selectedFolderRoot=run->root;run->input.currentAudio=run->source;run->input.modelRoot=modelRoot;run->check("visible_input",ui->setInput(run->input,error));run->check("actual_full_inference_button",run->press(ui,"japanese.infer.full"));
    run->deadline=juce::Time::getMillisecondCounterHiRes()+600000;juce::Component::SafePointer<MainComponent> safe(this);
    run->tick=[safe,run]
    {
        if(!safe){run->check("Main_lifetime",false);run->finish();return;}auto* ui=safe->japaneseAutoAnnotationState?safe->japaneseAutoAnnotationState->wizard.getComponent():nullptr;
        if(!ui||juce::Time::getMillisecondCounterHiRes()>run->deadline){run->check("actual_workflow_completion",false);run->log+="stage="+juce::String(run->stage)+" actualButton="+run->lastFiredButton+" awaitedButton="+run->awaitButton+" fired="+juce::String(juce::int64(run->firedSerial))+" awaited="+juce::String(juce::int64(run->awaitSerial))+" modal="+(juce::Component::getCurrentlyModalComponent()?juce::Component::getCurrentlyModalComponent()->getName():juce::String("none"))+"\n";if(ui){run->log+=ui->reportText();ui->cancel();}safe->pendingNativeAnalyses=run->pendingBefore;run->finish();return;}
        if(run->firedSerial<run->awaitSerial||run->lastFiredButton!=run->awaitButton||ui->busy())return;
        juce::String error;const auto manifest=run->root.getChildFile("otto.ini");
        if(run->stage==0)
        {
            const auto* plan=ui->previewPlan();run->check("full_mode_actual_ASR_and_TIFA",plan&&plan->asrCallCount==1&&plan->alignmentCallCount==1&&!plan->results.empty()&&ui->manualInputText().isNotEmpty());
            if(!plan){run->log+=ui->reportText();safe->pendingNativeAnalyses=run->pendingBefore;run->finish();return;}
            run->check("actual_confirm_button_opens_modal",run->press(ui,"japanese.confirm"));run->stage=1;
        }
        else if(run->stage==1){if(!run->pressModal(safe->strings.text("jaAuto.cancel")))return;run->check("actual_modal_cancel_button",true);run->stage=2;}
        else if(run->stage==2)
        {if(juce::Component::getCurrentlyModalComponent()!=nullptr)return;run->check("cancelled_confirmation_writes_nothing",juce::SHA256(manifest).toHexString()==run->initialManifestHash&&!safe->communityDatasetImportBlocksProjectEdits());run->check("actual_recognition_only_button",run->press(ui,"japanese.infer.recognize"));run->stage=3;}
        else if(run->stage==3)
        {
            auto* confirm=dynamic_cast<juce::Button*>(Run::component(ui,"japanese.confirm"));
            const bool notReady=!ui->previewReady(),confirmExists=confirm!=nullptr,confirmDisabled=confirm&&!confirm->isEnabled();
            const bool manualPresent=ui->manualInputText().isNotEmpty(),zeroTifa=ui->reportText().contains("TIFA calls=0");
            const bool passed=notReady&&confirmExists&&confirmDisabled&&manualPresent&&zeroTifa;
            if(!passed)
            {
                run->log+="recognition_predicates notReady="+juce::String(int(notReady))+" confirmExists="+juce::String(int(confirmExists))+" confirmEnabled="+juce::String(confirm?int(confirm->isEnabled()):-1)
                    +" manualNonempty="+juce::String(int(manualPresent))+" zeroTifa="+juce::String(int(zeroTifa))+" oneAsr="+juce::String(int(ui->reportText().contains("ASR calls=1")))
                    +" actualButton="+run->lastFiredButton+" fired="+juce::String(juce::int64(run->firedSerial))+" awaited="+juce::String(juce::int64(run->awaitSerial))
                    +" modal="+(juce::Component::getCurrentlyModalComponent()?juce::Component::getCurrentlyModalComponent()->getName():juce::String("none"))+"\nmanualText="+juce::JSON::toString(juce::var(ui->manualInputText()))+"\nreportText_BEGIN\n"+ui->reportText()+"\nreportText_END\n";
            }
            run->check("recognition_only_is_not_publishable",passed);
            ui->setManualInputText("so re",true);run->check("actual_manual_alignment_button",run->press(ui,"japanese.infer.align"));run->stage=4;
        }
        else if(run->stage==4)
        {
            const auto* plan=ui->previewPlan();run->check("manual_alignment_reuses_real_TIFA_without_ASR",plan&&plan->asrCallCount==0&&plan->alignmentCallCount==1&&!plan->results.empty()&&plan->focusAudio==run->source);
            if(!plan||plan->results.empty()){run->log+=ui->reportText();safe->pendingNativeAnalyses=run->pendingBefore;run->finish();return;}
            run->newId=plan->results[0].audioId;run->humanRow=plan->results[0].rows.back();run->newRegion=run->humanRow.materialRegionId;
            // Existing bare-native note exactly matches a generated source row:
            // physical-path legacy propagation would falsely bind it on Save.
            const auto& first=plan->results[0].rows.front();run->seed.tracks[0].clips[0].notes[1].nativeSourceStartSeconds=first.regionStartSeconds;run->seed.tracks[0].clips[0].notes[1].nativeSourceEndSeconds=first.regionEndSeconds;
            auto invalid=run->seed;invalid.tracks[0].clips[0].notes[0].materialAudioId=run->newId;safe->project.resetDocument(invalid);
            run->check("secondary_new_ID_in_use_guard",!ui->publishConfirmed(error)&&error.isNotEmpty());safe->project.resetDocument(run->seed);run->musicHash=safe->project.contentFingerprint();
            safe->pendingNativeAnalyses=run->pendingBefore+1; // counter-only regression, no fake inference backend
            run->check("actual_confirm_button_with_unbound_native_and_pending_counter",run->press(ui,"japanese.confirm"));run->stage=5;
        }
        else if(run->stage==5){if(!run->pressModal(safe->strings.text("jaAuto.publish")))return;run->check("actual_publish_modal_affirmative_button",true);run->stage=6;}
        else if(run->stage==6)
        {
            safe->pendingNativeAnalyses=run->pendingBefore;
            run->check("atomic_commit_and_gate_released_before_human_Save",manifest.existsAsFile()&&!safe->communityDatasetImportBlocksProjectEdits()&&safe->isEnabled());
            run->check("SAME_origin_editor_retargets_exact_new_variant",run->human&&run->human->materialAudioVariantId()==run->newId&&safe->japaneseAutoAnnotationState->originEditor==run->human.getComponent());
            run->check("append_does_not_rebind_or_modify_existing_music",safe->project.contentFingerprint()==run->musicHash);
            if(!run->human||run->human->materialAudioVariantId()!=run->newId){run->log+=ui->reportText();run->finish();return;}
            // Old-ID lookup must open a distinct old-ID editor after rekeying;
            // never accidentally return the now-new-ID human window.
            safe->openMaterialEditor(run->source,manifest,false,{},"existing-authored-variant");
            for(const auto& editor:safe->otoPreviews)if(editor&&editor->materialSourceAudio()==run->source&&editor->materialAudioVariantId()=="existing-authored-variant")run->otherAuthor=editor;
            run->check("window_registry_old_ID_does_not_reuse_new_editor",run->otherAuthor&&run->otherAuthor.getComponent()!=run->human.getComponent());
            run->check("generated_row_visible_in_original_human_editor",run->human->selectMaterialPronunciation(run->newRegion,false));
            const auto room=run->humanRow.regionEndSeconds-std::max(run->humanRow.alignmentSeconds,run->humanRow.regionStartSeconds+run->humanRow.fixedDurationSeconds);
            run->newEnd=run->humanRow.regionEndSeconds-std::min(.00025,std::max(0.0,room)*.1);
            run->check("actual_human_alias_and_boundary_fields",room>0&&run->type(run->human.getComponent(),"material.overview.alias","manual-corrected")&&run->type(run->human.getComponent(),"material.overview.sourceEnd",juce::String(run->newEnd*1000,9)));
            run->check("actual_human_source_Save_button",run->press(run->human.getComponent(),"material.overview.save"));run->stage=7;
        }
        else if(run->stage==7)
        {
            OttoMaterialIO::Document cold;const bool loaded=OttoMaterialIO::loadDocument(manifest,cold,error);const OttoMaterialIO::Audio* section=nullptr;for(const auto& item:cold.audio)if(item.id==run->newId)section=&item;
            const SampleRegionSetting* changed=nullptr;if(section)for(const auto& row:section->rows)if(row.materialRegionId==run->newRegion)changed=&row;
            run->check("human_alias_boundary_Save_cold_OTTO",loaded&&changed&&changed->name=="manual-corrected"&&std::abs(changed->regionEndSeconds-run->newEnd)<1.e-10);
            run->check("human_Save_foreign_and_unbound_music_fingerprint_unchanged",safe->project.contentFingerprint()==run->musicHash);
            if(!changed){run->log+="human_Save_error_or_stale_session\n";run->finish();return;}
            run->humanRow=*changed;auto data=safe->project.snapshot();NoteData placed;placed.id="new-bound";placed.label=changed->name;placed.materialLabelFollowsAlias=true;placed.materialBoundAlias=changed->name;placed.materialAliasFollowsPrimary=true;placed.materialAudioId=changed->materialAudioId;placed.materialRegionId=changed->materialRegionId;placed.startSeconds=changed->regionStartSeconds;placed.durationSeconds=changed->regionEndSeconds-changed->regionStartSeconds;placed.materialSourceStartSeconds=placed.nativeSourceStartSeconds=changed->regionStartSeconds;placed.materialSourceEndSeconds=placed.nativeSourceEndSeconds=changed->regionEndSeconds;placed.nativeSegments=SampleSettings::nativeSegmentsFor(*changed);placed.nativePronunciationEdges=changed->nativePronunciationEdges;placed.consonantSeconds=changed->fixedDurationSeconds;placed.utauPreutteranceOverrideEnabled=true;placed.utauPreutteranceSeconds=changed->alignmentSeconds-changed->regionStartSeconds;placed.utauOverlapOverrideEnabled=true;placed.utauOverlapSeconds=changed->overlapSeconds;placed.nativeUnpitched=changed->nativeUnpitched;data.tracks[0].clips[0].notes.push_back(placed);safe->project.resetDocument(data);
            run->check("referenced_generated_variant_human_alias_field",run->type(run->human.getComponent(),"material.overview.alias","used-variant-edited"));run->check("actual_Save_on_referenced_generated_variant",run->press(run->human.getComponent(),"material.overview.save"));run->stage=8;
        }
        else if(run->stage==8)
        {
            auto data=safe->project.snapshot();const auto& notes=data.tracks[0].clips[0].notes;const auto used=std::find_if(notes.begin(),notes.end(),[](const auto& note){return note.id=="new-bound";});run->check("scoped_source_Save_updates_only_exact_referenced_variant",used!=notes.end()&&used->label=="used-variant-edited");
            std::erase_if(data.tracks[0].clips[0].notes,[](const auto& note){return note.id=="new-bound";});ProjectModel foreign;foreign.resetDocument(data);run->check("scope_preserves_old_bound_and_bare_notes_no_unresolved_or_autobind",foreign.contentFingerprint()==run->musicHash);
            auto rows=run->human->diagnosticMaterialRows();auto wrong=rows;wrong.front().materialAudioId="foreign-id";
            run->check("typed_scope_rejects_foreign_publication_rows",safe->project.applySourceSettingsWithReport(run->source,wrong,false,run->newId).rejected);
            for(auto& row:rows)if(row.materialRegionId==run->newRegion){row.regionEndSeconds-=.0001;break;}
            run->check("typed_scope_rejects_shared_clock_boundary_retime",safe->project.applySourceSettingsWithReport(run->source,rows,false,run->newId).rejected);
            safe->japaneseAutoAnnotationWindows.clear();run->check("folder_menu_actual_host_entry",safe->assetManager->diagnosticRequestJapaneseAutoAnnotation(run->root));ui=safe->japaneseAutoAnnotationState->wizard.getComponent();run->input.currentAudio=juce::File{};run->check("folder_visible_input",ui&&ui->setInput(run->input,error));run->beforeFolder=safe->project.contentFingerprint();run->check("folder_actual_full_button",run->press(ui,"japanese.infer.full"));run->stage=9;
        }
        else if(run->stage==9)
        {const auto* plan=ui->previewPlan();run->check("folder_ready_with_explicit_visible_recording",plan&&plan->focusAudio==run->source&&!plan->results.empty());if(!plan||plan->results.empty()){run->log+=ui->reportText();run->finish();return;}run->newId=plan->results[0].audioId;run->check("folder_actual_confirm_button",run->press(ui,"japanese.confirm"));run->stage=10;}
        else if(run->stage==10){if(!run->pressModal(safe->strings.text("jaAuto.publish")))return;run->stage=11;}
        else
        {
            run->check("folder_publication_refreshes_selected_clean_existing_editor",run->human&&run->human->materialAudioVariantId()==run->newId&&safe->assetManager->selectedMaterialVariantId()==run->newId);
            run->check("folder_preserves_other_author_editor_and_music",run->otherAuthor&&run->otherAuthor->materialAudioVariantId()=="existing-authored-variant"&&safe->project.contentFingerprint()==run->beforeFolder);
            run->check("source_PCM_never_rewritten",juce::SHA256(run->source).toHexString()==run->sourceHash);
            safe->pendingNativeAnalyses=run->pendingBefore;safe->japaneseAutoAnnotationWindows.clear();run->finish();
        }
    };run->startTimer(50);
}
}

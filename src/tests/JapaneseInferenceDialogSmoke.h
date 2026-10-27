#pragma once
#include "../JapaneseAutoAnnotationDialog.h"
#include "../Theme.h"
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>
#include <map>
#include <iostream>

namespace hachi
{
// Actual visible buttons, native ASR/TIFA and on-disk OTTO; Host gate callbacks
// here are a contract harness, not proof of Main's editor/root refresh wiring.
inline void runJapaneseInferenceDialogSmoke(const juce::File& output,const juce::File& audio,
    const juce::File& models,std::function<void(bool)> finished)
{
    struct Run final : juce::Timer,std::enable_shared_from_this<Run>
    {
        struct Task final : juce::ThreadPoolJob
        {
            std::function<void()> action;std::atomic<int>& executed;
            Task(std::function<void()> f,std::atomic<int>& count):juce::ThreadPoolJob("Japanese-real-mode-dialog"),action(std::move(f)),executed(count){}
            JobStatus runJob() override {++executed;auto once=std::move(action);once();return jobHasFinished;}
        };
        struct Window final : juce::DocumentWindow
        {
            Window(juce::Component* child,HachiLookAndFeel& look):juce::DocumentWindow("Japanese inference modes — real native GUI",Palette::panel,juce::DocumentWindow::closeButton)
            {setLookAndFeel(&look);setUsingNativeTitleBar(true);setContentOwned(child,true);centreWithSize(1040,820);setVisible(true);}
            ~Window() override {setLookAndFeel(nullptr);}
            void closeButtonPressed() override {setVisible(false);}
        };
        std::shared_ptr<Run> self;std::function<void(bool)> done;
        HachiLookAndFeel look;juce::ThreadPool pool{1};std::unique_ptr<Window> window;I18n strings;
        juce::File base,bank,copied,original,modelRoot;juce::String audioHash,log,recognizedText,publishedAudioId;
        std::map<juce::String,juce::String> modelHashes;std::atomic<int> executed{0};
        bool ok=true,gate=false,finishing=false;int stage=0,ticks=0,workers=0,acquires=0,releases=0,preflights=0,publications=0;
        double deadline=0;JapaneseAutoAnnotationDialog::Input input;
        JapaneseAutoAnnotationDialog* ui() const {return window?dynamic_cast<JapaneseAutoAnnotationDialog*>(window->getContentComponent()):nullptr;}
        void check(const char* key,bool value){ok&=value;log+=juce::String(key)+"="+(value?"PASS":"FAIL")+"\n";}
        static juce::Component* find(juce::Component& parent,const juce::String& id)
        {if(parent.getComponentID()==id)return &parent;for(int i=0;i<parent.getNumChildComponents();++i)if(auto* child=find(*parent.getChildComponent(i),id))return child;return nullptr;}
        bool click(const char* id)
        {auto* button=ui()?dynamic_cast<juce::Button*>(find(*ui(),id)):nullptr;if(!button||!button->isEnabled())return false;button->triggerClick();ticks=0;deadline=juce::Time::getMillisecondCounterHiRes()+600000;return true;}
        void capture(const char* name)
        {if(!window)return;auto stream=base.getChildFile(name).createOutputStream();check("visible_native_dialog_capture",stream&&juce::PNGImageFormat().writeImageToStream(window->createComponentSnapshot(window->getLocalBounds()),*stream));}
        void stop()
        {
            if(finishing)return;finishing=true;if(ui())ui()->cancel();
            if(auto* modal=juce::Component::getCurrentlyModalComponent())modal->exitModalState(0);
            deadline=juce::Time::getMillisecondCounterHiRes()+30000;
        }
        void finish()
        {
            auto keep=shared_from_this();stopTimer();window.reset();
            check("all_accepted_worker_jobs_executed_once",executed.load()==workers&&pool.getNumJobs()==0);
            check("publication_gate_balanced",!gate&&acquires==releases);
            check("original_and_copied_WAV_bytes_preserved",juce::SHA256(original).toHexString()==audioHash&&juce::SHA256(copied).toHexString()==audioHash);
            for(const auto& item:modelHashes)check("actual_model_artifact_bytes_unchanged",juce::SHA256(modelRoot.getChildFile(item.first)).toHexString()==item.second);
            log+="SCOPE=actual three-button dialog/native inference/real confirmation/shared OTTO publication; Host is a gate harness, not Main refresh acceptance.\n";
            const bool written=base.getChildFile("japanese-inference-dialog.txt").replaceWithText(log);
            std::cout<<"japanese_inference_dialog="<<ok<<"|fixture="<<base.getFullPathName()<<std::endl;if(!ok)std::cerr<<log<<std::endl;
            auto callback=std::move(done);self.reset();if(callback)callback(ok&&written);
        }
        bool setup(const juce::File& out,const juce::File& source,const juce::File& models,std::function<void(bool)> callback)
        {
            done=std::move(callback);original=source;modelRoot=models;base=out.getChildFile("japanese-dialog-"+juce::Uuid().toString());bank=base.getChildFile("material");
            if(!source.existsAsFile()||!models.isDirectory()||bank.createDirectory().failed())return false;
            copied=bank.getChildFile(source.getFileName());if(!source.copyFileTo(copied))return false;audioHash=juce::SHA256(source).toHexString();
            for(const auto& entry:juce::RangedDirectoryIterator(modelRoot,true,"*",juce::File::findFiles))
                modelHashes[entry.getFile().getRelativePathFrom(modelRoot)]=juce::SHA256(entry.getFile()).toHexString();
            strings.setLanguage(I18n::Language::zhCN);input.selectedFolderRoot=bank;input.currentAudio=copied;input.modelRoot=modelRoot;input.execution.requested=backend::InferenceBackend::cpu;input.execution.deviceIndex=-1;input.execution.intraOpThreads=2;
            auto weak=std::weak_ptr<Run>(shared_from_this());JapaneseAutoAnnotationDialog::Host host;
            host.runOnWorker=[weak](std::function<void()> task){if(auto run=weak.lock()){++run->workers;run->pool.addJob(new Task(std::move(task),run->executed),true);}};
            host.publishing=[weak](bool value){if(auto run=weak.lock()){run->check("gate_callback_on_message_thread",juce::MessageManager::getInstance()->isThisTheMessageThread());if(value){run->check("gate_not_nested",!run->gate);run->gate=true;++run->acquires;}else{run->check("owned_gate_released",run->gate);run->gate=false;++run->releases;}}};
            host.preflight=[weak](const JapaneseAnnotationBatch::Plan& plan,juce::String& error)
            {if(auto run=weak.lock()){++run->preflights;const bool valid=run->gate&&plan.ready&&plan.results.size()==1&&plan.results[0].audio==run->copied;run->check("real_aligned_plan_preflight_under_gate",valid);if(!valid)error="Unexpected test publication plan";return valid;}error="test owner closed";return false;};
            host.published=[weak](const JapaneseAnnotationBatch::Plan& plan)
            {if(auto run=weak.lock()){++run->publications;run->check("gate_false_BEFORE_actual_published_callback",!run->gate&&run->releases==run->acquires);if(!plan.results.empty())run->publishedAudioId=plan.results[0].audioId;}};
            auto* dialog=new JapaneseAutoAnnotationDialog(strings,std::move(host));window=std::make_unique<Window>(dialog,look);juce::String error;
            check("current_actual_recording_input",dialog->setInput(input,error)&&dialog->selectedManualAudio()==copied);dialog->setManualInputText("",false);
            const auto* full=find(*dialog,"japanese.infer.full");const auto* recognition=find(*dialog,"japanese.infer.recognize");const auto* align=find(*dialog,"japanese.infer.align");const auto* confirm=find(*dialog,"japanese.confirm");
            check("three_real_bottom_buttons_and_upper_confirmation",full&&recognition&&align&&confirm&&full->getY()==recognition->getY()&&full->getY()==align->getY()&&confirm->getY()<full->getY());
            check("actual_empty_alignment_button_click",click("japanese.infer.align"));capture("three-mode-initial.png");startTimer(50);return true;
        }
        void timerCallback() override
        {
            auto keep=shared_from_this();
            if(finishing){if(pool.getNumJobs()==0&&!gate){finish();return;}if(juce::Time::getMillisecondCounterHiRes()>deadline){check("cancelled_jobs_drain_timeout",false);/* keep pumping accepted jobs */deadline+=30000;}return;}
            if(!ui()||juce::Time::getMillisecondCounterHiRes()>deadline){check("actual_native_mode_completion_before_timeout",false);if(ui())log+=ui()->reportText()+"\n";stop();return;}
            if(++ticks<2||ui()->busy())return;const auto manifest=bank.getChildFile("otto.ini");
            if(stage==0)
            {
                check("empty_align_shows_prompt_without_jobs_or_plan",ui()->reportText().contains(strings.text("jaAuto.inputRequired"))&&!ui()->previewReady()&&!ui()->previewPlan()&&workers==0&&!manifest.exists());
                check("actual_recognition_only_button_click",click("japanese.infer.recognize"));stage=1;
            }
            else if(stage==1)
            {
                recognizedText=ui()->manualInputText().trim();
                check("recognition_backfills_normalized_syllables_only",recognizedText.isNotEmpty()&&!ui()->previewReady()&&ui()->previewPlan()==nullptr
                    &&ui()->reportText().contains("ASR calls=1; TIFA calls=0")&&!manifest.exists());
                capture("recognition-only-backfill.png");if(recognizedText.isEmpty()){log+=ui()->reportText();stop();return;}
                check("actual_manual_alignment_button_click",click("japanese.infer.align"));stage=2;
            }
            else if(stage==2)
            {
                const auto* plan=ui()->previewPlan();check("manual_alignment_plan_has_no_ASR_stage",plan&&plan->ready&&plan->asrCallCount==0&&plan->alignmentCallCount==1&&plan->results.size()==1
                    &&plan->results[0].recognizedTokens.isEmpty()&&plan->results[0].asrBackend.isEmpty()&&!plan->results[0].alignmentBackend.isEmpty());
                if(!plan){log+=ui()->reportText();stop();return;}ui()->setManualInputText("ga");
                check("actual_full_inference_button_with_stale_manual",click("japanese.infer.full"));stage=3;
            }
            else if(stage==3)
            {
                const auto* plan=ui()->previewPlan();check("full_mode_reruns_both_actual_native_stages",plan&&plan->ready&&plan->asrCallCount==1&&plan->alignmentCallCount==1&&plan->results.size()==1);
                if(!plan||plan->results.empty()){log+=ui()->reportText();stop();return;}
                check("full_backfill_matches_actual_result_not_stale_or_filename",ui()->manualInputText().trim()==plan->results[0].normalizedSyllables.joinIntoString(" ")
                    &&!plan->results[0].recognizedTokens.isEmpty()&&plan->results[0].manualTokens.isEmpty());
                capture("full-mode-confirm-ready.png");check("actual_confirm_button_clicked",click("japanese.confirm"));stage=4;
            }
            else if(stage==4)
            {
                auto* alert=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
                check("actual_confirmation_is_modal_alert",alert&&alert->getNumButtons()>=2);
                if(!alert||!alert->getButton(0)){stop();return;}alert->getButton(0)->triggerClick();ticks=0;stage=5;
            }
            else
            {
                if(publications==0)return;
                OttoMaterialIO::Document cold;juce::String error;const bool loaded=OttoMaterialIO::loadDocument(manifest,cold,error);
                check("actual_confirmation_published_cold_OTTO",loaded&&cold.audio.size()==1&&cold.audio[0].id==publishedAudioId&&!cold.audio[0].rows.empty());
                check("one_publication_one_owned_gate",publications==1&&preflights==1&&acquires==1&&releases==1&&!gate&&!ui()->publicationInFlight());
                capture("published-final.png");stop();
            }
        }
    };
    auto run=std::make_shared<Run>();run->self=run;
    if(!run->setup(output,audio,models,std::move(finished)))
    {
        run->check("actual_recording_models_and_setup",false);
        if(run->base==juce::File{})run->base=output;
        run->finish();
    }
}
}

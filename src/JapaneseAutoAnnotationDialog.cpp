#include "JapaneseAutoAnnotationDialog.h"
#include "Theme.h"
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>
#include <exception>
#include <map>

namespace hachi
{
namespace
{
using Batch=JapaneseAnnotationBatch;
using Input=JapaneseAutoAnnotationDialog::Input;
using Mode=JapaneseAutoAnnotation::InferenceMode;
using juce::String;using juce::File;
struct Prepared { Batch::Plan plan;String fingerprint,summary;juce::uint64 generation=0; };
struct Job { std::atomic<bool> cancelled{false};juce::uint64 generation=0;JapaneseAutoAnnotationDialog::Host host; };
juce::StringArray tokens(const String& text)
{auto result=juce::StringArray::fromTokens(text," \t\r\n","");result.removeEmptyStrings();return result;}
String fingerprint(const Input& input)
{
    juce::MemoryOutputStream bytes;
    bytes.writeString(input.selectedFolderRoot.getFullPathName());bytes.writeString(input.modelRoot.getFullPathName());
    bytes.writeString(input.currentAudio.getFullPathName());
    for(const auto& token:input.correctedTokens)bytes.writeString(token);
    bytes.writeInt(int(input.execution.requested));bytes.writeInt(input.execution.deviceIndex);bytes.writeInt(input.execution.intraOpThreads);
    return juce::SHA256(bytes.getData(),bytes.getDataSize()).toHexString();
}
bool reject(String& error,const String& message){error=message;return false;}
bool onMessageThread(){const auto* manager=juce::MessageManager::getInstanceWithoutCreating();return manager&&manager->isThisTheMessageThread();}
}
struct JapaneseAutoAnnotationDialog::Impl
{
    JapaneseAutoAnnotationDialog& owner;I18n& strings;Host host;HachiLookAndFeel look;
    juce::Label title,notice,rootLabel,modelLabel,audioLabel,correctionLabel,backendLabel,manualRecordingLabel,reportLabel;
    juce::TextEditor root,model,audio,correction,report;
    juce::TextButton rootBrowse,modelBrowse,audioBrowse,inferButton,recognizeButton,alignButton,publishButton,cancelButton;
    juce::ComboBox backendChoice,manualRecording;
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<Job> operation;std::shared_ptr<Prepared> prepared;
    backend::OrtExecutionConfig execution;
    std::vector<File> manualFiles;std::map<String,String> manualTexts;File manualAudio;
    juce::uint64 generation=0;bool loading=false,working=false,committing=false;

    bool hostReady() const{return host.runOnWorker&&host.publishing&&host.preflight&&host.published;}
    Impl(JapaneseAutoAnnotationDialog& o,I18n& s,Host h):owner(o),strings(s),host(std::move(h))
    {
        look.setUsingNativeAlertWindows(false);
        owner.setLookAndFeel(&look);
        title.setText(strings.text("jaAuto.title"),juce::dontSendNotification);
        notice.setText(strings.text("jaAuto.notice"),juce::dontSendNotification);notice.setJustificationType(juce::Justification::centredLeft);
        rootLabel.setText(strings.text("jaAuto.root"),juce::dontSendNotification);modelLabel.setText(strings.text("jaAuto.models"),juce::dontSendNotification);
        audioLabel.setText(strings.text("jaAuto.audio"),juce::dontSendNotification);correctionLabel.setText(strings.text("jaAuto.correction"),juce::dontSendNotification);
        backendLabel.setText(strings.text("jaAuto.backend"),juce::dontSendNotification);manualRecordingLabel.setText(strings.text("jaAuto.manualRecording"),juce::dontSendNotification);
        reportLabel.setText(strings.text("jaAuto.result"),juce::dontSendNotification);
        backendChoice.addItem("CPU",2);backendChoice.addItem("DirectML GPU",3);backendChoice.setSelectedId(2,juce::dontSendNotification);
        for(auto* button:{&rootBrowse,&modelBrowse,&audioBrowse})button->setButtonText(strings.text("jaAuto.browse"));
        inferButton.setButtonText(strings.text("jaAuto.infer"));recognizeButton.setButtonText(strings.text("jaAuto.recognizeOnly"));alignButton.setButtonText(strings.text("jaAuto.alignOnly"));
        publishButton.setButtonText(strings.text("jaAuto.publish"));cancelButton.setButtonText(strings.text("jaAuto.cancel"));
        inferButton.setComponentID("japanese.infer.full");recognizeButton.setComponentID("japanese.infer.recognize");alignButton.setComponentID("japanese.infer.align");
        publishButton.setComponentID("japanese.confirm");cancelButton.setComponentID("japanese.cancel");correction.setComponentID("japanese.manual");manualRecording.setComponentID("japanese.manualRecording");
        audio.setTextToShowWhenEmpty(strings.text("jaAuto.allFiles"),Palette::textMuted);
        correction.setTextToShowWhenEmpty(strings.text("jaAuto.correctionHint"),Palette::textMuted);
        report.setMultiLine(true);report.setReturnKeyStartsNewLine(true);report.setReadOnly(true);report.setScrollbarsShown(true);
        for(auto* component:std::initializer_list<juce::Component*>{&title,&notice,&rootLabel,&modelLabel,&audioLabel,&correctionLabel,&backendLabel,&manualRecordingLabel,&reportLabel,
            &root,&model,&audio,&correction,&report,&rootBrowse,&modelBrowse,&audioBrowse,&inferButton,&recognizeButton,&alignButton,&publishButton,&cancelButton,&backendChoice,&manualRecording})owner.addAndMakeVisible(component);
        for(auto* field:{&root,&audio})field->onTextChange=[this]{scopeChanged();};
        model.onTextChange=[this]{changed();};
        correction.onTextChange=[this]{rememberManual();changed();};backendChoice.onChange=[this]{changed();};manualRecording.onChange=[this]{selectManual(manualRecording.getSelectedId()-1);};
        rootBrowse.onClick=[this]{browse(0);};modelBrowse.onClick=[this]{browse(1);};audioBrowse.onClick=[this]{browse(2);};
        inferButton.onClick=[this]{infer(Mode::recognizeAndAlign);};recognizeButton.onClick=[this]{infer(Mode::recognizeOnly);};alignButton.onClick=[this]{infer(Mode::alignOnly);};
        publishButton.onClick=[this]{confirmPublish();};cancelButton.onClick=[this]{cancel();};
    }
    ~Impl(){if(operation)operation->cancelled=true;chooser.reset();owner.setLookAndFeel(nullptr);}
    Input input() const
    {
        const auto file=[](const String& text){return text.trim().isEmpty()?File{}:File(text.trim());};
        Input value;value.selectedFolderRoot=file(root.getText());value.modelRoot=file(model.getText());value.currentAudio=file(audio.getText());
        value.correctedTokens=tokens(correction.getText());value.execution=execution;
        value.execution.requested=backendChoice.getSelectedId()==3?backend::InferenceBackend::directML:backend::InferenceBackend::cpu;return value;
    }
    void rememberManual(){if(!loading&&manualAudio!=File{})manualTexts[manualAudio.getFullPathName()]=correction.getText();}
    void buttons()
    {
        const bool idle=hostReady()&&!working;
        inferButton.setEnabled(idle);recognizeButton.setEnabled(idle);alignButton.setEnabled(idle);
        publishButton.setEnabled(idle&&prepared&&prepared->plan.ready&&prepared->generation==generation&&prepared->fingerprint==fingerprint(input()));
        cancelButton.setEnabled(working);
        for(auto* component:std::initializer_list<juce::Component*>{&root,&model,&audio,&rootBrowse,&modelBrowse,&audioBrowse,&backendChoice,&manualRecording})component->setEnabled(!committing);
        manualRecording.setEnabled(!working&&!committing);
        correction.setEnabled(!committing&&manualAudio!=File{});
    }
    void changed()
    {
        if(loading||committing)return;
        ++generation;prepared.reset();if(operation)operation->cancelled=true;
        report.setText(strings.text("jaAuto.changed"),false);buttons();
    }
    void resetManualScope(const File& file,const String& text={})
    {
        loading=true;manualTexts.clear();manualFiles.clear();manualRecording.clear(juce::dontSendNotification);manualAudio=file;
        if(file!=File{}){manualFiles.push_back(file);manualTexts[file.getFullPathName()]=text;manualRecording.addItem(file.getFileName(),1);manualRecording.setSelectedId(1,juce::dontSendNotification);}
        correction.setText(text,false);loading=false;
    }
    void scopeChanged()
    {
        if(loading||committing)return;
        const auto text=audio.getText().trim();resetManualScope(text.isEmpty()?File{}:File(text));changed();
    }
    bool selectManual(int index)
    {
        if(loading||working||committing||index<0||index>=int(manualFiles.size()))return false;
        const bool stillCurrent=prepared&&prepared->fingerprint==fingerprint(input());
        rememberManual();loading=true;manualAudio=manualFiles[std::size_t(index)];
        manualRecording.setSelectedId(index+1,juce::dontSendNotification);correction.setText(manualTexts[manualAudio.getFullPathName()],false);loading=false;
        if(prepared)
        {
            if(stillCurrent){prepared->plan.focusAudio=manualAudio;prepared->fingerprint=fingerprint(input());}
            else{++generation;prepared.reset();}
        }
        buttons();return true;
    }
    void fillManual(const Batch::Plan& plan,Mode mode)
    {
        const auto previous=manualAudio;rememberManual();loading=true;
        if(mode!=Mode::alignOnly)manualFiles.clear();
        const auto add=[&](const JapaneseAutoAnnotation::Result& result)
        {
            if(result.audio==File{}||result.normalizedSyllables.isEmpty())return;
            if(std::find(manualFiles.begin(),manualFiles.end(),result.audio)==manualFiles.end())manualFiles.push_back(result.audio);
            if(mode!=Mode::alignOnly)manualTexts[result.audio.getFullPathName()]=result.normalizedSyllables.joinIntoString(" ");
        };
        for(const auto& result:plan.recognitions)add(result);
        for(const auto& result:plan.results)add(result);
        manualRecording.clear(juce::dontSendNotification);
        for(std::size_t i=0;i<manualFiles.size();++i)
        {
            auto label=manualFiles[i].getRelativePathFrom(plan.root);if(label.isEmpty())label=manualFiles[i].getFileName();
            manualRecording.addItem(label,int(i+1));
        }
        auto selected=std::find(manualFiles.begin(),manualFiles.end(),previous);
        if(selected==manualFiles.end()&&!manualFiles.empty())selected=manualFiles.begin();
        if(selected!=manualFiles.end())
        {
            manualAudio=*selected;manualRecording.setSelectedId(int(selected-manualFiles.begin()+1),juce::dontSendNotification);
            correction.setText(manualTexts[manualAudio.getFullPathName()],false);
        }
        loading=false;
    }
    bool setInput(const Input& value,String& error)
    {
        error.clear();if(committing)return reject(error,strings.text("jaAuto.gated"));
        if(value.currentAudio==File{}&&!value.correctedTokens.isEmpty())return reject(error,strings.text("jaAuto.singleOnly"));
        if(value.execution.requested!=backend::InferenceBackend::cpu&&value.execution.requested!=backend::InferenceBackend::directML&&value.execution.requested!=backend::InferenceBackend::automatic)
            return reject(error,strings.text("jaAuto.backendOnly"));
        const auto text=[](const File& file){return file==File{}?String{}:file.getFullPathName();};
        loading=true;root.setText(text(value.selectedFolderRoot),false);model.setText(text(value.modelRoot),false);audio.setText(text(value.currentAudio),false);execution=value.execution;
        backendChoice.setSelectedId(value.execution.requested==backend::InferenceBackend::directML?3:2,juce::dontSendNotification);loading=false;
        resetManualScope(value.currentAudio,value.correctedTokens.joinIntoString(" "));changed();return true;
    }
    void browse(int kind)
    {
        auto* field=kind==0?&root:kind==1?&model:&audio;
        chooser=std::make_unique<juce::FileChooser>(strings.text("jaAuto.browse"),field->getText().isEmpty()?File{}:File(field->getText()),kind==2?"*.wav;*.flac;*.aif;*.aiff":String{});
        juce::Component::SafePointer<JapaneseAutoAnnotationDialog> safe(&owner);
        chooser->launchAsync(juce::FileBrowserComponent::openMode|(kind==2?juce::FileBrowserComponent::canSelectFiles:juce::FileBrowserComponent::canSelectDirectories),[safe,kind](const juce::FileChooser& chosen)
        {if(safe&&chosen.getResult()!=File{}&&!safe->impl->committing)(kind==0?safe->impl->root:kind==1?safe->impl->model:safe->impl->audio).setText(chosen.getResult().getFullPathName(),true);});
    }
    void cancel(){if(operation){operation->cancelled=true;report.setText(strings.text(committing?"jaAuto.cancelPublish":"jaAuto.cancelPreview"),false);}}
    void syncManualSelection()
    {
        const auto index=manualRecording.getSelectedId()-1;
        if(index>=0&&index<int(manualFiles.size())&&manualFiles[std::size_t(index)]!=manualAudio)selectManual(index);
    }
    void infer(Mode mode)
    {
        if(working)return;
        syncManualSelection();
        prepared.reset();const auto selected=input();rememberManual();const auto focus=manualAudio;
        if(!hostReady()){report.setText(strings.text("jaAuto.hostMissing"),false);buttons();return;}
        if(mode==Mode::alignOnly&&(focus==File{}||selected.correctedTokens.isEmpty()))
        {report.setText(strings.text("jaAuto.inputRequired"),false);buttons();return;}
        working=true;committing=false;auto job=std::make_shared<Job>();job->host=host;job->generation=generation;operation=job;
        report.setText(strings.text("jaAuto.running"),false);buttons();
        juce::Component::SafePointer<JapaneseAutoAnnotationDialog> safe(&owner);const auto localized=strings;
        try{job->host.runOnWorker([job,selected,focus,mode,safe,localized]
        {
            auto candidate=std::make_shared<Prepared>();candidate->generation=job->generation;String error;bool valid=false;
            try
            {
                if(onMessageThread())error="Host executor ran Japanese annotation on the message thread";
                else
                {
                    Batch::Input request;request.root=selected.selectedFolderRoot;request.modelRoot=selected.modelRoot;request.execution=selected.execution;request.mode=mode;request.focusAudio=focus;
                    if(mode==Mode::alignOnly){request.recordings.push_back(focus);request.correctedTokens=selected.correctedTokens;}
                    else if(selected.currentAudio!=File{})request.recordings.push_back(selected.currentAudio);
                    valid=Batch::preview(request,candidate->plan,error,[job]{return job->cancelled.load();},[job,safe](const String& progress)
                    {juce::MessageManager::callAsync([job,safe,progress]{if(safe&&safe->impl->operation==job&&!job->cancelled)safe->impl->report.setText(progress,false);});});
                }
            }
            catch(const std::exception& exception){error=exception.what();}catch(...){error="Unknown Japanese inference failure";}
            candidate->summary=localized.text(mode==Mode::recognizeOnly?"jaAuto.recognitionReady":"jaAuto.review")+"\n"+candidate->plan.report;
            candidate->summary+="\nASR calls="+String(candidate->plan.asrCallCount)+"; TIFA calls="+String(candidate->plan.alignmentCallCount)+"\n";
            for(const auto& result:candidate->plan.results)
            {
                candidate->summary+="\n"+result.audio.getFullPathName()+"\nSHA256: "+result.sourceFingerprint+"\n";
                for(const auto& span:result.alignment.phones)candidate->summary+=span.phone+" ["+String(span.startSeconds,4)+", "+String(span.endSeconds,4)+"] cosine="+String(span.cosineAgreement,5)+"\n";
                for(const auto& warning:result.warnings)candidate->summary+=warning+"\n";
            }
            if(!valid)candidate->summary+="\n"+localized.text("jaAuto.notPublished")+"\n"+error;
            juce::MessageManager::callAsync([job,candidate,safe,valid,mode]
            {
                if(!safe||safe->impl->operation!=job)return;
                auto& ui=*safe->impl;ui.working=false;ui.operation.reset();
                if(job->cancelled||ui.generation!=job->generation)ui.report.setText(ui.strings.text("jaAuto.cancelled"),false);
                else
                {
                    if(valid||!candidate->plan.recognitions.empty())ui.fillManual(candidate->plan,mode);
                    candidate->plan.focusAudio=ui.manualAudio;candidate->fingerprint=fingerprint(ui.input());
                    ui.report.setText(candidate->summary,false);if(valid&&candidate->plan.ready)ui.prepared=candidate;
                }
                ui.buttons();
            });
        });}
        catch(const std::exception& exception){working=false;operation.reset();report.setText(String(exception.what()),false);buttons();}
        catch(...){working=false;operation.reset();report.setText(strings.text("jaAuto.hostMissing"),false);buttons();}
    }
    bool publishConfirmed(String& error)
    {
        syncManualSelection();
        error.clear();if(working||!prepared||!prepared->plan.ready||prepared->generation!=generation||prepared->fingerprint!=fingerprint(input()))return reject(error,strings.text("jaAuto.previewRequired"));
        if(!hostReady())return reject(error,strings.text("jaAuto.hostMissing"));
        const auto candidate=prepared;candidate->plan.focusAudio=manualAudio;auto job=std::make_shared<Job>();job->host=host;job->generation=generation;const auto callbacks=host;
        juce::Component::SafePointer<JapaneseAutoAnnotationDialog> safe(&owner);
        callbacks.publishing(true);
        if(!safe){callbacks.publishing(false);return reject(error,"Dialog closed before publication preflight");}
        if(!callbacks.preflight(candidate->plan,error)){callbacks.publishing(false);return false;}
        if(!safe){callbacks.publishing(false);return reject(error,"Dialog closed during publication preflight");}
        if(working||prepared!=candidate||generation!=job->generation||candidate->fingerprint!=fingerprint(input()))
        {callbacks.publishing(false);return reject(error,strings.text("jaAuto.previewRequired"));}
        working=committing=true;operation=job;report.setText(strings.text("jaAuto.publishing"),false);buttons();
        try{job->host.runOnWorker([candidate,job,safe]
        {
            String failure;bool committed=false;
            try{if(onMessageThread())failure="Host executor ran publication on the message thread";else committed=Batch::publish(candidate->plan,failure,[job]{return job->cancelled.load();});}
            catch(const std::exception& exception){failure=exception.what();}catch(...){failure="Unknown annotation publication failure";}
            juce::MessageManager::callAsync([candidate,job,safe,committed,failure]
            {
                if(safe&&safe->impl->operation==job)
                {
                    auto& ui=*safe->impl;ui.working=ui.committing=false;ui.operation.reset();ui.prepared.reset();
                    ui.report.setText(candidate->summary+"\n"+ui.strings.text(committed?"jaAuto.published":"jaAuto.notPublished")+"\n"+failure,false);ui.buttons();
                }
                // Disk commit is finished. Release the shared gate before the
                // host reloads/retargets the existing manual editor and its Save.
                job->host.publishing(false);
                if(committed)job->host.published(candidate->plan);
            });
        });}
        catch(const std::exception& exception){callbacks.publishing(false);if(safe){working=committing=false;operation.reset();buttons();}return reject(error,String(exception.what()));}
        catch(...){callbacks.publishing(false);if(safe){working=committing=false;operation.reset();buttons();}return reject(error,"Host executor rejected publication");}
        return true;
    }
    void confirmPublish()
    {
        if(!prepared||working)return;
        const auto candidate=prepared;const auto epoch=generation;juce::Component::SafePointer<JapaneseAutoAnnotationDialog> safe(&owner);
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,strings.text("jaAuto.confirmTitle"),candidate->plan.manifest.getFullPathName()+"\n\n"+strings.text("jaAuto.confirm")+"\n"+String(int(candidate->plan.results.size()))+" / "+String(candidate->plan.failures.size()),strings.text("jaAuto.publish"),strings.text("jaAuto.cancel"),&owner,
            juce::ModalCallbackFunction::create([safe,candidate,epoch](int accepted)
            {
                if(!safe||accepted!=1)return;
                auto& ui=*safe->impl;String error;
                if(ui.prepared!=candidate||ui.generation!=epoch)error=ui.strings.text("jaAuto.previewRequired");else if(ui.publishConfirmed(error))return;
                if(safe)safe->impl->report.setText(error,false);
            }));
    }
};
JapaneseAutoAnnotationDialog::JapaneseAutoAnnotationDialog(I18n& strings,Host host,Input initial):impl(std::make_unique<Impl>(*this,strings,std::move(host)))
{String error;if(!impl->setInput(initial,error))impl->report.setText(error,false);else impl->report.setText(strings.text(impl->hostReady()?"jaAuto.initial":"jaAuto.hostMissing"),false);setSize(1000,780);}
JapaneseAutoAnnotationDialog::JapaneseAutoAnnotationDialog(I18n& strings,Host host):JapaneseAutoAnnotationDialog(strings,std::move(host),Input{}){}
JapaneseAutoAnnotationDialog::~JapaneseAutoAnnotationDialog()=default;
void JapaneseAutoAnnotationDialog::paint(juce::Graphics& graphics){graphics.fillAll(Palette::panel);}
void JapaneseAutoAnnotationDialog::resized()
{
    if(!impl)return;auto& ui=*impl;auto area=getLocalBounds().reduced(14);
    ui.title.setBounds(area.removeFromTop(28));ui.notice.setBounds(area.removeFromTop(64));area.removeFromTop(6);
    const auto row=[&](juce::Label& label,juce::TextEditor& editor,juce::TextButton* browse)
    {auto line=area.removeFromTop(34);label.setBounds(line.removeFromLeft(230));if(browse)browse->setBounds(line.removeFromRight(94).reduced(3,2));editor.setBounds(line.reduced(2,2));};
    row(ui.rootLabel,ui.root,&ui.rootBrowse);row(ui.modelLabel,ui.model,&ui.modelBrowse);row(ui.audioLabel,ui.audio,&ui.audioBrowse);
    auto line=area.removeFromTop(34);ui.manualRecordingLabel.setBounds(line.removeFromLeft(230));ui.manualRecording.setBounds(line.reduced(2));
    row(ui.correctionLabel,ui.correction,nullptr);line=area.removeFromTop(34);ui.backendLabel.setBounds(line.removeFromLeft(230));ui.backendChoice.setBounds(line.removeFromLeft(240).reduced(2));area.removeFromTop(6);
    auto commands=area.removeFromBottom(38);const auto width=commands.getWidth()/3;
    ui.inferButton.setBounds(commands.removeFromLeft(width).reduced(2));ui.recognizeButton.setBounds(commands.removeFromLeft(width).reduced(2));ui.alignButton.setBounds(commands.reduced(2));
    auto status=area.removeFromTop(36);ui.reportLabel.setBounds(status.removeFromLeft(180));ui.cancelButton.setBounds(status.removeFromRight(110).reduced(2));ui.publishButton.setBounds(status.removeFromRight(280).reduced(2));
    area.removeFromBottom(6);ui.report.setBounds(area);
}
bool JapaneseAutoAnnotationDialog::setInput(const Input& value,String& error){return impl->setInput(value,error);}
void JapaneseAutoAnnotationDialog::infer(InferenceMode mode){impl->infer(mode);}
void JapaneseAutoAnnotationDialog::preview(){impl->infer(Mode::recognizeAndAlign);}
String JapaneseAutoAnnotationDialog::manualInputText() const{return impl->correction.getText();}
void JapaneseAutoAnnotationDialog::setManualInputText(const String& text,bool notify){impl->correction.setText(text,notify);}
File JapaneseAutoAnnotationDialog::selectedManualAudio() const{return impl->manualAudio;}
bool JapaneseAutoAnnotationDialog::selectManualAudio(const File& file){const auto found=std::find(impl->manualFiles.begin(),impl->manualFiles.end(),file);return found!=impl->manualFiles.end()&&impl->selectManual(int(found-impl->manualFiles.begin()));}
void JapaneseAutoAnnotationDialog::showPublishConfirmation(){if(impl->publishButton.onClick)impl->publishButton.onClick();}
bool JapaneseAutoAnnotationDialog::publishConfirmed(String& error){return impl->publishConfirmed(error);}
bool JapaneseAutoAnnotationDialog::previewReady() const{return !impl->working&&impl->prepared&&impl->prepared->plan.ready&&impl->prepared->generation==impl->generation&&impl->prepared->fingerprint==fingerprint(impl->input());}
const Batch::Plan* JapaneseAutoAnnotationDialog::previewPlan() const{return previewReady()?&impl->prepared->plan:nullptr;}
String JapaneseAutoAnnotationDialog::reportText() const{return impl->report.getText();}
bool JapaneseAutoAnnotationDialog::busy() const{return impl->working;}
bool JapaneseAutoAnnotationDialog::publicationInFlight() const{return impl->committing;}
void JapaneseAutoAnnotationDialog::cancel(){impl->cancel();}
}

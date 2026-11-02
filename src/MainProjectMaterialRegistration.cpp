#include "MainComponent.h"
#include "ProjectMaterialRegistrationFacts.h"
#include "MelodyneJapaneseSliceBatch.h"
#include <thread>

namespace hachi
{
struct MainComponent::ProjectRegistrationOperation
{
    ProjectData snapshot;
    std::uint64_t revision=0;
    juce::String fingerprint,error;
    std::vector<ProjectMaterialRegistration::Locator> owners;
    std::atomic_bool cancel{false};
    std::thread worker;
    std::unique_ptr<ProjectMaterialRegistration::Lease> lease;
    bool japanesePronunciation = false;
    MelodyneJapaneseSliceBatch::Options japaneseOptions;
    juce::String japaneseReport;
    std::map<juce::String,juce::String> recognizedSourceHashes;
    std::unique_ptr<juce::ToggleButton> japaneseChoice;
    juce::Component::SafePointer<juce::AlertWindow> prompt;
};
namespace
{
std::vector<ProjectMaterialRegistration::Locator> unregisteredProjectOwners(const ProjectData& data,const std::atomic_bool& cancel)
{
    std::vector<ProjectMaterialRegistration::Locator> result;
    struct Read{bool valid=false;std::vector<SampleRegionSetting> rows;};std::map<juce::String,Read> cache;
    for(const auto& track:data.tracks)if(trackShowsAllNativeRegions(track)&&!track.accompaniment)
        for(const auto& parent:track.clips)
        {
            const auto inspect=[&](const ClipData& owner,const juce::String& part)
            {
                if(cancel.load()||!owner.sourceFile.existsAsFile())return;
                bool hasNotes=false,bound=true;const auto manifest=OttoMaterialIO::findManifest(owner.sourceFile);
                for(const auto& note:parent.notes)if(note.clipPartId==part)
                {
                    hasNotes=true;
                    if(note.materialAudioId.isEmpty()||note.materialRegionId.isEmpty()||!manifest.existsAsFile()){bound=false;continue;}
                    const auto key=manifest.getFullPathName()+"|"+owner.sourceFile.getFullPathName()+"|"+note.materialAudioId;
                    auto found=cache.find(key);
                    if(found==cache.end())
                    {Read read;juce::String error;read.valid=OttoMaterialIO::loadAudio(manifest,owner.sourceFile,read.rows,error,nullptr,note.materialAudioId);found=cache.emplace(key,std::move(read)).first;}
                    const auto& read=found->second;
                    bound&=read.valid&&std::any_of(read.rows.begin(),read.rows.end(),[&](const auto& r){return r.materialAudioId==note.materialAudioId&&r.materialRegionId==note.materialRegionId;});
                }
                if(hasNotes&&!bound)result.push_back({track.id,parent.id,part,{}});
            };
            if(parent.parts.empty())inspect(parent,{});else for(const auto& part:parent.parts)inspect(part,part.id);
        }
    return result;
}
}

void MainComponent::cancelProjectMaterialRegistration()
{
    auto operation=std::exchange(projectRegistrationOperation,{});
    if(registrationInfoBar.isVisible()){registrationInfoBar.setVisible(false);resized();}
    if(operation){operation->cancel=true;if(operation->prompt&&operation->prompt->isCurrentlyModal())operation->prompt->exitModalState(0);}
    projectRegistrationChooser.reset();
    // IO phases NEVER wait for a UI callback. Joining cannot form a GUI cycle.
    if(operation&&operation->worker.joinable())operation->worker.join();
    if(operation&&operation->lease){juce::String error;operation->lease->rollback(error);}
}

void MainComponent::offerMaterialFolderForImport(const ProjectData& imported,bool explicitRequest)
{
    if(closingUI||!preferences)return;cancelProjectMaterialRegistration();registrationInfoBar.setVisible(false);
    // "Never for this project" only silences the automatic offer on open.
    const auto projectKey="ui.skipRegistration."+juce::String::toHexString(currentProjectFile.getFullPathName().toLowerCase().hashCode64());
    if(!explicitRequest&&currentProjectFile!=juce::File{}&&preferences->getBoolValue(projectKey,false))return;
    juce::ignoreUnused(imported); // current live snapshot is the one adoption will guard
    auto operation=std::make_shared<ProjectRegistrationOperation>();operation->snapshot=project.snapshot();operation->revision=project.revisionNumber();
    registrationInfoRegister.setButtonText(strings.text("Register copies..."));
    registrationInfoLater.setVisible(true);registrationInfoNever.setVisible(true);
    operation->fingerprint=ProjectModel::registrationMusicSourceFingerprint(operation->snapshot);projectRegistrationOperation=operation;
    projectRegistrationTrace="offer_start revision="+juce::String(static_cast<juce::int64>(operation->revision))+" tracks="+juce::String(static_cast<int>(operation->snapshot.tracks.size()));
    juce::Component::SafePointer<MainComponent> safe(this);
    operation->worker=std::thread([safe,operation,explicitRequest,projectKey]
    {
        operation->owners=unregisteredProjectOwners(operation->snapshot,operation->cancel);
        juce::MessageManager::callAsync([safe,operation,explicitRequest,projectKey]
        {
            if(!safe||safe->closingUI||operation->cancel||safe->projectRegistrationOperation!=operation)return;
            if(operation->worker.joinable())operation->worker.join();
            safe->projectRegistrationTrace+="\nprobe owners="+juce::String(static_cast<int>(operation->owners.size()))+" expected_revision="+juce::String(static_cast<juce::int64>(operation->revision))
                +" live_revision="+juce::String(static_cast<juce::int64>(safe->project.revisionNumber()))+" music_fp_equal="+juce::String(ProjectModel::registrationMusicSourceFingerprint(safe->project.snapshot())==operation->fingerprint?1:0);
            juce::Logger::writeToLog(safe->projectRegistrationTrace);
            if(operation->owners.empty()){safe->projectRegistrationOperation.reset();return;}
            if(safe->project.revisionNumber()!=operation->revision){safe->cancelProjectMaterialRegistration();return;}
            if(!explicitRequest)
            {
                // Automatic offer: a dismissible strip that never blocks editing.
                // Choosing Register re-validates the SAME operation's revision.
                auto* self=safe.getComponent();
                self->registrationInfoText.setText(self->strings.text("Unregistered source recordings were found in this project. Register copies as project materials?")
                    +"  ("+juce::String(static_cast<int>(operation->owners.size()))+")",juce::dontSendNotification);
                self->registrationInfoRegister.onClick=[safe,operation]{if(!safe)return;safe->registrationInfoBar.setVisible(false);safe->resized();
                    // Edits made while the strip was shown make the probe stale:
                    // re-probe the live project explicitly instead of failing.
                    if(safe->projectRegistrationOperation==operation&&safe->project.revisionNumber()==operation->revision)
                     safe->confirmProjectMaterialRegistration(operation);
                    else safe->offerMaterialFolderForImport(safe->project.snapshot(),true);};
                self->registrationInfoLater.onClick=[safe]{if(!safe)return;safe->registrationInfoBar.setVisible(false);safe->cancelProjectMaterialRegistration();safe->resized();};
                self->registrationInfoNever.onClick=[safe,projectKey]{if(!safe)return;if(safe->preferences)safe->preferences->setValue(projectKey,true);
                    safe->registrationInfoBar.setVisible(false);safe->cancelProjectMaterialRegistration();safe->resized();};
                self->registrationInfoBar.setVisible(true);self->resized();
                return;
            }
            safe->confirmProjectMaterialRegistration(operation);
        });
    });
}

void MainComponent::confirmProjectMaterialRegistration(const std::shared_ptr<ProjectRegistrationOperation>& operation)
{
    if(closingUI||operation->cancel||projectRegistrationOperation!=operation)return;
    if(project.revisionNumber()!=operation->revision)
    {cancelProjectMaterialRegistration();showError(strings.text("The project changed before registration. Reopen the registration action for the current project."));return;}
    juce::Component::SafePointer<MainComponent> safe(this);
    auto* dialog=new juce::AlertWindow(strings.text("Register project source materials"),
        strings.text("registration.confirmWithRecognition"),juce::MessageBoxIconType::QuestionIcon,this);
    operation->prompt=dialog;
    dialog->setComponentID("project-material-registration-confirm");
    operation->japaneseChoice=std::make_unique<juce::ToggleButton>(strings.text("registration.japaneseRecognition"));
    auto& japanese=*operation->japaneseChoice;
    japanese.setComponentID("registration-auto-japanese-pronunciation");
    japanese.setToggleState(false,juce::dontSendNotification); // each registration is an explicit opt-in
    japanese.setTooltip(strings.text("registration.japaneseRecognitionHelp"));
    japanese.setSize(560,48);
    dialog->addCustomComponent(&japanese); // AlertWindow does not own custom components
    dialog->addButton(strings.text("Register copies..."),1);
    dialog->addButton(strings.text("Skip"),0,juce::KeyPress(juce::KeyPress::escapeKey));
    dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,operation,dialog](int choice)
    {
        std::unique_ptr<juce::AlertWindow> owned(dialog);
        dialog->removeCustomComponent(0);
        if(!safe||safe->closingUI||operation->cancel||safe->projectRegistrationOperation!=operation)return;
        if(choice!=1){safe->cancelProjectMaterialRegistration();return;}
        operation->japanesePronunciation=operation->japaneseChoice->getToggleState();
        operation->japaneseChoice.reset();
        safe->chooseProjectMaterialRegistrationDestination(operation);
    }),false);
}

void MainComponent::chooseProjectMaterialRegistrationDestination(const std::shared_ptr<ProjectRegistrationOperation>& operation)
{
    if(projectRegistrationOperation!=operation||operation->cancel||closingUI)return;
    if(project.revisionNumber()!=operation->revision){cancelProjectMaterialRegistration();showError(strings.text("The project changed before registration. Reopen the registration action for the current project."));return;}
    juce::Component::SafePointer<MainComponent> safe(this);
    const auto chosen=[safe,operation](const juce::File& destination)
    {
        if(!safe||safe->closingUI||operation->cancel||safe->projectRegistrationOperation!=operation)return;
        if(!destination.isDirectory()){safe->cancelProjectMaterialRegistration();return;}
        safe->stageProjectMaterialRegistration(operation,destination);
    };
    if(projectRegistrationFolderPickerForTest){projectRegistrationFolderPickerForTest(chosen);return;}
    projectRegistrationChooser=std::make_unique<juce::FileChooser>(strings.text("Choose parent folder for NEW project materials"),juce::File{},juce::String{},true);
    projectRegistrationChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,
        [chosen](const juce::FileChooser& chooser){chosen(chooser.getResult());});
}

void MainComponent::stageProjectMaterialRegistration(const std::shared_ptr<ProjectRegistrationOperation>& operation,const juce::File& destination)
{
    if(closingUI||projectRegistrationOperation!=operation||operation->cancel)return;
    if(project.revisionNumber()!=operation->revision){cancelProjectMaterialRegistration();showError(strings.text("The project changed; registration was cancelled without changing references."));return;}
    if(operation->worker.joinable())operation->worker.join();
    const auto legal=juce::File::createLegalFileName(operation->snapshot.name);const auto child=(legal.isEmpty()?juce::String("project"):legal)+" materials "+juce::Uuid().toString().substring(0,8);
    juce::Component::SafePointer<MainComponent> safe(this);
    // Read only: a dedicated Japanese root, never the NSF/vocoder directory.
    operation->japaneseOptions.modelRoot=JapaneseAutoAnnotation::defaultModelRoot();
    if(preferences)
    {
        const auto root=preferences->getValue("japanese.modelRoot").trim();
        if(root.isNotEmpty())operation->japaneseOptions.modelRoot=juce::File(root);
    }
    if(projectRegistrationJapaneseModelRootForTest)operation->japaneseOptions.modelRoot=projectRegistrationJapaneseModelRootForTest();
    if(operation->japanesePronunciation)
    {
        registrationInfoText.setText(strings.text("registration.recognizing"),juce::dontSendNotification);
        registrationInfoRegister.setButtonText(strings.text("registration.cancelRecognition"));
        registrationInfoRegister.onClick=[safe,operation]
        {if(safe&&safe->projectRegistrationOperation==operation){safe->cancelProjectMaterialRegistration();safe->showStatusText(safe->strings.text("registration.recognitionCancelled"),5000);}};
        registrationInfoLater.setVisible(false);registrationInfoNever.setVisible(false);
        registrationInfoBar.setVisible(true);resized();
    }
    operation->worker=std::thread([safe,operation,destination,child]
    {
        const auto cancel=[operation]{return operation->cancel.load();};
        const ProjectMaterialRegistration::OwnerScope scope=operation->owners;
        auto facts=ProjectMaterialRegistrationFacts::derive(operation->snapshot,ProjectMaterialRegistrationFacts::Policy::ConfirmedCarriedPlacements,scope);
        if(!facts.ready)operation->error=facts.diagnostics.joinIntoString("\n");
        else
        {
            bool recognized=true;
            if(operation->japanesePronunciation&&!cancel())
            {
                MelodyneJapaneseSliceBatch::Result result;
                const auto progress=[safe,operation](const juce::String& text)
                {
                    if(operation->cancel.load())return;
                    juce::MessageManager::callAsync([safe,operation,text]
                    {
                        if(!safe||safe->closingUI||operation->cancel||safe->projectRegistrationOperation!=operation)return;
                        safe->registrationInfoText.setText(safe->strings.text("registration.recognizing")+"  "+text,juce::dontSendNotification);
                    });
                };
                recognized=MelodyneJapaneseSliceBatch::recognize(operation->snapshot,facts.variants,operation->japaneseOptions,result,operation->error,cancel,progress)&&result.ready;
                operation->japaneseReport=result.report;
                if(recognized){facts.variants=std::move(result.variants);operation->recognizedSourceHashes=std::move(result.sourceFingerprints);}
            }
            // One candidate, one lease, one OTTO atomic write. No recognition
            // outputs are published or adopted independently of registration.
            if(recognized&&!cancel())
            {
                auto plan=ProjectMaterialRegistration::prepare(operation->snapshot,operation->revision,destination,child,facts.variants,
                    ProjectModel::registrationMusicSourceFingerprint,cancel,scope);
                if(!plan.ready)operation->error=plan.diagnostics.joinIntoString("\n");
                else
                {
                    for(const auto& recording:plan.recordings)for(const auto& alias:recording.aliases)
                        if(const auto old=operation->recognizedSourceHashes.find(alias.getFullPathName());old!=operation->recognizedSourceHashes.end()&&old->second!=recording.sha256)
                            operation->error="Source recording changed between Japanese recognition and material staging";
                    if(operation->error.isEmpty())operation->lease=ProjectMaterialRegistration::stage(plan,operation->error,cancel);
                }
            }
        }
        juce::MessageManager::callAsync([safe,operation]
        {
            if(!safe||safe->closingUI||operation->cancel||safe->projectRegistrationOperation!=operation)return;
            if(operation->worker.joinable())operation->worker.join();
            if(!operation->lease){const auto error=operation->error;safe->cancelProjectMaterialRegistration();safe->showError(error);return;}
            if(safe->project.revisionNumber()!=operation->revision||ProjectModel::registrationMusicSourceFingerprint(safe->project.snapshot())!=operation->fingerprint)
            {safe->cancelProjectMaterialRegistration();safe->showError(safe->strings.text("The project/source snapshot changed. Only the owned unadopted folder was rolled back."));return;}
            safe->publishProjectMaterialRegistration(operation);
        });
    });
}

void MainComponent::publishProjectMaterialRegistration(const std::shared_ptr<ProjectRegistrationOperation>& operation)
{
    if(closingUI||projectRegistrationOperation!=operation||operation->cancel||!operation->lease)return;
    if(operation->worker.joinable())operation->worker.join();juce::Component::SafePointer<MainComponent> safe(this);
    operation->worker=std::thread([safe,operation]
    {
        const bool published=operation->lease->publish(operation->error,[operation]{return operation->cancel.load();});
        juce::MessageManager::callAsync([safe,operation,published]
        {
            if(!safe||safe->closingUI||operation->cancel||safe->projectRegistrationOperation!=operation)return;
            if(operation->worker.joinable())operation->worker.join();
            if(!published){const auto error=operation->error;safe->cancelProjectMaterialRegistration();safe->showError(error);return;}
            const auto& plan=operation->lease->plan();juce::String error;
            if(!safe->project.adoptRegisteredMaterialBindings(operation->revision,operation->fingerprint,plan.owners,plan.bindings,error))
            {safe->cancelProjectMaterialRegistration();safe->showError(error);return;}
            // COMMIT BOUNDARY: no checks, IO or fallible preference write may
            // intervene before this in-memory no-fail permanent retain seam.
            operation->lease->retainAfterAdoption();
            const auto folder=plan.publishedFolder;
            const bool registered=safe->registerRetainedMaterialRoot(folder,error);
            safe->registrationInfoBar.setVisible(false);safe->resized();
            safe->projectRegistrationOperation.reset();safe->projectRegistrationChooser.reset();
            if(!registered)
            {
                if(std::find(safe->projectRegistrationRetryRoots.begin(),safe->projectRegistrationRetryRoots.end(),folder)==safe->projectRegistrationRetryRoots.end())safe->projectRegistrationRetryRoots.push_back(folder);
                safe->showError(safe->strings.text("Project references and the copied audio/OTTO are safely retained at:\n")+folder.getFullPathName()+safe->strings.text("\nLibrary preference registration failed: ")+error+safe->strings.text("\nUse File > Retry material folder registration. No project file was automatically saved."));
            }
            else
            {
                safe->statusLabel.setText(safe->strings.text("Project source materials registered. Musical edits were preserved; choose Save to persist new references."),juce::dontSendNotification);
                if(operation->japaneseReport.isNotEmpty())
                    juce::Logger::writeToLog(operation->japaneseReport);
            }
            safe->menuItemsChanged();
        });
    });
}

bool MainComponent::registerRetainedMaterialRoot(const juce::File& root,juce::String& error)
{
    if(projectRegistrationRootWriterForTest)return projectRegistrationRootWriterForTest(root,error);
    setAssetManagerVisible(true);
    if(!assetManager){error=strings.text("Material manager is unavailable");return false;}
    if(!assetManager->registerMaterialRoot(root,error))return false;
    auto roots=SampleSettings::materialRoots();roots.addIfNotAlreadyThere(root.getFullPathName());SampleSettings::setMaterialRoots(roots);
    // Views only: adoption above already rebound the project from the plan.
    materialSourcesChanged({root.getFullPathName()},false);
    return true;
}
void MainComponent::retryProjectMaterialRootRegistration()
{
    std::vector<juce::File> pending;juce::StringArray errors;
    for(const auto& root:projectRegistrationRetryRoots){juce::String error;if(!registerRetainedMaterialRoot(root,error)){pending.push_back(root);errors.add(root.getFullPathName()+": "+error);}}
    projectRegistrationRetryRoots=std::move(pending);menuItemsChanged();if(!errors.isEmpty())showError(errors.joinIntoString("\n"));
}
}

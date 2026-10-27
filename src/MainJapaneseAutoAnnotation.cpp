// Main orchestration only. Native JapaneseAnnotationBatch owns inference,
// append-only variant planning and the one conflict-protected OTTO publisher.
// Build/CLI registration is owned by the coordinating parent.
#include "MainComponent.h"
#include "JapaneseAutoAnnotationDialog.h"
#include "OtoWaveformEditorComponent.h"
#include "backend/UtauRenderer.h"
#include <set>
#include <algorithm>

namespace hachi
{
struct MainComponent::JapaneseAutoAnnotationState
{
    juce::Component::SafePointer<JapaneseAutoAnnotationDialog> wizard;
    juce::Component::SafePointer<OtoWaveformEditorComponent> originEditor;
    bool stopping = false;
    std::uint64_t expectedRevision = 0;
    juce::File currentAudio, requestedRoot;
    juce::String publicationTrace;
};
namespace
{
constexpr const char* japaneseGateOwner = "japanese-auto-annotation";
juce::String japanesePathKey(const juce::File& file)
{
    auto key = file.getFullPathName().replaceCharacter('\\','/');
#if JUCE_WINDOWS
    key = key.toLowerCase();
#endif
    return key;
}
}

void MainComponent::cancelJapaneseAutoAnnotation()
{
    const auto state = japaneseAutoAnnotationState;
    if (!state) return;
    state->stopping = true;
    if (state->wizard) state->wizard->cancel();
    japaneseAutoAnnotationWindows.clear();
    drainAnnotationWorker(); // accepted closures observe cancellation before shutdown
    setAnnotationPublicationGate(japaneseGateOwner, false); // never releases another owner
    japaneseAutoAnnotationState.reset();
}

void MainComponent::showJapaneseAutoAnnotation(const juce::File& root, const juce::File& currentAudio, OtoWaveformEditorComponent* origin)
{
    if (closingUI) return;
    if (communityDatasetImportBlocksProjectEdits() || importInProgress || exportWaitingForRender || projectRegistrationOperation)
    { showError(strings.text("jaAuto.busy")); return; }
    if (!root.isDirectory() || (currentAudio != juce::File{} && (!currentAudio.existsAsFile() || !currentAudio.isAChildOf(root))))
    { showError(strings.text("jaAuto.unavailable")); return; }
    // Do not silently lose an overview's pending fields or source draft. Users
    // must explicitly save/discard with the existing shared editor first.
    if(origin&&(origin->materialSourceAudio()!=currentAudio||origin->hasMaterialNoteContext()||origin->hasUnpublishedMaterialEdits()))
    {showError(strings.text("jaAuto.saveOrDiscard"));return;}
    constexpr const char* windowKey = "japanese-auto-annotation";
    japaneseAutoAnnotationWindows.removeClosed();
    if (japaneseAutoAnnotationState && japaneseAutoAnnotationState->wizard
        && (japaneseAutoAnnotationState->currentAudio != currentAudio || japaneseAutoAnnotationState->requestedRoot != root
            ||(origin&&japaneseAutoAnnotationState->originEditor.getComponent()!=origin)))
    { showError(strings.text("jaAuto.busy")); return; } // never silently reuse another file/folder's choices
    if (japaneseAutoAnnotationWindows.showExisting(windowKey)) return;
    if (!assetManager) setAssetManagerVisible(true);
    if (!assetManager) { showError(strings.text("jaAuto.unavailable")); return; }
    if (!japaneseAutoAnnotationState) japaneseAutoAnnotationState = std::make_shared<JapaneseAutoAnnotationState>();
    const auto state = japaneseAutoAnnotationState;
    state->currentAudio = currentAudio; state->requestedRoot = root;state->originEditor=origin;
    juce::Component::SafePointer<MainComponent> safe(this);
    JapaneseAutoAnnotationDialog::Host host;
    host.runOnWorker = ownedAnnotationExecutor(); // same Main-owned pool as CSV, never detached/global
    host.publishing = [safe,state](bool acquiring)
    {
        if (!safe || safe->japaneseAutoAnnotationState != state) return;
        if (acquiring && (safe->closingUI || state->stopping)) return;
        safe->setAnnotationPublicationGate(japaneseGateOwner, acquiring);
    };
    host.preflight = [safe,state](const JapaneseAnnotationBatch::Plan& plan, juce::String& error)
    {
        error.clear();
        const auto reject = [&](const juce::String& text) { error = text; return false; };
        if (!safe || safe->closingUI || state->stopping || safe->japaneseAutoAnnotationState != state
            || !safe->annotationPublicationGateHeldBy(japaneseGateOwner))
            return reject(safe ? safe->strings.text("jaAuto.unavailable") : juce::String("Main unavailable"));
        if (safe->importInProgress || safe->exportWaitingForRender || safe->projectRegistrationOperation)
            return reject(safe->strings.text("jaAuto.busy"));
        if (!safe->assetManager || !plan.ready || plan.results.empty() || plan.manifest != plan.root.getChildFile("otto.ini"))
            return reject(safe->strings.text("jaAuto.unavailable"));
        std::set<juce::String> ids;
        for (const auto& result : plan.results)
        {
            if (result.audioId.isEmpty() || !ids.insert(result.audioId).second || !result.audio.isAChildOf(plan.root))
                return reject(safe->strings.text("jaAuto.unavailable"));
            for (const auto& row : result.rows) if (row.materialAudioId != result.audioId)
                return reject(safe->strings.text("jaAuto.unavailable"));
            if(state->originEditor&&state->originEditor->materialSourceAudio()==result.audio&&state->originEditor->hasUnpublishedMaterialEdits())
                return reject(safe->strings.text("jaAuto.saveOrDiscard"));
        }
        // Native append-only publication is independent of existing unbound
        // placements/acoustic analysis, but ONLY with the original OTTO token
        // and byte-identical canonical encodings of every retained section.
        OttoMaterialIO::Document baseline;juce::String proofError,before,after;
        if(plan.manifest.existsAsFile())
        {if(!OttoMaterialIO::loadDocument(plan.manifest,baseline,proofError))return reject(proofError);}
        if(baseline.existed!=plan.candidate.existed||baseline.fingerprint!=plan.candidate.fingerprint)
            return reject(safe->strings.text("jaAuto.previewRequired"));
        for(const auto& old:baseline.audio)if(ids.count(old.id))return reject(safe->strings.text("jaAuto.inUse"));
        auto retained=plan.candidate;std::erase_if(retained.audio,[&](const auto& audio){return ids.count(audio.id)!=0;});
        if(!OttoMaterialIO::encode(baseline,before,proofError)||!OttoMaterialIO::encode(retained,after,proofError)||before!=after)
            return reject(safe->strings.text("jaAuto.previewRequired"));
        for(const auto& result:plan.results)
        {
            const auto candidate=std::find_if(plan.candidate.audio.begin(),plan.candidate.audio.end(),[&](const auto& entry){return entry.id==result.audioId;});
            if(candidate==plan.candidate.audio.end())return reject(safe->strings.text("jaAuto.unavailable"));
            OttoMaterialIO::Document wanted,actual;wanted.audio={{result.audioId,result.audio.getRelativePathFrom(plan.root).replaceCharacter('\\','/'),result.rows}};actual.audio={*candidate};
            if(!OttoMaterialIO::encode(wanted,before,proofError)||!OttoMaterialIO::encode(actual,after,proofError)||before!=after)
                return reject(safe->strings.text("jaAuto.previewRequired"));
        }
        const auto revision = safe->project.revisionNumber();
        const auto data = safe->project.snapshot();
        if (safe->project.revisionNumber() != revision) return reject(safe->strings.text("jaAuto.drift"));
        for (const auto& track : data.tracks)
        {
            // Refuse legacy/ambiguous voicebank resolution rather than falsely
            // claiming appended source timing has been applied to live music.
            if (trackUsesVoicebankSynthesis(track) && track.voicebankDirectory != juce::File{}
                && (japanesePathKey(track.voicebankDirectory) == japanesePathKey(plan.root)
                    || track.voicebankDirectory.isAChildOf(plan.root) || plan.root.isAChildOf(track.voicebankDirectory)))
                return reject(safe->strings.text("jaAuto.inUse"));
            for (const auto& clip : track.clips)
            {
                for (const auto& note : clip.notes)
                    if (ids.count(note.materialAudioId)) return reject(safe->strings.text("jaAuto.inUse"));
                // No physical-path model action follows this append. Existing
                // unbound/foreign native notes remain exactly as they were.

            }
        }
        state->expectedRevision = revision;state->publicationTrace="append-only OTTO proof passed; model references/timing unchanged";
        return true; // Batch owns exact source hashes + original manifest revision token
    };
    host.published = [safe,state](const JapaneseAnnotationBatch::Plan& plan)
    {
        if (!safe || safe->closingUI || state->stopping || safe->japaneseAutoAnnotationState != state)
        { juce::Logger::writeToLog("Japanese annotation committed and retained: " + plan.root.getFullPathName()); return; }
        juce::String error;
        const bool registered = safe->assetManager && safe->assetManager->registerMaterialRoot(plan.root,error);
        backend::UtauRenderer::invalidateVoicebankCache();
        if (!registered)
        {
            if (std::find(safe->projectRegistrationRetryRoots.begin(),safe->projectRegistrationRetryRoots.end(),plan.root) == safe->projectRegistrationRetryRoots.end())
                safe->projectRegistrationRetryRoots.push_back(plan.root);
            safe->showError(safe->strings.text("jaAuto.retained") + "\n" + plan.root.getFullPathName() + "\n" + error);
        }
        else safe->statusLabel.setText(safe->strings.text("jaAuto.success"),juce::dontSendNotification);
        if (safe->project.revisionNumber() != state->expectedRevision)
        {state->publicationTrace+="; independent project revision changed during publication; no annotation rebind performed";juce::Logger::writeToLog(state->publicationTrace);}
        safe->menuItemsChanged();
        const auto configureScoped=[safe](OtoWaveformEditorComponent& editor,const juce::File& audio,const juce::String& audioId)
        {
                editor.setMaterialSourcePublicationCallbacks(
                    [safe,audio,audioId](const std::vector<SampleRegionSetting>& rows)
                    {
                        if(!safe||safe->closingUI)return;
                        safe->materialPublicationCompleted(audio,rows,audioId);
                        if(safe->assetManager){safe->assetManager->refreshMaterialAnnotations(audio);safe->assetManager->selectMaterialRegistration(audio,audioId);}
                    },
                    [safe,audio,audioId](const std::vector<SampleRegionSetting>& rows,juce::String& why)
                    {
                        if(!safe||safe->closingUI){why="Project window closed";return false;}
                        return safe->preflightMaterialPublication(audio,rows,why,audioId);
                    });
        };
        const auto focus=plan.focusAudio!=juce::File{}?plan.focusAudio:state->currentAudio;
        for(const auto& result:plan.results)
        {
            // Shared fan-out: manager node, other open editors, project bindings.
            safe->materialSourcesChanged({result.audio.getFullPathName()},true);
            juce::Component::SafePointer<OtoWaveformEditorComponent> target;
            if(state->originEditor&&state->originEditor->materialSourceAudio()==result.audio)target=state->originEditor;
            else
            {
                std::vector<juce::Component::SafePointer<OtoWaveformEditorComponent>> clean;
                for(const auto& editor:safe->otoPreviews)
                    if(editor&&editor->materialSourceAudio()==result.audio&&!editor->hasMaterialNoteContext()
                        &&editor->editScope()==OtoWaveformEditorComponent::EditScope::source&&!editor->hasUnpublishedMaterialEdits())clean.push_back(editor);
                if(clean.size()==1)target=clean.front();
                else if(focus==result.audio&&safe->assetManager&&safe->assetManager->selectedMaterialAudio()==result.audio)
                {
                    const auto selected=safe->assetManager->selectedMaterialVariantId();
                    for(const auto& editor:clean)if(editor->materialAudioVariantId()==selected){if(target){target=nullptr;break;}target=editor;}
                }
            }
            bool retargeted=false;juce::String refreshError;
            if(target)
            {
                if(safe->assetManager&&safe->assetManager->ownsMaterialEditor(target.getComponent()))
                    retargeted=safe->assetManager->retargetCleanMaterialEditor(*target,plan.manifest,result.audioId,refreshError);
                else
                {
                    const auto key="material-quick|"+result.audio.getFullPathName()+"|"+result.audioId;
                    if(safe->noteOtoWindows.canRekeyContent(target.getComponent(),key))
                    {retargeted=target->retargetCleanSourceVariant(plan.manifest,result.audioId,refreshError);if(retargeted)safe->noteOtoWindows.rekeyContent(target.getComponent(),key);}
                    else refreshError="Another live editor owns the target registration, or the source window no longer belongs to Main";
                }
                if(!retargeted)juce::Logger::writeToLog("Japanese annotation retained; human editor not replaced: "+refreshError);
            }
            if(!retargeted&&focus==result.audio)
            {
                // No ambiguous author window is chosen. Opening is exact-ID;
                // dirty/note-bound contexts remain intact in their own windows.
                safe->openMaterialEditor(result.audio,plan.manifest,false,{},result.audioId);
                target=nullptr;
                for(const auto& editor:safe->otoPreviews)if(editor&&editor->materialSourceAudio()==result.audio
                    &&editor->materialAudioVariantId()==result.audioId&&!editor->hasMaterialNoteContext())
                {if(target){target=nullptr;break;}target=editor;}
            }
            if(target&&(retargeted||target->materialAudioVariantId()==result.audioId))
            {
                const auto audio=result.audio;const auto audioId=result.audioId;
                configureScoped(*target,audio,audioId);
            }
            // Other CLEAN independent source windows retain their chosen author
            // variant, but reload its exact Session token after the folder commit.
            // Dirty and note-bound windows are deliberately not touched.
            for(const auto& other:safe->otoPreviews)
                if(other&&other.getComponent()!=target.getComponent()&&other->materialSourceAudio()==result.audio
                    &&!other->hasMaterialNoteContext()&&other->editScope()==OtoWaveformEditorComponent::EditScope::source&&!other->hasUnpublishedMaterialEdits())
                {
                    const auto sameId=other->materialAudioVariantId();juce::String reloadError;
                    if(sameId.isNotEmpty()&&other->retargetCleanSourceVariant(plan.manifest,sameId,reloadError))configureScoped(*other,result.audio,sameId);
                    else if(reloadError.isNotEmpty())juce::Logger::writeToLog("Clean author session not refreshed: "+reloadError);
                }
            if(focus==result.audio&&safe->assetManager)safe->assetManager->selectMaterialRegistration(result.audio,result.audioId);
        }

    };
    JapaneseAutoAnnotationDialog::Input input;
    input.selectedFolderRoot = root; input.currentAudio = currentAudio;
    auto* dialog = new JapaneseAutoAnnotationDialog(strings,std::move(host),std::move(input)); state->wizard = dialog;
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = strings.text("jaAuto.menu"); options.dialogBackgroundColour = Palette::panel;
    options.escapeKeyTriggersCloseButton = true; options.useNativeTitleBar = true; options.resizable = true; options.content.setOwned(dialog);
    if (auto* window = japaneseAutoAnnotationWindows.show(options,windowKey)) window->setResizeLimits(920,740,1800,1400);
}
}

#include "tests/MainJapaneseAutoAnnotationSmoke.h"

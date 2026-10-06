#pragma once
#include "../MainComponent.h"
#include "../DiffSingerPhonemeEditor.h"
#include "../DiffSingerPronunciationEditor.h"
#include <iostream>

namespace hachi
{
inline void MainComponent::diagnosticLoadDiffSingerBank(const juce::String& trackId, const juce::File& directory)
{
    loadDiffSingerVoicebank(trackId, directory);
}

inline bool MainComponent::diagnosticDiffSingerExpressionLane()
{
    const auto data=project.snapshot();
    const auto& track=data.tracks.front();
    std::vector<juce::String> ids;
    for (const auto& note:track.clips.front().notes) ids.push_back(note.id);
    pianoRoll.setSelectedNoteIds(ids);
    refreshSelectedNoteParameter();
    if (!flagCurveButton.isEnabled()) return false;
    flagCurveButton.setToggleState(true,juce::dontSendNotification);
    flagCurveButton.onClick();
    if (!flagEnvelopeButton.isEnabled()) return false;
    project.setNoteUtauFlagCurve(ids.front(),"DS:DYN",{{0,-120},{.7,0}});
    project.setNoteUtauFlagCurve(ids.front(),"DS:GENC",{{0,-60},{.7,60}});
    project.dispatchPendingMessages(); pianoRoll.diagnosticRefresh();
    flagEnvelopeButton.onClick();
    pianoRoll.setFlagLaneFlag("DS:GENC");
    const auto gender=pianoRoll.flagLaneFlag()=="DS:GENC";
    pianoRoll.setFlagLaneFlag("DS:BREC");
    const auto disabled=pianoRoll.flagLaneFlag()=="DS:GENC";
    pianoRoll.setFlagLaneFlag("DS:PEXP");
    const auto default100=pianoRoll.flagLaneCurveFor(project.snapshot().tracks.front().clips.front().notes.front()).front().value==100;
    pianoRoll.setFlagLaneFlag("DS:DYN"); resized();
    return gender && disabled && default100 && pianoRoll.flagCurveActiveFor(ids.front())
        && pianoRoll.currentTool()==PianoRollComponent::Tool::flagCurve;
}

struct DiffSingerUiSmoke : juce::Timer, std::enable_shared_from_this<DiffSingerUiSmoke>
{
    MainComponent component;
    juce::File folder;
    std::function<void(bool)> done;
    std::shared_ptr<DiffSingerUiSmoke> keep;
    juce::File targetBank;
    juce::String targetTrack;
    std::uint64_t revision = 0;
    int phase = 0, ticks = 0;
    bool ok = true;
    std::vector<PitchCurveEditPoint> lockedPitch, lockedReference;
    juce::String originalLyric;
    void start(const juce::File& projectFile)
    {
        keep = shared_from_this();
        juce::String error;
        auto& model = component.diagnosticProject();
        if (!model.load(projectFile, error)) { finish(false); return; }
        auto data = model.snapshot();
        for (auto& track : data.tracks) for (auto& clip : track.clips) for (auto& note : clip.notes)
        { note.pitchControlPoints.clear(); note.contour.clear(); }
        targetBank = data.tracks.front().voicebankDirectory;
        targetTrack = data.tracks.front().id;
        data.tracks.front().voicebankDirectory = juce::File{};
        model.replace(data); model.dispatchPendingMessages();
        component.diagnosticSelectTrack(data.tracks.front().id);
        component.diagnosticRefreshControls(); component.setSize(1400, 860);
        phase = -1;
        component.diagnosticLoadDiffSingerBank(targetTrack, targetBank);
        ok = component.diagnosticEditMenuItemEnabled(54).value_or(false);
        startTimer(100);
    }
    void timerCallback() override
    {
        if (++ticks > 1200) { finish(false); return; }
        if (component.diagnosticEditMenuItemEnabled(54).value_or(false)) return;
        auto& model = component.diagnosticProject();
        if (phase == -1)
        {
            ok = model.snapshot().tracks.front().voicebankDirectory == targetBank
                && model.snapshot().tracks.front().utauMode == UtauMode::mou && ok;
            ok = component.diagnosticStatusText().contains(juce::String::fromUTF8("预检通过")) && ok;
            std::cout << "ui_preflight_load=" << ok << "|status=" << component.diagnosticStatusText() << std::endl;
            component.diagnosticRefreshControls();
            ok = component.diagnosticEditMenuItemEnabled(53).value_or(false) && ok;
            revision = model.revisionNumber();
            phase = 0;
            component.diagnosticChooseMenuItem(53);
        }
        else if (phase == 0)
        {
            ok = model.revisionNumber() == revision + 1 && ok;
            const auto data = model.snapshot();
            for (const auto& note : data.tracks.front().clips.front().notes)
                ok = note.pitchControlPoints.size() > 100
                    && note.diffSingerPitchReference.size()==note.pitchControlPoints.size()
                    && std::count_if(note.pitchControlPoints.begin(),note.pitchControlPoints.end(),
                        [](const auto& p){return !p.diffSingerRestoreSupport;})<=16 && ok;
            component.diagnosticRefreshControls();
            ok = component.diagnosticDiffSingerExpressionLane() && ok;
            const auto screenshot = component.createComponentSnapshot(component.getLocalBounds());
            if (auto stream = folder.getChildFile("DiffSinger-editor.png").createOutputStream())
                juce::PNGImageFormat().writeImageToStream(screenshot, *stream);
            std::cout << "ui_generate=" << ok << "|status=" << component.diagnosticStatusText() << std::endl;
            revision = model.revisionNumber(); phase = 1;
            component.diagnosticChooseMenuItem(53);
            component.diagnosticChooseMenuItem(54);
        }
        else if (phase == 1)
        {
            ok = model.revisionNumber() == revision && ok;
            std::cout << "ui_cancel_preserves_project=" << ok << std::endl;
            ok = component.diagnosticEditMenuItemEnabled(55).value_or(false) && ok;
            component.diagnosticSelectNotes({model.snapshot().tracks.front().clips.front().notes.back().id});
            phase = 2; component.diagnosticChooseMenuItem(55);
        }
        else if (phase == 2)
        {
            auto* dialog = dynamic_cast<juce::DialogWindow*>(juce::Component::getCurrentlyModalComponent());
            auto* editor = dialog != nullptr ? dynamic_cast<DiffSingerPhonemeEditor*>(dialog->getContentComponent()) : nullptr;
            if (editor == nullptr) { finish(false); return; }
            const auto rows = editor->phonemes();
            ok = rows.size() == 6 && editor->displayedPhonemes().size()==2
                && editor->timings().size()==1 && ok;
            if (rows.size() != 6) { dialog->exitModalState(0); finish(false); return; }
            ok = editor->moveBoundary(3,rows[3].start-.06) && ok;
            if (auto stream=folder.getChildFile("DS-phoneme-dialog.png").createOutputStream())
                juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*stream);
            for (auto* child : editor->getChildren())
                if (auto* button=dynamic_cast<juce::TextButton*>(child); button != nullptr
                    && button->getButtonText()==juce::String::fromUTF8("应用")) { button->onClick(); break; }
            ok = model.revisionNumber()==revision+1 && ok;
            ok = model.snapshot().tracks.front().clips.front().notes.back().diffSingerTiming.isNotEmpty() && ok;
            std::cout << "ui_phoneme_dialog_apply=" << ok << std::endl;
            // Closing a JUCE modal is asynchronous. In a fast GPU worker the
            // next preview may finish before the old modal has left the stack.
            // Start the next user action only after the previous window closes.
            phase = 7;
        }
        else if (phase == 7)
        {
            if (juce::Component::getCurrentlyModalComponent() != nullptr) return;
            phase = 3;
            component.diagnosticChooseMenuItem(56);
        }
        else if (phase == 3)
        {
            auto* dialog = dynamic_cast<juce::DialogWindow*>(juce::Component::getCurrentlyModalComponent());
            auto* editor = dialog != nullptr ? dynamic_cast<DiffSingerPronunciationEditor*>(dialog->getContentComponent()) : nullptr;
            if (editor == nullptr) { finish(false); return; }
            ok = editor->readings().size()==1 && editor->previewText("1").contains("zh/w") && ok;
            originalLyric = model.snapshot().tracks.front().clips.front().notes.back().label;
            editor->setReading("1", "[zh/b zh/ang]");
            phase=4; editor->validate(false);
        }
        else if (phase == 4)
        {
            auto* dialog = dynamic_cast<juce::DialogWindow*>(juce::Component::getCurrentlyModalComponent());
            auto* editor = dialog != nullptr ? dynamic_cast<DiffSingerPronunciationEditor*>(dialog->getContentComponent()) : nullptr;
            if (editor == nullptr) { finish(false); return; }
            ok = editor->previewText("1").contains("zh/b") && ok;
            if (auto stream=folder.getChildFile("DS-pronunciation-dialog.png").createOutputStream())
                juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),*stream);
            revision=model.revisionNumber(); phase=5; editor->validate(true);
        }
        else if (phase == 5)
        {
            auto data=model.snapshot();
            const auto& note=data.tracks.front().clips.front().notes.back();
            ok = model.revisionNumber()==revision+1 && note.label==originalLyric
                && note.diffSingerPronunciation=="[zh/b zh/ang]" && note.diffSingerTiming.isEmpty() && ok;
            juce::String error;
            const auto saved=folder.getChildFile("pronunciation-roundtrip.hjpx");
            ok=model.save(saved,error)&&ok; ProjectModel reopened;
            ok=reopened.load(saved,error)&&ok;
            ok=reopened.snapshot().tracks.front().clips.front().notes.back().diffSingerPronunciation=="[zh/b zh/ang]"&&ok;
            model.undo();
            ok=model.snapshot().tracks.front().clips.front().notes.back().diffSingerPronunciation.isEmpty()&&ok;
            model.redo();
            ok=model.snapshot().tracks.front().clips.front().notes.back().diffSingerPronunciation=="[zh/b zh/ang]"&&ok;
            lockedPitch=model.snapshot().tracks.front().clips.front().notes.front().pitchControlPoints;
            lockedReference=model.snapshot().tracks.front().clips.front().notes.front().diffSingerPitchReference;
            std::cout<<"ui_pronunciation_preview_apply_save_undo="<<ok<<std::endl;
            revision=model.revisionNumber(); phase=6;
            component.diagnosticChooseMenuItem(53);
        }
        else if (phase == 6)
        {
            const auto data=model.snapshot();
            const auto& first=data.tracks.front().clips.front().notes.front();
            ok=model.revisionNumber()==revision+1 && first.pitchControlPoints.size()==lockedPitch.size()&&ok;
            for (size_t i=0;i<std::min(first.pitchControlPoints.size(),lockedPitch.size());++i)
                ok=first.pitchControlPoints[i].timeSeconds==lockedPitch[i].timeSeconds
                    && first.pitchControlPoints[i].targetMidi==lockedPitch[i].targetMidi&&ok;
            ok=data.tracks.front().clips.front().notes.back().pitchControlPoints.size()>100&&ok;
            ok=first.diffSingerPitchReference.size()==lockedReference.size()&&ok;
            for(size_t i=0;i<std::min(first.diffSingerPitchReference.size(),lockedReference.size());++i)
                ok=first.diffSingerPitchReference[i].timeSeconds==lockedReference[i].timeSeconds
                    &&first.diffSingerPitchReference[i].targetMidi==lockedReference[i].targetMidi&&ok;
            const auto& last=data.tracks.front().clips.front().notes.back();
            ok=last.diffSingerPitchReference.size()==last.pitchControlPoints.size()&&ok;
            for(size_t i=0;i<std::min(last.diffSingerPitchReference.size(),last.pitchControlPoints.size());++i)
                ok=last.diffSingerPitchReference[i].targetMidi==last.pitchControlPoints[i].targetMidi&&ok;
            const auto insideHandles=std::count_if(last.pitchControlPoints.begin(),last.pitchControlPoints.end(),
                [&](const auto& p){return !p.diffSingerRestoreSupport&&p.timeSeconds>=0&&p.timeSeconds<=last.durationSeconds;});
            ok=insideHandles<=16&&ok;
            std::cout<<"ui_compact_regenerated_handles="<<insideHandles<<"|passed="<<ok<<std::endl;
            std::cout<<"ui_reference_generation_and_local_retake="<<ok<<std::endl;
            std::cout<<"ui_local_pitch_preserves_other_note="<<ok<<"|status="<<component.diagnosticStatusText()<<std::endl;
            finish(ok);
        }
    }
    void finish(bool success)
    {
        stopTimer();
        if (!success)
        {
            auto* modal = juce::Component::getCurrentlyModalComponent();
            std::cout << "ui_failure_phase=" << phase << "|modal=" << (modal ? modal->getName() : juce::String("none"))
                << "|status=" << component.diagnosticStatusText() << std::endl;
        }
        auto self = keep;
        juce::MessageManager::callAsync([self, success]
        { self->done(success); self->keep.reset(); });
    }
};
inline void runDiffSingerUiSmoke(const juce::File& project, const juce::File& folder,
                                 std::function<void(bool)> done)
{
    auto test = std::make_shared<DiffSingerUiSmoke>();
    test->folder = folder; folder.createDirectory(); test->done = std::move(done);
    test->start(project);
}
}

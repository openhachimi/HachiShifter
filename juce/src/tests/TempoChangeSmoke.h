#pragma once
#include "../TimelineComponent.h"
#include <iostream>

namespace hachi
{
// Exercise both the model invariant and the actual modal dialog callback.
struct TempoChangeSmoke : std::enable_shared_from_this<TempoChangeSmoke>
{
    ProjectModel model;
    TimelineComponent timeline { model };
    juce::Component::SafePointer<juce::AlertWindow> dialog;
    std::function<void(bool)> completion;
    juce::File screenshot;
    bool ok = true;
    int phase = 0, waits = 0;
    juce::int64 beforeDialog = 0;

    static ProjectData fixture()
    {
        ProjectData data;
        ClipData clip;
        clip.id = "tempo-clip"; clip.startSeconds = 0.5; clip.durationSeconds = 4.5;
        clip.fadeInSeconds = 0.1; clip.fadeOutSeconds = 0.2;
        clip.sourceTimeMap = { { 0.0, 0.0 }, { 2.0, 1.5 }, { 4.5, 4.0 } };
        const double starts[] = { 0.0, 1.0, 2.0, 3.5 };
        const double lengths[] = { 0.5, 1.0, 0.5, 0.5 };
        for (int i = 0; i < 4; ++i)
        {
            NoteData note;
            note.id = "tempo-note-" + juce::String(i); note.label = "a";
            note.startSeconds = starts[i]; note.durationSeconds = lengths[i];
            note.consonantSeconds = 0.05;
            note.contour = { { 0.0, 0.0f }, { 0.25, 30.0f } };
            note.pitchControlPoints = { { -0.1, 60.0f }, { 0.25, 62.0f } };
            note.amplitudeEnvelope = { { -0.1, -60.0f, true }, { 0.25, 0.0f, false } };
            note.sibilantMarkers = { 0.1 };
            note.utauOto.enabled = true; note.utauOto.offsetMs = 125;
            note.vibratoEnabled = true; note.vibratoEndPercent = 80;
            note.nativeSegments = { { "vowel", "a", NativeSegmentRole::vowel, 0.0, 0.25 } };
            clip.notes.push_back(note);
        }
        TrackData track; track.id = "tempo-track"; track.compose = true;
        track.pitchAlgorithm = PitchAlgorithm::utau; track.clips = { clip };
        data.tracks.push_back(track);
        track.id = "recorded-track"; track.compose = false;
        track.clips[0].id = "recorded-clip";
        for (auto& note : track.clips[0].notes) note.id += "-recorded";
        data.tracks.push_back(track);
        data.nativeConnections = { { "tempo-join", "tempo-note-1", "tempo-note-2",
            "pitch-and-amplitude", 2.5, { { 0.0, 60.0f } }, { { 0.0, 0.0f } } } };
        return data;
    }

    void expect(const char* name, bool value)
    {
        std::cout << name << "=" << value << std::endl;
        ok = ok && value;
    }

    static juce::int64 fingerprint(const ProjectData& data)
    {
        ProjectModel copy; copy.replace(data); return copy.contentFingerprint();
    }

    static bool sameExceptTempo(ProjectData actual, const ProjectData& before)
    {
        actual.bpm = before.bpm;
        actual.tempoChanges = before.tempoChanges;
        return fingerprint(actual) == fingerprint(before);
    }

    void checkModel()
    {
        for (const auto& edit : std::vector<std::pair<double, double>> {
            { 4.0, 60.0 }, { 4.0, 240.0 }, { 8.0, 90.0 }, { 0.0, 75.0 } })
        {
            auto before = fixture();
            before.beatOriginSeconds = 0.25;
            before.tempoChanges = { { 8.0, 180.0 } };
            model.replace(before);
            const auto original = model.contentFingerprint();
            model.setTempoChange(edit.first, edit.second, false);
            const auto after = model.snapshot();
            const auto changed = model.contentFingerprint();
            expect("tempo_only_keeps_all_content", sameExceptTempo(after, before));
            expect("tempo_map_changes", after.tempoAtQuarterPosition(edit.first) == edit.second
                && changed != original);
            expect("tempo_only_undo", model.undo() && model.contentFingerprint() == original);
            expect("tempo_only_redo", model.redo() && model.contentFingerprint() == changed);
            const auto revision = model.revisionNumber();
            model.setTempoChange(edit.first, edit.second, false);
            expect("identical_tempo_is_noop", model.revisionNumber() == revision);
        }
        auto file = juce::File::createTempFile(".hjpx");
        juce::String error;
        ProjectModel reopened;
        expect("tempo_only_save_reload", model.save(file, error) && reopened.load(file, error)
            && reopened.contentFingerprint() == model.contentFingerprint());
        file.deleteFile();

        model.replace(fixture());
        const auto original = model.contentFingerprint();
        model.setTempoChange(4.0, 60.0); // default must retain the old behaviour
        const auto changed = model.snapshot();
        const auto& clip = changed.tracks[0].clips[0];
        expect("sync_before_marker_unchanged", clip.notes[0].startSeconds == 0.0
            && clip.notes[0].durationSeconds == 0.5);
        expect("sync_crossing_note_stretches", clip.notes[1].startSeconds == 1.0
            && clip.notes[1].durationSeconds == 1.5);
        expect("sync_later_note_moves_and_stretches", clip.notes[2].startSeconds == 2.5
            && clip.notes[2].durationSeconds == 1.0 && clip.durationSeconds == 7.5);
        expect("sync_curves_stretch", clip.notes[2].contour[1].timeSeconds == 0.5
            && clip.notes[2].pitchControlPoints[1].timeSeconds == 0.5
            && clip.notes[2].amplitudeEnvelope[1].timeSeconds == 0.5);
        expect("sync_recording_unchanged", changed.tracks[1].clips[0].durationSeconds == 4.5
            && changed.tracks[1].clips[0].notes[2].durationSeconds == 0.5);
        const auto synced = model.contentFingerprint();
        expect("sync_undo", model.undo() && model.contentFingerprint() == original);
        expect("sync_redo", model.redo() && model.contentFingerprint() == synced);
    }

    void nextDialog()
    {
        if (phase == 4) { completion(ok); return; }
        model.replace(fixture());
        beforeDialog = model.contentFingerprint();
        timeline.diagnosticRefresh();
        timeline.diagnosticShowTempoDialog(4.0);
        dialog = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        if (dialog == nullptr) { expect("dialog_exists", false); completion(false); return; }
        auto* choice = dialog->getComboBoxComponent("noteTiming");
        expect("dialog_has_both_modes_and_legacy_default", choice != nullptr
            && choice->getNumItems() == 2 && choice->getSelectedItemIndex() == 0);
        if (choice != nullptr && phase != 1)
            choice->setSelectedItemIndex(1, juce::dontSendNotification);
        dialog->getTextEditor("bpm")->setText(phase == 3 ? "19" : "60");
        if (phase == 0 && screenshot != juce::File())
        {
            screenshot.getParentDirectory().createDirectory();
            auto stream = screenshot.createOutputStream();
            juce::PNGImageFormat png;
            expect("dialog_screenshot", stream != nullptr
                && png.writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()), *stream));
        }
        dialog->exitModalState(phase == 2 ? 0 : 1);
        waits = 0;
        juce::MessageManager::callAsync([self = shared_from_this()] { self->afterDialog(); });
    }

    void afterDialog()
    {
        // Wait for the real modal callback to apply the edit and delete the dialog.
        if (dialog != nullptr)
        {
            if (++waits > 400) { expect("dialog_callback_completed", false); completion(false); return; }
            juce::MessageManager::callAsync([self = shared_from_this()] { self->afterDialog(); });
            return;
        }
        const auto after = model.snapshot();
        if (phase == 0)
            expect("dialog_tempo_only_applied", sameExceptTempo(after, fixture())
                && after.tempoAtQuarterPosition(4.0) == 60.0);
        else if (phase == 1)
            expect("dialog_sync_applied", after.tracks[0].clips[0].notes[2].durationSeconds == 1.0
                && after.tempoAtQuarterPosition(4.0) == 60.0);
        else
            expect(phase == 2 ? "dialog_cancel_unchanged" : "invalid_bpm_unchanged",
                model.contentFingerprint() == beforeDialog);
        ++phase;
        nextDialog();
    }
};

inline void runTempoChangeSmoke(std::function<void(bool)> completion, const juce::File& screenshot)
{
    auto check = std::make_shared<TempoChangeSmoke>();
    check->completion = std::move(completion);
    check->screenshot = screenshot;
    check->checkModel();
    check->nextDialog();
}
}

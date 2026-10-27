#pragma once
// Included ONLY by MainCommunityDatasetImport.cpp: exercises its real state,
// production manager action, Main Host/executor/gate and real folder registration.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>

namespace hachi
{
void MainComponent::diagnosticCommunityDatasetImportUi(const juce::File& output, std::function<void(bool)> finished)
{
    struct Run final : juce::Timer, std::enable_shared_from_this<Run>
    {
        std::shared_ptr<Run> self;
        std::function<void()> tick;
        std::function<void(bool)> done;
        juce::File base, root, source, csv;
        juce::String log, sourceHash, csvHash;
        juce::int64 musicHash = 0;
        CommunityDatasetImportDialog::Input input;
        bool ok = true; int stage = 0; double deadline = 0; std::uint64_t revision = 0;
        void check(const char* name, bool pass)
        { ok &= pass; log += juce::String(name) + "=" + (pass ? "PASS" : "FAIL") + "\n"; }
        void timerCallback() override { auto keep = shared_from_this(); if (tick) tick(); }
        void finish()
        {
            auto keep = shared_from_this(); stopTimer(); tick = {};
            const bool written = base.getChildFile("verification.txt").replaceWithText(log);
            std::cout << "main_community_import=" << ok << "|fixture=" << base.getFullPathName() << std::endl;
            if (!ok) std::cerr << log << std::endl;
            auto callback = std::move(done); self.reset(); if (callback) callback(ok && written);
        }
    };
    auto run = std::make_shared<Run>(); run->self = run; run->done = std::move(finished);
    run->base = output.getChildFile("main-community-import-" + juce::Uuid().toString());
    run->root = run->base.getChildFile("dataset"); run->source = run->root.getChildFile("wavs/one.wav");
    run->csv = run->root.getChildFile("transcriptions.csv");
    if (run->source.getParentDirectory().createDirectory().failed()) { run->check("fixture_directory", false); run->finish(); return; }
    juce::AudioBuffer<float> pcm(1, 24000); pcm.clear(); auto stream = run->source.createOutputStream(); juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(stream ? wav.createWriterFor(stream.release(), 48000, 1, 16, {}, 0) : nullptr);
    run->check("synthetic_CSV_format_fixture_only_no_voice_or_audio_oracle", writer && writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples())); writer.reset();
    run->check("CSV_dictionary_fixture", run->csv.replaceWithText("name,ph_seq,ph_dur\none,k a,0.1 0.4\n")
        && run->root.getChildFile("dictionary.txt").replaceWithText("ka\tk a\n"));
    if (!run->ok) { run->finish(); return; }
    run->input.root = run->root; run->input.dictionary = run->root.getChildFile("dictionary.txt");
    run->input.options.confirmNucleusAsAlignmentAndConsonant = true;
    run->input.options.specials = CommunityDatasetIO::Specials::skipWithReport;
    run->input.provenanceNotes = "Generated silence; CSV-format/GUI fixture only, not ASR, human voice or audio oracle.";
    run->sourceHash = juce::SHA256(run->source).toHexString(); run->csvHash = juce::SHA256(run->csv).toHexString();
    audio.stop();
    ProjectData seed; seed.bpm = 137.0; TrackData keptTrack; keptTrack.id = "csv-existing-music";
    ClipData keptClip; keptClip.id = "csv-existing-clip"; keptClip.sourceFile = run->base.getChildFile("independent-recording.wav");
    NoteData keptNote; keptNote.id = "csv-existing-note"; keptNote.materialAudioId = "explicit-existing-variant"; keptNote.label = "keep";
    keptClip.notes.push_back(keptNote); keptTrack.clips.push_back(keptClip); seed.tracks.push_back(keptTrack); project.resetDocument(seed);
    setAssetManagerVisible(true);
    const auto menu = assetManager->diagnosticCommunityDatasetImportMenu(); int entries = 0; bool enabled = false;
    for (juce::PopupMenu::MenuItemIterator it(menu); it.next();) { ++entries; enabled = it.getItem().isEnabled; }
    run->check("one_production_manager_CSV_action_bound_to_Main", entries == 1 && enabled
        && assetManager->diagnosticRequestCommunityDatasetImport(run->root));
    auto* wizard = communityDatasetImportState ? communityDatasetImportState->wizard.getComponent() : nullptr;
    run->check("production_Main_owned_dialog_and_executor_created", wizard && bool(communityDatasetImportState));
    if (!wizard) { run->finish(); return; }
    juce::String error;
    run->check("explicit_input", wizard->setInput(run->input, error));
    run->musicHash = project.contentFingerprint(); run->revision = project.revisionNumber();
    wizard->preview(); run->deadline = juce::Time::getMillisecondCounterHiRes() + 30000;
    juce::Component::SafePointer<MainComponent> safe(this);
    run->tick = [safe,run]
    {
        if (!safe) { run->check("Main_lifetime", false); run->finish(); return; }
        auto* ui = safe->communityDatasetImportState ? safe->communityDatasetImportState->wizard.getComponent() : nullptr;
        if (!ui || juce::Time::getMillisecondCounterHiRes() > run->deadline)
        { run->check("async_completion_before_timeout", false); if (ui) ui->cancel(); run->finish(); return; }
        if (ui->busy()) return;
        juce::String error;
        const auto manifest = run->root.getChildFile("otto.ini");
        if (run->stage == 0)
        {
            run->check("real_worker_preview_is_read_only", ui->previewReady() && !manifest.existsAsFile());
            auto image = run->base.getChildFile("production-main-wizard-preview.png").createOutputStream(); juce::PNGImageFormat png;
            run->check("actual_production_wizard_capture", image && png.writeImageToStream(ui->createComponentSnapshot(ui->getLocalBounds()), *image));
            for (const auto language : {I18n::Language::zhCN,I18n::Language::zhTW,I18n::Language::jaJP,I18n::Language::koKR,I18n::Language::enUS})
            {
                I18n localized; localized.setLanguage(language); bool translated = true;
                const auto verifyKey = [&](const juce::String& key)
                {
                    const bool found = localized.text(key).isNotEmpty() && localized.text(key) != key;
                    translated &= found;
                    if (!found) run->log += "missing_catalog_row=" + key + "|language=" + juce::String(static_cast<int>(language)) + "\n";
                };
                for (int i = 0; i < 27; ++i) verifyKey("csv.ui" + juce::String(i));
                for (const auto* key : {"csv.menu","csv.wait","csv.success"}) verifyKey(key);
                run->check("five_language_CSV_controls_have_catalog_rows", translated);
            }
            auto changed = run->input; changed.provenanceNotes += " changed";
            run->check("stale_input_rejects_publish", ui->setInput(changed,error) && !ui->publishConfirmed(error) && !manifest.existsAsFile());
            ui->setInput(run->input,error); ui->preview(); ui->cancel(); run->stage = 10;
        }
        else if (run->stage == 10)
        {
            run->check("cancelled_production_preview_preserves_source", !ui->previewReady() && !manifest.existsAsFile()
                && juce::SHA256(run->source).toHexString() == run->sourceHash);
            ui->preview(); run->stage = 1;
        }
        else if (run->stage == 1)
        {
            run->check("preview_again_ready", ui->previewReady());
            run->csv.appendText("\n");
            run->check("publication_gate_acquired", ui->publishConfirmed(error) && safe->communityDatasetImportBlocksProjectEdits() && !safe->isEnabled());
            bool closed = false; safe->requestClose([&closed] { closed = true; });
            run->check("normal_close_is_gated", !closed); run->stage = 2;
        }
        else if (run->stage == 2)
        {
            run->check("source_hash_change_rejects_without_OTTO", !manifest.existsAsFile() && !safe->communityDatasetImportBlocksProjectEdits() && safe->isEnabled());
            run->csv.replaceWithText("name,ph_seq,ph_dur\none,k a,0.1 0.4\n");
            ui->setInput(run->input,error); ui->preview(); run->stage = 3;
        }
        else if (run->stage == 3)
        {
            run->check("valid_preview_after_stale_source", ui->previewReady());
            run->check("guarded_publish_accepted", ui->publishConfirmed(error)); run->stage = 4;
        }
        else if (run->stage == 4)
        {
            run->check("real_atomic_publish_and_gate_release", manifest.existsAsFile() && !safe->communityDatasetImportBlocksProjectEdits() && safe->isEnabled());
            run->check("real_manager_registered_root", safe->assetManager->rootForAudio(run->source) == run->root);
            run->check("music_tempo_variants_revision_unchanged", safe->project.contentFingerprint() == run->musicHash && safe->project.revisionNumber() == run->revision);
            run->check("source_PCM_and_CSV_unchanged", juce::SHA256(run->source).toHexString() == run->sourceHash && juce::SHA256(run->csv).toHexString() == run->csvHash);
            // Exercise the REAL manager's persistence failure, not a fake root
            // writer: a regular file blocks creation of the preference directory.
            const auto blocked = run->base.getChildFile("blocked-preferences-parent"); blocked.replaceWithText("not a directory");
            juce::PropertiesFile::Options prefsOptions; prefsOptions.applicationName = "csv-failure-test";
            prefsOptions.storageFormat = juce::PropertiesFile::storeAsXML;
            juce::PropertiesFile failedPreferences(blocked.getChildFile("preferences.xml"), prefsOptions);
            {
                AssetManagerComponent failedManager(safe->strings, failedPreferences);
                const auto committed = juce::SHA256(manifest).toHexString();
                run->check("real_manager_preferences_failure_retains_committed_source", !failedManager.registerMaterialRoot(run->root,error)
                    && error.isNotEmpty() && juce::SHA256(manifest).toHexString() == committed
                    && juce::SHA256(run->source).toHexString() == run->sourceHash);
            }
            // Replace preview remains explicit. Seed a carried note with the exact
            // preview registration; Main must reject rather than apply timing.
            run->input.options.replacePreviouslyImportedAudio = true;
            ui->setInput(run->input,error); ui->preview(); run->stage = 5;
        }
        else if (run->stage == 5)
        {
            const auto* plan = ui->previewPlan();
            run->check("explicit_replacement_preview", plan && !plan->audio.empty());
            if (!plan || plan->audio.empty()) { run->finish(); return; }
            ProjectData data; TrackData track; track.id = "csv-in-use"; ClipData clip; clip.id = "csv-carried"; clip.sourceFile = run->source;
            NoteData note; note.id = "csv-bound-note"; note.materialAudioId = plan->audio.front().audioId; clip.notes.push_back(note); track.clips.push_back(clip); data.tracks.push_back(track);
            safe->project.resetDocument(data); const auto hash = safe->project.contentFingerprint(); const auto before = juce::SHA256(manifest).toHexString();
            run->check("in_use_ID_preflight_rejects_without_rewriting_music_or_source", !ui->publishConfirmed(error) && error.isNotEmpty()
                && safe->project.contentFingerprint() == hash && juce::SHA256(manifest).toHexString() == before && !safe->communityDatasetImportBlocksProjectEdits());
            // Same file with no identity must not silently choose a first variant.
            data.tracks[0].clips[0].notes[0].materialAudioId.clear(); safe->project.resetDocument(data);
            run->check("ambiguous_carried_identity_rejects", !ui->publishConfirmed(error) && error.isNotEmpty() && juce::SHA256(manifest).toHexString() == before);
            safe->communityDatasetImportWindows.clear(); run->finish();
        }
    };
    run->startTimer(20);
}
}

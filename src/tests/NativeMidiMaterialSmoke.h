#pragma once
#include "../ProjectModel.h"
#include <cmath>
#include <iostream>

namespace hachi
{
// Model/standard-MIDI only: no picker, material writer, audio engine or NN.
// Main/CLI wiring is intentionally owned by the integrating caller.
inline bool runNativeMidiMaterialSmoke(const juce::File& output)
{
    if (!output.createDirectory()) return false;
    bool ok = true;
    int checks = 0;
    juce::Array<juce::var> results;
    const auto check = [&](const char* name, bool pass)
    {
        ok &= pass;
        ++checks;
        std::cout << name << '=' << pass << std::endl;
        auto* row = new juce::DynamicObject();
        row->setProperty("name", name);
        row->setProperty("pass", pass);
        results.add(juce::var(row));
    };
    const auto near = [](double a, double b) { return std::abs(a - b) < 1.e-6; };
    const auto addNote = [](juce::MidiMessageSequence& sequence, int pitch, double tick)
    {
        sequence.addEvent(juce::MidiMessage::noteOn(1, pitch, static_cast<juce::uint8>(100)), tick);
        sequence.addEvent(juce::MidiMessage::noteOff(1, pitch), tick + 240);
    };
    const auto addText = [](juce::MidiMessageSequence& sequence, int type,
                            const juce::String& text, double tick)
    {
        sequence.addEvent(juce::MidiMessage::textMetaEvent(type, text), tick);
    };
    const auto save = [](const juce::MidiFile& midi, const juce::File& file)
    {
        juce::MemoryOutputStream bytes;
        return midi.writeTo(bytes, 1) && file.replaceWithData(bytes.getData(), bytes.getDataSize());
    };
    const auto timed = [](double tick)
    {
        return tick <= 1920.0 ? tick / 960.0 : 2.0 + (tick - 1920.0) / 480.0;
    };
    const auto fixtureTick = [](int pitch)
    {
        if (pitch == 80) return 2400.0;
        return (pitch >= 69 ? pitch - 61 : pitch - 60) * 480.0;
    };
    const auto nativeScore = [](const TrackData& track, const juce::File& file)
    {
        return effectiveTrackType(track) == TrackType::hachi && track.compose
            && !track.accompaniment && track.clips.size() == 1
            && track.clips[0].sourceFile == file;
    };
    juce::String error;
    const auto lyric = juce::String::fromUTF8("歌詞");
    juce::MidiFile midi;
    midi.setTicksPerQuarterNote(480);
    juce::MidiMessageSequence conductor, lead, harmony;
    conductor.addEvent(juce::MidiMessage::tempoMetaEvent(500000), 0);
    conductor.addEvent(juce::MidiMessage::tempoMetaEvent(1000000), 1920);
    addText(conductor, 5, "wrong track", 4320);
    midi.addTrack(conductor);
    addText(lead, 3, "Lead", 0);
    for (int pitch = 60; pitch <= 72; ++pitch)
        addNote(lead, pitch, fixtureTick(pitch));
    addText(lead, 1, "ordinary Text is not Lyric", 0);
    addText(lead, 5, "", 480);
    addText(lead, 5, " \t ", 960);
    addText(lead, 5, "too early", 1439);
    addText(lead, 5, "too late", 1921);
    addText(lead, 5, lyric, 2400); // inserted after the equal-onset note
    addText(lead, 5, "same onset", 2880);
    addText(lead, 5, "first competing lyric", 3360);
    addText(lead, 5, "second competing lyric", 3360);
    addText(lead, 5, "ambiguous chord", 3840);
    addText(lead, 5, "recovered", 4800);
    midi.addTrack(lead);
    addText(harmony, 3, "Harmony", 0);
    addText(harmony, 5, "independent", 2400); // lyric inserted before its note
    addNote(harmony, 80, 2400);
    midi.addTrack(harmony);
    const auto file = output.getChildFile("not-a-lyric-name.midi");
    check("write_standard_type1_midi_with_real_0x05_events", save(midi, file));
    const auto choices = ProjectModel::midiTrackChoices(file, error);
    check("choices_skip_lyric_only_conductor_and_keep_indices_names_counts",
          choices.size() == 2 && choices[0].index == 1 && choices[0].name == "Lead"
          && choices[0].noteCount == 13 && choices[1].index == 2
          && choices[1].name == "Harmony" && choices[1].noteCount == 1);
    ProjectModel whole;
    whole.resetDocument(ProjectData{});
    const auto before = whole.contentFingerprint();
    check("whole_file_import", whole.addMidiFile(file, error));
    const auto all = whole.snapshot();
    check("whole_import_keeps_hachi_and_mid_source_for_existing_replacement",
          all.tracks.size() == 2 && nativeScore(all.tracks[0], file) && nativeScore(all.tracks[1], file));
    const auto expected = [&](int pitch) -> juce::String
    {
        if (pitch == 65) return lyric;
        if (pitch == 66) return "same onset";
        if (pitch == 71) return "recovered";
        if (pitch == 80) return "independent";
        return "-";
    };
    bool labels = all.tracks.size() == 2 && nativeScore(all.tracks[0], file)
        && nativeScore(all.tracks[1], file) && all.tracks[0].clips[0].notes.size() == 13
        && all.tracks[1].clips[0].notes.size() == 1;
    bool clock = labels;
    const auto imported = whole.contentFingerprint();
    for (const auto& track : all.tracks)
        for (const auto& clip : track.clips)
            for (const auto& note : clip.notes)
            {
                const auto pitch = static_cast<int>(note.midiNote);
                const auto tick = fixtureTick(pitch);
                labels &= note.label == expected(pitch);
                clock &= near(note.startSeconds, timed(tick))
                    && near(note.durationSeconds, timed(tick + 240) - timed(tick))
                    && note.sourceMidiCenter == note.midiNote && near(note.consonantSeconds, 0)
                    && note.contour.size() == 2 && near(note.contour.back().timeSeconds, note.durationSeconds);
            }
    check("no_empty_text_neighbor_text_cross_track_chord_or_duplicate_guessing", labels);
    check("whole_import_retains_tick_tempo_length_pitch_and_contour_contract", clock);
    check("whole_import_is_one_undo_and_redo_restores_labels",
          whole.canUndo() && whole.undo() && whole.contentFingerprint() == before
          && !whole.canUndo() && whole.redo() && whole.contentFingerprint() == imported);
    for (std::size_t index = 0; index < choices.size(); ++index)
    {
        ProjectModel single;
        single.resetDocument(ProjectData{});
        const auto initial = single.contentFingerprint();
        const auto id = single.addMidiTrack(file, choices[index].index, error);
        const auto data = single.snapshot();
        bool same = id.isNotEmpty() && data.tracks.size() == 1 && index < all.tracks.size()
            && nativeScore(data.tracks[0], file);
        if (same)
        {
            const auto& a = data.tracks[0].clips[0];
            const auto& b = all.tracks[index].clips[0];
            same &= a.notes.size() == b.notes.size() && near(a.durationSeconds, b.durationSeconds)
                && near(a.sourceDurationSeconds, b.sourceDurationSeconds);
            for (std::size_t n = 0; same && n < a.notes.size(); ++n)
                same &= a.notes[n].label == b.notes[n].label && a.notes[n].midiNote == b.notes[n].midiNote
                    && near(a.notes[n].startSeconds, b.notes[n].startSeconds)
                    && near(a.notes[n].durationSeconds, b.notes[n].durationSeconds);
        }
        check(index == 0 ? "lead_single_and_whole_match" : "harmony_single_and_whole_match", same);
        check("single_track_adopts_existing_tempo_map_contract", near(data.bpm, 120)
              && data.tempoChanges.size() == 1 && near(data.tempoChanges[0].bpm, 60));
        check("single_track_one_undo_redo", single.undo() && single.contentFingerprint() == initial
              && !single.canUndo() && single.redo());
    }
    // An entirely lyric-free file tests '-' independently of mixed event decoding
    // and uses the other recognized extension. Its name must never supply lyrics.
    juce::MidiFile bare;
    bare.setTicksPerQuarterNote(480);
    juce::MidiMessageSequence bareTrack;
    addText(bareTrack, 3, "Not sung", 0);
    addText(bareTrack, 1, "Not sung either", 0);
    addNote(bareTrack, 60, 0);
    bare.addTrack(bareTrack);
    const auto bareFile = output.getChildFile("invented-syllable.mid");
    check("write_lyric_free_standard_midi", save(bare, bareFile));
    ProjectModel bareWhole, bareSingle;
    bareWhole.resetDocument(ProjectData{});
    bareSingle.resetDocument(ProjectData{});
    const auto bareChoices = ProjectModel::midiTrackChoices(bareFile, error);
    const auto bareId = bareSingle.addMidiTrack(bareFile, 0, error);
    const auto bareLoaded = bareWhole.addMidiFile(bareFile, error);
    const auto bw = bareWhole.snapshot(), bs = bareSingle.snapshot();
    check("lyric_free_both_paths_show_dash_not_filename_or_text",
          bareLoaded && bareId.isNotEmpty() && bareChoices.size() == 1
          && bw.tracks.size() == 1 && bs.tracks.size() == 1
          && nativeScore(bw.tracks[0], bareFile) && nativeScore(bs.tracks[0], bareFile)
          && bw.tracks[0].clips[0].notes.size() == 1 && bs.tracks[0].clips[0].notes.size() == 1
          && bw.tracks[0].clips[0].notes[0].label == "-" && bs.tracks[0].clips[0].notes[0].label == "-");
    ProjectData occupied;
    occupied.bpm = 90;
    TrackData existing;
    existing.id = "existing";
    occupied.tracks.push_back(existing);
    ProjectModel retimed;
    retimed.resetDocument(occupied);
    const auto retimedId = retimed.addMidiTrack(file, 2, error);
    const auto rd = retimed.snapshot();
    check("single_track_into_existing_project_retains_project_beats_and_tempo",
          retimedId.isNotEmpty() && rd.tracks.size() == 2 && near(rd.bpm, 90) && rd.tempoChanges.empty()
          && near(rd.tracks[1].clips[0].notes[0].startSeconds, 5.0 * 60.0 / 90.0)
          && rd.tracks[1].clips[0].notes[0].label == "independent");
    auto* report = new juce::DynamicObject();
    report->setProperty("ok", ok);
    report->setProperty("checks", checks);
    report->setProperty("results", juce::var(results));
    report->setProperty("scope", "model-only; no audio/NN; existing material picker/replacement unchanged");
    const auto saved = output.getChildFile("verification.json").replaceWithText(juce::JSON::toString(juce::var(report)));
    std::cout << "native_midi_material_ok=" << (ok && saved) << "; checks=" << checks << std::endl;
    return ok && saved;
}
}

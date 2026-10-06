#pragma once
#include "../ProjectModel.h"
#include "../SampleSettings.h"
#include "../backend/NsfHifiganRenderer.h"
#include "../backend/UtauRenderer.h"
#include <iostream>

namespace hachi
{
inline bool runIntegratedSmoke()
{
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("hachi-integrated-" + juce::Uuid().toDashedString());
    if (!root.createDirectory()) return false;
    const auto media = root.getChildFile("a.wav");
    {
        juce::AudioBuffer<float> tone(1, 44100);
        for (int i = 0; i < tone.getNumSamples(); ++i)
            tone.setSample(0, i, 0.1f * std::sin(
                static_cast<float>(juce::MathConstants<double>::twoPi * 220.0 * i / 44100.0)));
        juce::WavAudioFormat wav;
        auto stream = media.createOutputStream();
        auto writer = std::unique_ptr<juce::AudioFormatWriter>(
            wav.createWriterFor(stream.release(), 44100.0, 1, 16, {}, 0));
        if (writer == nullptr) return false;
        writer->writeFromAudioSampleBuffer(tone, 0, tone.getNumSamples());
    }
    const auto oto = root.getChildFile("oto.ini");
    const juce::String originalOto = "a.wav=a,100,80,-700,50,20\n";
    oto.replaceWithText(originalOto);
    const auto initialOtoText = oto.loadFileAsString();
    NoteData note;
    note.id = "integrated-note"; note.label = "a";
    note.startSeconds = 0.3; note.durationSeconds = 0.4;
    note.utauOto.enabled = true;
    note.utauOto.offsetMs = 200; note.utauOto.cutoffMs = -500;
    note.utauOto.consonantMs = 70; note.utauOto.preutteranceMs = 40;
    note.utauOto.overlapMs = 15;
    note.vibratoEnabled = true; note.vibratoEndPercent = 73;
    note.utauAutoPitchTransition = false;
    note.amplitudeEnvelopeBasePercent = 125;
    note.amplitudeEnvelope = { { -0.04, -60, true }, { 0.0, 0, true }, { 0.4, -60, false } };
    note.nativeSegments = { { "vowel", "a", NativeSegmentRole::vowel, 0.0, 0.4,
                             "user", 1.0f, 0.0, 0.015, true, 1.0 } };
    auto second = note; second.id = "integrated-second"; second.startSeconds = 0.7;
    ClipData clip; clip.id = "clip"; clip.durationSeconds = 1.2;
    clip.notes = { note, second };
    TrackData track; track.id = "track"; track.voicebankDirectory = root;
    track.pitchAlgorithm = PitchAlgorithm::utau; track.clips = { clip };
    ProjectData data; data.tracks = { track };
    data.nativeConnections = { { "join", note.id, second.id, "pitch-and-amplitude", 0.7, {}, {} } };
    ProjectModel model; model.replace(data);
    model.setTrackPitchAlgorithm("track", PitchAlgorithm::nsfHifigan);
    model.setTrackPitchAlgorithm("track", PitchAlgorithm::utau);
    juce::String error;
    const auto file = root.getChildFile("combined.hjpx");
    ProjectModel reopened;
    auto roundTrip = model.save(file, error) && reopened.load(file, error);
    if (roundTrip)
    {
        const auto loaded = reopened.snapshot();
        roundTrip = loaded.tracks.size() == 1 && !loaded.tracks[0].clips.empty()
            && loaded.tracks[0].clips[0].notes.size() == 2;
        if (roundTrip)
        {
            const auto& n = loaded.tracks[0].clips[0].notes[0];
            roundTrip = n.utauOto.enabled && n.utauOto.offsetMs == 200
                && n.vibratoEndPercent == 73 && !n.utauAutoPitchTransition
                && n.amplitudeEnvelopeBasePercent == 125
                && n.amplitudeEnvelope.size() == 3 && n.amplitudeEnvelope[0].linearToNext
                && n.nativeSegments.size() == 1 && loaded.nativeConnections.size() == 1;
        }
    }
    model.setNoteLabel(note.id, "b");
    const auto rebound = model.snapshot().tracks[0].clips[0].notes[0];
    const auto rebind = !rebound.utauOto.enabled && rebound.utauStpSeconds == 0
        && !rebound.nativeSegments.empty() && rebound.nativeSegments[0].alias == "b";
    const auto resolved = backend::UtauRenderer::resolveVoiceSample(root, "a", 60, 100,
        false, false, 0, false, 0, false, 0, &note.utauOto);
    const auto localOto = resolved.found && std::abs(resolved.offsetSeconds - 0.2) < 1e-6
        && std::abs(resolved.endSeconds - 0.7) < 1e-6 && oto.loadFileAsString() == initialOtoText;
    // Deliberately contradict the preview. NSF must read the full timeline at
    // actual frame times, including a lead-in beyond the old preview padding.
    const auto midi = backend::buildNsfUtauTargetMidi(60, { { 0, -900 } }, 5, 2, -0.8,
        [](double time) { return static_cast<float>(time * 100); });
    const auto timeline = std::abs(midi.front() - 59.2f) < 1e-5f
        && std::abs(midi.back() - 61.2f) < 1e-5f;
    SampleRegionSetting row; row.name = "a"; row.regionEndSeconds = 1;
    row.amplitudeEnvelope = note.amplitudeEnvelope; row.segments = note.nativeSegments;
    const auto sidecarSaved = SampleSettings::save(media, { row }, error);
    const auto rows = SampleSettings::loadOrDerive(media, ProjectData{});
    const auto sidecar = sidecarSaved && rows.size() == 1
        && rows[0].amplitudeEnvelope.size() == 3 && rows[0].amplitudeEnvelope[0].linearToNext;
    std::cout << "combined_project_roundtrip=" << roundTrip << "|native_nsf_note_oto=" << localOto
              << "|native_nsf_full_timeline=" << timeline << "|hjm_linear_envelope=" << sidecar
              << "|native_alias_releases_oto=" << rebind << "|error=" << error << std::endl;
    root.deleteRecursively();
    return roundTrip && localOto && timeline && sidecar && rebind;
}
}

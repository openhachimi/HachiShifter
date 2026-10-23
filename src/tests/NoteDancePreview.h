#pragma once
#include "../PianoRollComponent.h"

namespace hachi
{
inline bool renderNoteDancePreview(const juce::File& directory)
{
    directory.createDirectory();
    ProjectData data;
    TrackData track; track.id = "dance-track"; track.compose = true;
    track.pitchAlgorithm = PitchAlgorithm::utau;
    ClipData clip; clip.id = "dance-clip"; clip.durationSeconds = 8.0;
    for (int i = 0; i < 8; ++i)
    {
        NoteData note; note.id = "dance-" + juce::String(i); note.label = "a";
        note.startSeconds = i * 0.7 + 0.3; note.durationSeconds = 0.55;
        note.midiNote = 60.0f;
        clip.notes.push_back(note);
    }
    track.clips.push_back(clip); data.tracks.push_back(track);
    ProjectModel model; model.replace(data);
    I18n strings; PianoRollComponent roll(model, strings);
    roll.setPixelsPerSecond(160.0f); roll.setRowHeight(22.0f);
    roll.setFocusedTrack(track.id); roll.setShowWaveforms(false);
    roll.diagnosticRefresh();
    const juce::Rectangle<int> area(0, static_cast<int>(roll.diagnosticYForMidi(60.0f)) - 120,
                                    1024, 240);
    const auto save = [&](const char* filename)
    {
        auto image = roll.createComponentSnapshot(area);
        auto stream = directory.getChildFile(filename).createOutputStream();
        return stream && juce::PNGImageFormat().writeImageToStream(image, *stream);
    };
    auto ok = !roll.isNoteDanceEnabled() && save("off.png");
    roll.setNoteDanceEnabled(true);
    ok = ok && roll.isNoteDanceEnabled() && save("on.png");
    roll.setNoteDanceEnabled(false);
    ok = ok && !roll.isNoteDanceEnabled() && save("closed.png");
    // Verify the original variable GIF timings and that all four embedded images draw.
    NoteDanceAnimation dance; dance.setEnabled(true);
    ok = ok && dance.frameFor(0, 249) == 0 && dance.frameFor(0, 250) == 1
        && dance.frameFor(1, 170) == 1 && dance.frameFor(3, 1000) == 0;
    juce::MemoryBlock off, on, closed;
    return ok && directory.getChildFile("off.png").loadFileAsData(off)
        && directory.getChildFile("on.png").loadFileAsData(on)
        && directory.getChildFile("closed.png").loadFileAsData(closed)
        && off == closed && off != on;
}
}

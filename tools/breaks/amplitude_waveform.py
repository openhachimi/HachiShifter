"""The faults --smoke-amplitude-waveform has to notice.

Each is the inverse of one thing the loudness lane's audio now is: drawn at
all, drawn from the piece before the envelope, only when the waveform switch
is on, only for a note that is still the one that was rendered, and only where
there is actually sound.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
P = "PianoRollComponent.cpp"

CASES = {
    # Nothing is drawn: the lane goes back to a curve against an empty grid.
    "lane_draws_nothing": [(P,
        """    if (!showUtauWaveforms || utauWaveforms == nullptr || utauWaveforms->empty()) return 0;""",
        """    return 0; // BREAK lane_draws_nothing
    if (!showUtauWaveforms || utauWaveforms == nullptr || utauWaveforms->empty()) return 0;""")],
    # The audio already shaped by the envelope is drawn instead, so a point
    # pulled to silence erases the sound it is silencing.
    "lane_uses_the_shaped_audio": [(P,
        """    const auto value = high ? waveform.unshapedMaxima[bucket]
                            : waveform.unshapedMinima[bucket];""",
        """    const auto value = high ? waveform.maxima[bucket] // BREAK lane_uses_the_shaped_audio
                            : waveform.minima[bucket];"""), (P,
        """        return std::max(waveform.unshapedMaxima[bucket], -waveform.unshapedMinima[bucket]);""",
        """        return std::max(waveform.maxima[bucket], -waveform.minima[bucket]); // BREAK lane_uses_the_shaped_audio""")],
    # Drawn whatever the waveform switch says.
    "lane_ignores_the_switch": [(P,
        """    if (!showUtauWaveforms || utauWaveforms == nullptr || utauWaveforms->empty()) return 0;""",
        """    if (utauWaveforms == nullptr || utauWaveforms->empty()) return 0; // BREAK lane_ignores_the_switch""")],
    # A note edited since it was rendered keeps its old picture, which is a
    # picture of a sound that will not be heard.
    "lane_ignores_the_hash": [(P,
        """        if (waveform.audioHash != AudioEngine::utauNoteAudioHash(*note)) continue;

        // Its own peak reaches the 100% line""",
        """        // BREAK lane_ignores_the_hash

        // Its own peak reaches the 100% line""")],
    # An envelope edit hides the picture again, as it did before: the row
    # waits for a render instead of reshaping the audio it already has.
    "preview_never_happens": [(P,
        """                if (waveform.audioHash != AudioEngine::utauNoteAudioHash(note)) continue;""",
        """                if (waveform.renderHash != AudioEngine::utauNoteRenderHash(note)) continue; // BREAK preview_never_happens""")],
    # The audio is redrawn without the envelope, so nothing about the edit
    # shows.
    "preview_does_not_reshape": [(P,
        """    const auto db = amplitudeDbAt(envelope, seconds);
    return db <= -59.9f ? 0.0f : value * std::pow(10.0f, db / 20.0f);""",
        """    juce::ignoreUnused(seconds, envelope); // BREAK preview_does_not_reshape
    return value;""")],
    # The point under the pointer is ignored, so the picture only catches up
    # once the drag is let go -- which is when it stops being a preview.
    "preview_ignores_the_drag": [(P,
        """                const auto envelope = displayAmplitudeEnvelope(
                    note, clip.startSeconds + note.startSeconds);""",
        """                const auto envelope = amplitudeEnvelopeFor( // BREAK preview_ignores_the_drag
                    note, clip.startSeconds + note.startSeconds);""")],
    # The audio hash stops telling two notes apart, so a note that really does
    # sound different keeps the old picture reshaped by a new envelope.
    "audio_hash_says_nothing": [("AudioEngine.cpp",
        """std::uint64_t AudioEngine::utauNoteAudioHash(const NoteData& note)
{
    return noteRenderHashImpl(note, false);
}""",
        """std::uint64_t AudioEngine::utauNoteAudioHash(const NoteData& note)
{
    juce::ignoreUnused(note); return 1; // BREAK audio_hash_says_nothing
}""")],
    # The lane goes back to the render hash, so an envelope edit blanks the
    # audio the envelope is being drawn over.
    "lane_uses_the_render_hash": [(P,
        """        if (waveform.audioHash != AudioEngine::utauNoteAudioHash(*note)) continue;""",
        """        if (waveform.renderHash != AudioEngine::utauNoteRenderHash(*note)) continue;""")],
    # The lane goes back to showing only the piece before the envelope, so
    # nothing in it moves when the envelope is edited.
    "lane_ignores_the_envelope": [(P,
        """            g.setColour(shapedColour);
            g.fillRect(x, baseline - shapedHeight, columnWidth,
                       std::max(1.0f, shapedHeight));""",
        """            juce::ignoreUnused(shapedColour, shapedHeight); // BREAK lane_ignores_the_envelope""")],
    # Only the shaped audio is drawn, so a stretch pulled to silence leaves an
    # empty lane that says nothing about what was silenced.
    "lane_has_no_ghost": [(P,
        """            g.setColour(ghostColour);
            g.fillRect(x, baseline - ghostHeight, columnWidth,
                       std::max(1.0f, ghostHeight));""",
        """            juce::ignoreUnused(ghostColour, ghostHeight); // BREAK lane_has_no_ghost""")],
    # Every column drawn at the piece's peak, so silence looks as loud as the
    # vowel and the lane says nothing about where the sound is.
    "lane_is_flat": [(P,
        """            const auto ghostHeight = (baseline - fullScale) * loudest / peak;""",
        """            const auto ghostHeight = (baseline - fullScale); // BREAK lane_is_flat""")],
}


def load(name):
    return io.open(SRC / name, encoding="utf-8", newline="").read()


if sys.argv[1] == "list":
    print(chr(10).join(CASES))
    sys.exit(0)

if sys.argv[1] == "check":
    problems = 0
    for which, swaps in CASES.items():
        for name, good, bad in swaps:
            text = load(name)
            ok = text.count(good) == 1 and text.count(bad) == 0
            problems += 0 if ok else 1
            print(("ok  " if ok else "BAD "), which, "good", text.count(good), "bad", text.count(bad))
    print("cases with problems:", problems)
    sys.exit(1 if problems else 0)

action, names = sys.argv[1], sys.argv[2:]
for which in names:
    swaps = CASES[which] if action == "break" else list(reversed(CASES[which]))
    for name, good, bad in swaps:
        text = load(name)
        old, new = (good, bad) if action == "break" else (bad, good)
        assert text.count(old) == 1, (which, action, text.count(old), old[:60])
        io.open(SRC / name, "w", encoding="utf-8", newline="").write(text.replace(old, new, 1))
    print(action, which)

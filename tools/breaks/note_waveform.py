"""The faults --smoke-note-waveform has to notice.

Each is the inverse of one thing a note's waveform now is: its own audio rather
than its share of the mix, measured before any of it is mixed but shaped by
the gain the mix will put on it, labelled with the note it came from, and drawn
from where the piece starts for as long as the piece lasts.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
R = "backend/UtauRenderer.cpp"
E = "AudioEngine.cpp"
P = "PianoRollComponent.cpp"

CASES = {
    # What it was: the finished mix, which holds the note before this one as
    # well wherever the two overlap.
    "piece_from_the_mix": [(R,
        """            request.notePiece(index, rendered.audio, mixSampleRate, rendered.preutterance,""",
        """            request.notePiece(index, result.buffer, // BREAK piece_from_the_mix
                              mixSampleRate, rendered.preutterance,""")],
    # Nothing is handed over, so nothing is drawn.
    "piece_not_handed_over": [(R,
        """        if (request.notePiece && rendered.audio.getNumSamples() > 0)""",
        """        if (false && request.notePiece) // BREAK piece_not_handed_over""")],
    # The piece is measured but its lead-in is forgotten, so the roll has no
    # way to know it begins before the note.
    "lead_in_dropped": [(E,
        """                                waveform.leadInSeconds = leadInSeconds;""",
        """                                waveform.leadInSeconds = 0.0; // BREAK lead_in_dropped""")],
    # Every piece is labelled with the first note that is waiting for one.
    "pieces_mixed_up": [(E,
        """                                if (note.requestIndex != index) continue;""",
        """                                juce::ignoreUnused(index); // BREAK pieces_mixed_up""")],
    # The picture goes back to being the bare recording, with nothing of the
    # shape the envelope lane draws.
    "envelope_not_applied": [(E,
        """                                                                    gainAt, leadInSeconds,
                                                                    fadesAt);""",
        """                                                                    {}, leadInSeconds, // BREAK envelope_not_applied
                                                                    fadesAt);""")],
    # The envelope is read from the piece's start rather than the note's, so
    # its shape lands a lead-in early.
    "envelope_not_anchored": [(E,
        """                const auto localSeconds = static_cast<double>(index) / sampleRate
                    - leadInSeconds;""",
        """                const auto localSeconds = static_cast<double>(index) / sampleRate; // BREAK envelope_not_anchored""")],
    # The renderer turns the note's time back into a sample of the piece
    # without the lead-in, so every gain is read a preutterance late in the
    # piece -- and the consonant, before sample zero, reads as silence.
    "fades_read_from_the_piece_start": [("backend/UtauRenderer.cpp",
        """                return static_cast<int>(std::llround(
                    (localSeconds + preutterance) * mixSampleRate));""",
        """                juce::ignoreUnused(preutterance); // BREAK fades_read_from_the_piece_start
                return static_cast<int>(std::llround(localSeconds * mixSampleRate));""")],
    # Drawn from where the note starts, which is where the consonant was lost
    # into the row above.
    "drawn_at_the_note_start": [(P,
        """    return { noteStartSeconds - waveform.leadInSeconds, waveform.durationSeconds };""",
        """    return { noteStartSeconds, waveform.durationSeconds }; // BREAK drawn_at_the_note_start""")],
    # Drawn for the note's own length rather than the piece's.
    "drawn_for_the_note_length": [(P,
        """    return { noteStartSeconds - waveform.leadInSeconds, waveform.durationSeconds };""",
        """    return { noteStartSeconds - waveform.leadInSeconds, // BREAK drawn_for_the_note_length
             waveform.durationSeconds - waveform.leadInSeconds };""")],
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

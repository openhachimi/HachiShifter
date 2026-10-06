"""The faults --smoke-missing-alias-piano has to notice.

Each is the inverse of one thing a lyric the voicebank lacks now does: sound as
the piano, at the note's own pitch, and still be reported as missing.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
R = "backend/UtauRenderer.cpp"

CASES = {
    # What it was: a lyric with no sample leaves a hole in the melody.
    "missing_is_silent": [(R,
        """                destination.missing = true;
                destination.piano = true;
                destination.preutterance = 0.0;
                destination.overlap = 0.004;
                destination.audio = renderPianoPreview(note);""",
        """                destination.missing = true;
                destination.preutterance = 0.0;
                destination.overlap = 0.004; // BREAK missing_is_silent""")],
    # It plays, and nobody is told the lyric is wrong.
    "missing_not_reported": [(R,
        """        if (rendered.missing) ++missingCount;""",
        """        // BREAK missing_not_reported""")],
    # The piano plays one pitch whatever the note says.
    "piano_ignores_the_pitch": [(R,
        """        (static_cast<double>(note.midiNote) - 69.0) / 12.0);""",
        """        (60.0 - 69.0) / 12.0); // BREAK piano_ignores_the_pitch""")],
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

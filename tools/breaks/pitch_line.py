"""The faults --smoke-pitch-line has to notice.

The switch has to hide the whole pitch line -- the line itself, the S between
two notes, and a note's vibrato -- and nothing else.  Each of these either
leaves a piece of it behind, takes something else with it, or stops drawing a
piece the check then could not see disappear.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
P = "PianoRollComponent.cpp"

BRIDGE = """                if (utauMode && tool != Tool::amplitude && showPitchLine)
                {
                // Anchor handles and the shape menu belong to the point"""
VIBRATO = """                if (utauMode && tool != Tool::amplitude && showPitchLine
                    && note.vibratoEnabled && note.durationSeconds > 1.0e-9)
                {"""

CASES = {
    # The join between two notes is drawn whatever the switch says: the
    # residue that was reported.
    "join_ignores_the_switch": [(P, BRIDGE,
        """                if (utauMode && tool != Tool::amplitude) // BREAK join_ignores_the_switch
                {
                // Anchor handles and the shape menu belong to the point""")],
    # A note's vibrato stays drawn, so every note carrying one keeps a curve.
    "vibrato_ignores_the_switch": [(P, VIBRATO,
        """                if (utauMode && tool != Tool::amplitude // BREAK vibrato_ignores_the_switch
                    && note.vibratoEnabled && note.durationSeconds > 1.0e-9)
                {""")],
    # The join is never drawn at all, so nothing can be seen to go with the
    # switch: the check has to notice that its own fixture went quiet.
    "join_never_drawn": [(P, BRIDGE,
        """                if (false && utauMode) // BREAK join_never_drawn
                {
                // Anchor handles and the shape menu belong to the point""")],
    # The same for the vibrato.
    "vibrato_never_drawn": [(P, VIBRATO,
        """                if (false && utauMode) // BREAK vibrato_never_drawn
                {""")],
    # Gated one block too high: the notes go with the line.
    "hides_the_notes_too": [(P,
        """            for (const auto& note : clip.notes)
            {
                const auto movingSelected = dragMode == DragMode::moveUtauNote""",
        """            for (const auto& note : clip.notes)
            {
                if (!showPitchLine) continue; // BREAK hides_the_notes_too
                const auto movingSelected = dragMode == DragMode::moveUtauNote""")],
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

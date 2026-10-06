"""The faults --smoke-forward-bend has to notice.

A bend drawn past its note's end is an ordinary stretch of one line: the two
notes either side share that line, both are sent it, and a note with nothing
to join is still sent its own points out there.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
M = "ProjectModel.cpp"
A = "AudioEngine.cpp"

CASES = {
    # What it was: only a bend reaching back joins two notes, so a bend
    # running forward was drawn and never sung.
    "forward_bend_joins_nothing": [(M,
        """        return last != edited.points.end() && last->timeSeconds > right.start + 1.0e-9;""",
        """        return false; // BREAK forward_bend_joins_nothing""")],
    # A note on its own is sent only as far as its end again.
    "own_points_cut_at_the_end": [(A,
        """            auto lastTime = note.durationSeconds;
            for (const auto& point : note.pitchControlPoints)
                lastTime = std::max(lastTime, point.timeSeconds);""",
        """            const auto lastTime = note.durationSeconds; // BREAK own_points_cut_at_the_end"""),
        (A,
        """                return (evaluatePitchCurve(note.pitchControlPoints, time) - note.midiNote)""",
        """                return (evaluatePitchCurve(note.pitchControlPoints, // BREAK own_points_cut_at_the_end
                            std::min(time, note.durationSeconds)) - note.midiNote)""")],
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

"""The faults --smoke-ust-pitch has to notice on real songs under a shared line.

Real USTs are where these show: bends written with widths that run backwards,
bends that open with a jump two points wide, and steep bends a few
milliseconds long.  Each case undoes one thing that keeps every note sung, in
the stretch that is its own, exactly as it was before lines were shared.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
M = "ProjectModel.cpp"
A = "AudioEngine.cpp"

CASES = {
    # The points re-sorted by time: a bend written backwards sings otherwise.
    "own_points_resorted": [(M,
        """            entry.points = replaced ? *replacedPoints : ownPitchPoints(note);
            for (auto& point : entry.points) point.timeSeconds += entry.start;""",
        """            entry.points = replaced ? *replacedPoints : ownPitchPoints(note);
            std::stable_sort(entry.points.begin(), entry.points.end(), // BREAK own_points_resorted
                [](const auto& left, const auto& right) { return left.timeSeconds < right.timeSeconds; });
            for (auto& point : entry.points) point.timeSeconds += entry.start;""")],
    # Sampled on a grid of its own rather than the one the note always was.
    "grid_not_aligned": [(A,
        """            const auto origin = note.pitchControlPoints.empty()
                ? 0.0 : std::min(0.0, note.pitchControlPoints.front().timeSeconds);""",
        """            const auto origin = -before; // BREAK grid_not_aligned""")],
    # The join starts from the note's first point there instead of its last,
    # sweeping away the points it placed before the hand-over.
    "join_from_the_first_point": [(M,
        """                    lastPoint[index] = any ? std::max(lastPoint[index], point.timeSeconds)
                                           : point.timeSeconds;""",
        """                    lastPoint[index] = any ? std::min(lastPoint[index], point.timeSeconds) // BREAK join_from_the_first_point
                                           : point.timeSeconds;""")],
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

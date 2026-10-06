"""The faults --smoke-dragged-transition has to notice.

The automatic transition runs from where one note's own line ends to where the
next one's begins -- the stretch the roll draws it over -- and both notes are
sent it.  A note nobody has touched hands over an inset in from each end.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
A = "AudioEngine.cpp"
P = "PianoRollComponent.cpp"

CASES = {
    # What it was: twenty milliseconds either side of the boundary, however
    # far the points had been dragged.
    "transition_ignores_the_points": [(A,
        """        auto previousTailTime = previousPlan.ownPointsPlaced
            ? previousPlan.ownLast : previous.durationSeconds - inset;
        auto nextHeadTime = nextPlan.ownPointsPlaced ? nextPlan.ownFirst : inset;""",
        """        auto previousTailTime = previous.durationSeconds - inset; // BREAK transition_ignores_the_points
        auto nextHeadTime = inset;""")],
    # Only the note before it is sent the transition.
    "only_one_note_is_sent_it": [(A,
        """        replaceCurveRange(next, previousTailTime - previous.durationSeconds,
                          nextHeadTime, startMidi, endMidi);""",
        """        juce::ignoreUnused(next); // BREAK only_one_note_is_sent_it""")],
    # Paired among the notes sent rather than along the track: a note
    # rendered on its own has no neighbour, and so no transition at all.
    "only_the_sent_notes_are_paired": [(A,
        """    for (const auto& trackClip : track.clips)
        for (const auto& note : trackClip.notes)
            timeline.emplace_back(trackClip.startSeconds + note.startSeconds, &note);""",
        """    for (const auto& note : clip.notes) // BREAK only_the_sent_notes_are_paired
        timeline.emplace_back(clip.startSeconds + note.startSeconds, &note);""")],
    # The drawn transition stops where the note does, so what is drawn and
    # what is sung part company again.
    "drawn_transition_starts_elsewhere": [(P,
        """    bridge.startSeconds = absoluteStart + anchors.back().timeSeconds;""",
        """    bridge.startSeconds = absoluteStart + note.durationSeconds; // BREAK drawn_transition_starts_elsewhere""")],
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

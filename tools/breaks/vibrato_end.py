"""The faults --smoke-vibrato-end has to notice.

Each is the inverse of one thing a vibrato's end now is: where the swing stops,
the note's end unless moved, the point its length is measured back from, a
handle on screen that moves the end alone -- the start moving the start alone,
the fade-out counted from it -- kept apart from the fade-out handle, carried
through setting, saving, rendering and baking.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
M = "ProjectModel.cpp"
A = "AudioEngine.cpp"
P = "PianoRollComponent.cpp"

CASES = {
    # The swing runs on to the note's end whatever the end says.
    "swing_runs_on_past_the_end": [(M,
        """    if (localSeconds > window.end + 1.0e-9) return 0.0;""",
        """    // BREAK swing_runs_on_past_the_end""")],
    # The length is measured back from the note's end, as it was.
    "length_from_the_note_end": [(M,
        """    span.end = duration * juce::jlimit(0.0, 100.0, note.vibratoEndPercent) / 100.0;""",
        """    span.end = duration; // BREAK length_from_the_note_end""")],
    # No handle there to take hold of.
    "no_end_handle": [(P,
        """                    VibratoHandle::fadeIn, VibratoHandle::fadeOut, VibratoHandle::end,
                    VibratoHandle::length""",
        """                    VibratoHandle::fadeIn, VibratoHandle::fadeOut, // BREAK no_end_handle
                    VibratoHandle::length""")],
    # Dragging the end carries the start along with it.
    "end_drag_moves_the_start": [(P,
        """                previewVibrato.vibratoLengthPercent = (moved - start) / duration * 100.0;
                break;""",
        """                break; // BREAK end_drag_moves_the_start""")],
    # Dragging the start measures from the note's end again.
    "start_drag_from_the_note_end": [(P,
        """                previewVibrato.vibratoLengthPercent = (end
                    - juce::jlimit(0.0, std::max(0.0, end - shortest), local)) / duration * 100.0;""",
        """                previewVibrato.vibratoLengthPercent = // BREAK start_drag_from_the_note_end
                    juce::jlimit(1.0, 100.0, (duration - local) / duration * 100.0);""")],
    # The fade-out handle counted from the note's end.
    "fade_out_from_the_note_end": [(P,
        """            return { end - span * juce::jlimit(0.0, 100.0,
                        note.vibratoFadeOutPercent) / 100.0, 0.0, 0.0 };""",
        """            return { note.durationSeconds - span * juce::jlimit(0.0, 100.0, // BREAK fade_out_from_the_note_end
                        note.vibratoFadeOutPercent) / 100.0, 0.0, 0.0 };""")],
    # With no fade-out the two handles lie one on the other.
    "handles_stacked": [(P,
        """        if (std::abs(fadeOutX - centre.x) < 9.0f) centre.y += 9.0f;""",
        """        juce::ignoreUnused(fadeOutX); // BREAK handles_stacked""")],
    # Setting a note's vibrato forgets where it ends.
    "setting_forgets_the_end": [(M,
        """                    note.vibratoEndPercent = juce::jlimit(1.0, 100.0, parameters.vibratoEndPercent);""",
        """                    // BREAK setting_forgets_the_end""")],
    # Not written to the project.
    "not_saved": [(M,
        """                noteTree.setProperty("vibratoEndPercent", note.vibratoEndPercent, nullptr);""",
        """                // BREAK not_saved""")],
    # The render cannot tell two ends apart, so a moved end is never re-sung.
    "render_deaf_to_it": [(A,
        """    stream.writeDouble(note.vibratoEndPercent);""",
        """    // BREAK render_deaf_to_it""")],
    # Baked into points, the swing runs on to the note's end.
    "bake_runs_to_the_note_end": [(M,
        """                    const auto swingEnd = swingSpan.end;""",
        """                    const auto swingEnd = note.durationSeconds; // BREAK bake_runs_to_the_note_end""")],
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

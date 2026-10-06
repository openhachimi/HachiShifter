"""The faults --smoke-envelope-base has to notice.

Each is the inverse of one thing 包络基础值 now does: scaling in UTAU's linear
percent rather than in decibels, leaving silence silent, reaching the engine,
reaching a note that never had an envelope, showing in the lane what will be
heard, writing itself at the note's top left corner when it is not 100
and climbing as the shape it raises grows out of the note, taking
itself back off again when a dragged shape is stored, holding to 0..200,
surviving a save, and being in the menu at all.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
M = "ProjectModel.cpp"
A = "AudioEngine.cpp"
P = "PianoRollComponent.cpp"

CASES = {
    # Scaled in decibels: 200 then means +200 dB-ish rather than twice as loud.
    "scaled_in_decibels": [(M,
        """    const auto level = std::pow(10.0, gainDb / 20.0) * factor;""",
        """    const auto level = std::pow(10.0, gainDb * factor / 20.0); // BREAK scaled_in_decibels""")],
    # Silence is scaled like anything else, so every UST envelope's two ends
    # start to sound.
    "silence_is_raised": [(M,
        """    if (gainDb <= -59.9f || factor <= 1.0e-9) return -60.0f;""",
        """    if (factor <= 1.0e-9) return -60.0f; // BREAK silence_is_raised""")],
    # The engine is sent the envelope as drawn, so nothing is heard of it.
    "engine_never_sees_it": [(A,
        """            shaped = scaledAmplitudeEnvelope(shaped, base);""",
        """            juce::ignoreUnused(base); // BREAK engine_never_sees_it""")],
    # A note that never had an envelope has no flat line to raise.
    "no_line_for_a_bare_note": [(A,
        """            if (shaped.empty() && std::abs(base - 100.0f) > 1.0e-6f)
                shaped = { { 0.0, 0.0f, true },
                           { std::max(0.01, note.durationSeconds), 0.0f, true } };""",
        """            // BREAK no_line_for_a_bare_note""")],
    # The lane goes on showing the shape as drawn, so the line says one thing
    # and the audio does another.
    "lane_shows_the_bare_shape": [(P,
        """    const auto withBase = [&note](std::vector<AmplitudeEnvelopePoint> points)
    {
        return scaledAmplitudeEnvelope(points, note.amplitudeEnvelopeBasePercent);
    };""",
        """    const auto withBase = [&note](std::vector<AmplitudeEnvelopePoint> points)
    {
        juce::ignoreUnused(note); return points; // BREAK lane_shows_the_bare_shape
    };""")],
    # The base is not taken back off when a dragged shape is stored, so it is
    # multiplied in a second time on the next read.
    "write_back_keeps_the_base": [(P,
        """        if (const auto* target = findNote(id); target != nullptr)
            mapped = unscaledAmplitudeEnvelope(mapped, target->amplitudeEnvelopeBasePercent);""",
        """        // BREAK write_back_keeps_the_base""")],
    # Any number is accepted, so a typo can ask for eight times the level.
    "range_not_held": [(M,
        """        const auto next = juce::jlimit(0.0f, 200.0f, basePercent);""",
        """        const auto next = basePercent; // BREAK range_not_held""")],
    # Not written to the project, so it is gone the next time it is opened.
    "not_saved": [(M,
        """                noteTree.setProperty("amplitudeEnvelopeBase",
                                     note.amplitudeEnvelopeBasePercent, nullptr);""",
        """                // BREAK not_saved""")],
    # The number is never written, so a note carrying a base looks like any
    # other and there is nothing to read but the shape.
    "label_never_drawn": [(P,
        """                        g.drawText(text, static_cast<int>(left),
                                   std::max(0, static_cast<int>(ceiling) - 11),
                                   44, 10, juce::Justification::centredLeft, false);""",
        """                        juce::ignoreUnused(text, left, ceiling); // BREAK label_never_drawn""")],
    # Pinned to the note's top edge again, so the shape -- which the base is
    # what raises -- is drawn straight through the number.
    "label_sits_on_the_note": [(P,
        """                        g.drawText(text, static_cast<int>(left),
                                   std::max(0, static_cast<int>(ceiling) - 11),
                                   44, 10, juce::Justification::centredLeft, false);""",
        """                        juce::ignoreUnused(ceiling); // BREAK label_sits_on_the_note
                        g.drawText(text, static_cast<int>(left),
                                   static_cast<int>(displayBounds.getY()) - 10,
                                   44, 10, juce::Justification::centredLeft, false);""")],
    # It stays on the roll after the envelope display is switched off, which is
    # the residue the user asked to be rid of elsewhere.
    "label_ignores_the_switch": [(P,
        """                    if (showEnvelope
                        && std::abs(note.amplitudeEnvelopeBasePercent - 100.0f) > 0.05f)""",
        """                    if (std::abs(note.amplitudeEnvelopeBasePercent - 100.0f) > 0.05f)
                        // BREAK label_ignores_the_switch""")],
    # Written on every note, 100 included: a column of "100%" over the whole
    # roll, saying nothing and covering the notes.
    "label_at_a_hundred_too": [(P,
        """                    if (showEnvelope
                        && std::abs(note.amplitudeEnvelopeBasePercent - 100.0f) > 0.05f)""",
        """                    if (showEnvelope) // BREAK label_at_a_hundred_too""")],
    # Not in the menu, so there is no way to reach it.
    "not_in_the_menu": [(P,
        """    { 23, Scope::utauOnly  },   // 包络基础值…       -- the envelope is a UTAU-mode thing""",
        """    { 23, Scope::plainOnly },   // BREAK not_in_the_menu""")],
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

"""The faults --smoke-ust-vibrato has to notice.

One at a time: each is the inverse of something the UST importer or the model
does with a VBR line, applied to the real source, so a check that passes with
the fault in place was not watching that thing at all.
"""
import io, pathlib, sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
U = "backend/UstImporter.cpp"
M = "ProjectModel.cpp"

CASES = {
    "vbr_not_parsed": [(U,
        """        else if (key.equalsIgnoreCase("VBR"))""",
        """        else if (false && key.equalsIgnoreCase("VBR")) // BREAK vbr_not_parsed""")],
    "vbr_not_applied": [(M,
        """        if (source.hasVibrato)
        {
            note.vibratoEnabled = true;""",
        """        if (false && source.hasVibrato) // BREAK vbr_not_applied
        {
            note.vibratoEnabled = true;""")],
    "length_zero_enables": [(U,
        """                current.hasVibrato = current.vibratoLengthPercent > 0.0
                    && current.vibratoDepthCents != 0.0
                    && current.vibratoCycleMs > 0.0;""",
        """                current.hasVibrato = current.vibratoDepthCents != 0.0
                    && current.vibratoCycleMs > 0.0; // BREAK length_zero_enables""")],
    "fields_shifted": [(U,
        """                current.vibratoFadeInPercent = at(3);
                current.vibratoFadeOutPercent = at(4);
                current.vibratoPhasePercent = at(5);""",
        """                current.vibratoFadeInPercent = at(5); // BREAK fields_shifted
                current.vibratoFadeOutPercent = at(4);
                current.vibratoPhasePercent = at(3);""")],
    "offset_ignored": [(M,
        """            note.vibratoOffsetPercent = juce::jlimit(-100.0, 100.0, source.vibratoOffsetPercent);""",
        """            // BREAK offset_ignored""")],
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
        io.open(SRC / name, "w", encoding="utf-8", newline="").write(text.replace(old, new))
    print(action, which)

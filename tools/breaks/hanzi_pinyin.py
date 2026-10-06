"""The faults --smoke-hanzi-pinyin has to notice.

Each is the inverse of one thing Edit > 汉字转拼音 does: turn every Chinese
character into its pinyin, spelt the way the voicebanks spell their aliases,
keep the rest of each lyric exactly, touch UTAU tracks only, undo in one step,
and be offered in the edit menu on a UTAU track and nowhere else.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
M = "ProjectModel.cpp"
Y = "Pinyin.cpp"
T = "PinyinTable.inc"
W = "MainComponent.cpp"

CASES = {
    # Nothing is looked up: the lyric comes back as it went in.
    "nothing_converted": [(Y,
        """        if (const auto* syllable = pinyinFor(character))
            converted << syllable;
        else""",
        """        if (false) // BREAK nothing_converted
            converted << pinyinFor(character);
        else""")],
    # Only the Chinese characters survive: "- 你" comes back "ni", and a
    # VCV alias loses its hyphen.
    "the_rest_of_the_lyric_dropped": [(Y,
        """        else
            converted << juce::String::charToString(character);""",
        """        // BREAK the_rest_of_the_lyric_dropped""")],
    # u umlaut spelt u: 绿 becomes lu, which no voicebank here has for it.
    "umlaut_spelt_u": [(T,
        """ "lv",""",
        """ "lu", /* BREAK umlaut_spelt_u */""")],
    # Any kind of track is converted, not only UTAU ones.
    "every_kind_of_track": [(M,
        """            if (track.id != trackId || track.pitchAlgorithm != PitchAlgorithm::utau) continue;
            for (const auto& clip : track.clips)
                for (const auto& note : clip.notes)
                {
                    const auto converted = lyricInPinyin(note.label);""",
        """            if (track.id != trackId) continue; // BREAK every_kind_of_track
            for (const auto& clip : track.clips)
                for (const auto& note : clip.notes)
                {
                    const auto converted = lyricInPinyin(note.label);""")],
    # One undo step per note, so undoing it once puts back one lyric.
    "one_step_per_note": [(M,
        """    setNoteLabels(labels);
    return static_cast<int>(labels.size());""",
        """    for (const auto& [id, label] : labels) setNoteLabel(id, label); // BREAK one_step_per_note
    return static_cast<int>(labels.size());""")],
    # Offered whatever the track, when it can only do anything on a UTAU one.
    "offered_everywhere": [(W,
        """        menu.addItem(hanziToPinyinMenuItem, strings.text("edit.hanziToPinyin"),
                     selectedTrackIsUtau());""",
        """        menu.addItem(hanziToPinyinMenuItem, strings.text("edit.hanziToPinyin"),
                     true); // BREAK offered_everywhere""")],
    # Not in the menu at all.
    "not_in_the_menu": [(W,
        """        menu.addItem(hanziToPinyinMenuItem, strings.text("edit.hanziToPinyin"),
                     selectedTrackIsUtau());""",
        """        // BREAK not_in_the_menu""")],
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

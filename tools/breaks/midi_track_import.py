"""The faults --smoke-midi-track-import has to notice.

Each is the inverse of one thing 导入 MIDI 轨道 does: offer only the tracks
with notes, bring the chosen one in with its lyrics read in the file's own
encoding, put it on the project's beats, take the song's tempo only into an
empty project, ask which track only when there is a choice, and import only
what was picked, only when the dialog says so.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
M = "ProjectModel.cpp"
W = "MainComponent.cpp"

CASES = {
    # The conductor track, and any other with no notes, is offered too.
    "empty_tracks_offered": [(M,
        """        if (part.notes.empty()) continue;
        const auto decoded = decodeMidiTexts(texts);""",
        """        // BREAK empty_tracks_offered
        const auto decoded = decodeMidiTexts(texts);""")],
    # The lyrics are read past, and every note comes in without one.
    "lyrics_dropped": [(M,
        """                pendingLyric = metaEventBytes(message);""",
        """                juce::ignoreUnused(pendingLyric); // BREAK lyrics_dropped""")],
    # Text is taken as UTF-8 whatever it is, and Shift-JIS turns to noise.
    "lyrics_read_as_utf8": [(M,
        """    auto lines = juce::StringArray::fromTokens(backend::UstImporter::decode(joined, encoding),""",
        """    auto lines = juce::StringArray::fromTokens(juce::String::fromUTF8( // BREAK lyrics_read_as_utf8
        static_cast<const char*>(joined.getData()), static_cast<int>(joined.getSize())),""")],
    # The song's tempo replaces the project's even beside tracks already there.
    "tempo_always_taken": [(M,
        """        else if (!song->inSeconds)
        {
            if (song->bpm) project.bpm = *song->bpm;""",
        """        if (!song->inSeconds) // BREAK tempo_always_taken
        {
            if (song->bpm) project.bpm = *song->bpm;""")],
    # Beats are not turned into seconds at all.
    "beats_not_placed": [(M,
        """            if (!song->inSeconds)
            {
                const auto startQuarters = note.startSeconds;""",
        """            if (false) // BREAK beats_not_placed
            {
                const auto startQuarters = note.startSeconds;""")],
    # The new track ignores the workflow the project is in.
    "workflow_not_followed": [(M,
        """            track.pitchAlgorithm = project.tracks.back().pitchAlgorithm;
            track.stretchAlgorithm = project.tracks.back().stretchAlgorithm;
            track.renderOrder = project.tracks.back().renderOrder;
        }
        // The song's own tempo""",
        """            juce::ignoreUnused(track); // BREAK workflow_not_followed
        }
        // The song's own tempo""")],
    # The menu does not offer it.
    "menu_item_missing": [(W,
        """    menu.addItem(importMidiTrackMenuItem, strings.text("track.importMidi"));""",
        """    // BREAK menu_item_missing""")],
    # Never asked: a file with several parts brings in its first.
    "never_asks": [(W,
        """    if (choices.size() == 1)
    {
        addMidiTrackFrom(file, choices.front().index);""",
        """    if (choices.size() >= 1) // BREAK never_asks
    {
        addMidiTrackFrom(file, choices.front().index);""")],
    # Always asked, even when there is nothing to choose between.
    "always_asks": [(W,
        """    if (choices.size() == 1)
    {
        addMidiTrackFrom(file, choices.front().index);""",
        """    if (false) // BREAK always_asks
    {
        addMidiTrackFrom(file, choices.front().index);""")],
    # Whatever is picked, the first comes in.
    "first_always_taken": [(W,
        """                    safe->addMidiTrackFrom(file, choices[static_cast<std::size_t>(picked)].index);""",
        """                    safe->addMidiTrackFrom(file, choices.front().index); // BREAK first_always_taken""")],
    # Cancelling imports anyway.
    "cancel_imports": [(W,
        """                if (safe != nullptr && result == 1 && picked >= 0""",
        """                if (safe != nullptr && picked >= 0 // BREAK cancel_imports""")],
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

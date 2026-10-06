"""The faults --smoke-ust-import has to notice.

Each is the inverse of one thing importing a UST now does: opening the song
rather than adding it, letting the song that was opened own the tempo, naming
the track that was imported so the roll can show it, doing it all in one undo
step, and leaving the project alone when the user asked to add instead.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
M = "ProjectModel.cpp"
H = "MainComponent.h"

CLEAR = """        if (mode == UstImportMode::replaceProject) project = ProjectData{};"""
TEMPO_BLOCK = """        if (project.tracks.empty())
        {
            project.bpm = juce::jlimit(20.0, 400.0, parsed->tempo);
            project.tempoChanges = tempoChanges;
        }
        else if (!tempoChanges.empty())
            warnings.add("tempo changes ignored: the project already has tracks");"""

COMMENT = """
        // The tempo map has to be in place before a musical position can be
        // turned into seconds, and an imported song owns the tempo only when
        // it is the first thing in the project.
"""

CASES = {
    # Nothing is replaced: the second song is added behind the first, which is
    # what it used to do.
    "never_replaces": [(M, CLEAR, """        // BREAK never_replaces""")],
    # Replaced, but only after the tempo has been decided -- so the song that
    # was opened is bent to the speed of the one it replaced.
    "tempo_decided_first": [(M, CLEAR + COMMENT + TEMPO_BLOCK,
                             COMMENT.lstrip("\n") + TEMPO_BLOCK + "\n" + CLEAR
                             + " // BREAK tempo_decided_first")],
    # The track that was imported is never named, so nothing can be shown.
    "track_not_named": [(M,
        """    if (importedTrackId != nullptr) *importedTrackId = newTrackId;""",
        """    // BREAK track_not_named""")],
    # Named, but with the track that was already there: the roll goes on
    # showing the old song, which is the fault as it was reported.
    "names_the_old_track": [(M,
        """    if (importedTrackId != nullptr) *importedTrackId = newTrackId;""",
        """    if (importedTrackId != nullptr) // BREAK names_the_old_track
        *importedTrackId = snapshot().tracks.front().id;""")],
    # An undo step of its own, so undo hands back an empty project instead of
    # the one that was replaced.
    "two_undo_steps": [(M, CLEAR,
        """        if (mode == UstImportMode::replaceProject) // BREAK two_undo_steps
        {
            project = ProjectData{};
            pushUndoLocked();
        }""")],
    # Adding replaces as well, so laying a harmony part beside a lead throws
    # the lead away.
    "adding_replaces_too": [(M, CLEAR,
        """        project = ProjectData{}; // BREAK adding_replaces_too""")],
    # Thrown away before the file has been read, so a UST that turns out to be
    # unreadable takes the open project with it.
    "clears_before_reading": [(M,
        """    const auto parsed = backend::UstImporter::read(file, error, warnings);""",
        """    if (mode == UstImportMode::replaceProject) clear(); // BREAK clears_before_reading
    const auto parsed = backend::UstImporter::read(file, error, warnings);""")],
    # The question is never put: a song already open is replaced without being
    # asked about.
    "never_asks": [(H,
        """        return !data.tracks.empty();""",
        """        return false; // BREAK never_asks""")],
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

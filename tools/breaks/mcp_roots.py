"""The faults --smoke-mcp-roots has to notice: the rule itself.

Each is the inverse of one thing "inside a folder this session may read" means:
whole folders rather than names that begin alike, a folder's contents as well
as the folder, nothing when nothing is allowed, and a project naming the
voicebanks and media its own work lives in.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
M = "backend/McpServer.cpp"

CASES = {
    # Names compared as text: "D:/work" then contains "D:/workshop/secret".
    "prefix_match": [(M,
        """        if (resolved == within || resolved.isAChildOf(within)) return true;""",
        """        if (resolved.getFullPathName().startsWith(within.getFullPathName()))
            return true; // BREAK prefix_match""")],
    # Everything is inside everything.
    "everything_allowed": [(M,
        """    if (target == juce::File()) return false;""",
        """    juce::ignoreUnused(target, roots); return true; // BREAK everything_allowed
    if (target == juce::File()) return false;""")],
    # The folder itself and nothing in it.
    "only_the_folder_itself": [(M,
        """        if (resolved == within || resolved.isAChildOf(within)) return true;""",
        """        if (resolved == within) return true; // BREAK only_the_folder_itself""")],
    # A project names nowhere, so an open song's own media stays unreadable.
    "no_project_roots": [(M,
        """    for (const auto& track : data.tracks)
    {
        add(track.voicebankDirectory);
        for (const auto& clip : track.clips)
            if (clip.sourceFile != juce::File())
                add(clip.sourceFile.getParentDirectory());
    }""",
        """    juce::ignoreUnused(data, add); // BREAK no_project_roots""")],
    # The voicebank is named and the recordings are not.
    "media_not_named": [(M,
        """        for (const auto& clip : track.clips)
            if (clip.sourceFile != juce::File())
                add(clip.sourceFile.getParentDirectory());""",
        """        // BREAK media_not_named""")],
    # A folder that is not on this machine is named anyway, which is a root
    # nothing can be read from and a promise that it could.
    "names_folders_that_are_gone": [(M,
        """        if (folder == juce::File() || !folder.isDirectory()) return;
        for (const auto& known : roots) if (known == folder) return;
        roots.push_back(folder);""",
        """        if (folder == juce::File()) return; // BREAK names_folders_that_are_gone
        for (const auto& known : roots) if (known == folder) return;
        roots.push_back(folder);""")],
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

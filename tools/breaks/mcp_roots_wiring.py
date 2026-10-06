"""The faults tools/check_mcp_roots.py has to notice: the rule being reached.

A rule that is right and never asked is the same as no rule.  Each of these
leaves the rule intact and cuts the path between it and a call: the guard on a
tool, what makes a folder allowed, and the folders the server is started with.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
M = "backend/McpServer.cpp"

CASES = {
    # read_file reads anything again.
    "read_file_unchecked": [(M,
        """        if (!pathWithinRoots(allowedRoots(), file))
            return toolResult(outsideRootsMessage(file, allowedRoots()), true);""",
        """        // BREAK read_file_unchecked""")],
    # list_directory lists anything again.
    "list_directory_unchecked": [(M,
        """        if (!pathWithinRoots(allowedRoots(), directory))
            return toolResult(outsideRootsMessage(directory, allowedRoots()), true);""",
        """        // BREAK list_directory_unchecked""")],
    # A path that failed is allowed anyway, so naming a file to a tool that
    # cannot read it opens its folder.
    "failure_allows_it_too": [(M,
        """    if (!static_cast<bool>(result.getProperty("isError", false)))""",
        """    if (true) // BREAK failure_allows_it_too""")],
    # Working somewhere no longer makes it readable.
    "work_allows_nothing": [(M,
        """            if (const auto given = string(args, key); given.isNotEmpty())
                allow(juce::File(given));""",
        """            juce::ignoreUnused(key); // BREAK work_allows_nothing""")],
    # The folders the server was started with are dropped.
    "configured_roots_ignored": [(M,
        """    auto roots = configuredRoots;""",
        """    std::vector<juce::File> roots; // BREAK configured_roots_ignored""")],
    # The refusal says nothing about how to allow a folder, which reads as a
    # broken tool rather than a rule.
    "refusal_says_nothing": [(M,
        """    return "Outside the folders this session may read: " + path.getFullPathName()""",
        """    juce::ignoreUnused(roots); return "denied"; // BREAK refusal_says_nothing
    return "Outside the folders this session may read: " + path.getFullPathName()""")],
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

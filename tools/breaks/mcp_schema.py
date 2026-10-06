"""The faults --smoke-mcp-schema has to notice.

Each is the inverse of one thing the schema builder does: naming a tool's
parameters at all, typing them, describing them, saying which are required,
listing the few words a value may be, and saying what is inside a list.
"""
import io
import pathlib
import sys

SRC = pathlib.Path("C:/hachi-src/juce/src")
M = "backend/McpServer.cpp"

CASES = {
    # What it was before: forty-eight tools sharing one empty object.
    "all_permissive": [(M,
        """    set(tool, "inputSchema", typedSchema(std::move(params)));""",
        """    juce::ignoreUnused(params); // BREAK all_permissive
    auto empty = object();
    set(empty, "type", "object");
    set(empty, "additionalProperties", true);
    set(tool, "inputSchema", std::move(empty));""")],
    # Names with nothing said about them.
    "no_descriptions": [(M,
        """        set(entry, "description", juce::String::fromUTF8(param.description));""",
        """        // BREAK no_descriptions""")],
    # Nothing is required, so a caller cannot tell what a tool cannot do without.
    "no_required": [(M,
        """    if (!required.empty()) set(schema, "required", array(std::move(required)));""",
        """    // BREAK no_required""")],
    # Required names that are not among the properties.
    "required_misspelt": [(M,
        """        if (param.required) required.emplace_back(param.name);""",
        """        if (param.required) required.emplace_back(juce::String(param.name) + "s");""")],
    # The few words a value may be are left unsaid.
    "no_enums": [(M,
        """            set(entry, "enum", array(std::move(choices)));""",
        """            juce::ignoreUnused(choices); // BREAK no_enums""")],
    # A list of points becomes a bare list of anything.
    "no_items": [(M,
        """        if (param.shape != nullptr && type == "array") set(entry, "items", param.shape());""",
        """        // BREAK no_items""")],
    # Everything is a string, whatever it really is.
    "one_type_for_all": [(M,
        """        set(entry, "type", param.type);""",
        """        set(entry, "type", "float"); // BREAK one_type_for_all""")],
    # One tool forgets what it takes.
    "add_note_says_nothing": [(M,
        """            makeTool("add_note", "Create a note in a clip / 在采样中创建音符", {
                { .name = "clip_id", .type = "string",
                  .description = "The clip to put it in", .required = true },""",
        """            makeTool("add_note", "Create a note in a clip / 在采样中创建音符", {
                // BREAK add_note_says_nothing""")],
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

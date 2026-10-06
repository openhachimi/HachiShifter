"""Hold each MCP tool's schema to the arguments its dispatcher really reads.

A schema is what a client shows the model it drives: a parameter the code reads
but the schema does not name cannot be called by anything that reads the
schema, and one the schema names but the code never reads is a promise nothing
keeps.  Neither shows up as a failure anywhere -- the call just quietly does
less than it was asked to.

The tool list comes from the running server (tools/list over stdio); the
argument names come from reading the dispatcher in McpServer.cpp.

    python tools/check_mcp_schemas.py
"""
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXE = ROOT / "build-win" / "HachiShifterNext_artefacts" / "RelWithDebInfo" / "HachiShifter Next.exe"
SOURCE = ROOT / "juce" / "src" / "backend" / "McpServer.cpp"

# The five every tool that analyses audio takes, through analysisConfig(args).
ANALYSIS = ["game_model_dir", "fcpe_model", "game_model", "inference", "device_index"]

READS = re.compile(
    r'(?:string|number|boolean|strings|integer)\(\s*(?:args|arguments)\s*,\s*"([A-Za-z_0-9]+)"'
    r'|args\.hasProperty\("([A-Za-z_0-9]+)"\)'
    r'|args\.getProperty\("([A-Za-z_0-9]+)"'
    r'|args\["([A-Za-z_0-9]+)"\]')


def arguments_read():
    """What each tool's branch of the dispatcher asks the arguments for."""
    text = SOURCE.read_text(encoding="utf-8")
    body = text[text.index("juce::var McpServer::callTool"):]
    marks = [(m.start(), m.group(1)) for m in re.finditer(r'name == "([a-z_0-9]+)"', body)]
    found = {}
    for index, (position, tool) in enumerate(marks):
        end = marks[index + 1][0] if index + 1 < len(marks) else len(body)
        block = body[position:end]
        names = []
        for match in READS.finditer(block):
            name = next(group for group in match.groups() if group)
            if name not in names:
                names.append(name)
        if "analysisConfig(args)" in block:
            names += [name for name in ANALYSIS if name not in names]
        found[tool] = names
    return found


def tools_listed():
    """What the server tells a client each tool takes."""
    requests = [
        {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}},
        {"jsonrpc": "2.0", "id": 2, "method": "tools/list"},
    ]
    done = subprocess.run([str(EXE), "--mcp"],
                          input="\n".join(json.dumps(r) for r in requests) + "\n",
                          capture_output=True, text=True, encoding="utf-8", timeout=300)
    for line in done.stdout.splitlines():
        reply = json.loads(line)
        if reply.get("id") == 2:
            return {tool["name"]: tool for tool in reply["result"]["tools"]}
    sys.exit("the server did not answer tools/list:\n" + done.stdout[-2000:])


def main():
    if not EXE.exists():
        sys.exit("build the editor first: %s" % EXE)
    listed = tools_listed()
    read = arguments_read()
    problems = []

    for name in sorted(set(read) - set(listed)):
        problems.append("%s is in the dispatcher but not in tools/list" % name)
    for name in sorted(set(listed) - set(read)):
        problems.append("%s is offered but the dispatcher has no branch for it" % name)

    for name, tool in sorted(listed.items()):
        schema = tool.get("inputSchema", {})
        properties = schema.get("properties", {})
        required = schema.get("required", [])
        wanted = read.get(name, [])
        if schema.get("type") != "object":
            problems.append("%s: the schema is not an object" % name)
        if wanted and not properties:
            problems.append("%s: reads %s and names none of it" % (name, ", ".join(wanted)))
        for argument in wanted:
            if argument not in properties:
                problems.append("%s: reads %s, which the schema does not name" % (name, argument))
        for declared in properties:
            if declared not in wanted:
                problems.append("%s: names %s, which the dispatcher never reads" % (name, declared))
        for entry in required:
            if entry not in properties:
                problems.append("%s: requires %s, which is not a property" % (name, entry))
        for key, entry in properties.items():
            if not entry.get("description"):
                problems.append("%s: %s has no description" % (name, key))
            if entry.get("type") not in ("string", "number", "integer", "boolean",
                                         "array", "object"):
                problems.append("%s: %s has type %r" % (name, key, entry.get("type")))

    declared = sum(len(tool.get("inputSchema", {}).get("properties", {}))
                   for tool in listed.values())
    print("tools %d | parameters named %d | tools with parameters %d"
          % (len(listed), declared,
             sum(1 for t in listed.values() if t["inputSchema"].get("properties"))))
    print("problems:", len(problems))
    for problem in problems:
        print("  ", problem)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())

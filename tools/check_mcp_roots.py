"""Ask a running MCP server to read things it should not, and see it refuse.

read_file and list_directory see only where the session's own work lives: the
folders it was started with (--roots=A;B, HACHISHIFTER_MCP_ROOTS), the ones it
has since been asked to work in, and the voicebanks and media of the open
project.  --smoke-mcp-roots measures the rule; this drives a real server over
stdio, which is the only way to see the rule actually reached from a call.

    python tools/check_mcp_roots.py
"""
import json
import pathlib
import subprocess
import sys
import tempfile
import uuid

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXE = ROOT / "build-win" / "HachiShifterNext_artefacts" / "RelWithDebInfo" / "HachiShifter Next.exe"
CRLF = "\r\n"


def talk(calls, roots=None, env_roots=None):
    """One server, one conversation: [(tool, arguments), ...] -> [text, ...]."""
    lines = [json.dumps({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}})]
    for index, (tool, args) in enumerate(calls, start=2):
        lines.append(json.dumps({"jsonrpc": "2.0", "id": index, "method": "tools/call",
                                 "params": {"name": tool, "arguments": args}}))
    argv = [str(EXE), "--mcp"]
    if roots:
        argv.append("--roots=" + ";".join(str(r) for r in roots))
    import os
    environment = dict(os.environ)
    if env_roots:
        environment["HACHISHIFTER_MCP_ROOTS"] = ";".join(str(r) for r in env_roots)
    else:
        environment.pop("HACHISHIFTER_MCP_ROOTS", None)
    done = subprocess.run(argv, input="\n".join(lines) + "\n", capture_output=True,
                          text=True, encoding="utf-8", timeout=300, env=environment)
    answers = {}
    for line in done.stdout.splitlines():
        if not line.strip():
            continue
        reply = json.loads(line)
        result = reply.get("result", {})
        if "content" in result:
            answers[reply["id"]] = (result.get("isError", False),
                                    result["content"][0]["text"])
    return [answers.get(index, (True, "no answer")) for index in range(2, len(calls) + 2)]


def main():
    if not EXE.exists():
        sys.exit("build the editor first: %s" % EXE)
    work = pathlib.Path(tempfile.gettempdir()) / ("hachi-mcp-roots-" + uuid.uuid4().hex[:8])
    inside, outside = work / "song", work / "elsewhere"
    inside.mkdir(parents=True)
    outside.mkdir(parents=True)
    # A name that begins with the allowed one: a root that matched by prefix
    # rather than by folder would hand this over.
    sibling = work / "song-private"
    sibling.mkdir()
    (sibling / "secret.txt").write_text("not for the server", encoding="utf-8")
    (outside / "secret.txt").write_text("not for the server", encoding="utf-8")
    (inside / "notes.txt").write_text("part of the work", encoding="utf-8")
    ust = inside / "song.ust"
    ust.write_text(CRLF.join(["[#VERSION]", "UST Version1.2", "[#SETTING]", "Tempo=120.00",
                              "Tracks=1", "ProjectName=roots", "[#0000]", "Length=480",
                              "Lyric=a", "NoteNum=60", "[#TRACKEND]", ""]), encoding="utf-8")

    problems = []

    def want(label, condition, saw):
        if not condition:
            problems.append("%s -- saw %r" % (label, saw))

    # 1. A fresh server has been asked to work nowhere, so it may read nothing.
    read, listed = talk([("read_file", {"path": str(outside / "secret.txt")}),
                         ("list_directory", {"path": str(outside)})])
    want("a fresh server refuses to read a file", read[0], read[1])
    want("a fresh server says how to allow a folder",
         "--roots" in read[1] or "open a project" in read[1], read[1])
    want("a fresh server refuses to list a folder", listed[0], listed[1])

    # 2. Importing from a folder makes that folder readable, and nothing else.
    imported, near, far, prefix = talk([
        ("import_ust", {"path": str(ust)}),
        ("read_file", {"path": str(inside / "notes.txt")}),
        ("read_file", {"path": str(outside / "secret.txt")}),
        ("read_file", {"path": str(sibling / "secret.txt")}),
    ])
    want("the UST is imported", not imported[0], imported[1])
    want("a file beside the imported one is read", not near[0], near[1])
    want("a file elsewhere is still refused", far[0], far[1])
    want("a folder whose name starts with an allowed one is refused", prefix[0], prefix[1])

    # 3. A folder that failed to be worked in is not allowed by the attempt.
    failed, after = talk([("import_ust", {"path": str(outside / "secret.txt")}),
                          ("read_file", {"path": str(outside / "secret.txt")})])
    want("a UST that cannot be read fails", failed[0], failed[1])
    want("and the attempt allows nothing", after[0], after[1])

    # 4. Started with a root: that folder is readable from the first call.
    given, listing = talk([("read_file", {"path": str(outside / "secret.txt")}),
                           ("list_directory", {"path": str(outside)})], roots=[outside])
    want("--roots allows a folder", not given[0], given[1])
    want("--roots allows listing it", not listing[0], listing[1])

    # 5. The environment variable does the same.
    from_env, = talk([("read_file", {"path": str(outside / "secret.txt")})],
                     env_roots=[outside])
    want("HACHISHIFTER_MCP_ROOTS allows a folder", not from_env[0], from_env[1])

    # 6. An allowed folder's subfolders come with it; its parent does not.
    deep = outside / "deeper" / "still"
    deep.mkdir(parents=True)
    (deep / "file.txt").write_text("deep", encoding="utf-8")
    child, parent = talk([("read_file", {"path": str(deep / "file.txt")}),
                          ("list_directory", {"path": str(work)})], roots=[outside])
    want("a file deeper inside an allowed folder is read", not child[0], child[1])
    want("the folder above an allowed one is refused", parent[0], parent[1])

    # 7. Writing somewhere makes that folder readable as well.  Nothing in the
    #    project points at what was written, so this is the only thing that
    #    says a folder became the session's own work by being worked in --
    #    importing proves less, since the clip remembers the file it came from.
    written = work / "written"
    written.mkdir()
    before, imported2, exported, after_write = talk([
        ("read_file", {"path": str(written / "song.mid")}),
        ("import_ust", {"path": str(ust)}),
        ("export_midi", {"path": str(written / "song.mid")}),
        ("read_file", {"path": str(written / "song.mid")}),
    ])
    want("a folder nothing has happened in is refused", before[0], before[1])
    want("the MIDI is exported", not exported[0], exported[1])
    want("and what was written there can be read back", not after_write[0], after_write[1])

    print("checks: 16 | problems:", len(problems))
    for problem in problems:
        print("  ", problem)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())

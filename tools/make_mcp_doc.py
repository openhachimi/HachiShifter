"""Write the tool table in docs/mcp.md from the server's own tools/list.

The table is what a reader looks at first, and a table typed by hand goes stale
the first time a parameter is renamed.  This asks the server and writes the
section between the <!-- TOOLS --> marker and the heading after it.

    python tools/make_mcp_doc.py
"""
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXE = ROOT / "build-win" / "HachiShifterNext_artefacts" / "RelWithDebInfo" / "HachiShifter Next.exe"
DOC = ROOT / "docs" / "mcp.md"
MARKER = "<!-- TOOLS -->"

# The analysis settings every tool that reads audio takes; they are described
# once above the table rather than five times inside it.
ANALYSIS = {"game_model_dir", "fcpe_model", "game_model", "inference", "device_index"}

GROUPS = [
    ("工程", ["project_new", "project_open", "project_save", "project_snapshot"]),
    ("导入与导出", ["import_audio", "import_midi", "import_ust", "import_melodyne",
                    "export_midi", "export_wav", "analyse_audio", "analysis_status"]),
    ("速度与音轨", ["set_tempo", "add_track", "set_track", "remove_track"]),
    ("采样", ["set_clip", "move_clip", "resize_clip", "duplicate_clip", "remove_clip"]),
    ("音符", ["add_note", "set_note", "resize_note", "transpose_note", "edit_notes_pitch",
              "duplicate_notes", "set_pitch_curve", "toggle_note_connection", "remove_note"]),
    ("撤销", ["undo", "redo"]),
    ("渲染与播放", ["utau_render_selection", "set_utau_resampler", "render_prepare",
                    "render_status", "transport_play", "transport_stop", "transport_seek",
                    "transport_status"]),
    ("音源与分段", ["sample_settings_read", "sample_settings_save", "oto_import",
                    "oto_export", "jie_oto_create", "voicebank_import"]),
    ("文件", ["read_file", "list_directory"]),
]


def tools():
    requests = [{"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}},
                {"jsonrpc": "2.0", "id": 2, "method": "tools/list"}]
    done = subprocess.run([str(EXE), "--mcp"],
                          input="\n".join(json.dumps(r) for r in requests) + "\n",
                          capture_output=True, text=True, encoding="utf-8", timeout=300)
    for line in done.stdout.splitlines():
        reply = json.loads(line)
        if reply.get("id") == 2:
            return {tool["name"]: tool for tool in reply["result"]["tools"]}
    sys.exit("the server did not answer tools/list")


def row(tool):
    schema = tool.get("inputSchema", {})
    properties = schema.get("properties", {})
    required = set(schema.get("required", []))
    names = []
    for name in properties:
        if name in ANALYSIS:
            continue
        names.append("**%s**" % name if name in required else name)
    if not names and properties:
        names = ["（分析参数）"]
    chinese = tool["description"].split("/")[-1].strip()
    return "| `%s` | %s | %s |" % (tool["name"], chinese, "、".join(names) or "—")


def main():
    if not EXE.exists():
        sys.exit("build the editor first: %s" % EXE)
    listed = tools()
    lines = [MARKER, "", "粗体是必填的参数。", ""]
    written = set()
    for title, names in GROUPS:
        lines += ["### " + title, "", "| 工具 | 做什么 | 参数 |", "|---|---|---|"]
        for name in names:
            if name not in listed:
                sys.exit("docs name a tool the server does not offer: " + name)
            lines.append(row(listed[name]))
            written.add(name)
        lines.append("")
    missing = sorted(set(listed) - written)
    if missing:
        sys.exit("these tools are in no group, so the table would hide them: "
                 + ", ".join(missing))

    text = DOC.read_text(encoding="utf-8")
    start = text.index(MARKER)
    end = text.index("\n## ", start)
    DOC.write_text(text[:start] + "\n".join(lines) + text[end:], encoding="utf-8")
    print("docs/mcp.md: %d tools in %d groups" % (len(listed), len(GROUPS)))


if __name__ == "__main__":
    main()

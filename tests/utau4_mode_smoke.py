#!/usr/bin/env python3
"""Model-free smoke test for the four-region UTAU mode.

The mode is TrackData::utauFourRegion layered on PitchAlgorithm::utau rather
than a separate enum value, so the things worth guarding are that it is
settable, that it is reported distinctly from plain UTAU, that it survives a
project save/open round trip, and that clearing it really goes back to plain
UTAU.  Over MCP the mode is spelled "utau4".
"""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile


class McpClient:
    def __init__(self, binary: pathlib.Path) -> None:
        self.sequence = 0
        self.process = subprocess.Popen(
            [str(binary), "--mcp"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            encoding="utf-8",
        )

    def call(self, name: str, arguments: dict | None = None) -> str:
        self.sequence += 1
        request = {
            "jsonrpc": "2.0",
            "id": self.sequence,
            "method": "tools/call",
            "params": {"name": name, "arguments": arguments or {}},
        }
        assert self.process.stdin is not None
        assert self.process.stdout is not None
        self.process.stdin.write(json.dumps(request, separators=(",", ":")) + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError("MCP process ended before responding: " + self.stderr())
        result = json.loads(line)["result"]
        text = result["content"][0]["text"]
        if result.get("isError"):
            raise RuntimeError(f"{name}: {text}")
        return text

    def stderr(self) -> str:
        if self.process.stderr is None:
            return ""
        return self.process.stderr.read().strip()

    def close(self) -> None:
        if self.process.stdin is not None:
            self.process.stdin.close()
        self.process.wait(timeout=15)
        if self.process.returncode != 0:
            raise RuntimeError(f"MCP exited with {self.process.returncode}: {self.stderr()}")


def algorithms(snapshot_text: str) -> list[tuple[str, str]]:
    data = json.loads(snapshot_text)
    return [(track.get("name"), track.get("pitch_algorithm")) for track in data["tracks"]]


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: utau4_mode_smoke.py HachiShifterNext")
    binary = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="hachishifter-utau4-") as directory_text:
        project_file = pathlib.Path(directory_text) / "utau4.hjpx"
        client = McpClient(binary)

        client.call("project_new")
        client.call("add_track", {"name": "jie"})
        track_id = json.loads(client.call("project_snapshot"))["tracks"][0]["id"]

        client.call("set_track", {"track_id": track_id, "pitch_algorithm": "utau4"})
        assert algorithms(client.call("project_snapshot"))[0][1] == "utau4", \
            "set_track did not report utau4"

        client.call("project_save", {"path": str(project_file)})
        client.call("project_new")
        assert algorithms(client.call("project_snapshot")) == [], \
            "project_new left tracks behind"

        client.call("project_open", {"path": str(project_file)})
        assert algorithms(client.call("project_snapshot"))[0][1] == "utau4", \
            "utau4 did not survive save/open"

        reopened_id = json.loads(client.call("project_snapshot"))["tracks"][0]["id"]
        client.call("set_track", {"track_id": reopened_id, "pitch_algorithm": "utau"})
        assert algorithms(client.call("project_snapshot"))[0][1] == "utau", \
            "clearing the four-region flag did not return to plain utau"

        client.close()
    print("utau4 mode: set, report, persist and clear all behave")
    return 0


if __name__ == "__main__":
    sys.exit(main())

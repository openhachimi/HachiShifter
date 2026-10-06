#!/usr/bin/env python3
"""Guards that editing a note's Flags actually produces different audio.

Flags reach the resampler through the command line, so nothing about them is
visible in the rendered result until the note is rendered again.  Every layer
between the edit and the engine caches: the host keys a whole-clip render, the
UTAU renderer keys each note, and the MCP server skips re-preparing audio when
it believes the project is unchanged.  A field missing from any one of those
keys makes a Flags edit silent -- the previous audio simply plays again -- and
that is invisible in a screenshot and inaudible unless you know the flag.

The test renders the same note four times: without flags, with two different
volume flags, then without flags again.  The two flagged renders must differ
from the plain one and from each other, and returning to no flags must
reproduce the first render exactly, which also proves the caching still works
rather than having been disabled to force a re-render.

Needs a UTAU voicebank containing the alias and a resampler that understands
the flag, so both paths are arguments; without them the test skips.
"""

from __future__ import annotations

import hashlib
import json
import pathlib
import struct
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from utau4_mode_smoke import McpClient  # noqa: E402


def write_midi(path: pathlib.Path) -> None:
    """One note, D4, one second at 120 bpm."""
    events = bytes([0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20])
    events += bytes([0x00, 0x90, 62, 100])
    events += bytes([0x81, 0x40, 0x80, 62, 64])
    events += bytes([0x00, 0xFF, 0x2F, 0x00])
    path.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 96)
                     + b"MTrk" + struct.pack(">I", len(events)) + events)


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit("usage: flag_rerender_smoke.py BINARY VOICEBANK RESAMPLER ALIAS")
    binary, voicebank, resampler, alias = (pathlib.Path(sys.argv[1]).resolve(),
                                           pathlib.Path(sys.argv[2]),
                                           pathlib.Path(sys.argv[3]),
                                           sys.argv[4])
    if not voicebank.is_dir() or not resampler.is_file():
        print("flag re-render: skipped, no voicebank or resampler")
        return 0

    with tempfile.TemporaryDirectory(prefix="hachishifter-flags-") as directory_text:
        directory = pathlib.Path(directory_text)
        midi = directory / "one.mid"
        write_midi(midi)
        client = McpClient(binary)

        client.call("project_new")
        client.call("import_midi", {"path": str(midi)})
        track = json.loads(client.call("project_snapshot"))["tracks"][0]
        note_id = track["clips"][0]["notes"][0]["id"]
        client.call("set_track", {"track_id": track["id"], "pitch_algorithm": "utau",
                                  "voicebank_directory": str(voicebank), "compose": True})
        client.call("set_note", {"note_id": note_id, "label": alias})
        client.call("set_utau_resampler", {"path": str(resampler)})
        client.call("utau_render_selection", {"note_ids": [note_id]})

        digests = []
        for flags in ("", "P50", "P10", ""):
            client.call("set_note", {"note_id": note_id, "utau_flags": flags})
            output = directory / "render.wav"
            output.unlink(missing_ok=True)
            client.call("export_wav", {"path": str(output)})
            digests.append(hashlib.md5(output.read_bytes()).hexdigest())

        plain, first, second, again = digests
        client.close()

    assert first != plain, "P50 rendered the same audio as no flags at all"
    assert second != plain, "P10 rendered the same audio as no flags at all"
    assert first != second, "P50 and P10 rendered the same audio"
    assert again == plain, "returning to no flags did not reproduce the first render"
    print("flag re-render: every Flags edit re-renders, and reverting still hits the cache")
    return 0


if __name__ == "__main__":
    sys.exit(main())

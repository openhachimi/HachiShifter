#!/usr/bin/env python3
"""Guards that a note's Flags win over the track's, kernel selection included.

The engine parses flags first-wins, so the two strings have to be concatenated
note-first or the track silently overrides every note.  Nothing about that is
visible except in the audio, and the two orders are equally plausible to read
in the source, so it is worth pinning.

Kernel flags (K/L, V, M, HF) are one group rather than independent settings: a
note asking for K2 while the track asks for HF must get K2 alone, not both at
once.  The renders below check that by comparing against the note-only render,
which is what "the note decides" has to mean.
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
        raise SystemExit("usage: flag_precedence_smoke.py BINARY VOICEBANK RESAMPLER ALIAS")
    binary, voicebank, resampler, alias = (pathlib.Path(sys.argv[1]).resolve(),
                                           pathlib.Path(sys.argv[2]),
                                           pathlib.Path(sys.argv[3]),
                                           sys.argv[4])
    if not voicebank.is_dir() or not resampler.is_file():
        print("flag precedence: skipped, no voicebank or resampler")
        return 0

    with tempfile.TemporaryDirectory(prefix="hachishifter-precedence-") as text:
        directory = pathlib.Path(text)
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

        def render(note_flags: str, track_flags: str) -> str:
            client.call("set_track", {"track_id": track["id"],
                                      "utau_global_flags": track_flags})
            client.call("set_note", {"note_id": note_id, "utau_flags": note_flags})
            output = directory / "render.wav"
            output.unlink(missing_ok=True)
            client.call("export_wav", {"path": str(output), "timeout_seconds": 400})
            return hashlib.md5(output.read_bytes()).hexdigest()

        # A plain value the track also sets: the note's must be what is heard.
        note_only = render("P50", "")
        track_only = render("", "P10")
        both = render("P50", "P10")

        # Kernel flags are one group.  K2 on the note has to shut out HF on the
        # track completely, not run alongside it.
        kernel_note_only = render("K2", "")
        kernel_both = render("K2", "HF")

        client.close()

    assert note_only != track_only, "P50 and P10 rendered the same, test proves nothing"
    assert both == note_only, "the track's Flags overrode the note's"
    assert kernel_both == kernel_note_only, \
        "the track's kernel flag survived alongside the note's"
    print("flag precedence: note Flags win, and a note kernel shuts out the track's")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Guards that a drawn envelope still covers a note that has since grown.

An envelope belongs to the note as it stood when it was drawn, and the note
moves afterwards -- stretched longer, or handed more time by a slower consonant
on the note after it.  Past its last point an envelope holds that point's gain,
so one that ends early silences the rest of the note.

Carrying only the closing point out to the new end is not enough: that stretches
the fall it belongs to, so a note twice as long fades for twice as long, which
is a shape nobody chose.  Both ends follow and each takes its ramp with it --
the same rule the piano roll draws by, so what is heard is what is shown.

This measures the rendered audio rather than the numbers, because the numbers
were right on screen while the mix was doing something else.
"""

from __future__ import annotations

import json
import math
import pathlib
import struct
import sys
import tempfile
import wave

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from utau4_mode_smoke import McpClient  # noqa: E402

NOTE_SECONDS = 1.0
# As if drawn while the note was a fifth of its present length.
DRAWN = [[0.0, -60], [0.015, 0], [0.165, 0], [0.20, -60]]


def write_midi(path: pathlib.Path) -> None:
    """One note, D4, a whole second of it at 120 bpm."""
    events = bytes([0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20])
    events += bytes([0x00, 0x90, 62, 100])
    events += bytes([0x81, 0x40, 0x80, 62, 64])    # 192 ticks = two quarters
    events += bytes([0x00, 0xFF, 0x2F, 0x00])
    path.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 96)
                     + b"MTrk" + struct.pack(">I", len(events)) + events)


def rms(path: pathlib.Path, start: float, end: float) -> float:
    with wave.open(str(path), "rb") as source:
        rate, width, channels = (source.getframerate(), source.getsampwidth(),
                                 source.getnchannels())
        first = max(0, int(start * rate))
        count = max(1, int((end - start) * rate))
        source.setpos(min(first, source.getnframes() - 1))
        raw = source.readframes(min(count, source.getnframes() - first))
    step = width * channels
    total, samples = 0.0, 0
    full = float(1 << (width * 8 - 1))
    for offset in range(0, len(raw) - step + 1, step):
        chunk = raw[offset:offset + width]
        value = int.from_bytes(chunk, "little", signed=True) / full
        total += value * value
        samples += 1
    return math.sqrt(total / samples) if samples else 0.0


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(
            "usage: envelope_stretch_smoke.py BINARY VOICEBANK RESAMPLER ALIAS")
    binary, voicebank, resampler, alias = (pathlib.Path(sys.argv[1]).resolve(),
                                           pathlib.Path(sys.argv[2]),
                                           pathlib.Path(sys.argv[3]),
                                           sys.argv[4])
    if not voicebank.is_dir() or not resampler.is_file():
        print("envelope stretch: skipped, no voicebank or resampler")
        return 0

    with tempfile.TemporaryDirectory(prefix="hachishifter-stretch-") as text:
        directory = pathlib.Path(text)
        midi = directory / "one.mid"
        write_midi(midi)
        output = directory / "mix.wav"
        client = McpClient(binary)
        try:
            client.call("project_new")
            client.call("import_midi", {"path": str(midi)})
            track = json.loads(client.call("project_snapshot"))["tracks"][0]
            note = track["clips"][0]["notes"][0]["id"]
            client.call("set_track", {"track_id": track["id"],
                                      "pitch_algorithm": "utau",
                                      "voicebank_directory": str(voicebank),
                                      "compose": True})
            client.call("set_note", {"note_id": note, "label": alias})
            client.call("set_note", {"note_id": note, "amplitude_envelope": DRAWN})
            client.call("set_utau_resampler", {"path": str(resampler)})
            client.call("utau_render_selection", {"note_ids": [note]})
            client.call("export_wav", {"path": str(output), "timeout_seconds": 400})
        finally:
            client.close()

        # Inside the plateau as drawn, and well past where it used to end.
        early = rms(output, 0.05, 0.15)
        late = rms(output, 0.55, 0.75)

    assert early > 1.0e-4, \
        f"the note is silent even where it was drawn ({early}); nothing to compare"
    # Held out, the two are within a few dB of each other.  Stretched, the fall
    # has been running since 0.165 and by 0.65 has taken the level far down.
    ratio = late / early
    assert ratio > 0.35, (
        f"the note fades away where its envelope used to end: {ratio:.3f} of the"
        " level it has at the start, so the closing ramp was stretched over the"
        " whole note rather than carried to its end")
    print(f"envelope stretch: a grown note keeps its level to the end"
          f" ({ratio:.2f} of the early level)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""The native pitch backends still make a sound, and still change the pitch.

llsm2 turns a recording into LLSM frames, moves their F0 and resynthesises.
It refuses -- returning nothing at all -- on a long list of conditions: a
non-finite sample rate, an F0 outside 40 Hz .. 0.45 fs, a time map that is not
monotonic, more frames than it will hold, a frame that fails its own layer-1
check.  Any of those firing when it should not turns the whole track silent
rather than wrong, which is easy to miss and hard to attribute.

So: render one note through each native backend, twice, an octave apart.  Each
render has to be audible, and the two have to differ -- if they came out the
same, the pitch was never moved and this proves nothing about the transform.

usage: native_pitch_smoke.py BINARY SOURCE.wav
"""

from __future__ import annotations

import json
import pathlib
import struct
import sys
import tempfile
import wave

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from utau4_mode_smoke import McpClient  # noqa: E402

BACKENDS = ("llsm2", "mld5", "world")


def write_midi(path: pathlib.Path) -> None:
    """One two-second note at 120 bpm, long enough to be worth transforming."""
    events = bytes([0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20])
    events += bytes([0x00, 0x90, 60, 100])
    events += bytes([0x83, 0x00, 0x80, 60, 64])
    events += bytes([0x00, 0xFF, 0x2F, 0x00])
    path.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 96)
                     + b"MTrk" + struct.pack(">I", len(events)) + events)


def samples(path: pathlib.Path):
    with wave.open(str(path)) as w:
        n, width, channels = w.getnframes(), w.getsampwidth(), w.getnchannels()
        raw = w.readframes(n)
    step = width * channels
    return [int.from_bytes(raw[i:i + width], "little", signed=True)
            for i in range(0, len(raw) - step + 1, step)]


def peak(values) -> float:
    return max((abs(v) for v in values), default=0) / 32768.0


def difference(a, b) -> float:
    """Best-fit gain removed, then what is left, as a fraction of level."""
    n = min(len(a), len(b))
    a, b = a[:n], b[:n]
    den = sum(v * v for v in a) or 1e-12
    gain = sum(p * q for p, q in zip(a, b)) / den
    left = sum((gain * p - q) ** 2 for p, q in zip(a, b))
    return (left / (sum(q * q for q in b) or 1e-12)) ** 0.5


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(__doc__.strip().splitlines()[-1])
    binary, source = pathlib.Path(sys.argv[1]).resolve(), pathlib.Path(sys.argv[2])
    if not source.is_file():
        print("native pitch: skipped, no source recording")
        return 0

    report = []
    with tempfile.TemporaryDirectory(prefix="hachishifter-native-") as text:
        directory = pathlib.Path(text)
        midi = directory / "one.mid"
        write_midi(midi)
        client = McpClient(binary)
        try:
            for backend in BACKENDS:
                client.call("project_new")
                client.call("import_midi", {"path": str(midi)})
                snapshot = json.loads(client.call("project_snapshot"))
                track = snapshot["tracks"][0]
                client.call("set_track", {"track_id": track["id"],
                                          "pitch_algorithm": backend,
                                          "compose": True})
                client.call("import_audio", {"path": str(source),
                                             "track_id": track["id"]})
                # Importing audio brings its own clip, analysed into its own
                # notes.  Those are the ones the renderer transforms; the notes
                # the MIDI arrived with belong to a clip with no recording
                # behind it and moving them changes nothing.
                snapshot = json.loads(client.call("project_snapshot"))
                clip = next(c for t in snapshot["tracks"] for c in t["clips"]
                            if str(c.get("source_file", "")).lower().endswith(".wav")
                            and c.get("notes"))
                note = clip["notes"][0]["id"]

                def render(name: str, semitones: float) -> pathlib.Path:
                    output = directory / name
                    output.unlink(missing_ok=True)
                    if semitones:
                        client.call("transpose_note", {"note_id": note,
                                                       "semitones": semitones})
                    client.call("export_wav", {"path": str(output),
                                               "timeout_seconds": 600})
                    return output

                low = samples(render(f"{backend}-low.wav", 0.0))
                high = samples(render(f"{backend}-high.wav", 12.0))
                lowPeak, highPeak = peak(low), peak(high)
                moved = difference(low, high)
                report.append(f"{backend} {lowPeak:.3f}/{highPeak:.3f} moved {moved:.3f}")
                assert lowPeak > 0.005 and highPeak > 0.005, (
                    f"{backend} rendered silence ({lowPeak:.4f}, {highPeak:.4f});"
                    " the backend refused the render rather than performing it")
                assert moved > 0.05, (
                    f"{backend} sounds the same an octave apart ({moved:.4f}),"
                    " so the pitch was never moved")
        finally:
            client.close()

    print("native pitch: " + "; ".join(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""A per-region flag acts, and acts only on its own region.

V1 and V2 synthesise their harmonics through the L1 path, which used to be
handed one set of flags for the whole note -- so a per-region flag was
inaudible in the default kernel, silently, while working in L2.  This renders
the same note with and without a flag on the nucleus alone and requires the
nucleus to change and the consonant not to.

usage: region_flag_smoke.py BINARY VOICEBANK RESAMPLER ALIAS
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

NOTE_START = 0.5
NOTE_SECONDS = 1.0


def write_midi(path: pathlib.Path) -> None:
    events = bytes([0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20])
    events += bytes([0x60, 0x90, 62, 100])
    events += bytes([0x81, 0x40, 0x80, 62, 64])
    events += bytes([0x00, 0xFF, 0x2F, 0x00])
    path.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 96)
                     + b"MTrk" + struct.pack(">I", len(events)) + events)


def samples(path: pathlib.Path, start: float, end: float) -> list[int]:
    with wave.open(str(path), "rb") as source:
        rate, width, channels = (source.getframerate(), source.getsampwidth(),
                                 source.getnchannels())
        first = max(0, int(start * rate))
        if first >= source.getnframes():
            return []
        source.setpos(first)
        count = min(int((end - start) * rate), source.getnframes() - first)
        raw = source.readframes(max(1, count))
    step = width * channels
    return [int.from_bytes(raw[at:at + width], "little", signed=True)
            for at in range(0, len(raw) - step + 1, step)]


def difference(first: list[int], second: list[int]) -> float:
    energy = sum(value * value for value in second)
    if energy <= 0 or not first:
        return 1.0
    gain = sum(a * b for a, b in zip(first, second)) / energy
    peak = max(1, max(abs(value) for value in first))
    return max(abs(a - gain * b) for a, b in zip(first, second)) / peak


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit("usage: region_flag_smoke.py BINARY VOICEBANK RESAMPLER ALIAS")
    binary, voicebank, resampler, alias = (pathlib.Path(sys.argv[1]).resolve(),
                                           pathlib.Path(sys.argv[2]),
                                           pathlib.Path(sys.argv[3]),
                                           sys.argv[4])
    if not voicebank.is_dir() or not resampler.is_file():
        print("region flags: skipped, no voicebank or resampler")
        return 0

    results = {}
    with tempfile.TemporaryDirectory(prefix="hachishifter-regionflag-") as text:
        directory = pathlib.Path(text)
        midi = directory / "one.mid"
        write_midi(midi)
        client = McpClient(binary)
        try:
            client.call("project_new")
            client.call("import_midi", {"path": str(midi)})
            track = json.loads(client.call("project_snapshot"))["tracks"][0]
            note = track["clips"][0]["notes"][0]["id"]
            client.call("set_track", {"track_id": track["id"],
                                      "pitch_algorithm": "utau4",
                                      "voicebank_directory": str(voicebank),
                                      "compose": True})
            client.call("set_note", {"note_id": note, "label": alias})
            client.call("set_utau_resampler", {"path": str(resampler)})

            def render(name: str, **note_args) -> pathlib.Path:
                output = directory / name
                output.unlink(missing_ok=True)
                client.call("set_note", dict({"note_id": note}, **note_args))
                client.call("export_wav", {"path": str(output),
                                           "timeout_seconds": 600})
                return output

            # "" is the default kernel (V2), whose harmonics come from L1 --
            # the one this was broken in.  L2 is the control that always worked.
            for kernel in ("", "L2"):
                plain = render(f"plain{kernel or 'V2'}.wav", utau_flags=kernel,
                               flag_split=False, region_flags=["", "", "", ""])
                split = render(f"split{kernel or 'V2'}.wav", utau_flags=kernel,
                               flag_split=True, region_flags=["", "", "g-40", ""])
                results[kernel or "V2"] = (plain, split)
        finally:
            client.close()

        # The consonant sounds before the beat; the nucleus is well inside.
        consonant = (NOTE_START - 0.06, NOTE_START - 0.01)
        nucleus = (NOTE_START + 0.45, NOTE_START + 0.80)
        measured = {}
        for kernel, (plain, split) in results.items():
            measured[kernel] = (
                difference(samples(split, *nucleus), samples(plain, *nucleus)),
                difference(samples(split, *consonant), samples(plain, *consonant)))

    for kernel, (moved, consonant_moved) in measured.items():
        assert moved > 0.20, (
            f"{kernel}: a flag on the nucleus changed nothing there"
            f" ({moved:.3f}) -- per-region flags are not reaching this kernel")
        assert consonant_moved < 0.10, (
            f"{kernel}: a flag on the nucleus reached the consonant too"
            f" ({consonant_moved:.3f})")
    print("region flags: act on their own region only "
          + ", ".join(f"{kernel} nucleus {moved:.3f} / consonant {rest:.3f}"
                      for kernel, (moved, rest) in sorted(measured.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())

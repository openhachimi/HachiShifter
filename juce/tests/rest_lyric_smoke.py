#!/usr/bin/env python3
"""A lyric of RR is a rest: that stretch of the phrase comes out silent.

Not the same as an empty lyric, which renders the piano preview tone, and not
the same as an alias the voicebank happens not to have, which is silent too but
is reported back as missing.  RR is silent because it was asked to be.

Two notes are rendered twice: once both sung, once with the second one's lyric
set to RR.  The first note has to come out the same either way -- a rest has no
lead-in to reach back with and must not take anything from what came before --
and the second note's stretch has to fall to nothing.

usage: rest_lyric_smoke.py BINARY VOICEBANK RESAMPLER ALIAS
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


def write_midi(path: pathlib.Path) -> None:
    """Two half-second notes at 120 bpm, half a second apart: one to sing and
    one to silence, with a gap so neither reaches into the other."""
    events = bytes([0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20])   # 120 bpm
    events += bytes([0x60, 0x90, 62, 100])                        # on  at 0.5s
    events += bytes([0x60, 0x80, 62, 64])                         # off at 1.0s
    events += bytes([0x60, 0x90, 62, 100])                        # on  at 1.5s
    events += bytes([0x60, 0x80, 62, 64])                         # off at 2.0s
    events += bytes([0x00, 0xFF, 0x2F, 0x00])
    path.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 96)
                     + b"MTrk" + struct.pack(">I", len(events)) + events)



def samples(path: pathlib.Path):
    with wave.open(str(path)) as w:
        n, width, channels = w.getnframes(), w.getsampwidth(), w.getnchannels()
        raw = w.readframes(n)
        rate = w.getframerate()
    step = width * channels
    return rate, [int.from_bytes(raw[i:i + width], "little", signed=True)
                  for i in range(0, len(raw) - step + 1, step)]


def level(values, rate, lo, hi):
    """Peak level over [lo, hi) seconds, as a fraction of full scale."""
    a, b = int(lo * rate), min(len(values), int(hi * rate))
    if b <= a:
        return 0.0
    return max(abs(v) for v in values[a:b]) / 32768.0


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(__doc__.strip().splitlines()[-1])
    binary, voicebank, resampler, alias = (pathlib.Path(sys.argv[1]).resolve(),
                                           pathlib.Path(sys.argv[2]),
                                           pathlib.Path(sys.argv[3]),
                                           sys.argv[4])
    if not voicebank.is_dir() or not resampler.is_file():
        print("rest lyric: skipped, no voicebank or resampler")
        return 0

    with tempfile.TemporaryDirectory(prefix="hachishifter-rest-") as text:
        directory = pathlib.Path(text)
        midi = directory / "one.mid"
        write_midi(midi)
        client = McpClient(binary)
        try:
            client.call("project_new")
            client.call("import_midi", {"path": str(midi)})
            track = json.loads(client.call("project_snapshot"))["tracks"][0]
            notes = track["clips"][0]["notes"]
            if len(notes) < 2:
                print("rest lyric: skipped, the fixture has one note")
                return 0
            client.call("set_track", {"track_id": track["id"],
                                      "pitch_algorithm": "utau",
                                      "voicebank_directory": str(voicebank),
                                      "compose": True})
            client.call("set_utau_resampler", {"path": str(resampler)})
            for note in notes:
                client.call("set_note", {"note_id": note["id"], "label": alias})

            def render(name: str) -> pathlib.Path:
                output = directory / name
                output.unlink(missing_ok=True)
                client.call("export_wav", {"path": str(output),
                                           "timeout_seconds": 600})
                return output

            sung = render("sung.wav")
            client.call("set_note", {"note_id": notes[1]["id"], "label": "RR"})
            rest = render("rest.wav")
        finally:
            client.close()

        rate, a = samples(sung)
        _, b = samples(rest)
        # Where the two notes are, from the imported MIDI.
        first = (notes[0]["start_seconds"], notes[0]["start_seconds"]
                 + notes[0]["duration_seconds"])
        second = (notes[1]["start_seconds"], notes[1]["start_seconds"]
                  + notes[1]["duration_seconds"])
        # Clear of the boundary at either end: a lead-in reaches back before a
        # note starts, and a tail runs on past where it ends.
        inset = 0.05
        sung_second = level(a, rate, second[0] + inset, second[1] - inset)
        rest_second = level(b, rate, second[0] + inset, second[1] - inset)
        sung_first = level(a, rate, first[0] + inset, first[1] - inset)
        rest_first = level(b, rate, first[0] + inset, first[1] - inset)

        assert sung_second > 0.01, (
            f"the second note is silent even when sung ({sung_second:.4f});"
            " nothing was rendered and this proves nothing")
        assert rest_second < sung_second * 0.02, (
            f"RR still sounds: {rest_second:.4f} against {sung_second:.4f}"
            " sung, so the lyric was not read as a rest")
        assert abs(rest_first - sung_first) < sung_first * 0.05 + 1e-4, (
            f"the note before the rest changed ({sung_first:.4f} ->"
            f" {rest_first:.4f}); a rest has no lead-in and must take nothing"
            " from what came before it")

    print(f"rest lyric: RR falls to {rest_second:.4f} where the sung note is"
          f" {sung_second:.4f}, and the note before it is unchanged"
          f" ({sung_first:.4f} -> {rest_first:.4f})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

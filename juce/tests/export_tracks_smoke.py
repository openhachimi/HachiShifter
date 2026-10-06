#!/usr/bin/env python3
"""Exporting writes one file per track, holding only that track's audio.

Two UTAU tracks, each with a single note, sounding at different moments.
Nothing is selected first: rendering is selection-driven, so an export that
did not ask for the whole song would write silence.  Each track's file has to
carry its own note and nothing at the other's moment, and the mix both.

usage: export_tracks_smoke.py BINARY VOICEBANK RESAMPLER ALIAS
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

FIRST_SECONDS = 0.5
SECOND_SECONDS = 2.0
NOTE_SECONDS = 0.5


def variable_length(value: int) -> bytes:
    out = bytes([value & 0x7F])
    value >>= 7
    while value:
        out = bytes([(value & 0x7F) | 0x80]) + out
        value >>= 7
    return out


def write_midi(path: pathlib.Path, start_seconds: float, midi_note: int) -> None:
    """One note at 120 bpm, 96 ticks to the quarter, starting where asked."""
    ticks = int(round(start_seconds / 0.5 * 96))
    events = bytes([0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20])
    events += variable_length(ticks) + bytes([0x90, midi_note, 100])
    events += variable_length(96) + bytes([0x80, midi_note, 64])
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


def peak(path: pathlib.Path, start: float, end: float) -> float:
    with wave.open(str(path), "rb") as source:
        rate, width, channels = (source.getframerate(), source.getsampwidth(),
                                 source.getnchannels())
        first = max(0, int(start * rate))
        if first >= source.getnframes():
            return 0.0
        source.setpos(first)
        count = min(int((end - start) * rate), source.getnframes() - first)
        raw = source.readframes(max(1, count))
    step = width * channels
    values = [abs(int.from_bytes(raw[at:at + width], "little", signed=True))
              for at in range(0, len(raw) - step + 1, step)]
    ceiling = float(1 << (width * 8 - 1))
    return (max(values) if values else 0.0) / ceiling


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(
            "usage: export_tracks_smoke.py BINARY VOICEBANK RESAMPLER ALIAS")
    binary, voicebank, resampler, alias = (pathlib.Path(sys.argv[1]).resolve(),
                                           pathlib.Path(sys.argv[2]),
                                           pathlib.Path(sys.argv[3]),
                                           sys.argv[4])
    if not voicebank.is_dir() or not resampler.is_file():
        print("export tracks: skipped, no voicebank or resampler")
        return 0

    with tempfile.TemporaryDirectory(prefix="hachishifter-export-") as text:
        directory = pathlib.Path(text)
        first_midi, second_midi = directory / "first.mid", directory / "second.mid"
        write_midi(first_midi, FIRST_SECONDS, 62)
        write_midi(second_midi, SECOND_SECONDS, 67)
        client = McpClient(binary)
        try:
            client.call("project_new")
            client.call("import_midi", {"path": str(first_midi)})
            client.call("import_midi", {"path": str(second_midi)})
            client.call("set_utau_resampler", {"path": str(resampler)})
            tracks = json.loads(client.call("project_snapshot"))["tracks"]
            assert len(tracks) >= 2, f"expected two tracks, got {len(tracks)}"
            tracks = tracks[:2]
            for index, track in enumerate(tracks):
                client.call("set_track", {"track_id": track["id"],
                                          "name": f"voice{index + 1}",
                                          "pitch_algorithm": "utau4",
                                          "voicebank_directory": str(voicebank),
                                          "compose": True})
                for clip in track["clips"]:
                    for note in clip["notes"]:
                        client.call("set_note", {"note_id": note["id"], "label": alias})

            # Audition a phrase first, the way anyone would, and stop where
            # that phrase ends.  The stop-here mark stays set afterwards, and
            # the export runs through the same block callback that honours it:
            # left in place it switched the transport off partway through and
            # the rest of every file came out empty.
            client.call("transport_play", {"position_seconds": 0.0,
                                           "play_until_seconds": FIRST_SECONDS + 0.2,
                                           "timeout_seconds": 600})
            client.call("transport_stop")

            # No utau_render_selection anywhere: the export has to ask for the
            # whole song itself.
            files = []
            for index, track in enumerate(tracks):
                output = directory / f"track{index + 1}.wav"
                client.call("export_wav", {"path": str(output),
                                           "track_id": track["id"],
                                           "timeout_seconds": 600})
                files.append(output)
            mixed = directory / "mix.wav"
            client.call("export_wav", {"path": str(mixed), "timeout_seconds": 600})
            # A stretch of the timeline rather than the whole song: this is
            # what "export the last render" writes, over the marquee's own
            # span.  Take the second note's neighbourhood only.
            stretch = directory / "stretch.wav"
            client.call("export_wav", {"path": str(stretch),
                                       "from_seconds": SECOND_SECONDS - 0.2,
                                       "to_seconds": SECOND_SECONDS + NOTE_SECONDS,
                                       "timeout_seconds": 600})
        finally:
            client.close()

        early = (FIRST_SECONDS, FIRST_SECONDS + NOTE_SECONDS)
        late = (SECOND_SECONDS, SECOND_SECONDS + NOTE_SECONDS)
        one_early, one_late = peak(files[0], *early), peak(files[0], *late)
        two_early, two_late = peak(files[1], *early), peak(files[1], *late)
        mix_early, mix_late = peak(mixed, *early), peak(mixed, *late)
        with wave.open(str(stretch), "rb") as handle:
            stretch_seconds = handle.getnframes() / handle.getframerate()
        # Inside the stretch the second note starts 0.2 s in.
        stretch_note = peak(stretch, 0.2, 0.2 + NOTE_SECONDS)
        # And the cut has to be that stretch of the song, sample for sample:
        # not a window of it, not shifted.  Compared against the mixdown over
        # the same seconds.
        cut = samples(stretch, 0.0, NOTE_SECONDS + 0.2)
        same = samples(mixed, SECOND_SECONDS - 0.2, SECOND_SECONDS + NOTE_SECONDS)
        scale = max(1, max(abs(value) for value in same)) if same else 1
        stretch_offset = (max(abs(a - b) for a, b in zip(cut, same)) / scale
                          if cut and same else 1.0)

    assert one_early > 0.02, (
        "the first track's file is silent where its note is, so nothing was"
        " rendered -- an export has to render the whole song, not the selection")
    assert two_late > 0.02, (
        "the second track's file is silent where its note is -- it sounds after"
        " the stop-here mark left by auditioning the first phrase, so the export"
        " stopped early")
    assert one_late < one_early * 0.05, (
        f"the first track's file carries the second track's note"
        f" ({one_late:.4f} against {one_early:.4f} of its own)")
    assert two_early < two_late * 0.05, (
        f"the second track's file carries the first track's note"
        f" ({two_early:.4f} against {two_late:.4f} of its own)")
    assert mix_early > 0.02 and mix_late > 0.02, (
        "the mixdown is missing one of the tracks")
    assert abs(stretch_seconds - (NOTE_SECONDS + 0.2)) < 0.02, (
        f"the stretch is {stretch_seconds:.3f} s long, not the"
        f" {NOTE_SECONDS + 0.2:.3f} s that was asked for")
    assert stretch_note > 0.02, "the stretch is missing the note it was cut around"
    assert stretch_offset < 0.02, (
        f"the cut is not the song over those seconds: it differs from the"
        f" mixdown there by {stretch_offset:.3f} of full scale, so the range"
        f" was read from the wrong place")
    print(f"export range: {stretch_seconds:.3f} s cut at {SECOND_SECONDS - 0.2:.2f} s,"
          f" note {stretch_note:.3f}, matches the mixdown there within"
          f" {stretch_offset:.4f}")
    print(f"export tracks: one file per track, each holding only its own"
          f" (track 1 {one_early:.3f}/{one_late:.4f}, track 2"
          f" {two_early:.4f}/{two_late:.3f}, mix {mix_early:.3f}/{mix_late:.3f})")
    return 0


if __name__ == "__main__":
    sys.exit(main())

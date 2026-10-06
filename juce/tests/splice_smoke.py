#!/usr/bin/env python3
"""Guards what splicing may and may not touch.

Splicing exists to remove the seam between two abutting notes, and the only
thing it is allowed to move is loudness: the earlier note fades out across the
overlap while the later one fades in across the same stretch, so the two
envelopes cross.  Everything else stays -- how long each note sounds, its
preutterance, its overlap.  Two earlier attempts broke exactly that, one by
writing an overlap override and one by carrying the earlier note on to its
nominal end, and both showed up as a note that had suddenly grown.

So there are four things to hold: the spans do not move, no timing override is
written, the two envelopes really do cross over the overlap, and the mix
changes when a boundary is spliced and changes back when it is not.
"""

from __future__ import annotations

import hashlib
import json
import pathlib
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from utau4_mode_smoke import McpClient  # noqa: E402


def write_midi(path: pathlib.Path) -> None:
    """Two abutting notes, D4 then F4, half a second each at 120 bpm."""
    events = bytes([0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20])
    events += bytes([0x00, 0x90, 62, 100])
    events += bytes([0x60, 0x80, 62, 64])          # +96 ticks = one quarter
    events += bytes([0x00, 0x90, 65, 100])
    events += bytes([0x60, 0x80, 65, 64])
    events += bytes([0x00, 0xFF, 0x2F, 0x00])
    path.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 96)
                     + b"MTrk" + struct.pack(">I", len(events)) + events)


class Shown:
    """One note as the roll draws it, with the envelope in absolute time."""

    def __init__(self, row: str) -> None:
        def pair(key: str) -> tuple[float, float]:
            left, right = row.split(key, 1)[1].split()[0].split("..")
            return float(left), float(right)

        self.nominal = pair("nominal=")
        self.sounding = pair("sounding=")
        self.envelope = []
        for chunk in row.split("envelope=", 1)[1].strip().split(")"):
            chunk = chunk.strip().lstrip("(")
            if chunk:
                time, gain = chunk.split(",")
                self.envelope.append((self.nominal[0] + float(time), float(gain)))


def shown(report: str) -> list[Shown]:
    return [Shown(row) for row in report.splitlines() if row.startswith("note=")]


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit("usage: splice_smoke.py BINARY VOICEBANK RESAMPLER ALIAS")
    binary, voicebank, resampler, alias = (pathlib.Path(sys.argv[1]).resolve(),
                                           pathlib.Path(sys.argv[2]),
                                           pathlib.Path(sys.argv[3]),
                                           sys.argv[4])
    if not voicebank.is_dir() or not resampler.is_file():
        print("splice: skipped, no voicebank or resampler")
        return 0

    with tempfile.TemporaryDirectory(prefix="hachishifter-splice-") as text:
        directory = pathlib.Path(text)
        midi = directory / "two.mid"
        write_midi(midi)
        client = McpClient(binary)

        client.call("project_new")
        client.call("import_midi", {"path": str(midi)})
        track = json.loads(client.call("project_snapshot"))["tracks"][0]
        notes = track["clips"][0]["notes"]
        assert len(notes) == 2, f"expected two notes, got {len(notes)}"
        first, second = notes[0]["id"], notes[1]["id"]
        client.call("set_track", {"track_id": track["id"], "pitch_algorithm": "utau",
                                  "voicebank_directory": str(voicebank), "compose": True})
        for note_id in (first, second):
            client.call("set_note", {"note_id": note_id, "label": alias})
        client.call("set_utau_resampler", {"path": str(resampler)})
        client.call("utau_render_selection", {"note_ids": [first, second]})

        def render() -> str:
            output = directory / "mix.wav"
            output.unlink(missing_ok=True)
            client.call("export_wav", {"path": str(output), "timeout_seconds": 400})
            return hashlib.md5(output.read_bytes()).hexdigest()

        def saved_project() -> bytes:
            saved = directory / "probe.hjpx"
            saved.unlink(missing_ok=True)
            client.call("project_save", {"path": str(saved)})
            return saved.read_bytes()

        def flag_is_set(raw: bytes, name: str) -> bool:
            """Is this boolean property true anywhere in the saved project?

            A serialised ValueTree writes the property name, a terminator, the
            payload size as a compressed int, then a type marker: 2 for true
            and 3 for false.  The name alone is always present, so searching
            for it proves nothing.
            """
            return name.encode() + bytes([0x00, 0x01, 0x01, 0x02]) in raw

        def overrides_written() -> bool:
            raw = saved_project()
            return flag_is_set(raw, "utauPreutteranceOverrideEnabled") \
                or flag_is_set(raw, "utauOverlapOverrideEnabled")

        def drawn() -> list[Shown]:
            project = directory / "state.hjpx"
            project.unlink(missing_ok=True)
            client.call("project_save", {"path": str(project)})
            return shown(subprocess.run(
                [str(binary), "--smoke-piano-roll", str(project)], capture_output=True,
                text=True, encoding="utf-8", errors="replace").stdout)

        plain, plain_drawn = render(), drawn()
        assert not overrides_written(), "timing overrides were set before splicing"

        # The flag belongs to the later note: "fade into what came before me".
        client.call("set_note", {"note_id": second, "utau_splice": True})
        spliced, spliced_drawn = render(), drawn()
        # Prove the instrument before trusting it: the same rule has to see the
        # flag this very step just set, or "no override" would mean nothing.
        assert flag_is_set(saved_project(), "utauSplice"), \
            "the saved-flag check cannot see a flag that is set"
        assert not overrides_written(), "splicing wrote a timing override"

        client.call("set_note", {"note_id": second, "utau_splice": False})
        unspliced = render()
        client.close()

    # Nothing about when either note sounds may move.  This is the whole
    # difference between a crossfade and a note that has been stretched.
    for index, (before, after) in enumerate(zip(plain_drawn, spliced_drawn)):
        assert abs(before.sounding[0] - after.sounding[0]) < 1.0e-9 \
            and abs(before.sounding[1] - after.sounding[1]) < 1.0e-9, \
            f"splicing moved note {index}: {before.sounding} -> {after.sounding}"
        assert before.nominal == after.nominal, f"splicing moved note {index} block"

    # The overlap: from where the later note starts sounding to where the
    # earlier one stops.  Both sides of the crossfade are measured against it.
    cross_start, cross_end = spliced_drawn[1].sounding[0], spliced_drawn[0].sounding[1]
    assert cross_end > cross_start + 0.003, \
        "this voicebank leaves no overlap, so the test cannot see a crossfade"

    earlier, later = spliced_drawn[0].envelope, spliced_drawn[1].envelope
    assert abs(earlier[-2][0] - cross_start) < 1.0e-4 and earlier[-2][1] == 0.0, \
        f"the earlier note starts falling at {earlier[-2]}, not at {cross_start}"
    assert abs(earlier[-1][0] - cross_end) < 1.0e-4 and earlier[-1][1] < -40.0, \
        f"the earlier note is still sounding at {earlier[-1]}"
    assert abs(later[0][0] - cross_start) < 1.0e-4 and later[0][1] < -40.0, \
        f"the later note starts rising at {later[0]}, not at {cross_start}"
    assert abs(later[1][0] - cross_end) < 1.0e-4 and later[1][1] == 0.0, \
        f"the later note reaches full at {later[1]}, not at {cross_end}"

    # Without the splice both ramps are the ordinary ones, so the shapes above
    # are something splicing did and not something that was always there.
    assert abs((plain_drawn[0].sounding[1] - plain_drawn[0].envelope[-2][0])
               - 0.035) < 1.0e-4, "the unspliced note already had a long fall"
    assert abs((plain_drawn[1].envelope[1][0] - plain_drawn[1].envelope[0][0])
               - 0.015) < 1.0e-4, "the unspliced note already had a long rise"

    assert spliced != plain, "splicing did not change the mix"
    assert unspliced == plain, "unsplicing did not restore the original mix"
    print("splice: crossfades the overlap, moves no span or timing, reverses cleanly")
    return 0


if __name__ == "__main__":
    sys.exit(main())

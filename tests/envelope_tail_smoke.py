#!/usr/bin/env python3
"""Guards that a drawn envelope keeps its shape while the note changes length.

How long a note sounds is not its own property.  It reaches back for its
consonant, so its own velocity moves where it starts; and it ends where the
next note starts sounding, so that note's velocity moves where it stops.  A
drawn envelope has to follow both, or the note is cut short at one end and
silent at the other.

Following must not mean stretching, though.  The opening and closing ramps are
a shape someone chose -- a preset, or their own hand -- and dragging only the
outermost point lengthens the very fade it belongs to: ask for a slower
consonant and the gentle rise you picked turns into a much longer one.  The
ramps move whole, the plateau between them takes up the difference, and points
placed inside stay exactly where they were put.
"""

from __future__ import annotations

import json
import pathlib
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from utau4_mode_smoke import McpClient  # noqa: E402
from splice_smoke import write_midi  # noqa: E402

DRAWN = [[0.0, -60], [0.05, 0], [0.20, -6], [0.30, -60]]


def envelope_of(text: str, index: int) -> list[tuple[float, float]]:
    line = [row for row in text.splitlines() if row.startswith("note=")][index]
    points = line.split("envelope=", 1)[1].strip()
    result = []
    for chunk in points.split(")"):
        chunk = chunk.strip().lstrip("(")
        if chunk:
            time, gain = chunk.split(",")
            result.append((float(time), float(gain)))
    return result


def sounding(text: str, index: int) -> tuple[float, float]:
    line = [row for row in text.splitlines() if row.startswith("note=")][index]
    start, end = line.split("sounding=", 1)[1].split()[0].split("..")
    return float(start), float(end)


def nominal_start(text: str, index: int) -> float:
    line = [row for row in text.splitlines() if row.startswith("note=")][index]
    return float(line.split("nominal=", 1)[1].split()[0].split("..")[0])


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit("usage: envelope_tail_smoke.py BINARY VOICEBANK ALIAS")
    binary, voicebank, alias = (pathlib.Path(sys.argv[1]).resolve(),
                                pathlib.Path(sys.argv[2]), sys.argv[3])
    if not voicebank.is_dir():
        print("envelope tail: skipped, no voicebank")
        return 0

    with tempfile.TemporaryDirectory(prefix="hachishifter-envelope-") as text:
        directory = pathlib.Path(text)
        midi = directory / "two.mid"
        write_midi(midi)
        project = directory / "envelope.hjpx"

        def build(second_velocity: int, first_velocity: int = 100) -> str:
            client = McpClient(binary)
            try:
                client.call("project_new")
                client.call("import_midi", {"path": str(midi)})
                track = json.loads(client.call("project_snapshot"))["tracks"][0]
                ids = [note["id"] for note in track["clips"][0]["notes"]]
                client.call("set_track", {"track_id": track["id"],
                                          "pitch_algorithm": "utau",
                                          "voicebank_directory": str(voicebank),
                                          "compose": True})
                for note_id in ids:
                    client.call("set_note", {"note_id": note_id, "label": alias})
                # Both notes carry the same drawn shape: the first one is where
                # a moving end can be seen, the second where a moving start can
                # -- the first note begins at zero, and no lead-in can reach
                # back past the start of the piece.
                for note_id in ids:
                    client.call("set_note", {"note_id": note_id,
                                             "amplitude_envelope": DRAWN})
                client.call("set_note", {"note_id": ids[0],
                                         "utau_consonant_velocity": first_velocity})
                client.call("set_note", {"note_id": ids[1],
                                         "utau_consonant_velocity": second_velocity})
                project.unlink(missing_ok=True)
                client.call("project_save", {"path": str(project)})
            finally:
                client.close()
            return subprocess.run([str(binary), "--smoke-piano-roll", str(project)],
                                  capture_output=True, text=True,
                                  encoding="utf-8", errors="replace").stdout

        slow, fast = build(100), build(200)

    def ramps(report: str, index: int) -> tuple[float, float]:
        points = envelope_of(report, index)
        assert len(points) == len(DRAWN), "the envelope gained or lost a point"
        return (points[1][0] - points[0][0], points[-1][0] - points[-2][0])

    # The second note's velocity moves where the first one stops sounding.
    ends = (sounding(slow, 0)[1], sounding(fast, 0)[1])
    assert abs(ends[0] - ends[1]) > 0.005, \
        "the consonant velocity change did not move where the first note stops"
    for report, end in ((slow, ends[0]), (fast, ends[1])):
        points = envelope_of(report, 0)
        assert abs(points[-1][0] + nominal_start(report, 0) - end) < 1.0e-4, \
            f"the envelope ends at {points[-1][0]}, the note at {end}"
        assert points[-1][1] == DRAWN[-1][1], "the closing gain was changed"
        assert abs(points[1][0] - DRAWN[1][0]) < 1.0e-6, \
            "a point at the far end of the note moved with the tail"
    assert abs(ramps(slow, 0)[1] - ramps(fast, 0)[1]) < 1.0e-6, \
        (f"the closing ramp changed length with the note: "
         f"{ramps(slow, 0)[1]} then {ramps(fast, 0)[1]}")

    # The second note's own velocity moves where it starts sounding.
    starts = (sounding(slow, 1)[0], sounding(fast, 1)[0])
    assert abs(starts[0] - starts[1]) > 0.005, \
        "the consonant velocity change did not move where the second note starts"
    for report, start in ((slow, starts[0]), (fast, starts[1])):
        points = envelope_of(report, 1)
        assert abs(points[0][0] + nominal_start(report, 1) - start) < 1.0e-4, \
            f"the envelope opens at {points[0][0]}, the note at {start}"
        assert points[0][1] == DRAWN[0][1], "the opening gain was changed"
    assert abs(ramps(slow, 1)[0] - ramps(fast, 1)[0]) < 1.0e-6, \
        (f"the opening ramp changed length with the lead-in: "
         f"{ramps(slow, 1)[0]} then {ramps(fast, 1)[0]}")

    print("envelope tail: both ends follow the note, and both ramps keep their"
          " length while they do")
    return 0


if __name__ == "__main__":
    sys.exit(main())

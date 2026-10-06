#!/usr/bin/env python3
"""Guards that moving the vowel boundaries leaves the consonant alone.

The four-region split has one boundary that ends the consonant and two that
divide the vowel after it.  Moving the vowel ones is an ordinary edit, and it
must not change how the consonant is pronounced -- it used to, and only a
forced reset brought it back.

The consonant now takes the length the oto4 mark gives it, scaled by the
consonant velocity and nothing else, so the three vowel boundaries have no say
in it.

This measures the rendered audio rather than the numbers, because the numbers
were right while the sound was not.  Two hand-placed splits differing only in
the last two boundaries: the opening of the note has to come out the same, and
the body of it has to differ, or nothing was edited and there would be nothing
to have left the consonant alone through.
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

# The note starts a beat in, so its lead-in has somewhere to be: a note at zero
# has its consonant cut off by the start of the piece.
NOTE_START_SECONDS = 0.5
CONSONANT_WINDOW = 0.040

# How far the consonant's shape may move, as a fraction of its own peak, once
# a gain has been fitted out.  What is left at this point is a few percent of
# the local level spread evenly through the consonant: the engine's synthesis
# grid answers to the whole note's plan, so redividing the vowel jogs the
# consonant's frames very slightly.  On a 35 ms consonant ("zhang", the
# shortest to hand) that reaches 0.07; on longer ones it is 0.001 to 0.003.
# It is not what this guards against -- a consonant actually being rendered
# differently, which is what the oto4 row mismatch caused, reads 1.14.
SHAPE_TOLERANCE = 0.08



def oto_entry(voicebank: pathlib.Path, alias: str):
    """The oto.ini row for this alias: (wav, offset, consonant, cutoff,
    preutterance) in ms.  The alias field is the file stem when empty."""
    for line in (voicebank / "oto.ini").read_text(
            encoding="shift_jis", errors="replace").splitlines():
        line = line.strip()
        if "=" not in line or line.startswith(";"):
            continue
        wav, rest = line.split("=", 1)
        fields = rest.split(",")
        if len(fields) < 5:
            continue
        name = fields[0].strip() or wav.rsplit(".", 1)[0]
        if name != alias:
            continue
        values = [float(field or 0) for field in fields[1:5]]
        return wav, values[0], values[1], values[2], values[3]
    return None


def oto4_regions(voicebank: pathlib.Path, wav: str, offset: float,
                 cutoff: float):
    """The four source region lengths in ms, read the way both the host and
    the engine read them: nearest oto_offset within half a millisecond, and
    the later row when two are equally near."""
    best, best_distance = None, 0.5
    for line in (voicebank / "oto4.ini").read_text(
            encoding="shift_jis", errors="replace").splitlines():
        line = line.strip()
        if "=" not in line or line.startswith(";"):
            continue
        name, rest = line.split("=", 1)
        if name.strip().lower() != wav.lower():
            continue
        fields = [field.strip() for field in rest.split(",")]
        if len(fields) < 4:
            continue
        if fields[0] == "":
            continue
        distance = abs(float(fields[0]) - offset)
        if distance <= best_distance:
            best, best_distance = [float(field) for field in fields[1:4]], distance
    if best is None:
        return None
    with wave.open(str(voicebank / wav), "rb") as source:
        duration = source.getnframes() / source.getframerate() * 1000.0
    end = offset - cutoff if cutoff < 0 else duration - cutoff
    span = max(0.0, end - offset)
    bounds = []
    floor = 0.0
    for value in best:
        floor = min(max(value, floor), span)
        bounds.append(floor)
    return [bounds[0], bounds[1] - bounds[0], bounds[2] - bounds[1],
            span - bounds[2]]



def levelled_difference(first: list[int], second: list[int]) -> tuple[float, float]:
    """How much the two differ in shape, and the gain between them.

    The engine normalises a render to its own peak, so two renders that divide
    the vowel differently come out at slightly different levels -- the
    consonant included, though nothing about it changed.  Dividing by each
    window's peak sample is too blunt to undo that: the peak is one noisy
    sample.  Fit the gain by least squares, then measure what is left.
    """
    energy = sum(value * value for value in second)
    if energy <= 0:
        return 1.0, 1.0
    gain = sum(a * b for a, b in zip(first, second)) / energy
    peak = max(abs(value) for value in first) or 1
    worst = max(abs(a - gain * b) for a, b in zip(first, second)) / peak
    return worst, gain


def write_midi(path: pathlib.Path) -> None:
    """One note, D4, a beat in and a beat long at 120 bpm."""
    events = bytes([0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20])
    events += bytes([0x60, 0x90, 62, 100])         # 96 ticks of rest first
    events += bytes([0x60, 0x80, 62, 64])
    events += bytes([0x00, 0xFF, 0x2F, 0x00])
    path.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 96)
                     + b"MTrk" + struct.pack(">I", len(events)) + events)


def samples(path: pathlib.Path, start: float, end: float) -> list[int]:
    with wave.open(str(path), "rb") as source:
        rate, width, channels = (source.getframerate(), source.getsampwidth(),
                                 source.getnchannels())
        first = max(0, int(start * rate))
        count = max(1, int((end - start) * rate))
        source.setpos(min(first, source.getnframes() - 1))
        raw = source.readframes(min(count, source.getnframes() - first))
    step = width * channels
    return [int.from_bytes(raw[offset:offset + width], "little", signed=True)
            for offset in range(0, len(raw) - step + 1, step)]


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(
            "usage: consonant_untouched_smoke.py BINARY VOICEBANK RESAMPLER ALIAS")
    binary, voicebank, resampler, alias = (pathlib.Path(sys.argv[1]).resolve(),
                                           pathlib.Path(sys.argv[2]),
                                           pathlib.Path(sys.argv[3]),
                                           sys.argv[4])
    if not voicebank.is_dir() or not resampler.is_file():
        print("consonant untouched: skipped, no voicebank or resampler")
        return 0

    entry = oto_entry(voicebank, alias)
    if entry is None:
        print(f"consonant untouched: skipped, no oto row for {alias}")
        return 0
    wav, offset, _consonant, cutoff, preutterance = entry
    regions = oto4_regions(voicebank, wav, offset, cutoff)
    if regions is None:
        print(f"consonant untouched: skipped, no oto4 row for {alias}")
        return 0
    if sum(1 for length in regions[1:] if length > 1.0) < 2:
        # The engine gives no output time to a region with no source, so with
        # only one vowel region left the boundaries have nothing to divide and
        # every split renders the same.  Nothing to measure here.
        print(f"consonant untouched: skipped, {alias} has one vowel region")
        return 0
    # The onset region fills the lead-in: it ends where the note starts, and
    # the oto4 mark says which source audio fills it.  So the window to
    # measure is the lead-in, not the mark.
    onset = preutterance / 1000.0
    sounding = NOTE_START_SECONDS - onset

    with tempfile.TemporaryDirectory(prefix="hachishifter-consonant-") as text:
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

            def render(name: str) -> pathlib.Path:
                output = directory / name
                output.unlink(missing_ok=True)
                client.call("utau_render_selection", {"note_ids": [note]})
                client.call("export_wav", {"path": str(output),
                                           "timeout_seconds": 400})
                return output

            # Three renders.  First with no split at all, which is how a note
            # starts life and the only state the user could get a right-
            # sounding consonant in for a while; then two hand-placed splits
            # with the same consonant boundary, where only the vowel ones move.
            bare = render("bare.wav")
            client.call("set_note", {"note_id": note,
                                     "jie_split": [0.18, 0.40, 0.70]})
            planned = render("first.wav")
            client.call("set_note", {"note_id": note,
                                     "jie_split": [0.18, 0.62, 0.88]})
            byHand = render("second.wav")
        finally:
            client.close()

        # Inside the consonant, measured from where the sound starts rather
        # than from the beat: how far the two sit apart is the preutterance,
        # which differs per alias, and a window pinned to the beat can land on
        # the region-0 boundary where the synthesis frames straddle it.
        opening = (sounding + 0.002, sounding + onset * 0.6)
        # The whole region, for scale.  Levelling by the measured window's own
        # peak flatters or punishes an alias depending on where in the
        # consonant its energy sits -- "zhang" is nearly silent for its first
        # half and peaks at the very end, so a window over the quiet part had
        # a peak a fifth of the region's and read five times the difference it
        # should.  The consonant's own peak is the scale that means something.
        whole = (sounding + 0.002, sounding + onset)
        body = (NOTE_START_SECONDS + 0.15, NOTE_START_SECONDS + 0.45)
        consonantBare = samples(bare, *opening)
        consonantBefore = samples(planned, *opening)
        consonantAfter = samples(byHand, *opening)
        scaleBare = samples(bare, *whole)
        scaleBefore = samples(planned, *whole)
        scaleAfter = samples(byHand, *whole)
        bodyBefore = samples(planned, *body)
        bodyAfter = samples(byHand, *body)

    assert consonantBefore, "no audio where the consonant should be"
    assert any(value != 0 for value in consonantBefore), \
        "the consonant window is silent, so matching it would prove nothing"
    assert bodyBefore != bodyAfter, \
        "moving the vowel boundaries changed nothing at all, so there is no" \
        " edit here to have left the consonant alone through"
    # The mix normalises itself to its own peak, so changing the vowel changes
    # the level of everything including the consonant.  What must not change is
    # its shape: the same waveform, however loud the rest made it.
    assert max(abs(value) for value in scaleBefore) > 0
    assert max(abs(value) for value in scaleAfter) > 0
    worst, gain = levelled_difference(consonantBefore, consonantAfter)
    assert 0.75 < gain < 1.33, (
        f"the consonant came out {gain:.2f} times its old level, which is more"
        f" than a differently normalised render explains")
    assert worst < SHAPE_TOLERANCE, (
        f"moving the vowel boundaries changed the consonant: waveforms differ"
        f" by up to {worst:.3f} of full scale after levelling")

    # And the first split of all has to leave it alone too.  This is the one
    # that used to change it, back when the host only sent per-note lengths
    # once a boundary had been moved by hand: until then the engine planned the
    # regions itself and put the onset at the oto4 mark, and the first drag of
    # any boundary switched it to the lead-in.  The lengths are sent either way
    # now, so there is one answer.
    #
    # Note that this no longer catches the host and the engine reading
    # different oto4 rows -- both renders take the same path now, so a wrong
    # source row is wrong in both alike.  wcs_region_rowpick_test.c in the
    # engine guards that.
    assert max(abs(value) for value in scaleBare) > 0
    firstSplit, firstGain = levelled_difference(consonantBare, consonantBefore)
    assert 0.75 < firstGain < 1.33, (
        f"placing a split changed the consonant's level by {firstGain:.2f}x")
    assert firstSplit < SHAPE_TOLERANCE, (
        f"placing a split at all changed the consonant: waveforms differ by up"
        f" to {firstSplit:.3f} of full scale after levelling, so a split and"
        f" no split are not being decided the same way")
    print(f"consonant untouched: splitting at all, and moving the vowel"
          f" boundaries after, leave the consonant alone"
          f" (within {firstSplit:.4f} and {worst:.4f} after levelling)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

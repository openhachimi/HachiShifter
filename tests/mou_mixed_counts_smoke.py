#!/usr/bin/env python3
"""Two, three and four regions living in one track of one project.

The region count is a property of an oto entry, not of a track or a mode, so
a single phrase can mix them: a plain CV syllable at four, a syllable whose
coda is its own consonant at three, a VC join at two.  Nothing may depend on
them all being the same.

The check is direct: render the three notes together, then render each one
by itself, and require every note to come out of the mixed pass exactly as it
does alone.  The notes are spaced so their sounding stretches do not touch,
so each one owns its window outright.  Gain is fitted inside each window
before comparing, since the mixdown and a solo render normalise to different
peaks.

A note kept alone is kept *where it was*: the lead-in is
min(preutterance, note start), so a note moved to zero has none at all and
would differ from itself for a reason that has nothing to do with regions.

usage: mou_mixed_counts_smoke.py BINARY VOICEBANK RESAMPLER WAVNAME
"""

from __future__ import annotations

import json
import pathlib
import shutil
import struct
import sys
import tempfile
import wave

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from utau4_mode_smoke import McpClient  # noqa: E402

# alias, class string, and the boundaries it needs (one fewer than regions)
ENTRIES = [
    ("four", "CVVC", [60.0, 150.0, 320.0]),
    ("three", "CVC", [60.0, 320.0]),
    ("two", "VC", [150.0]),
]
OTO = (25.0, 296.0, 125.778, 199.0, 67.237)
TPQ = 480
NOTE_TICKS = 480          # 0.5 s at 120 bpm
GAP_TICKS = 960           # 1.0 s of silence between notes


def build_bank(root: pathlib.Path, source_wav: pathlib.Path):
    root.mkdir(parents=True, exist_ok=True)
    off, con, cut, pre, ovl = OTO
    oto, mou = [], []
    for alias, classes, bounds in ENTRIES:
        shutil.copy(source_wav, root / f"{alias}.wav")
        oto.append(f"{alias}.wav={alias},{off},{con},{cut},{pre},{ovl}")
        mou.append(f"{alias}.wav={classes},{off},"
                   + ",".join(str(b) for b in bounds))
    (root / "oto.ini").write_text("\n".join(oto) + "\n", encoding="shift_jis")
    (root / "otomou.ini").write_text("\n".join(mou) + "\n", encoding="utf-8")


def write_midi(path: pathlib.Path, count: int):
    ev = bytearray()
    ev += b"\x00\xFF\x51\x03" + struct.pack(">I", 500000)[1:]

    def vlq(n):
        parts = [n & 0x7F]
        n >>= 7
        while n:
            parts.append((n & 0x7F) | 0x80)
            n >>= 7
        return bytes(reversed(parts))

    for index in range(count):
        ev += vlq(GAP_TICKS if index else 0) + bytes([0x90, 60, 100])
        ev += vlq(NOTE_TICKS) + bytes([0x80, 60, 0])
    ev += b"\x00\xFF\x2F\x00"
    path.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, TPQ)
                     + b"MTrk" + struct.pack(">I", len(ev)) + bytes(ev))


def render(client, bank, midi, labels, out: pathlib.Path, keep=None):
    """labels goes onto the notes in time order; keep, if given, is the one
    index to leave in place -- the others are removed rather than moved, so
    the survivor keeps its own start and therefore its own lead-in."""
    client.call("project_new", {})
    client.call("import_midi", {"path": str(midi)})
    snapshot = json.loads(client.call("project_snapshot", {}))
    holder = next(tr for tr in snapshot["tracks"]
                  if any(cl["notes"] for cl in tr["clips"]))
    client.call("set_track", {"track_id": holder["id"],
                              "pitch_algorithm": "utaumou",
                              "voicebank_directory": str(bank)})
    notes = sorted((n for cl in holder["clips"] for n in cl["notes"]),
                   key=lambda n: n["start_seconds"])
    assert len(notes) == len(labels), f"{len(notes)} notes for {len(labels)} labels"
    clip_start = holder["clips"][0].get("start_seconds", 0.0)
    starts = [clip_start + n["start_seconds"] for n in notes]
    for note, label in zip(notes, labels):
        client.call("set_note", {"note_id": note["id"], "label": label})
    if keep is not None:
        for index, note in enumerate(notes):
            if index != keep:
                client.call("remove_note", {"note_id": note["id"]})
        starts = [starts[keep]]
    if out.exists():
        out.unlink()
    client.call("export_wav", {"path": str(out)})
    if not out.exists():
        raise SystemExit("no wav written")
    with wave.open(str(out)) as w:
        rate, ch, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
        data = w.readframes(w.getnframes())
    # Every export is 16-bit mono.  Pinned here because this is a test that
    # actually writes one: it was 24-bit stereo, which for anything not panned
    # is the same signal written twice.
    assert ch == 1 and width == 2, (
        f"export came out {ch}ch {width * 8}-bit, not 16-bit mono")
    # getnframes() has been seen to disagree with how much readframes really
    # returns, so trust the bytes.
    frames = len(data) // (ch * width)
    raw = struct.unpack(f"<{frames}h", data[:frames * 2])
    return [v / 32768.0 for v in raw], rate, starts


def residual(a, b, lo, hi):
    x, y = a[lo:hi], b[lo:hi]
    if not x:
        return 100.0
    den = sum(v * v for v in x) or 1e-12
    gain = sum(p * q for p, q in zip(x, y)) / den
    left = sum((gain * p - q) ** 2 for p, q in zip(x, y))
    return (left / (sum(q * q for q in y) or 1e-12)) ** 0.5 * 100.0


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(__doc__.strip().splitlines()[-1])
    binary, bank_src, resampler, wav_name = sys.argv[1:5]

    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        bank = tmp / "bank"
        build_bank(bank, pathlib.Path(bank_src) / wav_name)
        aliases = [alias for alias, _, _ in ENTRIES]
        write_midi(tmp / "three.mid", 3)
        write_midi(tmp / "one.mid", 1)

        client = McpClient(pathlib.Path(binary))
        try:
            client.call("set_utau_resampler", {"path": resampler})
            mixed, rate, starts = render(client, bank, tmp / "three.mid",
                                         aliases, tmp / "mixed.wav")
            solos = [render(client, bank, tmp / "three.mid", aliases,
                            tmp / f"solo_{alias}.wav", keep=index)
                     for index, alias in enumerate(aliases)]
        finally:
            client.close()

    # Where each note really is, read back from the project rather than
    # worked out from the midi -- measure the middle of it, clear of the
    # lead-in ahead of it and of its release.
    apart = []
    for index, (alias, classes, _) in enumerate(ENTRIES):
        solo, _, solo_starts = solos[index]
        lo = int((starts[index] + 0.10) * rate)
        solo_lo = int((solo_starts[0] + 0.10) * rate)
        window = int(0.35 * rate)
        window = min(window, len(mixed) - lo, len(solo) - solo_lo)
        got = residual(solo[solo_lo:solo_lo + window],
                       mixed[lo:lo + window], 0, window)
        apart.append((alias, classes, got))


    # Every note matching itself is not enough on its own: if the classes
    # never reached the engine, all three would render as plain four-region
    # entries and still match.  The three wavs are copies of one sample with
    # one oto, so anything that tells them apart is the annotation.
    windows = []
    for index, _ in enumerate(ENTRIES):
        lo = int((starts[index] + 0.10) * rate)
        windows.append(mixed[lo:lo + int(0.35 * rate)])
    pairs = []
    for i in range(len(ENTRIES)):
        for j in range(i + 1, len(ENTRIES)):
            apart_ij = residual(windows[i], windows[j], 0, len(windows[i]))
            pairs.append((ENTRIES[i][1], ENTRIES[j][1], apart_ij))
    closest = min(value for _, _, value in pairs)
    assert closest > 10.0, (
        "two of the three region counts rendered alike, so the counts are not"
        " reaching the engine: "
        + " ".join(f"{a}/{b}:{v:.1f}%" for a, b, v in pairs))

    worst = max(value for _, _, value in apart)
    detail = " ".join(f"{alias}({classes}):{value:.2f}%"
                      for alias, classes, value in apart)
    assert worst < 5.0, (
        f"a note rendered differently in the mixed track than on its own,"
        f" so the region counts are interfering: {detail}")

    print(f"mixed region counts: four, three and two in one track at"
          f" {[round(s, 2) for s in starts]}s, each note within {worst:.2f}% of"
          f" rendering alone ({detail}), and no two of them alike"
          f" (closest {closest:.1f}%)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

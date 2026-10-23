#!/usr/bin/env python3
"""谋•OTO reaches the engine, and only in 谋•UTAU mode.

The annotation lives in otomou.ini beside oto.ini: an oto4 row with a class
string in front saying what each region is.  Four things have to hold, and
three of them are about not disturbing anything:

  1. 谋 with an annotation renders differently from 界 -- the classes got
     through, or the whole exercise is decoration.
  2. 谋 without an otomou.ini renders exactly as 界 does.
  3. 界 with an otomou.ini sitting right there renders exactly as if the file
     were not there.  界 has no notion of the annotation and must not read it.
  4. Plain UTAU likewise.

Observed through bh.  The editor works out the four output lengths itself
and hands them to the engine, and the engine executes those rather than its
own stretch policy -- so the class-driven stretch defaults never engage from
here.  What the annotation does reach from the editor is bh's gating (and
HF2's synthesis path, which needs the neural backend).  bh under CVVC finds
the coda, which is voiced; under CVVV there is nothing voiced in this /s/
onset for it to find, so the two renders separate cleanly.

A scratch voicebank is built for this rather than touching a real one: one
wav, one oto entry, and the three sidecars written by hand.

usage: mou_oto_smoke.py BINARY VOICEBANK RESAMPLER WAVNAME
"""

from __future__ import annotations

import hashlib
import json
import pathlib
import shutil
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from utau4_mode_smoke import McpClient  # noqa: E402

ALIAS = "mou"
# offset, consonant, cutoff, preutterance, overlap -- a long fixed stretch so
# every region has something in it.
OTO = (25.0, 296.0, 125.778, 199.0, 67.237)
BOUNDS = (60.0, 150.0, 320.0)
NOTE_SECONDS = 0.9


def build_bank(root: pathlib.Path, source_wav: pathlib.Path, classes: str | None):
    root.mkdir(parents=True, exist_ok=True)
    shutil.copy(source_wav, root / "sample.wav")
    off, con, cut, pre, ovl = OTO
    (root / "oto.ini").write_text(
        f"sample.wav={ALIAS},{off},{con},{cut},{pre},{ovl}\n", encoding="shift_jis")
    row = f"sample.wav={off},{BOUNDS[0]},{BOUNDS[1]},{BOUNDS[2]}\n"
    (root / "oto4.ini").write_text(row, encoding="utf-8")
    mou = root / "otomou.ini"
    if classes is None:
        if mou.exists():
            mou.unlink()
    else:
        mou.write_text(
            f"sample.wav={classes},{off},{BOUNDS[0]},{BOUNDS[1]},{BOUNDS[2]}\n",
            encoding="utf-8")


def render(client, bank, mode, out: pathlib.Path) -> str:
    client.call("project_new", {})
    # A clip to hang the note on: importing a tiny midi is the only way to
    # make one, and it brings its own track.
    client.call("import_midi", {"path": str(bank.parent / "one.mid")})
    snapshot = json.loads(client.call("project_snapshot", {}))
    holder = next(tr for tr in snapshot["tracks"]
                  if any(cl["notes"] for cl in tr["clips"]))
    track = holder["id"]
    client.call("set_track", {"track_id": track, "pitch_algorithm": mode,
                              "voicebank_directory": str(bank),
                              "utau_global_flags": "bh60"})
    note = holder["clips"][0]["notes"][0]["id"]
    client.call("set_note", {"note_id": note, "label": ALIAS})
    if out.exists():
        out.unlink()
    client.call("export_wav", {"path": str(out)})
    if not out.exists():
        raise SystemExit(f"no wav written for {mode}")
    return hashlib.sha1(out.read_bytes()).hexdigest()


def write_midi(path: pathlib.Path):
    import struct
    tpq = 480
    ev = bytearray()
    ev += b"\x00\xFF\x51\x03" + struct.pack(">I", 500000)[1:]
    ev += b"\x00" + bytes([0x90, 60, 100])
    ev += bytes([0x83, 0x60]) + bytes([0x80, 60, 0])       # ~0.9 s at 120 bpm
    ev += b"\x00\xFF\x2F\x00"
    path.write_bytes(b"MThd" + struct.pack(">IHHH", 6, 0, 1, tpq)
                     + b"MTrk" + struct.pack(">I", len(ev)) + bytes(ev))


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(__doc__.strip().splitlines()[-1])
    binary, bank_src, resampler, wav_name = sys.argv[1:5]
    source_wav = pathlib.Path(bank_src) / wav_name

    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        bank = tmp / "bank"
        write_midi(tmp / "one.mid")

        client = McpClient(pathlib.Path(binary))
        try:
            client.call("set_utau_resampler", {"path": resampler})

            build_bank(bank, source_wav, "CVVC")
            mou_marked = render(client, bank, "utaumou", tmp / "mou_marked.wav")
            jie_with_file = render(client, bank, "utau4", tmp / "jie_with.wav")
            classic_with_file = render(client, bank, "utau", tmp / "classic_with.wav")

            build_bank(bank, source_wav, None)
            mou_bare = render(client, bank, "utaumou", tmp / "mou_bare.wav")
            jie_without = render(client, bank, "utau4", tmp / "jie_without.wav")
            classic_without = render(client, bank, "utau", tmp / "classic_without.wav")
        finally:
            client.close()

    assert mou_marked != jie_with_file, (
        "mou with an annotated entry rendered the same as jie, so the classes"
        " never reached the engine")
    assert mou_bare == jie_without, (
        "mou with no otomou.ini rendered differently from jie; with nothing to"
        " annotate the two must be the same")
    assert jie_with_file == jie_without, (
        "jie rendered differently once an otomou.ini appeared beside oto.ini;"
        " jie has no annotation and must never read that file")
    assert classic_with_file == classic_without, (
        "plain UTAU rendered differently once an otomou.ini appeared")

    print(f"mou-OTO: the annotation changes mou ({mou_marked[:8]} against"
          f" {jie_with_file[:8]}), mou without one matches jie, and neither jie"
          f" nor UTAU notices the file is there")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

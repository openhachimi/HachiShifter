#!/usr/bin/env python3
"""A per-note g curve reaches the engine and bends the sound as it runs.

The curve is drawn against the note, sent as the engine's 16th argument in
milliseconds from the start of the rendered segment, and read per frame.  So
the opening of a ramped note has to match a note held at the ramp's first
value, and its end a note held at the last -- while the two constants differ
from each other, which is what says g did anything at all.

usage: flag_curve_smoke.py BINARY VOICEBANK RESAMPLER ALIAS [utau|utau4|mou]
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
LOW, HIGH = 0.0, 40.0


def write_midi(path: pathlib.Path) -> None:
    """One note, D4, a beat in, two beats long at 120 bpm."""
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
    """How far apart two stretches are once a gain is fitted out."""
    energy = sum(value * value for value in second)
    if energy <= 0 or not first:
        return 1.0
    gain = sum(a * b for a, b in zip(first, second)) / energy
    peak = max(1, max(abs(value) for value in first))
    return max(abs(a - gain * b) for a, b in zip(first, second)) / peak


def note_state(client, note_id: str) -> dict:
    """Everything about a note's flags, as the project holds it."""
    for track in json.loads(client.call("project_snapshot"))["tracks"]:
        for clip in track["clips"]:
            for note in clip["notes"]:
                if note["id"] == note_id:
                    return {key: note[key] for key in
                            ("utau_flags", "flag_split", "region_flags",
                             "flag_curve_enabled", "flag_curves")}
    raise AssertionError("note vanished from the project")


def main() -> int:
    if len(sys.argv) not in (5, 6):
        raise SystemExit("usage: flag_curve_smoke.py BINARY VOICEBANK RESAMPLER ALIAS [utau|utau4|mou]")
    binary, voicebank, resampler, alias = (pathlib.Path(sys.argv[1]).resolve(),
                                           pathlib.Path(sys.argv[2]),
                                           pathlib.Path(sys.argv[3]),
                                           sys.argv[4])
    mode = sys.argv[5] if len(sys.argv) == 6 else "utau4"
    assert mode in ("utau", "utau4", "mou"), mode
    if not voicebank.is_dir() or not resampler.is_file():
        print("flag curve: skipped, no voicebank or resampler")
        return 0

    with tempfile.TemporaryDirectory(prefix="hachishifter-flagcurve-") as text:
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
                                      "pitch_algorithm": mode,
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

            # Held at each end of the ramp, then the ramp itself.
            flat_low = render("low.wav", flag_curve_enabled=True,
                              flag_curve={"flag": "g", "points": [[0.0, LOW], [NOTE_SECONDS, LOW]]})
            flat_high = render("high.wav", flag_curve_enabled=True,
                               flag_curve={"flag": "g", "points": [[0.0, HIGH], [NOTE_SECONDS, HIGH]]})
            # Flat at each end and moving in between, so the two windows
            # compared below sit on a value the curve genuinely holds.  A
            # ramp that is still travelling cannot equal any constant, and
            # comparing it to one would only be measuring the slope.
            ramp = render("ramp.wav", flag_curve_enabled=True,
                          flag_curve={"flag": "g", "points": [[0.0, LOW], [0.3, LOW],
                                             [0.7, HIGH], [NOTE_SECONDS, HIGH]]})
            # And with the curve switched off, the same points must do nothing.
            off = render("off.wav", flag_curve_enabled=False)
            on_again = render("on.wav", flag_curve_enabled=True,
                              flag_curve={"flag": "g", "points": [[0.0, HIGH], [NOTE_SECONDS, HIGH]]})

            # Throwing the switch must not throw anything away.  Give the note
            # a plain flag string and a four-region split as well, then a
            # curve, then turn the curve off and on again and see that each
            # side survived the other untouched.
            plain_flags = "Mt50b10"
            region_flags = ["g-8", "", "Mt20", "b5"]
            kept_curve = [[0.0, -22.0], [0.4, 17.0], [NOTE_SECONDS, -3.0]]
            # Start from no curve: the checks above left one switched on, and
            # a baseline taken with it still running is not a plain-flag one.
            client.call("set_note", {"note_id": note, "utau_flags": plain_flags,
                                     "flag_split": True,
                                     "region_flags": region_flags,
                                     "flag_curve_enabled": False})
            # What this note sounds like on its plain flags alone, before a
            # curve has ever been drawn on it.
            before_curve = render("before_curve.wav")
            client.call("set_note", {"note_id": note, "flag_curve_enabled": True,
                                     "flag_curve": {"flag": "g", "points": kept_curve}})
            with_curve = render("kept_on.wav")
            while_on = note_state(client, note)
            client.call("set_note", {"note_id": note, "flag_curve_enabled": False})
            plain_again = render("kept_off.wav")
            while_off = note_state(client, note)
            client.call("set_note", {"note_id": note, "flag_curve_enabled": True})
            back_on = render("kept_back.wav")
            while_back = note_state(client, note)

            # Segment shapes.  Same two endpoints, different route between
            # them: at the halfway mark a straight line, an ease-in and an
            # ease-out must each be somewhere different.
            client.call("set_note", {"note_id": note, "utau_flags": "",
                                     "flag_split": False,
                                     "region_flags": ["", "", "", ""]})
            shaped = {}
            for shape in ("linear", "ease-in", "ease-out", "smooth"):
                shaped[shape] = render(f"shape_{shape}.wav", flag_curve_enabled=True,
                                       flag_curve={"flag": "g", "points": [[0.0, LOW, "linear"],
                                                    [NOTE_SECONDS, HIGH, shape]]})
            shape_state = note_state(client, note)

            # A second flag.  Its own curve has to be heard, and the two
            # together have to differ from either on its own -- otherwise one
            # is quietly overwriting the other on the way to the engine.
            def only(**curves):
                client.call("set_note", {"note_id": note, "flag_curve_enabled": True,
                                         "flag_curve": {"flag": "g", "points": []}})
                client.call("set_note", {"note_id": note,
                                         "flag_curve": {"flag": "Mt", "points": []}})
                for flag, points in curves.items():
                    client.call("set_note", {"note_id": note,
                                             "flag_curve": {"flag": flag,
                                                            "points": points}})
                output = directory / ("only_" + "_".join(sorted(curves)) + ".wav")
                output.unlink(missing_ok=True)
                client.call("export_wav", {"path": str(output), "timeout_seconds": 600})
                return output

            g_ramp = [[0.0, LOW], [NOTE_SECONDS, HIGH]]
            tension = [[0.0, -80.0], [NOTE_SECONDS, 80.0]]
            none_at_all = only()
            g_only = only(g=g_ramp)
            mt_only = only(Mt=tension)
            both = only(g=g_ramp, Mt=tension)
        finally:
            client.close()

        early = (NOTE_START + 0.04, NOTE_START + 0.26)     # inside the low plateau
        late = (NOTE_START + 0.74, NOTE_START + 0.96)      # inside the high one
        constants_differ = difference(samples(flat_low, *late),
                                      samples(flat_high, *late))
        ramp_early = difference(samples(ramp, *early), samples(flat_low, *early))
        ramp_late = difference(samples(ramp, *late), samples(flat_high, *late))
        ramp_late_wrong = difference(samples(ramp, *late), samples(flat_low, *late))
        switched_off = difference(samples(off, *late), samples(flat_low, *late))
        switched_on = difference(samples(on_again, *late), samples(flat_high, *late))
        # The curve on, off, and on again -- the sound has to come back to
        # exactly what it was, and the off pass has to be the plain-flag one.
        returned = difference(samples(back_on, *late), samples(with_curve, *late))
        curve_moved_it = difference(samples(plain_again, *late),
                                    samples(with_curve, *late))
        # Switching off has to land back on the plain-flag sound exactly, not
        # merely somewhere different from the curve.
        fell_back = difference(samples(plain_again, *late),
                               samples(before_curve, *late))
        # Halfway along, where the shapes are furthest apart.
        middle = (NOTE_START + 0.42, NOTE_START + 0.58)
        straight = samples(shaped["linear"], *middle)
        shape_apart = {name: difference(samples(path, *middle), straight)
                       for name, path in shaped.items() if name != "linear"}
        # Ease-in is behind the straight line at halfway and ease-out ahead, so
        # they must differ from each other by more than either does from it.
        in_vs_out = difference(samples(shaped["ease-in"], *middle),
                               samples(shaped["ease-out"], *middle))
        second_flag = difference(samples(mt_only, *late), samples(none_at_all, *late))
        both_vs_g = difference(samples(both, *late), samples(g_only, *late))
        both_vs_mt = difference(samples(both, *late), samples(mt_only, *late))

    # Nothing is lost either way.
    assert while_on["utau_flags"] == plain_flags, (
        f"the flag text was lost when the curve went on: {while_on['utau_flags']!r}")
    assert while_off["utau_flags"] == plain_flags, (
        f"the flag text was lost when the curve came off: {while_off['utau_flags']!r}")
    assert while_off["region_flags"] == region_flags and while_off["flag_split"], (
        f"the four-region flags were lost: {while_off['region_flags']}")
    assert while_on["region_flags"] == region_flags and while_on["flag_split"], (
        f"the four-region flags were lost while the curve was on:"
        f" {while_on['region_flags']}")
    assert not while_off["flag_curve_enabled"], "the switch did not come off"
    assert while_back["flag_curve_enabled"], "the switch did not go back on"
    for state, when in ((while_off, "while off"), (while_back, "on again")):
        assert len(state["flag_curves"]["g"]) == len(kept_curve), (
            f"the curve was lost {when}: {state['flag_curves']['g']}")
        for drawn, stored in zip(kept_curve, state["flag_curves"]["g"]):
            assert abs(drawn[0] - stored[0]) < 1.0e-6                 and abs(drawn[1] - stored[1]) < 1.0e-3, (
                f"the curve changed {when}: {stored} was drawn as {drawn}")
    assert returned < 0.05, (
        f"turning the curve off and on again did not come back to the same"
        f" sound ({returned:.3f})")
    assert fell_back < 0.05, (
        f"switching the curve off did not come back to the plain-flag sound"
        f" ({fell_back:.3f} from what the note was before a curve existed)")
    for name, apart in shape_apart.items():
        assert apart > 0.05, (
            f"a {name} segment renders the same as a straight one ({apart:.3f}),"
            f" so the shape is not reaching the engine")
    assert in_vs_out > 0.20, (
        f"ease-in and ease-out render the same ({in_vs_out:.3f})")
    assert shape_state["flag_curves"]["g"][1][2] == "smooth", (
        f"the shape was not stored: {shape_state['flag_curves']}")
    assert second_flag > 0.10, (
        f"a curve on Mt changed nothing ({second_flag:.3f}), so only g is"
        f" reaching the engine")
    assert both_vs_g > 0.10 and both_vs_mt > 0.10, (
        f"g and Mt together sound like one of them alone"
        f" (g {both_vs_g:.3f}, Mt {both_vs_mt:.3f}) -- one is overwriting the other")
    assert curve_moved_it > 0.20, (
        "the curve and the plain flags render the same, so this proves nothing")

    assert constants_differ > 0.20, (
        f"g {LOW:.0f} and g {HIGH:.0f} render the same ({constants_differ:.3f}),"
        f" so nothing below can be measuring the flag")
    assert ramp_early < 0.20, (
        f"the ramp does not start where it was told: {ramp_early:.3f} from a"
        f" note held at {LOW:.0f}")
    assert ramp_late < 0.20, (
        f"the ramp does not end where it was told: {ramp_late:.3f} from a note"
        f" held at {HIGH:.0f}")
    assert ramp_late_wrong > 0.20, (
        "the ramp's end matches its own beginning, so it never moved")
    assert switched_off < 0.05, (
        f"turning the curve off left it acting ({switched_off:.3f} from a"
        f" note with no curve at all)")
    assert switched_on < 0.20, (
        f"turning the curve back on did not take ({switched_on:.3f})")
    print(f"UTAU mode: {mode}")
    print(f"flag switch: text {while_on['utau_flags']!r} and region flags kept"
          f" both ways, curve of {len(while_off['flag_curves']['g'])} points kept"
          f" while off, off returns to the plain sound within {fell_back:.4f},"
          f" on again within {returned:.4f}"
          f" (the curve itself moves it by {curve_moved_it:.3f})")
    print(f"more than one flag: Mt alone moves it {second_flag:.3f}; both together"
          f" differ from g alone by {both_vs_g:.3f} and from Mt alone by {both_vs_mt:.3f}")
    print("flag shapes: " + ", ".join(
        f"{name} {apart:.3f} from straight" for name, apart in sorted(shape_apart.items()))
        + f"; ease-in vs ease-out {in_vs_out:.3f}")
    print(f"flag curve: g follows its curve across the note"
          f" (start {ramp_early:.3f} from g{LOW:.0f}, end {ramp_late:.3f} from"
          f" g{HIGH:.0f}, and {ramp_late_wrong:.3f} from where it started;"
          f" constants differ by {constants_differ:.3f}; off {switched_off:.3f})")
    return 0


if __name__ == "__main__":
    sys.exit(main())

"""Real model contract + audio checks. Run with packaged engines/python/python.exe.

Arguments: bridge.py voicebank_directory output_directory
"""
from pathlib import Path
import importlib.util
import json
import sys
import copy
import tempfile

script, bank_path, output = map(Path, sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location("diffsinger_bridge", script)
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)
np = bridge.np
import soundfile as sf

bank = bridge.Bank(bank_path)
notes = [dict(id="n1", start=.3, duration=.7, midi=60, lyric="ni"),
         dict(id="n2", start=1., duration=.9, midi=64, lyric="hao")]
plan = bank.plan(notes)
assert sum(plan["ph_dur"]) == sum(plan["note_dur"][0]) == plan["total"]
assert np.all(np.asarray(plan["ph_dur"]) >= 1)
onset = sum(plan["ph_dur"][:plan["phones"].index("zh/i")])
assert abs(plan["origin"] + onset*bank.main.dt - .3) < bank.main.dt
pitch = bank.pitch(plan)
assert np.isfinite(pitch).all() and np.ptp(pitch) > 3.0
request = dict(operation="pitch", voicebank=str(bank_path), duration=2.4, notes=notes)
predicted = bridge.execute(request, output/"pitch.json")
(output/"pitch.json").write_text(json.dumps(predicted, ensure_ascii=False), encoding="utf-8")
assert len(predicted["curves"]) == 2
request["operation"] = "render"
bridge.execute(request, output/"plain.json")
for note in request["notes"]:
    note["pitch"] = [[0, note["midi"]+12], [note["duration"], note["midi"]+12]]
bridge.execute(request, output/"octave.json")
plain, sr = sf.read(output/"plain.wav")
octave, sr2 = sf.read(output/"octave.wav")
assert sr == sr2 == 44100 and len(plain) == round(2.4*sr)
assert np.isfinite(plain).all() and np.max(np.abs(plain)) > .01
assert np.sqrt(np.mean((plain-octave)**2)) > .005

# Actual waveform pitch, not just the request values.
def measured_f0(wav):
    estimates=[]
    for start in np.arange(.52, .78, .04):
        frame=wav[round(start*sr):round(start*sr)+2048]
        frame=(frame-frame.mean())*np.hanning(len(frame))
        ac=np.fft.irfft(np.abs(np.fft.rfft(frame,4096))**2)[:2048]
        low, high = int(sr/650), int(sr/200)
        lag=low+int(np.argmax(ac[low:high]))
        estimates.append(sr/lag)
    return float(np.median(estimates))
f_plain, f_octave = measured_f0(plain), measured_f0(octave)
assert 1.85 < f_octave/f_plain < 2.15, (f_plain, f_octave)

slur=copy.deepcopy(notes); slur[1]["lyric"]="+"
slur_plan=bank.plan(slur)
assert slur_plan["phones"].count("zh/i") == 1 and sum(slur_plan["ph_dur"]) == slur_plan["total"]
overlap=copy.deepcopy(notes); overlap[1]["start"] = .8
try: bank.plan(overlap)
except ValueError as e: assert "单声部" in str(e)
else: raise AssertionError("Overlapping notes were accepted")
try: bank.phonemes("unknown-lyric-example")
except ValueError as e: assert "词典" in str(e)
else: raise AssertionError("Unknown lyrics silently substituted")
with tempfile.TemporaryDirectory() as temp:
    root=Path(temp)
    (root/"dsconfig.yaml").write_text("phonemes: phones.json\nacoustic: absent.onnx\n", encoding="utf-8")
    (root/"phones.json").write_text('{"SP": 0, "a": 1}', encoding="utf-8")
    missing=bridge.Bank(root)
    try: missing.pitch({})
    except ValueError as e: assert "dspitch" in str(e)
    else: raise AssertionError("Missing pitch model was accepted")
report=dict(ok=True, sample_rate=sr, peak=float(np.max(np.abs(plain))),
    plain_hz=f_plain, octave_hz=f_octave, pitch_points=sum(len(c["points"]) for c in predicted["curves"]),
    checks=["real duration and vowel anchors", "real pitch predictor", "real acoustic/vocoder",
            "manual octave audible in waveform", "slur", "overlap rejection", "unknown lyric", "missing predictor"])
(output/"report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(report, ensure_ascii=False))

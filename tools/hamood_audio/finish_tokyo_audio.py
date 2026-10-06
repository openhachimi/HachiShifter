"""Write listenable 24-bit delivery files from the final native exports."""
from pathlib import Path
import json, hashlib
import numpy as np
import soundfile as sf

work = Path(__file__).resolve().parents[3] / 'HAMOOD-东京泰迪熊试作'
records = []
for source, target in [('东京泰迪熊-HAMOOD和声.wav', '东京泰迪熊-HAMOOD和声-24bit.wav'),
                       ('mix-editor-float.wav', '东京泰迪熊-主轨伴奏与新和声.wav')]:
    audio, sr = sf.read(work / source, dtype='float32', always_2d=True)
    assert sr == 44100 and audio.shape[1] == 2 and len(audio) > 190 * sr
    assert np.isfinite(audio).all()
    peak = float(np.abs(audio).max())
    rms = float(np.sqrt(np.square(audio).mean()))
    assert peak > .02 and rms > .001
    scale = min(1., 10**(-1/20)/peak)
    sf.write(work / target, audio*scale, sr, subtype='PCM_24')
    records.append(dict(path=str(work/target), seconds=len(audio)/sr,
                        channels=2, sample_rate=sr, bit_depth=24,
                        original_peak=peak, rms=rms, gain_applied=scale,
                        sha256=hashlib.sha256((work/target).read_bytes()).hexdigest()))
    if source == 'mix-editor-float.wav':
        sf.write(work/'东京泰迪熊-和声试听片段.wav', audio[:30*sr]*scale, sr, subtype='PCM_24')
(work/'audio-validation.json').write_text(json.dumps({'ok':True,'exports':records},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(records,ensure_ascii=True),flush=True)

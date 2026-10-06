"""Reproducible, read-only-on-input chorus A/B; never edits an editor project."""
from pathlib import Path
import sys, os, json, hashlib, importlib.util, subprocess, time

ROOT = Path(__file__).resolve().parent
BASE = ROOT.parents[2]
WORK = BASE / 'HAMOOD-副歌漏检复查'
OLD = BASE / 'HAMOOD-KARA2和声分析'
sys.path.insert(0, str(BASE / 'HachiShifter-整合版-0.2.3-DiffSinger/engines/diffsinger/packages'))
import numpy as np
import soundfile as sf
import onnxruntime as ort
from scipy.signal import correlate

def save_json(path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')

def load_adapter(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'kara2.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def main():
    WORK.mkdir(exist_ok=True)
    baseline = json.loads((OLD / 'baseline.json').read_text(encoding='utf-8'))
    raw = next(Path(c['source_file']) for t in baseline['tracks'] for c in t['clips']
               if 'B三狼' in c.get('source_file', '') and c['source_file'].lower().endswith('.mp3'))
    decoded = WORK / '原曲-FFmpeg解码.wav'
    if not decoded.exists():
        subprocess.run(['F:/Ultimate Vocal Remover/ffmpeg.exe', '-v', 'error', '-nostdin', '-i', str(raw),
                        '-ar', '44100', '-ac', '2', '-c:a', 'pcm_f32le', str(decoded)], check=True,
                       creationflags=subprocess.CREATE_NO_WINDOW)
    mixed, sr = sf.read(decoded, dtype='float32', always_2d=True)
    stems = sorted(Path('F:/鬼畜/新UI').glob('*赤羽*_(Vocals).wav'))
    assert len(stems) == 1, stems
    vocal_path = stems[0]
    stem_paths = [Path(str(vocal_path).replace('(Vocals)', '(' + s + ')'))
                  for s in ['Vocals', 'Bass', 'Drums', 'Other']]
    waves = [sf.read(p, dtype='float32', always_2d=True)[0] for p in stem_paths]
    summed = sum(waves)
    # Direct sample correlation determines this decoder's MP3 delay. No guessed offset.
    lags = []
    for start in [5, 35, 70, 110, 150, 180]:
        at = round(start * sr)
        ref = summed[at:at + 5 * sr].mean(axis=1)
        begin = max(0, at - 2 * sr)
        search = mixed[begin:at + 7 * sr].mean(axis=1)
        scores = correlate(search, ref, mode='valid', method='fft')
        pos = int(scores.argmax())
        shifted = search[pos:pos + len(ref)]
        lags.append({'stem_second': start, 'lag_samples': begin + pos - at,
                     'correlation': float(np.corrcoef(ref, shifted)[0, 1])})
    lag = round(float(np.median([x['lag_samples'] for x in lags])))
    assert max(abs(x['lag_samples'] - lag) for x in lags) <= 2, lags
    assert min(x['correlation'] for x in lags) > .85, lags
    trim = round(1.0314285714285714 * sr)
    aligned, ar = sf.read(BASE / 'HAMOOD-原唱参考试作/原唱人声-已对齐.wav', dtype='float32', always_2d=True)
    assert sr == ar == 44100
    alignment_error = float(np.max(np.abs(aligned - waves[0][trim:trim + len(aligned)])))
    assert alignment_error < 2e-6, alignment_error
    model_path = Path('F:/Ultimate Vocal Remover/models/MDX_Net_Models/UVR-MDX-NET-Inst_HQ_3.onnx')
    with model_path.open('rb') as f:
        f.seek(-10000 * 1024, 2)
        uvr_hash = hashlib.md5(f.read()).hexdigest()
    meta = json.loads((model_path.parent / 'model_data/model_data.json').read_text())[uvr_hash]
    assert meta['primary_stem'] == 'Instrumental' and not meta.get('is_karaoke')
    mdx = load_adapter('chorus_mdx')
    mdx.FFT = meta['mdx_n_fft_scale_set']; mdx.BINS = meta['mdx_dim_f_set']
    mdx.FRAMES = 2 ** meta['mdx_dim_t_set']; mdx.TRIM = mdx.FFT // 2
    mdx.CHUNK = mdx.HOP * (mdx.FRAMES - 1); mdx.COMPENSATION = meta['compensate']
    settings = ort.SessionOptions(); settings.intra_op_num_threads = 4; settings.inter_op_num_threads = 1
    inst_model = ort.InferenceSession(str(model_path), settings, providers=['CPUExecutionProvider'])
    shape = inst_model.get_inputs()[0].shape
    print('InstHQ3 input', shape, flush=True)
    assert shape[1:] == [4, mdx.BINS, mdx.FRAMES], shape
    kara = load_adapter('chorus_kara')
    kara_model = kara.session()
    # Check the changed FFT path before trusting its inverse audio transform.
    test = np.random.default_rng(5).normal(0, .1, (2, mdx.CHUNK)).astype(np.float32)
    transform = mdx.Transform()
    transform_error = float(np.max(np.abs(transform.inverse(transform.forward(test)) - test)))
    assert transform_error < 1e-6, transform_error
    report = dict(raw_source=str(raw), model=str(model_path), model_uvr_hash=uvr_hash, model_settings=meta,
                  decoder_alignment=lags, raw_to_project_trim_samples=trim + lag,
                  vocal_alignment_error=alignment_error, transform_error=transform_error, excerpts=[])
    save_json(WORK / 'separation-comparison.json', report)
    for lo, hi in [(43, 69), (146, 172)]:
        folder = WORK / f'{lo}-{hi}秒'; folder.mkdir(exist_ok=True)
        samples = round((hi-lo)*sr); begin = round(lo*sr) + trim + lag
        original = mixed[begin:begin+samples]
        assert len(original) == samples
        sf.write(folder / '原曲.wav', original, sr, subtype='FLOAT')
        sf.write(folder / '旧分轨人声.wav', aligned[round(lo*sr):round(hi*sr)], sr, subtype='FLOAT')
        for kind in ['lead', 'backing']:
            with sf.SoundFile(OLD / 'separated' / (kind+'.wav')) as stream:
                stream.seek(round(lo*sr)); audio = stream.read(samples, dtype='float32', always_2d=True)
            sf.write(folder / ('旧KARA2-'+kind+'.wav'), audio, sr, subtype='FLOAT')
        started = time.monotonic()
        # Control the crop/window schedule: compare the two first-stage vocals
        # through exactly the same KARA2 excerpt inference, not full-song vs crop.
        if not (folder / '控制KARA2-backing.wav').exists():
            old_excerpt = aligned[round(lo*sr):round(hi*sr)]
            control_lead, control_backing = kara.separate(old_excerpt, kara_model,
                progress=lambda i,n: print(f'Control KARA2 {lo}: {i}/{n}', flush=True))
            assert np.max(np.abs(control_lead + control_backing - old_excerpt)) < 1e-6
            sf.write(folder / '控制KARA2-lead.wav', control_lead, sr, subtype='FLOAT')
            sf.write(folder / '控制KARA2-backing.wav', control_backing, sr, subtype='FLOAT')
        new_vocal = folder / '重新提取-完整人声.wav'
        if not new_vocal.exists():
            vocals, instrumental = mdx.separate(original, inst_model, progress=lambda i,n: print(f'InstHQ3 {lo}: {i}/{n}', flush=True))
            assert np.max(np.abs(vocals + instrumental - original)) < 1e-6
            sf.write(new_vocal, vocals, sr, subtype='FLOAT')
            sf.write(folder / '重新提取-伴奏.wav', instrumental, sr, subtype='FLOAT')
        else:
            vocals = sf.read(new_vocal, dtype='float32', always_2d=True)[0]
        if not (folder / '新KARA2-backing.wav').exists():
            lead, backing = kara.separate(vocals, kara_model, progress=lambda i,n: print(f'KARA2 {lo}: {i}/{n}', flush=True))
            assert np.max(np.abs(lead + backing - vocals)) < 1e-6
            sf.write(folder / '新KARA2-lead.wav', lead, sr, subtype='FLOAT')
            sf.write(folder / '新KARA2-backing.wav', backing, sr, subtype='FLOAT')
        metrics = {}
        # Comparison interiors exclude the two-second crop guard at either end.
        for name in ['旧分轨人声', '重新提取-完整人声', '旧KARA2-backing', '控制KARA2-backing', '新KARA2-backing']:
            a = sf.read(folder/(name+'.wav'), dtype='float32', always_2d=True)[0][2*sr:-2*sr]
            metrics[name] = dict(rms=float(np.sqrt(np.mean(a*a))), peak=float(np.abs(a).max()))
        report['excerpts'].append(dict(start=lo, end=hi, evaluation_start=lo+2, evaluation_end=hi-2,
                                       metrics=metrics, processing_seconds=time.monotonic()-started))
        save_json(WORK / 'separation-comparison.json', report)
        print(json.dumps(report['excerpts'][-1], ensure_ascii=True), flush=True)
    print('DONE', flush=True)

if __name__ == '__main__':
    main()

"""Offline KARA2 vocal-stem -> lead/backing separation (UVR MDX inference contract).

The fixed ONNX is loaded locally; no network, arbitrary plugins or model downloads.
Run with the editor's engines/python/python.exe; see docs/kara2.md.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import os
import sys
import time

ROOT = Path(__file__).resolve().parent
sys.path[:0] = [str(ROOT / 'deps'), str(ROOT.parent / 'diffsinger/packages')]
MODEL = ROOT / 'models/UVR_MDXNET_KARA_2.onnx'
SHA256 = 'bf32e15105a09c0f7dddd2b67346146334d6f3ecb399ed7638eba2ab07cbf5f4'
RATE, FFT, HOP, BINS, FRAMES = 44100, 5120, 1024, 2048, 256
CHUNK, TRIM = HOP * (FRAMES - 1), FFT // 2
COMPENSATION = 1.065


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


class Transform:
    def __init__(self):
        import numpy as np
        from scipy.signal.windows import hann
        self.window = hann(FFT, sym=False).astype(np.float32)
        self.divider = np.zeros(CHUNK + FFT, dtype=np.float32)
        for i in range(FRAMES):
            self.divider[i * HOP:i * HOP + FFT] += self.window ** 2

    def forward(self, audio):
        import numpy as np
        from scipy.fft import rfft
        padded = np.pad(audio, ((0, 0), (TRIM, TRIM)), mode='reflect')
        frames = np.lib.stride_tricks.sliding_window_view(padded, FFT, axis=1)[:, ::HOP]
        spectrum = rfft(frames * self.window, axis=-1).transpose(0, 2, 1)
        return np.stack([spectrum.real, spectrum.imag], axis=1).reshape(1, 4, FFT // 2 + 1, FRAMES)

    def inverse(self, tensor):
        import numpy as np
        from scipy.fft import irfft
        value = tensor.reshape(2, 2, tensor.shape[2], FRAMES)
        spectrum = value[:, 0] + 1j * value[:, 1]
        spectrum = np.pad(spectrum, ((0, 0), (0, FFT // 2 + 1 - spectrum.shape[1]), (0, 0)))
        frames = irfft(spectrum.transpose(0, 2, 1), n=FFT, axis=-1) * self.window
        result = np.zeros((2, CHUNK + FFT), dtype=np.float32)
        for i in range(FRAMES):
            result[:, i * HOP:i * HOP + FFT] += frames[:, i]
        result /= np.maximum(self.divider, 1e-8)
        return result[:, TRIM:TRIM + CHUNK]


def session(provider='cpu'):
    import onnxruntime as ort
    if not MODEL.is_file() or digest(MODEL) != SHA256:
        raise ValueError('KARA2 model is missing or SHA256 does not match the bundled manifest')
    options = ort.SessionOptions()
    options.intra_op_num_threads = min(4, os.cpu_count() or 1)
    options.inter_op_num_threads = 1
    if provider == 'directml':
        if 'DmlExecutionProvider' not in ort.get_available_providers():
            raise ValueError('DirectML is unavailable; use --provider cpu')
        options.enable_mem_pattern = False
        options.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
        providers = ['DmlExecutionProvider', 'CPUExecutionProvider']
    else:
        providers = ['CPUExecutionProvider']
    return ort.InferenceSession(str(MODEL), options, providers=providers)


def separate(audio, model, overlap=0.25, progress=None):
    import numpy as np
    if audio.ndim != 2 or audio.shape[1] != 2 or len(audio) == 0 or not np.isfinite(audio).all():
        raise ValueError('Expected nonempty finite stereo PCM')
    peak = float(np.max(np.abs(audio)))
    if peak < 1e-8:
        return audio.copy(), np.zeros_like(audio)
    original = audio.T / peak
    generate = CHUNK - 2 * TRIM
    padding = generate + TRIM - len(audio) % generate
    mixture = np.pad(original, ((0, 0), (TRIM, padding)))
    result = np.zeros_like(mixture)
    divider = np.zeros(mixture.shape[1], dtype=np.float32)
    transform = Transform()
    step = int(CHUNK * (1 - overlap))
    starts = range(0, mixture.shape[1], step)
    for count, start in enumerate(starts):
        end = min(start + CHUNK, mixture.shape[1])
        actual = end - start
        part = np.pad(mixture[:, start:end], ((0, 0), (0, CHUNK - actual)))
        spectrum = np.ascontiguousarray(transform.forward(part)[:, :, :BINS])
        spectrum[:, :, :3] = 0
        prediction = model.run(None, {model.get_inputs()[0].name: spectrum})[0]
        if not np.isfinite(prediction).all():
            raise ValueError('KARA2 produced non-finite samples')
        wave = transform.inverse(prediction)[:, :actual]
        window = np.hanning(actual).astype(np.float32)
        result[:, start:end] += wave * window
        divider[start:end] += window
        if progress:
            progress(count + 1, len(starts))
    backing = (result / np.maximum(divider, 1e-8))[:, TRIM:TRIM + len(audio)].T
    # UVR compensation is applied to the returned stem, then subtracted for lead.
    backing *= peak * COMPENSATION
    return audio - backing, backing


def main():
    parser = argparse.ArgumentParser(description='KARA2: isolated vocals -> lead / backing (offline)')
    parser.add_argument('--input', type=Path)
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--start', type=float, default=0)
    parser.add_argument('--duration', type=float)
    parser.add_argument('--provider', choices=['cpu', 'directml'], default='cpu')
    parser.add_argument('--overlap', type=float, default=0.25)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    import numpy as np
    import soundfile as sf
    import onnxruntime as ort
    model = session(args.provider)
    if args.check:
        print(json.dumps({'ok': True, 'model_bytes': MODEL.stat().st_size, 'sha256': SHA256,
                          'python': sys.executable, 'ort': ort.__version__,
                          'providers': model.get_providers(), 'input': model.get_inputs()[0].shape}))
        return
    if not args.input or not args.output_dir:
        parser.error('--input and --output-dir are required')
    if not math.isfinite(args.start) or args.start < 0 or (args.duration is not None and
            (not math.isfinite(args.duration) or args.duration <= 0)) or not 0 <= args.overlap <= 0.75:
        parser.error('Invalid start / duration / overlap')
    args.output_dir.mkdir(parents=True, exist_ok=True)
    destinations = {name: args.output_dir / (name + '.wav') for name in ['lead', 'backing']}
    if any(p.exists() for p in [*destinations.values(), args.output_dir / 'separation.json']):
        raise FileExistsError('Choose a new output directory; existing results are never overwritten')
    started = time.monotonic()
    with sf.SoundFile(args.input) as stream:
        source_rate = stream.samplerate
        stream.seek(min(len(stream), round(args.start * source_rate)))
        audio = stream.read(-1 if args.duration is None else round(args.duration * source_rate),
                            dtype='float32', always_2d=True)
    if audio.shape[1] == 1:
        audio = np.repeat(audio, 2, axis=1)
    if audio.shape[1] != 2:
        raise ValueError('Only mono or stereo vocal stems are supported')
    if source_rate != RATE:
        import soxr
        audio = soxr.resample(audio, source_rate, RATE).astype(np.float32)
    lead, backing = separate(audio, model, args.overlap,
        lambda i, n: print(f'KARA2 {i}/{n}', file=sys.stderr, flush=True))
    for name, samples in [('lead', lead), ('backing', backing)]:
        # Float WAV preserves overs without independent normalization or clipping.
        sf.write(destinations[name], samples, RATE, subtype='FLOAT')
    report = {'ok': True, 'model': 'UVR_MDXNET_KARA_2', 'sha256': SHA256,
              'source': str(args.input.resolve()), 'source_start_seconds': args.start,
              'sample_rate': RATE, 'frames': len(audio), 'duration': len(audio) / RATE,
              'provider': model.get_providers(), 'overlap': args.overlap,
              'compensation': COMPENSATION, 'seconds': time.monotonic() - started,
              'sum_error_max': float(np.max(np.abs(lead + backing - audio))),
              'stems': {name: {'path': str(destinations[name].resolve()),
                         'peak': float(np.max(np.abs(samples))),
                         'rms': float(np.sqrt(np.mean(samples ** 2)))}
                        for name, samples in [('lead', lead), ('backing', backing)]}}
    (args.output_dir / 'separation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=True), flush=True)


if __name__ == '__main__':
    main()

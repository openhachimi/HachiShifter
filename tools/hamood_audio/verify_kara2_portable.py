"""Exercise real models using only the editor's bundled interpreter and libraries."""
from pathlib import Path
import sys, json, os, time
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--runtime', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
root = args.runtime.resolve()
args.output.mkdir(parents=True, exist_ok=True)
sys.path[:0] = [str(root), str(root / 'deps'), str(root / 'vendor/BTC-ISMIR19')]
os.environ['NUMBA_CACHE_DIR'] = str(args.output / 'numba')
os.environ['NUMBA_CACHE_LOCATOR_CLASSES'] = 'UserProvidedCacheLocator'
import kara2
import numpy as np, torch, soundfile as sf
torch.set_num_threads(4)
transform = kara2.Transform()
audio = np.random.default_rng(0).normal(0, .1, (2, kara2.CHUNK)).astype(np.float32)
spectrum = transform.forward(audio)
expected = torch.stft(torch.from_numpy(audio), kara2.FFT, kara2.HOP,
                     window=torch.hann_window(kara2.FFT), center=True,
                     pad_mode='reflect', return_complex=True).numpy()
actual = spectrum.reshape(2, 2, kara2.FFT // 2 + 1, kara2.FRAMES)
stft_error = float(np.max(np.abs(actual[:, 0] + 1j * actual[:, 1] - expected)))
roundtrip_error = float(np.max(np.abs(transform.inverse(spectrum) - audio)))
assert stft_error < 1e-4 and roundtrip_error < 1e-6, (stft_error, roundtrip_error)
print('STFT and inverse match PyTorch reference', stft_error, roundtrip_error, flush=True)
model = kara2.session()
silence = np.zeros((1000, 2), np.float32)
lead, backing = kara2.separate(silence, model)
assert np.array_equal(lead + backing, silence)
del model

# Same real input as the earlier harmony/reference investigation, never modify it.
source = Path('E:/和声合成新UI/HAMOOD-原唱参考试作/原唱人声-已对齐.wav')
with sf.SoundFile(source) as f:
    f.seek(round(45 * f.samplerate))
    excerpt = f.read(round(22 * f.samplerate), dtype='float32', always_2d=True)
    rate = f.samplerate
test_audio = args.output / 'input-22s.wav'
sf.write(test_audio, excerpt, rate, subtype='FLOAT')
import subprocess
if not (args.output/'separated/separation.json').exists():
    subprocess.run([sys.executable, '-I', '-B', str(root/'kara2.py'),
        '--input', str(test_audio), '--output-dir', str(args.output/'separated')], check=True)
report = json.loads((args.output/'separated/separation.json').read_text(encoding='utf-8'))
assert report['sum_error_max'] < 2e-6 and report['frames'] == len(excerpt)
assert report['stems']['lead']['rms'] > .001 and report['stems']['backing']['rms'] > .00001

import worker
result = worker.analyse({'audio_path': str(test_audio), 'cache_dir': str(args.output/'analysis-cache'), 'force':True})
assert result['ok'] and result['beats'] and result['chords']
for name in ['torch','torchaudio','soxr','numpy','scipy','soundfile','librosa','onnxruntime']:
    module = __import__(name)
    path = Path(module.__file__).resolve()
    assert path.is_relative_to(root.parent.parent), (name, path)
summary = dict(ok=True, python=sys.executable, stft_error=stft_error, inverse_error=roundtrip_error,
    separation=report, beats=len(result['beats']), chords=len(result['chords']),
    external_python_dependencies=False)
(args.output/'validation.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(summary, ensure_ascii=True), flush=True)

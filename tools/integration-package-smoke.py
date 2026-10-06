"""Verify the actual portable package, MCP persistence and upstream render CLI."""
from pathlib import Path
import json
import os
import re
import struct
import subprocess
import sys

exe = Path(sys.argv[1]).resolve()
bank = Path(sys.argv[2]).resolve()
out = Path(__file__).resolve().parent.parent / 'integration-tests/package'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, HACHI_TEST_SETTINGS_DIR=str(out / 'settings'))

def run(args, stdin=None):
    result = subprocess.run([str(exe), *args], input=stdin,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding='utf-8',
        errors='replace', env=env, timeout=120, creationflags=subprocess.CREATE_NO_WINDOW)
    if result.returncode:
        raise RuntimeError(f'{args}: {result.returncode}\n{result.stdout}')
    return result.stdout

def mcp(commands):
    lines = [json.dumps({'jsonrpc': '2.0', 'id': i + 1, 'method': 'tools/call',
        'params': {'name': name, 'arguments': args}}) for i, (name, args) in enumerate(commands)]
    text = run(['--mcp'], '\n'.join(lines) + '\n')
    for line in text.splitlines():
        if line.startswith('{'):
            response = json.loads(line)
            assert not response.get('error') and not response.get('result', {}).get('isError'), response
    return text

ust = out / 'portable.ust'
project = out / 'portable.hjpx'
wav = out / 'portable.wav'
lines = ['[#VERSION]', 'UST Version1.2', '[#SETTING]', 'Tempo=120', 'Tracks=1',
         'ProjectName=Portable integration check', 'Mode2=True']
for i, pitch in enumerate([60, 62, 64, 60]):
    lines += [f'[#{i:04d}]', 'Length=480', 'Lyric=a', f'NoteNum={pitch}']
lines += ['[#TRACKEND]', '']
ust.write_text('\r\n'.join(lines), encoding='utf-8')
text = mcp([('project_new', {}), ('import_ust', {'path': str(ust)}),
            ('project_snapshot', {}), ('project_save', {'path': str(project)})])
track = re.search(r'track_[0-9a-fA-F]+', text).group(0)
text += mcp([('project_open', {'path': str(project)}),
             ('set_track', {'track_id': track, 'pitch_algorithm': 'utau', 'voicebank_directory': str(bank)}),
             ('project_save', {'path': str(project)})])
(out / 'mcp.log').write_text(text, encoding='utf-8')
checks = {}
for flag in ['--smoke-integrated', '--smoke-active-resampler']:
    text = run([flag])
    (out / (flag[2:] + '.log')).write_text(text, encoding='utf-8')
    checks[flag] = 'pass'
text = run(['--render-project', str(project), str(wav)])
(out / 'render-project.log').write_text(text, encoding='utf-8')
assert 'rendered=1' in text, text
raw = wav.read_bytes()
assert raw[:4] == b'RIFF' and raw[8:12] == b'WAVE'
chunks = {}
offset = 12
while offset + 8 <= len(raw):
    key, length = struct.unpack_from('<4sI', raw, offset)
    chunks[key] = raw[offset + 8:offset + 8 + length]
    offset += 8 + length + (length % 2)
fmt, channels, rate, _, block, bits = struct.unpack_from('<HHIIHH', chunks[b'fmt '])
assert bits == 24 and channels == 2 and rate >= 44100
samples = chunks[b'data']
peak = max(abs(int.from_bytes(samples[i:i+3], 'little', signed=True))
           for i in range(0, len(samples), 3)) / (1 << 23)
duration = len(samples) / block / rate
assert 1.9 <= duration <= 5 and peak > 0.001, (duration, peak)
checks['render-project'] = {'duration': duration, 'peak': peak, 'channels': channels,
                            'sample_rate': rate, 'bits': bits, 'log': text.strip()}
(out / 'results.json').write_text(json.dumps(checks, indent=2), encoding='utf-8')
print(json.dumps(checks, indent=2))

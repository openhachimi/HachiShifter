#!/usr/bin/env python3
"""Exercise native FLAG curves through the editor's actual MCP/export path.

Usage: python hifisampler_curve_smoke.py EDITOR MODEL_DIRECTORY OUTPUT_DIRECTORY
Uses an isolated synthetic voicebank. No user projects or preferences are edited.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import sys
import wave

import numpy as np
from scipy.io import wavfile
from scipy.signal import stft

from utau4_mode_smoke import McpClient
from flag_curve_smoke import write_midi


def main() -> int:
    binary, models, output = [Path(p).resolve() for p in sys.argv[1:4]]
    output.mkdir(parents=True, exist_ok=True)
    os.environ['HACHI_TEST_SETTINGS_DIR'] = str(output / 'settings')
    os.environ['HACHISHIFTER_MCP_ROOTS'] = str(output)
    os.environ['HACHISHIFTER_NSF_HIFIGAN_MODEL_DIR'] = str(models)
    bank = output / 'bank'
    bank.mkdir(exist_ok=True)
    rate = 44100
    time = np.arange(rate) / rate
    rng = np.random.default_rng(707)
    audio = .10 * np.sin(2 * np.pi * 220 * time)
    audio += .05 * np.sin(2 * np.pi * 660 * time)
    audio += .03 * np.sin(2 * np.pi * 1100 * time) + .008 * rng.standard_normal(rate)
    with wave.open(str(bank / 'a.wav'), 'wb') as wav:
        wav.setparams((1, 2, rate, rate, 'NONE', 'not compressed'))
        wav.writeframes(np.round(np.clip(audio, -1, 1) * 32767).astype('<i2').tobytes())
    for name in ('oto.ini', 'oto.jie.ini'):
        (bank / name).write_text('a.wav=a,0,100,-1000,80,30\n', encoding='utf-8')
    (bank / 'oto4.ini').write_text('a.wav=0,100,200,700\n', encoding='utf-8')
    (bank / 'otomou.ini').write_text('a.wav=CVVC,0,100,200,700\n', encoding='utf-8')
    midi = output / 'one.mid'
    write_midi(midi)
    checks, metrics = [], {}

    def check(name, value):
        passed = bool(value)
        checks.append({'name': name, 'passed': passed})
        print(f'{name}={int(passed)}', flush=True)

    def read(path):
        sr, value = wavfile.read(path)
        if np.issubdtype(value.dtype, np.integer):
            value = value.astype(np.float64) / (np.iinfo(value.dtype).max + 1)
        if value.ndim == 2:
            value = value.mean(axis=1)
        return sr, value

    def descriptor(path, start, end):
        sr, values = read(path)
        values = values[int(start * sr):int(end * sr)]
        _, _, spectrum = stft(values, sr, nperseg=1024, noverlap=512)
        return np.sqrt(np.mean(np.abs(spectrum) ** 2, axis=1))

    def distance(a, b):
        return float(np.linalg.norm(a - b) / max(1.e-8, np.linalg.norm(b)))

    client = McpClient(binary)
    try:
        client.call('import_midi', {'path': str(midi)})
        track = json.loads(client.call('project_snapshot'))['tracks'][0]
        track_id = track['id']
        note_id = track['clips'][0]['notes'][0]['id']
        client.call('set_track', {'track_id': track_id, 'pitch_algorithm': 'utaumou',
                    'voicebank_directory': str(bank), 'output_engine': 'pc-nsf-hifigan',
                    'volume': .2, 'normalize_volume': False})
        client.call('set_note', {'note_id': note_id, 'label': 'a', 'gain': 1,
                    'utau_flags': '', 'flag_split': False, 'amplitude_envelope': [],
                    'utau_auto_pitch_transition': False})

        def state():
            return json.loads(client.call('project_snapshot'))['tracks'][0]['clips'][0]['notes'][0]

        def curve(flag, points):
            client.call('set_note', {'note_id': note_id, 'flag_curve': {'flag': flag, 'points': points}})

        def render(name, **values):
            client.call('set_note', {'note_id': note_id, **values})
            path = output / (name + '.wav')
            response = client.call('export_wav', {'path': str(path), 'timeout_seconds': 180,
                                                 'sample_rate': rate, 'bit_depth': 32})
            metrics[name + '_render'] = response
            return path

        ranges = {'g': (-120, 120), 'Hb': (0, 200), 'Hv': (0, 140), 'Ht': (-60, 60),
                  'HG': (0, 80), 'P': (0, 100), 't': (0, 1200), 'A': (-80, 80)}
        for flag, (low, high) in ranges.items():
            if flag == 'A':
                # A modulates amplitude according to the pitch derivative.
                client.call('set_pitch_curve', {'note_id': note_id,
                             'points': [{'time_seconds': 0, 'midi': 62}, {'time_seconds': 1, 'midi': 69}]})
            name = 'HIFI:' + flag
            curve(name, [[-.08, low], [1, low]])
            a = render(flag + '-low', flag_curve_enabled=True)
            curve(name, [[-.08, high], [1, high]])
            b = render(flag + '-high')
            curve(name, [[-.08, low], [.3, low], [.7, high], [1, high]])
            ramp = render(flag + '-linear')
            off = render(flag + '-off', flag_curve_enabled=False)
            # Independent spectral descriptors avoid depending on random neural phase.
            early, late = (.56, .74), (1.26, 1.44)
            da, db = descriptor(a, *late), descriptor(b, *late)
            re, rl = descriptor(ramp, *early), descriptor(ramp, *late)
            ae, be = descriptor(a, *early), descriptor(b, *early)
            effect = distance(da, db)
            early_good, early_bad = distance(re, ae), distance(re, be)
            late_good, late_bad = distance(rl, db), distance(rl, da)
            metrics[flag] = {'constant_difference': effect, 'early_to_low': early_good,
                             'early_to_high': early_bad, 'late_to_high': late_good, 'late_to_low': late_bad}
            check(flag + '_actual_audio_responds', effect > .025)
            check(flag + '_linear_early_uses_low', early_good < early_bad)
            check(flag + '_linear_late_uses_high', late_good < late_bad)
            check(flag + '_disabled_preserves_points', len(state()['flag_curves'][name]) == 4)
            curve(name, [])
            plain = render(flag + '-plain', flag_curve_enabled=False)
            check(flag + '_off_restores_fixed_audio', distance(descriptor(off, *late), descriptor(plain, *late)) < .02)

        # All OTO modes read the native curve; a one-octave ramp gives a clear F0 probe.
        for mode in ('utau', 'utau4', 'utaumou'):
            client.call('set_track', {'track_id': track_id, 'pitch_algorithm': mode})
            client.call('set_pitch_curve', {'note_id': note_id, 'points': [{'time_seconds': 0, 'midi': 62}, {'time_seconds': 1, 'midi': 62}]})
            curve('HIFI:t', [[-.08, 0], [.3, 0], [.7, 1200], [1, 1200]])
            result = render(mode + '-linear', flag_curve_enabled=True)
            low = descriptor(result, .56, .74)
            high = descriptor(result, 1.26, 1.44)
            check(mode + '_linear_pitch_changes_through_note', np.argmax(high) > np.argmax(low) * 1.7)
            curve('HIFI:t', [])

        client.call('set_note', {'note_id': note_id, 'utau_flags': 'g120', 'flag_curve_enabled': True})
        curve('g', [[0, -40], [1, 40]])
        curve('HIFI:g', [[0, 300], [1, 300]])
        curve('HIFI:g', [])
        check('clear_preserves_legacy_and_blocks_fallback', state()['flag_curves'].get('g') == [[0, -40, 'linear'], [1, 40, 'linear']]
              and state()['flag_curves'].get('HIFI:g') == [])
        fixed = render('legacy-cleared', flag_curve_enabled=True)
        project_file = output / 'curves.hjpx'
        client.call('project_save', {'path': str(project_file)})
        client.call('project_open', {'path': str(project_file)})
        reopened = render('legacy-reopened')
        check('clear_override_survives_reopen', state()['flag_curves'].get('HIFI:g') == [])
        check('reopened_clear_matches_fixed_audio', distance(descriptor(fixed, 1.26, 1.44), descriptor(reopened, 1.26, 1.44)) < .02)
        curve('g', [])
        text = render('text-only')
        check('native_clear_really_restores_text_flag', distance(descriptor(fixed, 1.26, 1.44), descriptor(text, 1.26, 1.44)) < .02)
    finally:
        client.close()
    report = {'ok': all(check['passed'] for check in checks), 'checks': checks, 'metrics': metrics}
    (output / 'validation.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    return 0 if report['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

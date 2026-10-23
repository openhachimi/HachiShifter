#!/usr/bin/env python3
"""Real GAME+FCPE imports, including authored OTO/HJM regions and time edits."""
from __future__ import annotations
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import wave
from utau4_mode_smoke import McpClient


def main():
    binary, models, vocal, output = [Path(x).resolve() for x in sys.argv[1:]]
    output.mkdir(parents=True, exist_ok=True)
    os.environ['HACHISHIFTER_MCP_ROOTS'] = str(output)
    checks = []
    def check(name, ok):
        checks.append({'name': name, 'ok': bool(ok)})
        print(f'{name}={int(bool(ok))}', flush=True)
        assert ok, name
    def snap(client):
        return json.loads(client.call('project_snapshot'))
    def first(client):
        return snap(client)['tracks'][0]['clips'][0]
    config = {'game_model_dir': str(models/'game/medium'),
              'fcpe_model': str(models/'fcpe/fcpe.onnx'), 'inference': 'cpu'}
    plain = output/'vocal.wav'
    shutil.copy2(vocal, plain)
    for suffix in ('.hjm.csv', '.hachi.csv'):
        Path(str(plain)+suffix).unlink(missing_ok=True)
    with wave.open(str(plain)) as audio:
        duration = audio.getnframes()/audio.getframerate()
    client = McpClient(binary)
    try:
        response = client.call('import_audio', {'path': str(plain), **config})
        check('real_vocal_game_fcpe_backend', 'GAME+FCPE' in response)
        before = first(client)
        check('game_notes_have_dense_source_pitch', before['notes'] and all(
            len(n['contour']) > 10 for n in before['notes']))
        check('unmodified_audio_has_no_pitch_quantization', all(
            abs(n['midi']-n['source_midi_center']) < 1e-4 for n in before['notes']))
        reference = before['notes'][0]
        client.call('resize_clip', {'clip_id': before['id'], 'start_seconds': 1.5,
                                  'duration_seconds': before['duration_seconds']*2})
        stretched = first(client)
        check('real_f0_stretches_without_losing_frames', all(
            len(a['contour']) == len(b['contour']) and all(
                abs(p['time_seconds']*2-q['time_seconds']) < 1e-7
                and abs(p['relative_cents']-q['relative_cents']) < 1e-7
                and p['voiced'] == q['voiced'] for p,q in zip(a['contour'], b['contour']))
            for a,b in zip(before['notes'],stretched['notes'])))
        client.call('project_new')
        authored = output/'authored.wav'
        shutil.copy2(vocal, authored)
        header = ['name','region_start_sec','region_end_sec','note_alignment_sec',
            'fixed_duration_sec','relative_pitch_cents','melodyne_project_data',
            'melodyne_pitch_center_cents','melodyne_original_pitch_center_cents',
            'melodyne_pitch_drift_factor','melodyne_pitch_modulation_factor',
            'melodyne_transition_sec','melodyne_formant_offset_cents','melodyne_amplitude_factor',
            'melodyne_sibilant_balance','melodyne_attack_duration_sec','melodyne_decay_elongation',
            'utau_overlap_sec','hjm_version','native_role','native_provenance','native_confidence',
            'native_segments','native_amplitude_envelope']
        rows = []
        # Two explicit non-overlapping regions, independent of GAME boundaries.
        for i,(begin,end) in enumerate(((0,duration*.48),(duration*.52,duration))):
            row = dict.fromkeys(header,0)
            row.update(name=f'authored-{i+1}',region_start_sec=begin,region_end_sec=end,
                note_alignment_sec=begin+.04,fixed_duration_sec=.04,
                melodyne_pitch_drift_factor=1,melodyne_pitch_modulation_factor=1,
                melodyne_amplitude_factor=.75,melodyne_formant_offset_cents=100,
                hjm_version=2,native_role='vowel',native_provenance='manual',native_confidence=1,
                native_segments=f'seg{i}|a|vowel|{begin}|{end}|manual|1|{end}|0|1|1',
                native_amplitude_envelope='')
            rows.append(row)
        sidecar = Path(str(authored)+'.hjm.csv')
        with sidecar.open('w',encoding='utf-8',newline='') as stream:
            writer=csv.DictWriter(stream,fieldnames=header);writer.writeheader();writer.writerows(rows)
        original_sidecar=sidecar.read_bytes()
        response=client.call('import_audio',{'path':str(authored),**config})
        check('authored_audio_still_runs_game', 'GAME+FCPE' in response)
        annotated=first(client)
        check('authored_regions_kept', len(annotated['notes'])==2 and all(
            n['label']==r['name'] and abs(n['start_seconds']-r['region_start_sec'])<1e-6
            and abs(n['duration_seconds']-(r['region_end_sec']-r['region_start_sec']))<1e-6
            for n,r in zip(annotated['notes'],rows)))
        check('authored_controls_kept',all(abs(n['gain']-.75)<1e-6
            and abs(n['formant_semitones']-1)<1e-6 and n['native_segments'] for n in annotated['notes']))
        check('authored_flat_placeholder_replaced',all(len(n['contour'])>50 for n in annotated['notes'])
            and any(p['voiced'] and abs(p['relative_cents'])>10 for n in annotated['notes'] for p in n['contour']))
        check('source_annotation_not_overwritten',sidecar.read_bytes()==original_sidecar)
        saved=output/'authored.hjpx';client.call('project_save',{'path':str(saved)})
        client.call('project_new');client.call('project_open',{'path':str(saved)})
        check('real_pitch_survives_project_reopen',first(client)['notes']==annotated['notes'])
        n=first(client)['notes'][0]
        client.call('resize_note',{'note_id':n['id'],'start_seconds':n['start_seconds'],
                                 'duration_seconds':n['duration_seconds']*.75})
        edited=first(client)['notes'][0]
        check('note_resize_preserves_measured_pitch',len(edited['contour'])==len(n['contour'])
            and max(abs(a['relative_cents']-b['relative_cents']) for a,b in zip(n['contour'],edited['contour']))<1e-6
            and abs(edited['contour'][-1]['time_seconds']-edited['duration_seconds'])<1e-6)
        (output/'snapshot.json').write_text(json.dumps(snap(client),ensure_ascii=False,indent=2),encoding='utf-8')
    finally:
        client.close()
    report={'ok':True,'binary':str(binary),'exe_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
            'backend':'GAME+FCPE','checks':checks}
    (output/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'checks={len(checks)}',flush=True)


if __name__ == '__main__':
    main()

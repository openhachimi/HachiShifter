"""Validate note-relative curves, save/reopen, and render editable harmony trial."""
from pathlib import Path
import json,time,hashlib
from client import Client

BASE=Path(__file__).resolve().parents[3]
WORK=BASE/'HAMOOD-连续和声试作'

def main():
    report=json.loads((WORK/'project-result.json').read_text(encoding='utf-8'))
    plan=json.loads((WORK/'arrangement.json').read_text(encoding='utf-8'))
    exe=Path(report['exe']);project=Path(report['project'])
    c=Client(exe,'--mcp','--roots='+str(BASE),timeout=1100)
    try:
        c.rpc('initialize',{});c.call('project_open',{'path':str(project)})
        initial=c.call('project_snapshot')
        # Correct any first-run curves generated before checking the legacy
        # schema against ProjectModel: this API expects note-relative times.
        for kind,rows in plan['voices'].items():
            for r in rows:
                points=[dict(time_seconds=p['time_seconds']-r['start'],midi=p['midi']) for p in r['pitch_curve']]
                points.append(dict(time_seconds=r['end']-r['start'],midi=r['target_midi']))
                c.call('set_pitch_curve',dict(note_id=r['generated_note_id'],points=points))
            print('Pitch curves verified in note time:',kind,len(rows),flush=True)
        final=c.call('project_snapshot')
        for kind,hid in report['track_ids'].items():
            track=next(t for t in final['tracks'] if t['id']==hid)
            notes={n['id']:n for cl in track['clips'] for n in cl['notes']}
            for r in plan['voices'][kind]:
                n=notes[r['generated_note_id']]
                assert abs(n['midi']-r['target_midi'])<1e-6
                assert n['contour'] and any(p['has_manual_target'] for p in n['contour'])
                core=[p['rendered_target_cents'] for p in n['contour'] if .065<=p['time_seconds']<n['duration_seconds']-.015]
                if core:assert max(abs(v) for v in core)<65,(r['source_note_id'],core)
        for tid in [report['lead_track_id'],report['backing_track_id']]:
            assert next(t for t in final['tracks'] if t['id']==tid)==next(t for t in initial['tracks'] if t['id']==tid)
        c.call('project_save',{'path':str(project)})
        c.call('project_new');c.call('project_open',{'path':str(project)})
        assert c.call('project_snapshot')['tracks']==final['tracks']
        (WORK/'final-snapshot.json').write_text(json.dumps(final,ensure_ascii=False),encoding='utf-8')
        report.update(note_relative_curve_validation=True,save_reopen_verified=True)
        (WORK/'project-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print('Project save/reopen and curve range checks passed',flush=True)
        # Only the temporary rendering document loses muted historical trials.
        keep=[report['lead_track_id'],report['backing_track_id'],*report['track_ids'].values()]
        for t in final['tracks']:
            if t['id'] not in keep:c.call('remove_track',{'track_id':t['id']})
        c.call('set_utau_resampler',{'path':str(exe.parent/'engines/WCSNDM-0.0803.exe')})
        outputs=[]
        def export(name,start=0,end=0):
            args=dict(path=str(WORK/name),from_seconds=start,to_seconds=end,timeout_seconds=1000,
                      channels=2,sample_rate=48000,bit_depth=24)
            print('Rendering',name.encode('unicode_escape').decode(),flush=True)
            result=c.call('export_wav',args);outputs.append(result)
            (WORK/'render-results.json').write_text(json.dumps(outputs,ensure_ascii=False,indent=2),encoding='utf-8')
            print(json.dumps(result,ensure_ascii=True),flush=True)
        export('东京泰迪熊-新和声完整混音.wav')
        for hid in report['track_ids'].values():c.call('set_track',{'track_id':hid,'muted':True})
        for lo,hi in [(45,67),(148,170)]:export(f'{lo}-{hi}秒-A-主唱与伴奏.wav',lo,hi)
        for hid in report['track_ids'].values():c.call('set_track',{'track_id':hid,'muted':False})
        for tid in [report['lead_track_id'],report['backing_track_id']]:c.call('set_track',{'track_id':tid,'muted':True})
        export('东京泰迪熊-新和声单独.wav')
        assert hashlib.sha256(Path(report['source']).read_bytes()).hexdigest()==report['source_sha256']
    finally:
        c.p.terminate();c.p.wait(10)

if __name__=='__main__':main()

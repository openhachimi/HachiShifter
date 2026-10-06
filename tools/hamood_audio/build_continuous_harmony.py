"""Build an independent editable chorus arrangement through the live editor MCP."""
from pathlib import Path
import os,json,subprocess,time,hashlib
from client import Client

BASE=Path(__file__).resolve().parents[3]
WORK=BASE/'HAMOOD-连续和声试作'
OLD=BASE/'HAMOOD-KARA2和声分析'
exe=BASE/'HachiShifter-整合版-0.2.3-DiffSinger/HachiShifter Next.exe'
source=OLD/'东京泰迪熊-KARA2和声分析.hjpx'
out=WORK/'东京泰迪熊-连续和声试作.hjpx'
plan=json.loads((WORK/'arrangement.json').read_text(encoding='utf-8'))
baseline=json.loads((OLD/'final-snapshot.json').read_text(encoding='utf-8'))
digest=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    if out.exists():
        # A failed validation left only our initial, untouched source copy.
        # Resume only if the saved tracks still exactly match that baseline.
        assert not (WORK/'project-result.json').exists(),'A completed trial must not be overwritten'
        probe=Client(exe,'--mcp','--roots='+str(BASE))
        try:
            probe.rpc('initialize',{});probe.call('project_open',{'path':str(out)})
            assert probe.call('project_snapshot')['tracks']==baseline['tracks'],'Existing trial has edits; preserve it'
        finally:probe.p.terminate();probe.p.wait(10)
    original_hash=digest(source)
    registry=Path(os.environ['TEMP'])/'HachiShifter-MCP'
    known={p.parent.name for p in registry.glob('*/session.json')}
    startup=subprocess.STARTUPINFO();startup.dwFlags=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    log=(WORK/'project-build.log').open('w',encoding='utf-8')
    proc=subprocess.Popen([str(exe)],env=dict(os.environ,HACHI_TEST_SETTINGS_DIR=str(WORK/'settings')),
        stdout=log,stderr=log,startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW)
    c=None
    try:
        sid=None;started=time.monotonic()
        while time.monotonic()-started<45:
            found=[]
            for p in registry.glob('*/session.json'):
                if p.parent.name in known:continue
                try:
                    v=json.loads(p.read_text(encoding='utf-8-sig'))
                    if Path(v['executable']).resolve()==exe.resolve() and time.time()*1000-v['heartbeat_ms']<15000:found.append(v['session_id'])
                except (OSError,ValueError,KeyError):pass
            assert len(found)<2,'Ambiguous new sessions'
            if found:sid=found[0];break
            time.sleep(.2)
        assert sid,'No isolated editor session'
        c=Client(exe,'--mcp-live','--session='+sid,timeout=120);c.rpc('initialize',{})
        c.wait(c.edit('project_open',{'path':str(source)}))
        actual=c.call('project_snapshot');assert actual['tracks']==baseline['tracks']
        # Save to a new path before any edits; source and user's visible window stay intact.
        c.edit('project_save',{'path':str(out)})
        c.edit('set_utau_resampler',{'path':str(exe.parent/'engines/WCSNDM-0.0803.exe')})
        lead=actual['tracks'][0]
        backing=next(t for t in actual['tracks'] if t['accompaniment'] and '无和声' in t['clips'][0]['source_file'])
        originals={n['id']:n for cl in lead['clips'] for n in cl['notes']}
        created={};checks=0
        for kind,title,gain,pan in [('main','连续主和声',-9,-.25),('upper','高声部点缀',-14,.3)]:
            rows=plan['voices'][kind]
            c.edit('editor_select',{'track_id':lead['id'],'note_ids':[r['source_note_id'] for r in rows]})
            opts=dict(track_id=lead['id'],voices=[-2] if kind=='main' else [2],gain_db=gain,preserve_pitch=True,key_mode='manual',tonic=0)
            c.call('hamood_preview',opts)
            result=c.edit('hamood_generate',opts);hid=result['created_track_ids'][0];created[kind]=hid
            track=next(t for t in c.call('project_snapshot')['tracks'] if t['id']==hid)
            generated=[n for cl in track['clips'] for n in cl['notes']]
            assert len(generated)==len(rows)
            commands=[]
            for r in rows:
                old=originals[r['source_note_id']]
                matches=[n for n in generated if abs(n['start_seconds']-old['start_seconds'])<1e-7 and n['label']==old['label']]
                assert len(matches)==1
                n=matches[0];r['generated_note_id']=n['id']
                commands.append(dict(name='transpose_note',arguments=dict(note_id=n['id'],semitones=r['target_midi']-n['midi'])))
                # ProjectModel consumes NOTE-relative times, despite the legacy
                # MCP schema saying clip-relative. The plan stores project time.
                relative=[dict(time_seconds=p['time_seconds']-r['start'],midi=p['midi']) for p in r['pitch_curve']]
                relative.append(dict(time_seconds=r['end']-r['start'],midi=r['target_midi']))
                commands.append(dict(name='set_pitch_curve',arguments=dict(note_id=n['id'],points=relative)))
            commands.append(dict(name='set_track',arguments=dict(track_id=hid,name=f'HAMOOD · {title}',muted=False,solo=False,pan=pan)))
            for i in range(0,len(commands),24):
                c.edit('editor_batch',{'commands':commands[i:i+24]})
            print('Created',kind,len(rows),'notes',flush=True)
        for t in actual['tracks']:
            if t['id'] not in [lead['id'],backing['id']]:c.edit('set_track',{'track_id':t['id'],'muted':True,'solo':False})
        final=c.call('project_snapshot')
        assert final['tracks'][0]==lead
        assert next(t for t in final['tracks'] if t['id']==backing['id'])==backing
        for kind,hid in created.items():
            track=next(t for t in final['tracks'] if t['id']==hid)
            generated={n['id']:n for cl in track['clips'] for n in cl['notes']}
            for r in plan['voices'][kind]:
                n=generated[r['generated_note_id']];old=originals[r['source_note_id']]
                assert abs(n['midi']-r['target_midi'])<1e-6
                assert all(n.get(k)==v for k,v in old.items() if 'flag' in k or k in ['label','start_seconds','duration_seconds','gain'])
                # Legacy set_pitch_curve writes the manual sampled curve, not
                # UI control-point anchors. Check the actual rendered data.
                assert n['contour'] and any(p['has_manual_target'] for p in n['contour'])
                checks+=1
        c.edit('editor_select',{'track_id':created['main'],'note_ids':[]})
        c.edit('project_save',{'path':str(out)})
        c.edit('project_new');c.wait(c.edit('project_open',{'path':str(out)}))
        assert c.call('project_snapshot')['tracks']==final['tracks']
        assert digest(source)==original_hash
        report=dict(ok=True,project=str(out),source=str(source),source_sha256=original_hash,source_unchanged=True,
                    exe=str(exe),track_ids=created,lead_track_id=lead['id'],backing_track_id=backing['id'],
                    lead_unchanged=True,backing_unchanged=True,flag_lyric_timing_checks=checks,save_reopen_verified=True)
        (WORK/'project-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        (WORK/'final-snapshot.json').write_text(json.dumps(final,ensure_ascii=False),encoding='utf-8')
        (WORK/'arrangement.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=True),flush=True)
    finally:
        if c:c.p.terminate();c.p.wait(10)
        proc.terminate();proc.wait(10);log.close()

if __name__=='__main__':main()

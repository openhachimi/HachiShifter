from pathlib import Path
import os,sys,json,subprocess,time,hashlib,shutil
from client import Client
repo=Path(__file__).resolve().parents[2];work=repo.parent/'HAMOOD-KARA2和声分析'
base=json.loads((work/'baseline.json').read_text(encoding='utf-8'));status=json.loads((work/'live-status.json').read_text(encoding='utf-8'))
evidence=json.loads((work/'harmony-corroborated.json').read_text(encoding='utf-8'))
exe=repo.parent/'HachiShifter-整合版-0.2.3-DiffSinger/HachiShifter Next.exe';source=Path(status['project_path']);out=work/'东京泰迪熊-KARA2和声分析.hjpx'
assert not out.exists(),'Will not overwrite existing project'
digest=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
source_hash=digest(source)
registry=Path(os.environ['TEMP'])/'HachiShifter-MCP';known={p.parent.name for p in registry.glob('*/session.json')}
startup=subprocess.STARTUPINFO();startup.dwFlags=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
log=(work/'project-build.log').open('w',encoding='utf-8')
proc=subprocess.Popen([str(exe)],env=dict(os.environ,HACHI_TEST_SETTINGS_DIR=str(work/'settings')),stdout=log,stderr=log,startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW)
c=None
try:
 sid=None;start=time.monotonic()
 while time.monotonic()-start<45:
  found=[]
  for p in registry.glob('*/session.json'):
   if p.parent.name in known:continue
   try:
    v=json.loads(p.read_text(encoding='utf-8-sig'))
    if Path(v['executable']).resolve()==exe.resolve() and time.time()*1000-v['heartbeat_ms']<15000:found.append(v['session_id'])
   except (OSError,ValueError,KeyError):pass
  if len(found)==1:sid=found[0];break
  assert len(found)<2,'Ambiguous newly opened sessions'
  time.sleep(.15)
 assert sid,'No analysis window session'
 c=Client(exe,'--mcp-live','--session='+sid);c.rpc('initialize',{});c.wait(c.edit('project_open',{'path':str(source)}))
 actual=c.call('project_snapshot');assert actual['tracks']==base['tracks'],'Saved project changed during analysis'
 c.edit('project_save',{'path':str(out)})
 c.edit('set_utau_resampler',{'path':str(exe.parent/'engines/WCSNDM-0.0803.exe')})
 lead=base['tracks'][0];notes={n['id']:n for cl in lead['clips'] for n in cl['notes']}
 created={}
 for kind,title in [('strong','较强证据'),('review','待复核')]:
  rows=[r for r in evidence['rows'] if r['status']==kind]
  if not rows:continue
  c.edit('editor_select',{'track_id':lead['id'],'note_ids':[r['id'] for r in rows]})
  result=c.edit('hamood_generate',{'track_id':lead['id'],'voices':[-2],'gain_db':-9,'preserve_pitch':True,'key_mode':'manual','tonic':0})
  hid=result['created_track_ids'][0];track=next(t for t in c.call('project_snapshot')['tracks'] if t['id']==hid)
  generated=[n for cl in track['clips'] for n in cl['notes']];commands=[]
  for r in rows:
   original=notes[r['id']];matches=[n for n in generated if abs(n['start_seconds']-original['start_seconds'])<1e-7 and n['label']==original['label']]
   assert len(matches)==1
   n=matches[0];r['generated_note_id']=n['id']
   commands.append({'name':'transpose_note','arguments':{'note_id':n['id'],'semitones':r['chosen']['target_midi']-n['midi']}})
  commands.append({'name':'set_track','arguments':{'track_id':hid,'name':f'KARA2 · {title} {len(rows)} 音','muted':kind!='strong','pan':-.2}})
  # Batches remain bounded by the live tool limit.
  for i in range(0,len(commands),32):c.edit('editor_batch',{'commands':commands[i:i+32]})
  created[kind]=hid
  print('Created',kind,len(rows),flush=True)
 backing=next(t for t in base['tracks'] if t['accompaniment'] and '无和声' in t['clips'][0]['source_file'])
 for t in base['tracks']:
  if t['id'] not in [lead['id'],backing['id']]:c.edit('set_track',{'track_id':t['id'],'muted':True})
 result=c.edit('add_track',{'name':'KARA2 · 原唱背景声部（原八度参考）','accompaniment':True});rid=result.split('track_id=')[1].strip()
 c.wait(c.edit('import_audio',{'path':str(work/'separated/backing.wav'),'track_id':rid,'start_seconds':0}))
 c.edit('set_track',{'track_id':rid,'muted':True})
 final=c.call('project_snapshot');assert final['tracks'][0]==lead
 assert next(t for t in final['tracks'] if t['id']==backing['id'])==backing
 checks=0
 for kind,hid in created.items():
  track=next(t for t in final['tracks'] if t['id']==hid);generated={n['id']:n for cl in track['clips'] for n in cl['notes']}
  for r in (r for r in evidence['rows'] if r['status']==kind):
   n=generated[r['generated_note_id']];old=notes[r['id']]
   assert n['midi']==r['chosen']['target_midi']
   assert all(n.get(k)==v for k,v in old.items() if 'flag' in k or k in ['label','start_seconds','duration_seconds'])
   checks+=1
 if 'strong' in created:c.edit('editor_select',{'track_id':created['strong'],'note_ids':[]})
 c.edit('project_save',{'path':str(out)})
 c.edit('project_new');c.wait(c.edit('project_open',{'path':str(out)}));assert c.call('project_snapshot')['tracks']==final['tracks']
 assert digest(source)==source_hash
 report=dict(ok=True,project=str(out),original_project_sha256=source_hash,original_project_unchanged=True,
             lead_unchanged=True,backing_unchanged=True,flag_lyric_timing_checks=checks,save_reopen_verified=True,
             track_ids=created,reference_track_id=rid,exe=str(exe))
 (work/'project-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 (work/'final-snapshot.json').write_text(json.dumps(final,ensure_ascii=False),encoding='utf-8')
 (work/'harmony-corroborated.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps(report,ensure_ascii=True),flush=True)
finally:
 if c:c.p.terminate();c.p.wait(10)
 proc.terminate();proc.wait(10);log.close()

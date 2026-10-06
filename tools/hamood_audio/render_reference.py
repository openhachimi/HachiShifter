from pathlib import Path
import json,sys,time
sys.path.insert(0,str(Path(__file__).parent));from client import Client
repo=Path(__file__).resolve().parents[2];work=repo.parent/'HAMOOD-原唱参考试作';s=json.loads((work/'session.json').read_text(encoding='utf-8'))
d=json.loads((work/'final-snapshot.json').read_text(encoding='utf-8'));r=json.loads((work/'candidate-restoration.json').read_text(encoding='utf-8'));hid=r['harmony_track_id']
c=Client(s['executable'],'--mcp','--roots='+str(repo.parent),timeout=650)
try:
 c.rpc('initialize',{});c.call('project_open',{'path':s['project_path']})
 c.call('set_utau_resampler',{'path':str(repo.parent/'HachiShifter-整合版-0.2.3-DiffSinger/engines/WCSNDM-0.0803.exe')})
 lead=d['tracks'][0];back=next(t for t in d['tracks'] if t['accompaniment'] and '无和声' in t['clips'][0]['source_file'])
 for t in d['tracks']:
  if t['id'] not in [lead['id'],back['id'],hid]:c.call('remove_track',{'track_id':t['id']})
 # A disposable render document contains just this 22-second audition phrase.
 for t in [lead,next(t for t in d['tracks'] if t['id']==hid)]:
  for clip in t['clips']:
   for n in clip['notes']:
    if n['start_seconds']+n['duration_seconds']<44 or n['start_seconds']>68:c.call('remove_note',{'note_id':n['id']})
 print('rendering 44-68 seconds',flush=True)
 for name,track in [('原唱参考-和声候选试听.wav',None),('原唱参考-候选和声单独.wav',hid)]:
  args=dict(path=str(work/name),from_seconds=45,to_seconds=67,timeout_seconds=600)
  if track:args['track_id']=track
  print(c.call('export_wav',args),flush=True)
finally:c.p.terminate();c.p.wait(10)

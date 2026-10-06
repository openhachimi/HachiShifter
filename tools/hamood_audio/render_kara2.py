from pathlib import Path
import sys,json
from client import Client
repo=Path(__file__).resolve().parents[2];work=repo.parent/'HAMOOD-KARA2和声分析';r=json.loads((work/'project-result.json').read_text(encoding='utf-8'));d=json.loads((work/'final-snapshot.json').read_text(encoding='utf-8'))
c=Client(r['exe'],'--mcp','--roots='+str(repo.parent),timeout=650)
try:
 c.rpc('initialize',{});c.call('project_open',{'path':r['project']})
 c.call('set_utau_resampler',{'path':str(Path(r['exe']).parent/'engines/WCSNDM-0.0803.exe')})
 lead=d['tracks'][0];back=next(t for t in d['tracks'] if t['accompaniment'] and '无和声' in t['clips'][0]['source_file']);hid=r['track_ids']['strong']
 for t in d['tracks']:
  if t['id'] not in [lead['id'],back['id'],hid]:c.call('remove_track',{'track_id':t['id']})
 for t in [lead,next(t for t in d['tracks'] if t['id']==hid)]:
  for cl in t['clips']:
   for n in cl['notes']:
    at=cl['start_seconds']+n['start_seconds']
    if at+n['duration_seconds']<44 or at>68:c.call('remove_note',{'note_id':n['id']})
 for name,track in [('试听-45-67秒-翻唱加KARA2候选和声.wav',None),('试听-45-67秒-候选和声单独.wav',hid)]:
  args=dict(path=str(work/name),from_seconds=45,to_seconds=67,timeout_seconds=600,channels=2,sample_rate=48000,bit_depth=24)
  if track:args['track_id']=track
  print(c.call('export_wav',args),flush=True)
finally:c.p.terminate();c.p.wait(10)

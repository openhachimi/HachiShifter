from pathlib import Path
import json, sys, shutil
sys.path.insert(0,str(Path(__file__).parent))
from client import Client
repo=Path(__file__).resolve().parents[2];work=repo.parent/'HAMOOD-原唱参考试作'
s=json.loads((work/'session.json').read_text(encoding='utf-8'))
project=work/'东京泰迪熊-原唱参考对齐.hjpx'
assert not project.exists(),'Refuse to replace an existing trial'
shutil.copy2(s['project_path'],project)
oldcache=Path(s['project_path']+'.hamood-cache')
if oldcache.exists():shutil.copytree(oldcache,Path(str(project)+'.hamood-cache'))
c=Client(s['executable'],'--mcp-live','--session='+s['session_id'])
try:
 c.rpc('initialize',{});status=c.status();assert not status['unsaved'],'New user edits must be preserved'
 c.wait(c.edit('project_open',{'path':str(project)}))
 d=c.call('project_snapshot');assert len(d['tracks'])==5
 (work/'baseline.json').write_text(json.dumps(d,ensure_ascii=False),encoding='utf-8')
 s['project_path']=str(project);(work/'session.json').write_text(json.dumps(s,ensure_ascii=False),encoding='utf-8')
finally:c.p.terminate();c.p.wait(10)
# A separate headless document decodes only the two raw recordings through JUCE.
c=Client(s['executable'],'--mcp','--roots='+str(repo.parent))
try:
 c.rpc('initialize',{});c.call('project_open',{'path':str(project)})
 for t in d['tracks']:
  if not t['accompaniment']:c.call('remove_track',{'track_id':t['id']})
  else:c.call('move_clip',{'clip_id':t['clips'][0]['id'],'start_seconds':0})
 for t in d['tracks']:
  if t['accompaniment']:
   name='reference-editor.wav' if 'B三狼' in t['clips'][0]['source_file'] else 'backing-editor.wav'
   result=c.call('export_wav',{'path':str(work/name),'track_id':t['id'],'from_seconds':0,'to_seconds':t['clips'][0]['duration_seconds'],'timeout_seconds':90})
   print(name,result,flush=True)
finally:c.p.terminate();c.p.wait(10)

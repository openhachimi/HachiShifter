from pathlib import Path
import json,sys
sys.path.insert(0,str(Path('tools/hamood_audio').resolve()));from client import Client
work=Path('../HAMOOD-东京泰迪熊试作').resolve();s=json.loads((work/'session.json').read_text(encoding='utf-8'))
c=Client(s['exe'],'--mcp-live','--session='+s['session_id']);c.rpc('initialize',{})
original_second=None
try:
 data=c.call('project_snapshot');h=next(t for t in data['tracks'] if 'HAMOOD' in t['name']);back=next(t for t in data['tracks'] if t['accompaniment'])
 for label,track in [('东京泰迪熊-HAMOOD和声.wav',h['id']),('backing-editor-float.wav',back['id'])]:
  result=c.wait(c.edit('export_wav',{'path':str(work/label),'track_id':track,'channels':2,'sample_rate':44100,'bit_depth':32,'timeout_seconds':1800}),1810)
  print(label,result,flush=True)
 second=next(t for t in data['tracks'] if t['id'] not in [data['tracks'][0]['id'],h['id'],back['id']]);original_second=second
 c.edit('set_track',{'track_id':second['id'],'muted':True})
 result=c.wait(c.edit('export_wav',{'path':str(work/'mix-editor-float.wav'),'channels':2,'sample_rate':44100,'bit_depth':32,'timeout_seconds':1800}),1810)
 print('mix',result,flush=True)
finally:
 if original_second:c.edit('set_track',{'track_id':original_second['id'],'muted':original_second['muted']})
 c.edit('project_save',{'path':s['project']})
 c.p.terminate();c.p.wait(10)

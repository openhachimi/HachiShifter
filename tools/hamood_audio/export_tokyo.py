from pathlib import Path
import json,sys,time
sys.path.insert(0,str(Path('tools/hamood_audio').resolve()));from client import Client
work=Path('../HAMOOD-东京泰迪熊试作').resolve();s=json.loads((work/'session.json').read_text(encoding='utf-8'))
c=Client(s['exe'],'--mcp-live','--session='+s['session_id']);c.rpc('initialize',{})
try:
 data=c.call('project_snapshot');h=next(t for t in data['tracks'] if 'HAMOOD' in t['name'])
 job=c.edit('export_wav',{'path':str(work/'东京泰迪熊-HAMOOD和声.wav'),'track_id':h['id'],'channels':2,'sample_rate':44100,'bit_depth':24})
 (work/'export-job.json').write_text(json.dumps(job),encoding='utf-8');print(job,flush=True)
except Exception as e:print(repr(e));raise
finally:c.p.terminate();c.p.wait(10)

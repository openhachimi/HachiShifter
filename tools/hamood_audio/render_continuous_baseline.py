"""Render the A/B baseline from project zero, just like the full mixed version."""
from pathlib import Path
import json
from client import Client
BASE=Path(__file__).resolve().parents[3];WORK=BASE/'HAMOOD-连续和声试作'
report=json.loads((WORK/'project-result.json').read_text(encoding='utf-8'))
exe=Path(report['exe']);c=Client(exe,'--mcp','--roots='+str(BASE),timeout=1100)
try:
    c.rpc('initialize',{});c.call('project_open',{'path':report['project']})
    for t in c.call('project_snapshot')['tracks']:
        if t['id'] not in [report['lead_track_id'],report['backing_track_id']]:c.call('remove_track',{'track_id':t['id']})
    c.call('set_utau_resampler',{'path':str(exe.parent/'engines/WCSNDM-0.0803.exe')})
    result=c.call('export_wav',dict(path=str(WORK/'东京泰迪熊-对照主唱与伴奏.wav'),timeout_seconds=1000,
                    channels=2,sample_rate=48000,bit_depth=24))
    (WORK/'baseline-render.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(result,ensure_ascii=True),flush=True)
finally:c.p.terminate();c.p.wait(10)

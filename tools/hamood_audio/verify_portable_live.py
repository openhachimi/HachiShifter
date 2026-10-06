"""Check relative runtime resolution from a relocated, isolated editor window."""
from pathlib import Path
import os, sys, json, subprocess, time
from client import Client
repo = Path(__file__).resolve().parents[2]
work = repo.parent/'HAMOOD-KARA2验证/portable-live'
work.mkdir(parents=True, exist_ok=True)
package = repo.parent/'配布预检-037/HachiShifter-0.2.3-Windows-x64'
exe = package/'HachiShifter Next.exe'
env = dict(os.environ, HACHI_TEST_SETTINGS_DIR=str(work/'settings'),
           PATH=os.environ['SystemRoot']+'/System32', PYTHONNOUSERSITE='1')
for key in ['HACHI_HAMOOD_RUNTIME','PYTHONPATH','PYTHONHOME']:
    env.pop(key,None)
started = time.time()
startup = subprocess.STARTUPINFO()
startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
log = (work/'editor.log').open('w',encoding='utf-8')
proc = subprocess.Popen([str(exe)],env=env,cwd=work,stdout=log,stderr=log,
                        startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW)
client = None
try:
    registry = Path(os.environ['TEMP'])/'HachiShifter-MCP'
    sid = None
    while time.time()-started < 45:
        for f in registry.glob('*/session.json'):
            try:
                data = json.loads(f.read_text(encoding='utf-8-sig'))
                if data['heartbeat_ms']/1000 > started and Path(data['executable']).resolve() == exe.resolve():
                    sid=data['session_id'];break
            except (OSError,ValueError,KeyError):pass
        if sid:break
        assert proc.poll() is None, proc.returncode
        time.sleep(.15)
    assert sid, 'No test session found'
    client=Client(exe,'--mcp-live','--session='+sid)
    client.rpc('initialize',{})
    client.edit('add_track',{'name':'Portable runtime verification','accompaniment':True})
    track=client.call('project_snapshot')['tracks'][-1]
    client.wait(client.edit('import_audio',{'path':str(work.parent/'input-22s.wav'),'track_id':track['id']}))
    client.edit('project_save',{'path':str(work/'test.hjpx')})
    before=client.call('project_snapshot')
    clip=before['tracks'][-1]['clips'][0]
    result=client.wait(client.call('hamood_analyse_audio',{'clip_id':clip['id'],'force':True}),300)
    assert result['ok'] and result['beat_count']>0 and result['chord_count']>0,result
    assert client.call('project_snapshot')['tracks']==before['tracks']
    report=dict(ok=True,relocated_exe=str(exe),relative_runtime=True,path_excludes_anaconda=True,
                no_runtime_override=True,project_unchanged=True,result=result)
    (work/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=True),flush=True)
finally:
    if client:
        client.p.terminate();client.p.wait(10)
    proc.terminate();proc.wait(10);log.close()

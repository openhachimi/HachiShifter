"""Verify the installed runtime in its own visible, explicitly selected window."""
from pathlib import Path
import os, sys, subprocess, json, time
sys.path.insert(0, str(Path(__file__).parent.resolve()))
from client import Client

repo = Path(__file__).resolve().parents[2]
work = repo.parent / 'HAMOOD-东京泰迪熊试作'
exe = repo.parent / 'HachiShifter-整合版-0.2.3-DiffSinger/HachiShifter Next.exe'
project = work / '东京泰迪熊-HAMOOD试作.hjpx'
env = dict(os.environ, HACHI_TEST_SETTINGS_DIR=str(work / 'settings'))
env.pop('HACHI_HAMOOD_RUNTIME', None)
started = time.time()
with (work / 'installed-editor.log').open('w', encoding='utf-8') as log:
    p = subprocess.Popen([str(exe)], env=env, stdout=log, stderr=log,
                         creationflags=subprocess.CREATE_NO_WINDOW)
registry = Path(os.environ['TEMP']) / 'HachiShifter-MCP'
sid = None
while time.time()-started < 45:
    for f in registry.glob('*/session.json'):
        try:
            v = json.loads(f.read_text(encoding='utf-8-sig'))
            if v['heartbeat_ms']/1000 > started and v.get('project_path','') == '' and v['executable'].replace('\\','/').lower() == str(exe).replace('\\','/').lower():
                sid = v['session_id']; break
        except (OSError, ValueError): pass
    if sid: break
    time.sleep(.1)
assert sid, 'No installed session'
c = Client(exe, '--mcp-live', '--session='+sid)
state = {'pid':p.pid, 'session_id':sid, 'exe':str(exe), 'project':str(project)}
(work/'installed-session.json').write_text(json.dumps(state,ensure_ascii=False),encoding='utf-8')
try:
    c.rpc('initialize',{})
    c.wait(c.edit('project_open',{'path':str(project)}))
    data = c.call('project_snapshot')
    backing = next(t for t in data['tracks'] if t['accompaniment'])['clips'][0]
    harmony = next(t for t in data['tracks'] if 'HAMOOD' in t['name'])
    c.edit('editor_select', {'track_id':harmony['id'], 'note_ids':[]})
    result = c.wait(c.call('hamood_analyse_audio', {'clip_id':backing['id'], 'force':True}), 300)
    assert result['ok'] and result['chord_count'] == 177 and result['beat_count'] == 613, result
    context = c.call('hamood_audio_context', {'clip_id':backing['id'], 'whole_clip':True, 'limit':256})
    (work/'audio-context-final.json').write_text(json.dumps(context,ensure_ascii=False,indent=2),encoding='utf-8')
    assert c.call('project_snapshot')['tracks'] == data['tracks']
    report = dict(ok=True, runtime_override_used=False, exe=str(exe), analysis=result,
                  project_unchanged=True, source='engines/hamood beside installed executable')
    (work/'installed-runtime-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=True),flush=True)
finally:
    c.p.terminate(); c.p.wait(10)

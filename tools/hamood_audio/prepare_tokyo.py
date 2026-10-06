from pathlib import Path
import os,sys,subprocess,json,time,shutil
sys.path.insert(0,str(Path('tools/hamood_audio').resolve()))
from client import Client
work=Path('../HAMOOD-东京泰迪熊试作').resolve();work.mkdir(exist_ok=True)
original=Path('F:/鬼畜/新UI/【调教用】东京泰迪熊.hjpx')
project=work/'东京泰迪熊-HAMOOD试作.hjpx'
if not project.exists():shutil.copy2(original,project)
exe=Path('build-integrated/HachiShifterNext_artefacts/RelWithDebInfo/HachiShifter Next.exe').resolve()
env=dict(os.environ,HACHI_TEST_SETTINGS_DIR=str(work/'settings'),HACHI_HAMOOD_RUNTIME=str(Path('tools/hamood_audio/runtime.json').resolve()))
log=(work/'editor.log').open('w',encoding='utf-8')
p=subprocess.Popen([str(exe)],env=env,stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
registry=Path(os.environ['TEMP'])/'HachiShifter-MCP';start=time.time();sid=None
while time.time()-start<40:
 for f in registry.glob('*/session.json'):
  try:
   v=json.loads(f.read_text(encoding='utf-8-sig'))
   if v['heartbeat_ms']/1000>start and v.get('project_path','')=='' and v['executable'].replace('\\','/').lower()==str(exe).replace('\\','/').lower():sid=v['session_id'];break
  except (OSError,ValueError):pass
 if sid:break
 time.sleep(.1)
assert sid,'No session'
c=Client(exe,'--mcp-live','--session='+sid)
try:
 c.rpc('initialize',{})
 c.wait(c.edit('project_open',{'path':str(project)}))
 c.edit('set_utau_resampler',{'path':str(Path('../HachiShifter-整合版-0.2.3-DiffSinger/engines/WCSNDM-0.0803.exe').resolve())})
 state={'pid':p.pid,'session_id':sid,'exe':str(exe),'project':str(project),'original':str(original)}
 (work/'session.json').write_text(json.dumps(state,ensure_ascii=False),encoding='utf-8')
 (work/'baseline.json').write_text(json.dumps(c.call('project_snapshot'),ensure_ascii=False),encoding='utf-8')
 print(json.dumps(c.status(),ensure_ascii=True))
finally:c.p.terminate();c.p.wait(10)

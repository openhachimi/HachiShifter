"""Project boundary and recovery checks against an isolated real GUI session."""
import os, pathlib, sys, subprocess, json, time
work=pathlib.Path(sys.argv[2]).resolve();work.mkdir(parents=True,exist_ok=True)
os.environ['HACHI_TEST_SETTINGS_DIR']=str(work/'settings')
os.environ['TEMP']=os.environ['TMP']=str(work/'tmp');(work/'tmp').mkdir(exist_ok=True)
# Reuse the existing session-bound MCP client; every process has a private registry.
helpers=(pathlib.Path(__file__).parent/'mcp_live_smoke.py').read_text(encoding='utf-8').split('\ntry:\n',1)[0]
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=subprocess.SW_HIDE
helpers=helpers.replace("env=dict(os.environ,HACHI_MCP_TRACE='1'))", "env=dict(os.environ,HACHI_MCP_TRACE='1'),startupinfo=si)")
exec(helpers)
try:
    sid=window();c=Client('--mcp-live','--session='+sid);c.rpc('initialize',{})
    check(c.status()['session_id']==sid,'isolated GUI session bound')
    c.edit('add_track',{'name':'DOCUMENT-A'});c.edit('project_save',{'path':str(WORK/'a.hjpx')})
    c.edit('project_new');check(not c.status()['can_undo'] and not c.status()['can_redo'],'live new clears both histories')
    c.edit('add_track',{'name':'DOCUMENT-B'});c.edit('project_save',{'path':str(WORK/'b.hjpx')})
    c.wait(c.edit('project_open',{'path':str(WORK/'a.hjpx')}))
    c.edit('add_track',{'name':'UNSAVED-A'});before=c.status()
    r=c.raw('project_open',{'path':str(WORK/'b.hjpx'),'expected_revision':before['revision']})
    check(r.get('isError') and c.status()['revision']==before['revision'],'opening without discard leaves unsaved A intact')
    c.wait(c.edit('project_open',{'path':str(WORK/'b.hjpx'),'discard_unsaved':True}))
    st=c.status();check(not st['can_undo'] and not st['can_redo'] and st['project_path']==str(WORK/'b.hjpx'),'live open B commits a new undo boundary and correct path')
    r=c.raw('undo',{'expected_revision':st['revision']})
    check([x['name'] for x in c.call('project_snapshot')['tracks']]==['DOCUMENT-B'],'undo cannot restore A into B')
    c.edit('add_track',{'name':'B-edit'});c.edit('undo')
    check([x['name'] for x in c.call('project_snapshot')['tracks']]==['DOCUMENT-B'] and not c.status()['can_undo'],'ordinary live edits undo within B')
    before=c.status();job=c.edit('project_open',{'path':str(WORK/'missing.hjpx'),'discard_unsaved':True})
    try:c.wait(job);raise AssertionError('missing project unexpectedly opened')
    except AssertionError as e:
        if 'unexpectedly' in str(e):raise
    st=c.status();check(st['revision']==before['revision'] and st['project_path']==before['project_path'] and st['can_redo']==before['can_redo'],'failed background open retains document and redo')
    c.edit('redo');a_bytes=(WORK/'a.hjpx').read_bytes();b_bytes=(WORK/'b.hjpx').read_bytes()
    c.edit('project_save',{'path':str(WORK/'b.hjpx')})
    check((WORK/'b.backup.hjpx').read_bytes()==b_bytes and (WORK/'a.hjpx').read_bytes()==a_bytes,'live save backs up B without touching A')
    c.edit('add_track',{'name':'UNSAVED-RECOVERY'});manual=(WORK/'b.hjpx').read_bytes()
    deadline=time.monotonic()+40;recovery=[]
    while time.monotonic()<deadline:
        recovery=list((WORK/'settings'/'Recovery').glob('*.hjpx'))
        if recovery and recovery[0].stat().st_size>0:break
        time.sleep(.25)
    check(len(recovery)==1,'normal GUI timer writes automatic recovery within 40 seconds')
    check((WORK/'b.hjpx').read_bytes()==manual,'automatic recovery does not overwrite manual save')
    owned[0][0].terminate();owned[0][0].wait(15)
    check(recovery[0].is_file(),'simulated GUI crash leaves recoverable snapshot')
    sid2=window();c2=Client('--mcp-live','--session='+sid2);c2.rpc('initialize',{});time.sleep(.75)
    st=c2.status();r=c2.raw('project_new',{'expected_revision':st['revision'],'discard_unsaved':True})
    check(r.get('isError') and 'EDITOR_MODAL' in r['content'][0]['text'],'next GUI startup presents recovery picker')
    log=chr(10).join(p.read_text(encoding='utf-8-sig',errors='replace') for p in (WORK/'tmp'/'HachiShifterNext').glob('startup-*.log'))
    check('发现可恢复的工程' in log or 'Recoverable projects' in log,'startup modal is specifically the recovery picker')
    check(recovery[0].is_file(),'startup never silently discards recovery')
    (WORK/'report.json').write_text(json.dumps({'ok':True,'checks':checks},ensure_ascii=False,indent=2),encoding='utf-8')
    print('TOTAL',len(checks),flush=True)
finally:
    for client in clients:
        if client.p.poll() is None:client.p.terminate()
        client.p.wait(15)
    for p,log in owned:
        if p.poll() is None:p.terminate()
        p.wait(15);log.close()
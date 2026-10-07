"""Validate OTO envelope controls through a private editor MCP session."""
import os,pathlib,sys,subprocess,json,time
work=pathlib.Path(sys.argv[2]).resolve();work.mkdir(parents=True,exist_ok=True)
os.environ['HACHI_TEST_SETTINGS_DIR']=str(work/'settings');os.environ['TEMP']=os.environ['TMP']=str(work/'tmp');(work/'tmp').mkdir(exist_ok=True)
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=subprocess.SW_HIDE
helpers=(pathlib.Path(__file__).parent/'mcp_live_smoke.py').read_text(encoding='utf-8').split('\ntry:\n',1)[0]
helpers=helpers.replace("env=dict(os.environ,HACHI_MCP_TRACE='1'))","env=dict(os.environ,HACHI_MCP_TRACE='1'),startupinfo=si)");exec(helpers)
try:
    sid=window();c=Client('--mcp-live','--session='+sid);c.rpc('initialize',{})
    tool=next(x for x in c.rpc('tools/list')['result']['tools'] if x['name']=='set_note');check(tool['inputSchema']['properties']['tail_fade']['enum']==['off','linear','smooth'],'MCP schema exposes advanced envelope shapes')
    fixture=WORK/'fixture.ust';fixture.write_text('[#VERSION]\nUST Version1.2\n[#SETTING]\nTempo=120\nTracks=1\nProjectName=Tail envelope test\n[#0000]\nLength=480\nLyric=a\nNoteNum=60\nFlags=B25g-10\n[#0001]\nLength=480\nLyric=a\nNoteNum=62\n[#TRACKEND]\n',encoding='utf-8')
    c.wait(c.edit('import_ust',{'path':str(fixture)}));track=c.call('project_snapshot')['tracks'][0];a,b=track['clips'][0]['notes'];note_id=a['id'];c.edit('editor_select',{'track_id':track['id'],'note_ids':[note_id]})
    check(a['tail_fade']=='off','import defaults to no advanced envelope')
    c.edit('set_note',{'note_id':note_id,'tail_fade':'linear'});selected=c.call('editor_selection')['notes'];check(len(selected)==1 and selected[0]['tail_fade']=='linear','live selection exposes selected fade mode')
    notes=c.call('project_snapshot')['tracks'][0]['clips'][0]['notes'];check(notes[1]['tail_fade']=='off' and notes[0]['utau_flags']==a['utau_flags'],'setting one note preserves other notes and flags')
    c.edit('undo');check(c.call('project_snapshot')['tracks'][0]['clips'][0]['notes'][0]['tail_fade']=='off','one undo restores fade mode');c.edit('redo');check(c.call('project_snapshot')['tracks'][0]['clips'][0]['notes'][0]['tail_fade']=='linear','redo restores fade mode')
    before=c.status();r=c.raw('set_note',{'note_id':note_id,'tail_fade':'invalid','gain':.2,'expected_revision':before['revision']});check(r.get('isError') and c.status()['revision']==before['revision'],'invalid mode does not partially change note')
    c.edit('set_note',{'note_id':note_id,'tail_fade':'smooth'});saved=WORK/'tail.hjpx';c.edit('project_save',{'path':str(saved)});c.edit('project_new');c.wait(c.edit('project_open',{'path':str(saved)}));check(c.call('project_snapshot')['tracks'][0]['clips'][0]['notes'][0]['tail_fade']=='smooth','save and reopen retain smooth fade')
    ds=WORK/'ds-bank';ds.mkdir();(ds/'dsconfig.yaml').write_text('# DS identity',encoding='utf-8');c.edit('set_track',{'track_id':track['id'],'voicebank_directory':str(ds)})
    before=c.status();r=c.raw('set_note',{'note_id':note_id,'tail_fade':'linear','expected_revision':before['revision']});check(r.get('isError') and c.status()['revision']==before['revision'],'DS without OTO rejects effect')
    c.edit('project_save',{'path':str(WORK/'final.hjpx')})
    (WORK/'report.json').write_text(json.dumps({'ok':True,'checks':checks},ensure_ascii=False,indent=2),encoding='utf-8');print('TOTAL',len(checks),flush=True)
finally:
    for client in clients:
        if client.p.poll() is None:client.p.terminate()
        client.p.wait(15)
    for p,log in owned:
        if p.poll() is None:p.terminate()
        p.wait(15);log.close()

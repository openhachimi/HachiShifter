"""Exercise persistent HAMOOD context through a private real editor session."""
import os,pathlib,sys,subprocess,json,time,copy
work=pathlib.Path(sys.argv[2]).resolve();work.mkdir(parents=True,exist_ok=True)
os.environ['HACHI_TEST_SETTINGS_DIR']=str(work/'settings');os.environ['TEMP']=os.environ['TMP']=str(work/'tmp');(work/'tmp').mkdir(exist_ok=True)
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=subprocess.SW_HIDE
helpers=(pathlib.Path(__file__).parent/'mcp_live_smoke.py').read_text(encoding='utf-8').split('\ntry:\n',1)[0]
helpers=helpers.replace("env=dict(os.environ,HACHI_MCP_TRACE='1'))","env=dict(os.environ,HACHI_MCP_TRACE='1'),startupinfo=si)");exec(helpers)
try:
    sid=window();c=Client('--mcp-live','--session='+sid);c.rpc('initialize',{})
    tools=c.rpc('tools/list')['result']['tools'];check({'hamood_get_context','hamood_set_context'}<={x['name'] for x in tools},'context tools discoverable')
    before=c.status();empty=c.call('hamood_get_context');check(empty['schema']==1 and empty['chords']==[] and c.status()['revision']==before['revision'],'context read is side-effect-free')
    context={'schema':1,'settings':{'key_mode':'manual','tonic':0,'minor':False,'section_bars':8,'voices':[2],'preserve_pitch':True,'gain_db':-6,'use_chords':True,'manual_sections':[{'start_bar':1,'end_bar':8,'tonic':0,'minor':False,'confirmed':True}]},'chords':[{'id':'chord','start_quarter':0,'end_quarter':32,'label':'Cm','pitch_classes':[0,3,7],'score':1,'confirmed':True}],'sections':[{'id':'section','start_quarter':0,'end_quarter':32,'label':'副歌','confirmed':True}],'key_predictions':[]}
    c.edit('hamood_set_context',{'context':context});check(c.call('hamood_get_context')==context,'manual context applied intact')
    c.edit('undo');check(c.call('hamood_get_context')['chords']==[],'one undo restores previous context');c.edit('redo');check(c.call('hamood_get_context')==context,'redo restores context')
    c.edit('project_save',{'path':str(WORK/'context.hjpx')});c.edit('project_new');check(c.call('hamood_get_context')['chords']==[],'new project does not inherit old context')
    c.wait(c.edit('project_open',{'path':str(WORK/'context.hjpx')}));check(c.call('hamood_get_context')==context,'save and reopen retain keys chords sections confirmations')
    bad=copy.deepcopy(context);bad['chords'][0]['end_quarter']=-1;rev=c.status()['revision'];r=c.raw('hamood_set_context',{'context':bad,'expected_revision':rev})
    check(r.get('isError') and c.status()['revision']==rev and c.call('hamood_get_context')==context,'invalid context cannot partially modify project')
    r=c.raw('hamood_set_context',{'context':context,'expected_revision':rev-1});check(r.get('isError') and c.status()['revision']==rev,'stale context writes are rejected')
    fixture=WORK/'melody.ust';fixture.write_text('[#VERSION]\nUST Version1.2\n[#SETTING]\nTempo=120\nTracks=1\nProjectName=HAMOOD context test\n[#0000]\nLength=480\nLyric=a\nNoteNum=60\n[#0001]\nLength=480\nLyric=a\nNoteNum=62\n[#TRACKEND]\n',encoding='utf-8')
    c.wait(c.edit('import_ust',{'path':str(fixture)}));track=c.call('project_snapshot')['tracks'][0];note_ids=[n['id'] for cl in track['clips'] for n in cl['notes']]
    c.edit('editor_select',{'track_id':track['id'],'note_ids':note_ids});status=c.status();check(status['selected_note_count']==2 and status['selected_region_count']==1,'live selection reports note and region counts')
    before=c.status();plan=c.call('hamood_preview',{'track_id':track['id'],'whole_track':True});check(plan['key_mode']=='manual' and plan['audio_covered_notes']==2 and plan['audio_adjusted_targets']>0,'preview inherits saved keys and uses saved chords')
    check(c.status()['revision']==before['revision'] and c.call('hamood_get_context')==context,'preview does not mutate saved analysis')
    c.edit('hamood_generate',{'track_id':track['id'],'whole_track':True});check(len(c.call('project_snapshot')['tracks'])==2 and c.call('hamood_get_context')['key_predictions'],'generation saves used analysis with new tracks')
    check(c.call('hamood_get_context')['chords']==context['chords'],'generation preserves human chord corrections')
    c.edit('undo');check(len(c.call('project_snapshot')['tracks'])==1 and c.call('hamood_get_context')==context,'one undo removes generation and analysis update together')
    c.edit('project_save',{'path':str(WORK/'final.hjpx')})
    (WORK/'report.json').write_text(json.dumps({'ok':True,'checks':checks},ensure_ascii=False,indent=2),encoding='utf-8');print('TOTAL',len(checks),flush=True)
finally:
    for client in clients:
        if client.p.poll() is None:client.p.terminate()
        client.p.wait(15)
    for p,log in owned:
        if p.poll() is None:p.terminate()
        p.wait(15);log.close()
"""Validate detailed tail-fade controls through a private editor MCP session."""
import os,pathlib,sys,subprocess,json,time
work=pathlib.Path(sys.argv[2]).resolve();work.mkdir(parents=True,exist_ok=True)
os.environ['HACHI_TEST_SETTINGS_DIR']=str(work/'settings');os.environ['TEMP']=os.environ['TMP']=str(work/'tmp');(work/'tmp').mkdir(exist_ok=True)
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=subprocess.SW_HIDE
helpers=(pathlib.Path(__file__).parent/'mcp_live_smoke.py').read_text(encoding='utf-8').split('\ntry:\n',1)[0]
helpers=helpers.replace("env=dict(os.environ,HACHI_MCP_TRACE='1'))","env=dict(os.environ,HACHI_MCP_TRACE='1'),startupinfo=si)");exec(helpers)
try:
    sid=window();c=Client('--mcp-live','--session='+sid);c.rpc('initialize',{})
    props=next(x for x in c.rpc('tools/list')['result']['tools'] if x['name']=='set_note')['inputSchema']['properties']
    fields=['tail_fade_start_percent','tail_fade_end_percent','tail_fade_start_gain_percent','tail_fade_end_gain_percent','tail_fade_curve_power']
    check(props['tail_fade']['enum']==['off','linear','smooth'] and all(props[x]['type']=='number' for x in fields),'MCP schema exposes detailed tail fade settings')
    fixture=WORK/'fixture.ust';fixture.write_text('[#VERSION]\nUST Version1.2\n[#SETTING]\nTempo=120\nTracks=1\nProjectName=Tail envelope test\n[#0000]\nLength=480\nLyric=a\nNoteNum=60\nFlags=B25g-10\n[#0001]\nLength=480\nLyric=a\nNoteNum=62\n[#TRACKEND]\n',encoding='utf-8')
    c.wait(c.edit('import_ust',{'path':str(fixture)}));track=c.call('project_snapshot')['tracks'][0];a,b=track['clips'][0]['notes'];note_id=a['id'];c.edit('editor_select',{'track_id':track['id'],'note_ids':[note_id]})
    read=lambda:c.call('project_snapshot')['tracks'][0]['clips'][0]['notes'][0]
    defaults=dict(zip(fields,[0,100,100,0,1]));values=dict(zip(fields,[20,80,80,15,2]))
    check(a['tail_fade']=='off' and all(a[k]==v for k,v in defaults.items()),'import defaults preserve original no-fade behavior')
    c.edit('set_note',dict(note_id=note_id,tail_fade='smooth',**values));note=read()
    check(note['tail_fade']=='smooth' and all(note[k]==v for k,v in values.items()),'all detailed settings apply together')
    selected=c.call('editor_selection')['notes'];check(len(selected)==1 and all(selected[0][k]==v for k,v in values.items()),'selection readback includes exact detailed values')
    notes=c.call('project_snapshot')['tracks'][0]['clips'][0]['notes'];check(notes[1]['tail_fade']=='off' and notes[0]['utau_flags']==a['utau_flags'],'editing one note preserves other note and flags')
    c.edit('undo');note=read();check(note['tail_fade']=='off' and all(note[k]==v for k,v in defaults.items()),'one undo restores mode and every parameter')
    c.edit('redo');check(all(read()[k]==v for k,v in values.items()),'redo restores every parameter')
    for patch,message in [({'tail_fade_start_percent':90,'tail_fade_end_percent':20},'reversed range'),({'tail_fade_end_gain_percent':120},'rising gain'),({'tail_fade_curve_power':0},'invalid curve power'),({'tail_fade_start_percent':-1},'negative range')]:
        before=c.status();r=c.raw('set_note',dict(note_id=note_id,gain=.2,expected_revision=before['revision'],**patch));check(r.get('isError') and c.status()['revision']==before['revision'] and read()['gain']==note['gain'],message+' rejected without partial edits')
    c.edit('set_note',{'note_id':note_id,'tail_fade':'off'});note=read();check(note['tail_fade']=='off' and all(note[k]==v for k,v in values.items()),'disable retains detailed settings')
    c.edit('set_note',{'note_id':note_id,'tail_fade':'linear'});check(all(read()[k]==v for k,v in values.items()),'shape-only change retains detailed settings')
    c.edit('set_note',{'note_id':note_id,'tail_fade_curve_power':1.5});values['tail_fade_curve_power']=1.5;check(read()['tail_fade']=='linear' and read()['tail_fade_curve_power']==1.5,'parameter-only edit preserves enabled shape')
    saved=WORK/'tail.hjpx';c.edit('project_save',{'path':str(saved)});c.edit('project_new');c.wait(c.edit('project_open',{'path':str(saved)}));note=read();check(note['tail_fade']=='linear' and all(note[k]==v for k,v in values.items()),'save and reopen retain all detailed settings')
    bezier={'tail_fade_custom_curve':True,'tail_fade_control1_time_percent':15,'tail_fade_control1_progress_percent':70,'tail_fade_control2_time_percent':80,'tail_fade_control2_progress_percent':95}
    check(props['tail_fade_custom_curve']['type']=='boolean' and all(props[k]['type']=='number' for k in bezier if k!='tail_fade_custom_curve'),'MCP schema exposes Bezier controls')
    c.edit('set_note',dict(note_id=note_id,tail_fade='smooth',**bezier));check(all(read()[k]==v for k,v in bezier.items()),'Bezier controls apply and read back')
    c.edit('editor_select',{'track_id':track['id'],'note_ids':[note_id]});check(all(c.call('editor_selection')['notes'][0][k]==v for k,v in bezier.items()),'selection exposes Bezier controls')
    for patch in [{'tail_fade_control1_time_percent':90},{'tail_fade_control2_progress_percent':60},{'tail_fade_control1_time_percent':-1}]:
        before=c.status();r=c.raw('set_note',dict(note_id=note_id,gain=.2,expected_revision=before['revision'],**patch));check(r.get('isError') and c.status()['revision']==before['revision'],'invalid Bezier rejected atomically')
    c.edit('undo');check(not read()['tail_fade_custom_curve'],'Bezier undo');c.edit('redo');check(all(read()[k]==v for k,v in bezier.items()),'Bezier redo')
    c.edit('project_save',{'path':str(saved)});c.edit('project_new');c.wait(c.edit('project_open',{'path':str(saved)}));check(all(read()[k]==v for k,v in bezier.items()),'Bezier project roundtrip')
    head={'head_envelope':'curve','head_envelope_custom_curve':True,'head_envelope_start_percent':10,'head_envelope_end_percent':90,'head_envelope_start_gain_percent':10,'head_envelope_end_gain_percent':110,'head_envelope_control1_time_percent':20,'head_envelope_control1_progress_percent':40,'head_envelope_control2_time_percent':70,'head_envelope_control2_progress_percent':80}
    check(a['head_envelope']=='off' and props['head_envelope']['enum']==['off','linear','curve'],'old notes default head off and MCP schema exposes head')
    same_head=lambda n:all(n[k]==v if isinstance(v,(str,bool)) else abs(n[k]-v)<1e-8 for k,v in head.items())
    prior=read();c.edit('set_note',dict(note_id=note_id,**head));check(same_head(read()) and all(read()[k]==prior[k] for k in bezier),'head edit preserves tail')
    c.edit('editor_select',{'track_id':track['id'],'note_ids':[note_id]});check(same_head(c.call('editor_selection')['notes'][0]),'selection exposes head parameters')
    c.edit('undo');check(read()['head_envelope']=='off','head undo');c.edit('redo');check(same_head(read()),'head redo')
    for patch in [{'head_envelope_start_percent':95},{'head_envelope_start_gain_percent':201},{'head_envelope_control1_time_percent':90}]:
        before=c.status();r=c.raw('set_note',dict(note_id=note_id,gain=.2,expected_revision=before['revision'],**patch));check(r.get('isError') and c.status()['revision']==before['revision'],'invalid head rejected atomically')
    c.edit('set_note',{'note_id':note_id,'tail_fade':'off'});check(read()['head_envelope']=='curve','head works with tail disabled')
    c.edit('project_save',{'path':str(saved)});c.edit('project_new');c.wait(c.edit('project_open',{'path':str(saved)}));check(same_head(read()),'head project roundtrip')
    ds=WORK/'ds-bank';ds.mkdir();(ds/'dsconfig.yaml').write_text('# DS identity',encoding='utf-8');c.edit('set_track',{'track_id':track['id'],'voicebank_directory':str(ds)})
    before=c.status();r=c.raw('set_note',{'note_id':note_id,'tail_fade_curve_power':2,'expected_revision':before['revision']});check(r.get('isError') and c.status()['revision']==before['revision'],'DS rejects parameter-only OTO effect edit')
    (WORK/'report.json').write_text(json.dumps({'ok':True,'checks':checks},ensure_ascii=False,indent=2),encoding='utf-8');print('TOTAL',len(checks),flush=True)
finally:
    for client in clients:
        if client.p.poll() is None:client.p.terminate()
        client.p.wait(15)
    for p,log in owned:
        if p.poll() is None:p.terminate()
        p.wait(15);log.close()

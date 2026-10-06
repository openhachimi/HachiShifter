"""HAMOOD tests in an isolated live window; reuses the existing MCP test client."""
from pathlib import Path
exec(Path(__file__).with_name('mcp_live_smoke.py').read_text(encoding='utf-8').split('\ntry:\n',1)[0])
try:
    sid=window();c=Client('--mcp-live','--session='+sid);c.rpc('initialize',{})
    names={t['name'] for t in c.rpc('tools/list')['result']['tools']}
    check({'hamood_preview','hamood_generate'}<=names,'HAMOOD live schemas discoverable')
    ust=WORK/'lead.ust'
    ust.write_text('[#VERSION]\nUST Version1.2\n[#SETTING]\nTempo=120\nTracks=1\nProjectName=HAMOOD Lead\n'+''.join(f'[#000{i}]\nLength=480\nLyric=la\nNoteNum={p}\nIntensity=100\n' for i,p in enumerate([60,62,64,65,67,69,71,72]))+'[#TRACKEND]\n',encoding='utf-8')
    c.wait(c.edit('import_ust',{'path':str(ust)}))
    snapshot=c.call('project_snapshot');t=snapshot['tracks'][0];ns=t['clips'][0]['notes'];selected=[ns[0]['id'],ns[2]['id']]
    c.edit('editor_select',{'track_id':t['id'],'note_ids':selected})
    options={'key_mode':'manual','tonic':0,'voices':[2,-2]}
    before=c.status();preview=c.call('hamood_preview',options)
    check(preview['note_count']==2 and [n['harmony_midi'] for n in preview['notes']]==[[64,57],[67,60]],'selected notes preview yields diatonic upper/lower thirds')
    check(c.status()['revision']==before['revision'] and len(c.call('project_snapshot')['tracks'])==1,'preview does not mutate live project')
    check(c.raw('hamood_generate',options)['isError'],'generation requires revision guard')
    generated=c.edit('hamood_generate',options);after=c.call('project_snapshot')
    check(len(generated['created_track_ids'])==2 and len(after['tracks'])==3 and c.status()['revision']==before['revision']+1,'generation adds two tracks in one revision')
    check(after['tracks'][0]==snapshot['tracks'][0],'original lead track exactly preserved')
    check(all(len(t['clips'][0]['notes'])==2 for t in after['tracks'][1:]),'unselected notes excluded from new tracks')
    check([n['midi'] for n in after['tracks'][1]['clips'][0]['notes']]==[64,67],'generated notes match preview')
    c.edit('undo');check(len(c.call('project_snapshot')['tracks'])==1,'one live Undo removes generated voices')
    c.edit('redo');check(len(c.call('project_snapshot')['tracks'])==3,'live Redo restores generated voices')
    c.edit('editor_select',{'note_ids':[]})
    check(c.raw('hamood_generate',dict(options,expected_revision=c.status()['revision']))['isError'],'empty selection rejected without fallback')
    preview=c.call('hamood_preview',dict(options,whole_track=True));check(preview['note_count']==8,'explicit whole-track mode includes all source notes')
    saved=WORK/'hamood-live.hjpx';c.edit('project_save',{'path':str(saved)})
    c.edit('project_new');c.wait(c.edit('project_open',{'path':str(saved)}));check(len(c.call('project_snapshot')['tracks'])==3,'harmony survives save and reopen')
    segments=[{'start_bar':1,'end_bar':1,'tonic':0},{'start_bar':2,'end_bar':2,'tonic':0,'minor':True}]
    segmented={'track_id':t['id'],'whole_track':True,'key_mode':'manual','manual_sections':segments,'voices':[2]}
    rev=c.status()['revision']; preview=c.call('hamood_preview',segmented)
    check([(p['start_bar'],p['end_bar'],p['tonic'],p['minor']) for p in preview['passages']]==[(1,1,0,False),(2,2,0,True)],'MCP manual bar segments retain independent keys')
    check([n['harmony_midi'][0] for n in preview['notes']]==[64,65,67,69,70,73,75,75],'manual segments produce expected major and minor thirds, preserving chromatic alterations')
    invalid=[
        [{'start_bar':'1','end_bar':2,'tonic':0}],
        [{'start_bar':1,'end_bar':2}],
        [{'start_bar':1,'end_bar':2,'tonic':0,'minor':'false'}],
        [{'start_bar':1,'end_bar':2,'tonic':0,'unexpected':True}],
        [{'start_bar':1,'end_bar':2,'tonic':0},{'start_bar':2,'end_bar':3,'tonic':7}],
        [segments[0]],
        [{'start_bar':2,'end_bar':1,'tonic':0}],
        [{'start_bar':1,'end_bar':2,'tonic':12}],
    ]
    for i,sections in enumerate(invalid):
        check(c.raw('hamood_generate',dict(segmented,manual_sections=sections,expected_revision=rev))['isError'],f'invalid nested manual section {i} rejected')
    check(c.status()['revision']==rev and len(c.call('project_snapshot')['tracks'])==3,'invalid ranges leave live project unchanged')
    check(c.raw('hamood_preview',dict(segmented,key_mode='auto'))['isError'],'manual sections rejected outside manual mode')
    result=c.edit('hamood_generate',segmented)
    harmony=next(x for x in c.call('project_snapshot')['tracks'] if x['id']==result['created_track_ids'][0])
    check([n['midi'] for n in harmony['clips'][0]['notes']]==[n['harmony_midi'][0] for n in preview['notes']],'live generation matches manual segment preview')
    c.edit('undo');check(len(c.call('project_snapshot')['tracks'])==3,'one Undo removes segmented harmony')
    c.edit('redo');c.edit('project_save',{'path':str(WORK/'hamood-segmented.hjpx')})
    c.edit('project_new');c.wait(c.edit('project_open',{'path':str(WORK/'hamood-segmented.hjpx')}))
    restored=next(x for x in c.call('project_snapshot')['tracks'] if x['id']==harmony['id'])
    check([n['midi'] for n in restored['clips'][0]['notes']]==[n['midi'] for n in harmony['clips'][0]['notes']],'segmented pitches survive save and reopen')
    if len(sys.argv)>3:
        import shutil
        dsfile=WORK/'ds-fixture.hjpx';shutil.copyfile(sys.argv[3],dsfile)
        c.wait(c.edit('project_open',{'path':str(dsfile),'discard_unsaved':True}))
        source=next(t for t in c.call('project_snapshot')['tracks'] if t['diffsinger'])
        if len(sys.argv)>4:c.edit('set_track',{'track_id':source['id'],'voicebank_directory':sys.argv[4]})
        note=next(n for cl in source['clips'] for n in cl['notes'] if n['label'].strip() and n['label'] not in ['SP','AP','R'])
        c.edit('editor_select',{'track_id':source['id'],'note_ids':[note['id']]})
        opts={'key_mode':'manual','tonic':0,'voices':[2]}
        result=c.edit('hamood_generate',opts)
        harmony=next(t for t in c.call('project_snapshot')['tracks'] if t['id']==result['created_track_ids'][0])
        hn=harmony['clips'][0]['notes'][0]
        check(harmony['diffsinger'] and hn['label']==note['label'] and harmony['voicebank_directory']==(sys.argv[4] if len(sys.argv)>4 else source['voicebank_directory']).replace('/','\\'),'generated harmony retains real DS voicebank and lyrics')
        delta=hn['midi']-note['midi']
        check(hn['diffsinger_pitch_offset']==note['diffsinger_pitch_offset'] and all(abs(a[1]-b[1]-delta)<1.e-4 for a,b in zip(hn['diffsinger_pitch_reference'],note['diffsinger_pitch_reference'])),'real DS offsets remain additive while reference transposes')
        c.edit('editor_select',{'track_id':harmony['id'],'note_ids':[hn['id']]})
        c.wait(c.edit('render_prepare',{'wait':True}),180)
        out=WORK/'HAMOOD-DS-upper-third.wav'
        c.wait(c.edit('export_wav',{'path':str(out),'track_id':harmony['id'],'channels':1,'sample_rate':44100,'bit_depth':16}),180)
        with wave.open(str(out),'rb') as f:
            raw=f.readframes(f.getnframes());samples=struct.unpack('<'+'h'*(len(raw)//2),raw)
            check(len(samples)>10000 and max(abs(x) for x in samples)>100,'generated DS harmony renders an audible WAV')
        c.edit('project_save',{'path':str(WORK/'hamood-ds.hjpx')})
    (WORK/'report.json').write_text(json.dumps({'ok':True,'checks':checks,'tools':len(names)},ensure_ascii=False,indent=2),encoding='utf-8')
    print('TOTAL',len(checks),flush=True)
finally:
    for c in clients:
        if c.p.poll() is None:c.p.terminate()
    for p,log in owned:
        if p.poll() is None:p.terminate()
        p.wait(15);log.close()

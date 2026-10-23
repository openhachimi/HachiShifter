"""Exercise real editor windows through --mcp-live; never targets a user's session."""
import json, os, pathlib, queue, subprocess, sys, threading, time, wave, struct, math
EXE=pathlib.Path(sys.argv[1]).resolve()
WORK=pathlib.Path(sys.argv[2]).resolve(); WORK.mkdir(parents=True,exist_ok=True)
REG=pathlib.Path(os.environ['TEMP'])/'HachiShifter-MCP'
checks=[]; owned=[]; clients=[]
def check(value, message):
    assert value, message
    checks.append(message); print('PASS',message,flush=True)
def sessions():
    result={}
    for p in REG.glob('*/session.json'):
        try:
            v=json.loads(p.read_text(encoding='utf-8-sig'))
            if time.time()*1000-v['heartbeat_ms']<15000:result[v['session_id']]=v
        except (OSError,ValueError,KeyError):pass
    return result
class Client:
    def __init__(self,*args):
        self.p=subprocess.Popen([str(EXE),*args],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,encoding='utf-8',creationflags=subprocess.CREATE_NO_WINDOW)
        self.q=queue.Queue();self.seq=0;clients.append(self)
        def reader():
            for line in self.p.stdout:self.q.put(line)
        threading.Thread(target=reader,daemon=True).start()
    def rpc(self,method,params=None):
        self.seq+=1;req=dict(jsonrpc='2.0',id=self.seq,method=method)
        if params is not None:req['params']=params
        self.p.stdin.write(json.dumps(req,ensure_ascii=False)+'\n');self.p.stdin.flush()
        return json.loads(self.q.get(timeout=45))
    def raw(self,name,args=None):
        r=self.rpc('tools/call',dict(name=name,arguments=args or {}));assert 'result' in r,r
        return r['result']
    def call(self,name,args=None):
        r=self.raw(name,args)
        if r.get('isError'):raise AssertionError((name,r))
        text=r['content'][0]['text']
        try:return json.loads(text)
        except ValueError:return text
    def status(self):return self.call('editor_status')
    def edit(self,name,args=None):return self.call(name,dict(args or {},expected_revision=self.status()['revision']))
    def wait(self,value,limit=90):
        if not isinstance(value,dict) or 'job_id' not in value:return value
        end=time.monotonic()+limit
        while time.monotonic()<end:
            r=self.call('editor_job_status',{'job_id':value['job_id']})
            if r['state']!='running':
                if r['state']!='completed' or r['result'].get('isError'):raise AssertionError(r)
                text=r['result']['content'][0]['text']
                try:return json.loads(text)
                except ValueError:return text
            time.sleep(.08)
        raise AssertionError('Job timeout '+str(value))
def window():
    before=set(sessions())
    log=open(WORK/f'window-{len(owned)}.log','w',encoding='utf-8')
    p=subprocess.Popen([str(EXE)],stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW,env=dict(os.environ,HACHI_MCP_TRACE='1'))
    owned.append((p,log));end=time.monotonic()+50
    while time.monotonic()<end:
        after=set(sessions())-before
        if after:return after.pop()
        if p.poll() is not None:raise AssertionError('Editor exited '+str(p.returncode))
        time.sleep(.1)
    raise AssertionError('No editor session published')
try:
    sid=window();c=Client('--mcp-live','--session='+sid)
    initialized=c.rpc('initialize',{});print('INIT',initialized,flush=True)
    check(initialized['result']['serverInfo']['version']=='0.2.3','live initialize keeps 0.2.3')
    tool_list=c.rpc('tools/list')['result']['tools']
    toolnames={t['name'] for t in tool_list}
    check(tool_list[0]['name']=='editor_selection' and 'editor_selection FIRST' in initialized['result']['instructions'],'selection is the first discoverable tool and initialize recommendation')
    check({'editor_select','editor_batch','ds_generate_pitch','ds_generate_parameters','ds_set_pitch_offset','export_wav'}<=toolnames,'live schemas expose editor and DS tools')
    check(c.status()['session_id']==sid,'proxy binds exact GUI session')
    for _ in range(100):assert c.status()['session_id']==sid
    check(True,'100 mailbox requests survive heartbeat and atomic publication')
    rev=c.status()['revision'];r=c.raw('add_track',{'name':'missing guard'})
    check(r['isError'] and c.status()['revision']==rev,'unguarded write rejected without mutation')
    check(c.raw('add_track',{'name':'stale','expected_revision':rev+1})['isError'],'stale revision rejected')
    ust=WORK/'fixture.ust';ust.write_text('[#VERSION]\nUST Version1.2\n[#SETTING]\nTempo=120\nTracks=1\nProjectName=MCP Live Test\n[#0000]\nLength=480\nLyric=ni\nNoteNum=60\nIntensity=100\n[#0001]\nLength=480\nLyric=hao\nNoteNum=62\nIntensity=100\n[#TRACKEND]\n',encoding='utf-8')
    c.wait(c.edit('import_ust',{'path':str(ust)}))
    snap=c.call('project_snapshot');track=snap['tracks'][0];clip=track['clips'][0];n1,n2=clip['notes'][:2]
    check(len(clip['notes'])==2,'background import commits to live document')
    c.edit('editor_select',{'note_ids':[n1['id']]})
    q=c.call('editor_query');check([n['id'] for n in q['notes']]==[n1['id']],'query returns actual GUI selection only')
    check('contour' not in q['notes'][0],'compact query omits dense pitch by default')
    before_read=c.status()
    selected=c.call('editor_selection')
    row=selected['notes'][0]
    check(selected['total']==1 and row['id']==n1['id'] and row['lyric']=='ni' and row['pitch_name']=='C4' and row['track_name']==track['name'],'selection exposes only selected lyric, pitch name and track')
    check(row['project_start_seconds']==clip['start_seconds']+n1['start_seconds'] and row['project_end_seconds']==row['project_start_seconds']+row['duration_seconds'],'selection reports absolute project timing')
    check('ni | C4' in selected['summary'] and 'hao' not in selected['summary'] and 'contour' not in row,'human-readable summary excludes unselected lyrics and dense curves')
    rich=c.call('editor_selection',{'include_curves':True})
    check(len(rich['notes'])==1 and 'contour' in rich['notes'][0],'dense curves are opt-in for selected notes only')
    resources=c.rpc('resources/list')['result']['resources']
    check(resources[0]['uri']=='hachishifter://editor/selection','selection resource is discoverable first')
    resource=json.loads(c.rpc('resources/read',{'uri':resources[0]['uri']})['result']['contents'][0]['text'])
    check(resource['notes']==selected['notes'] and resource['summary']==selected['summary'],'selection resource matches live tool')
    after_read=c.status()
    check(all(after_read[k]==before_read[k] for k in ['revision','selected_note_ids','unsaved','can_undo','playing']),'selection reads do not edit, select, dirty, play or add undo')
    c.edit('editor_select',{'note_ids':[n2['id'],n1['id']]})
    both=c.call('editor_selection',{'limit':1})
    second=c.call('editor_selection',{'offset':both['next_offset'],'limit':1})
    check(both['total']==2 and both['notes'][0]['id']==n1['id'] and second['notes'][0]['id']==n2['id'] and second['next_offset'] is None,'multi-selection is chronological and paginated independent of click order')
    c.edit('editor_select',{'note_ids':[n2['id']]})
    check([n['id'] for n in c.call('editor_selection')['notes']]==[n2['id']],'selection changes are read live without cached previous notes')
    c.edit('editor_select',{'note_ids':[]})
    empty=c.call('editor_selection')
    check(empty['selection_empty'] and empty['total']==0 and empty['notes']==[] and empty['range'] is None and empty['next_offset'] is None,'empty selection explicitly returns zero notes and no project fallback')
    check(c.raw('editor_selection',{'scope':'project'})['isError'],'selection-only entry rejects whole-project scope')
    c.edit('editor_select',{'note_ids':[n1['id']]})
    q=c.call('editor_query',{'scope':'project','limit':1});check(q['total']==2 and q['next_offset']==1,'bounded query pagination')
    before=c.status()['revision']
    c.edit('editor_batch',{'commands':[{'name':'set_note','arguments':{'note_id':n1['id'],'label':'bang'}},{'name':'transpose_note','arguments':{'note_id':n1['id'],'semitones':2}}]})
    q=c.call('editor_query')['notes'][0]
    check(q['label']=='bang' and q['midi']==62 and c.status()['revision']==before+1,'batch commits once to current GUI document')
    c.edit('undo');q=c.call('editor_query')['notes'][0]
    check(q['label']=='ni' and q['midi']==60,'one GUI Undo restores entire batch')
    c.edit('redo');check(c.call('editor_query')['notes'][0]['label']=='bang','GUI Redo replays batch')
    before=c.status()['revision']
    r=c.raw('editor_batch',{'expected_revision':before,'commands':[{'name':'set_note','arguments':{'note_id':n1['id'],'label':'wrong'}},{'name':'remove_note','arguments':{'note_id':'missing'}}]})
    check(r['isError'] and c.status()['revision']==before and c.call('editor_query')['notes'][0]['label']=='bang','failed batch does not partially commit')
    check(c.raw('ds_generate_pitch',{'expected_revision':before})['isError'],'DS command rejects non-DS track')
    check(c.raw('project_new',{'expected_revision':before})['isError'],'unsaved replacement requires explicit discard')
    save=WORK/'saved.hjpx';c.edit('project_save',{'path':str(save)})
    check(save.is_file() and not c.status()['unsaved'] and pathlib.Path(c.status()['project_path'])==save,'save updates real file path and dirty state')
    check(c.status()['project_path']==str(save),'save binds cache lifecycle to the current file')
    c.edit('project_new');check(not c.call('project_snapshot')['tracks'],'new clears current GUI document')
    c.wait(c.edit('project_open',{'path':str(save)}));check(c.call('project_snapshot')['tracks'][0]['clips'][0]['notes'][0]['label']=='bang','reopen restores saved live edits')
    c.edit('editor_select',{'note_ids':[]});check(c.call('editor_query')['notes']==[],'empty selection does not expand to entire track')
    # Two tracks and nonzero clip origins catch selection/range leakage and local-time mistakes.
    c.edit('move_clip',{'clip_id':clip['id'],'start_seconds':7.5})
    c.wait(c.edit('import_ust',{'path':str(ust)}))
    other=next(t for t in c.call('project_snapshot')['tracks'] if t['id']!=track['id'])
    on=other['clips'][0]['notes'][0]
    c.edit('editor_select',{'note_ids':[n1['id'],on['id']]})
    cross=c.call('editor_selection')
    check([r['id'] for r in cross['notes']]==[on['id'],n1['id']] and len(cross['tracks'])==2 and cross['notes'][1]['project_start_seconds']==7.5,'cross-track selection sorts by absolute time after clip movement')
    filtered=c.call('editor_query',{'track_id':track['id'],'from_seconds':7.4,'to_seconds':7.6})
    check(filtered['total']==1 and filtered['notes'][0]['id']==n1['id'],'range and track filters intersect selection rather than including neighbours')
    check(c.raw('editor_query',{'from_seconds':8,'to_seconds':7})['isError'],'inverted time range rejected')
    c.wait(c.edit('project_open',{'path':str(save),'discard_unsaved':True}))
    sid2=window();c2=Client('--mcp-live','--session='+sid2)
    ambiguous=Client('--mcp-live');r=ambiguous.rpc('initialize',{})
    check('error' in r and 'Select a window' in r['error']['message'],'multiple windows require explicit target')
    c2.edit('add_track',{'name':'Window Two'});check(c2.call('project_snapshot')['tracks'][0]['name']=='Window Two','second window has independent model')
    check(c.call('project_snapshot')['tracks'][0]['name']!='Window Two','bound proxy cannot drift to another window')
    owned[1][0].terminate();owned[1][0].wait(15)
    r=c2.rpc('tools/call',{'name':'editor_status','arguments':{}})
    check('error' in r,'closed window returns disconnect, never attaches elsewhere')
    # Export an actual audio clip through the live engine with the requested format.
    c.edit('project_new',{'discard_unsaved':True})
    wav=WORK/'source.wav'
    with wave.open(str(wav),'wb') as f:
        f.setparams((1,2,22050,0,'NONE','not compressed'));f.writeframes(b''.join(struct.pack('<h',int(5000*math.sin(i*2*math.pi*220/22050))) for i in range(11025)))
    c.wait(c.edit('import_audio',{'path':str(wav)}))
    c.wait(c.edit('render_prepare',{'wait':True}))
    out=WORK/'mono.wav';c.wait(c.edit('export_wav',{'path':str(out),'channels':1,'sample_rate':22050,'bit_depth':16}))
    with wave.open(str(out),'rb') as f:check(f.getnchannels()==1 and f.getframerate()==22050 and f.getsampwidth()==2 and f.getnframes()>0,'live WAV export honors channels/rate/depth')
    c.wait(c.edit('transport_play',{'position_seconds':0}));c.call('transport_stop')
    check(not c.status()['playing'],'live transport uses window engine')
    if len(sys.argv)>3:
        import shutil
        dsfile=WORK/'ds-fixture.hjpx';shutil.copyfile(sys.argv[3],dsfile)
        c.wait(c.edit('project_open',{'path':str(dsfile),'discard_unsaved':True}))
        snap=c.call('project_snapshot');dt=next(t for t in snap['tracks'] if t['diffsinger'])
        if len(sys.argv)>4:c.edit('set_track',{'track_id':dt['id'],'voicebank_directory':sys.argv[4]})
        dn=next(n for cl in dt['clips'] for n in cl['notes'] if n['label'].strip() and n['label'] not in ['SP','AP','R'])
        def settled():
            until=time.monotonic()+180
            while time.monotonic()<until:
                st=c.status()
                if not st['rendering'] and not st['ds_busy']:
                    time.sleep(.25)
                    return
                time.sleep(.15)
            raise AssertionError('DS render timeout')
        c.edit('editor_select',{'track_id':dt['id'],'note_ids':[dn['id']]});settled()
        capability=c.wait(c.call('ds_capabilities'),180)
        (WORK/'ds-capabilities.json').write_text(json.dumps(capability,ensure_ascii=False,indent=2),encoding='utf-8')
        check(capability.get('ok') and any(x['key']=='DS:BREC' and x['supported'] for x in capability['expressions']),'real DS voicebank capabilities available')
        original=c.call('editor_query',{'include_curves':True})['notes'][0]
        ds_selection=c.call('editor_selection',{'include_curves':True})['notes'][0]
        check(ds_selection['diffsinger'] and all(ds_selection[k]==original[k] for k in ['diffsinger_pitch_reference','diffsinger_pitch_offset','flag_curves']),'selection preserves exact DS references, offsets and FLAG curves on demand')
        c.edit('ds_set_pitch_offset',{'note_id':dn['id'],'points':[[0,-2],[.3,-1]]})
        edited=c.call('editor_query',{'include_curves':True})['notes'][0]
        check(edited['diffsinger_pitch_offset']==[[0,-2],[.3,-1]] and edited['diffsinger_pitch_reference']==original['diffsinger_pitch_reference'],'DS pitch offset preserves original reference')
        settled()
        c.edit('ds_set_parameter',{'note_id':dn['id'],'flag':'BREC','layer':'offset','points':[[0,15],[.3,30]]})
        check('DS:BREC' in c.call('editor_query',{'include_curves':True})['notes'][0]['flag_curves'],'DS offset edits correct parameter namespace')
        settled()
        c.edit('ds_set_parameter',{'note_id':dn['id'],'flag':'BREC','layer':'actual','points':[[0,-35],[.3,-30]]})
        check('DS:ABS:BREC' in c.call('editor_query',{'include_curves':True})['notes'][0]['flag_curves'],'DS actual parameter remains separate from offset')
        settled()
        timing=c.wait(c.call('ds_query_phonemes'),180)
        check(timing['phonemes'] and all(x['note_id']==dn['id'] for x in timing['phonemes']),'phoneme query returns selected note with real context')
        rows=timing['phonemes'];r0=rows[0]
        # The first onset is adjusted by 1 ms; the remaining phonemes use prediction.
        starts=[None]*len(r0['tokens']);starts[0]=rows[0]['start']-rows[0]['note_start']+.001
        c.edit('ds_set_timing',{'note_id':dn['id'],'timing':{'context':r0['context'],'tokens':r0['tokens'],'starts':starts}})
        check(c.call('editor_query',{'include_curves':True})['notes'][0]['diffsinger_timing']['starts']==starts,'phoneme timing writes through ProjectModel')
        settled()
        c.edit('ds_set_pronunciation',{'note_id':dn['id'],'text':'bang'})
        reading=c.call('editor_query',{'include_curves':True})['notes'][0]
        check(reading['diffsinger_pronunciation']=='bang' and reading['diffsinger_timing'] is None,'pronunciation invalidates old phoneme timing')
        settled()
        before=c.call('project_snapshot')
        offset=reading['diffsinger_pitch_offset']
        c.wait(c.edit('ds_generate_pitch'),180);settled()
        after=c.call('editor_query',{'include_curves':True})['notes'][0]
        check(after['diffsinger_pitch_reference'] and after['diffsinger_pitch_offset']==offset,'local pitch generation preserves independent offset')
        c.wait(c.edit('ds_generate_parameters'),180);settled()
        after=c.call('editor_query',{'include_curves':True})['notes'][0]
        check('DS:REF:BREC' in after['flag_curves'] and 'DS:BREC' in after['flag_curves'],'local parameter generation preserves FLAG offset')
        now=c.call('project_snapshot')
        prev={n['id']:n for t in before['tracks'] for cl in t['clips'] for n in cl['notes']}
        untouched=[n for t in now['tracks'] for cl in t['clips'] for n in cl['notes'] if n['id']!=dn['id']]
        check(all(n['diffsinger_pitch_reference']==prev[n['id']]['diffsinger_pitch_reference'] for n in untouched),'local retake preserves other notes pitch references')
        c.edit('project_save',{'path':str(WORK/'ds-saved.hjpx')})
        check(pathlib.Path(str(WORK/'ds-saved.hjpx')+'.ds-cache').is_dir(),'real DS render cache migrates with Save As')
    c.edit('project_save',{'path':str(WORK/'final.hjpx')})
    (WORK/'report.json').write_text(json.dumps({'ok':True,'checks':checks,'tools':len(toolnames)},ensure_ascii=False,indent=2),encoding='utf-8')
    print('TOTAL',len(checks),flush=True)
finally:
    print('EDITOR EXIT STATES',[(p.pid,p.poll()) for p,_ in owned],flush=True)
    for c in clients:
        if c.p.poll() is None:c.p.terminate()
    for p,log in owned:
        if p.poll() is None:p.terminate()
        p.wait(15);log.close()


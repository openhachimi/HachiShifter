"""Exercise UST encoding/fidelity through a private live editor session."""
import os,pathlib,sys,subprocess,json,time
work=pathlib.Path(sys.argv[2]).resolve();work.mkdir(parents=True,exist_ok=True)
os.environ['HACHI_TEST_SETTINGS_DIR']=str(work/'settings');os.environ['TEMP']=os.environ['TMP']=str(work/'tmp');(work/'tmp').mkdir(exist_ok=True)
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=subprocess.SW_HIDE
helpers=(pathlib.Path(__file__).parent/'mcp_live_smoke.py').read_text(encoding='utf-8').split('\ntry:\n',1)[0]
helpers=helpers.replace("env=dict(os.environ,HACHI_MCP_TRACE='1'))","env=dict(os.environ,HACHI_MCP_TRACE='1'),startupinfo=si)");exec(helpers)
try:
 sid=window();c=Client('--mcp-live','--session='+sid);init=c.rpc('initialize',{})
 check(init['result']['serverInfo']['version']=='0.2.3','public version remains 0.2.3')
 tools={x['name']:x for x in c.rpc('tools/list')['result']['tools']}
 check('export_ust' in tools and 'encoding' in tools['import_ust']['inputSchema']['properties'],'live MCP exposes UST export and import encoding')
 text='[#VERSION]\r\nUST Version1.2\r\n[#SETTING]\r\nTempo=120.000\r\nProjectName=编码あ\r\nMode2=True\r\nVendor=original=payload\r\n[#0000]\r\nLength=480\r\nLyric=あ\r\nNoteNum=60\r\nFlags=B25g-10\r\nModulation=35\r\nPreUtterance=\r\nVoiceOverlap=0\r\nPitchBend=1,2,3\r\nPBS=-20;0\r\nPBW=100\r\nPBY=0\r\nMystery=\r\n[#TRACKEND]\r\n'
 fixture=WORK/'gbk.ust';raw=text.encode('gbk');fixture.write_bytes(raw)
 c.wait(c.edit('import_ust',{'path':str(fixture),'encoding':'GBK'}));track=c.call('project_snapshot')['tracks'][0];note=track['clips'][0]['notes'][0]
 check(note['label']=='あ' and note['utau_flags']=='B25g-10','explicit GBK reaches live project with correct lyric and flags')
 args={'path':str(WORK/'roundtrip.ust'),'track_id':track['id']};before=c.status();c.wait(c.edit('export_ust',args));after=c.status()
 check((WORK/'roundtrip.ust').read_bytes()==raw,'live unchanged export is byte-identical')
 check(all(after[k]==before[k] for k in ['revision','unsaved','can_undo']),'UST export leaves document revision and dirty state unchanged')
 saved=WORK/'saved.hjpx';c.edit('project_save',{'path':str(saved)});c.edit('project_new');c.wait(c.edit('project_open',{'path':str(saved)}));c.wait(c.edit('export_ust',args))
 check((WORK/'roundtrip.ust').read_bytes()==raw,'live HJPX save/reopen retains all original UST bytes')
 c.edit('set_note',{'note_id':note['id'],'utau_flags':'B30g-10'});c.wait(c.edit('export_ust',args))
 check((WORK/'roundtrip.ust').read_bytes().decode('gbk')==text.replace('Flags=B25g-10','Flags=B30g-10'),'only edited flag field changes; empty and inactive pitch data survive')
 c.edit('undo');c.wait(c.edit('export_ust',args));check((WORK/'roundtrip.ust').read_bytes()==raw,'undo restores exact UST export')
 c.edit('set_note',{'note_id':note['id'],'label':'😀'});before=(WORK/'roundtrip.ust').read_bytes()
 failed=False
 try:c.wait(c.edit('export_ust',args))
 except AssertionError:failed=True
 check(failed and (WORK/'roundtrip.ust').read_bytes()==before,'unrepresentable GBK lyric rejects export without overwriting destination')
 c.wait(c.edit('export_ust',dict(args,encoding='UTF-8')));utf=(WORK/'roundtrip.ust').read_bytes()
 check(utf.startswith(b'\xef\xbb\xbf') and 'Lyric=😀' in utf.decode('utf-8-sig'),'explicit UTF-8 export preserves Unicode lyric')
 c.edit('undo');c.edit('project_save',{'path':str(saved)});before=c.status()['revision'];failed=False
 try:c.wait(c.edit('import_ust',{'path':str(fixture),'encoding':'not-a-codec'}))
 except AssertionError:failed=True
 check(failed and c.status()['revision']==before,'unknown encoding rejected without document mutation')
 (WORK/'report.json').write_text(json.dumps({'ok':True,'checks':checks},ensure_ascii=False,indent=2),encoding='utf-8');print('TOTAL',len(checks),flush=True)
finally:
 for client in clients:
  if client.p.poll() is None:client.p.terminate()
  client.p.wait(15)
 for p,log in owned:
  if p.poll() is None:p.terminate()
  p.wait(15);log.close()
"""Checks against the saved Tokyo trial window; all temporary edits are undone."""
from pathlib import Path
import json,sys,time
sys.path.insert(0,str(Path(__file__).parent.resolve()))
from client import Client
work=Path(sys.argv[1]);state=json.loads((work/'session.json').read_text(encoding='utf-8'))
c=Client(state['exe'],'--mcp-live','--session='+state['session_id']);c.rpc('initialize',{})
checks=[]
def check(v,name):
 if not v:raise AssertionError(name)
 checks.append(name);print('PASS',name,flush=True)
try:
 data=c.call('project_snapshot');status=c.status();back=next(t for t in data['tracks'] if t['accompaniment']);clip=back['clips'][0]
 lead=data['tracks'][0];harmony=next(t for t in data['tracks'] if 'HAMOOD' in t['name'])
 names={x['name'] for x in c.rpc('tools/list')['result']['tools']}
 check({'hamood_analyse_audio','hamood_audio_context','hamood_alignment_preview'}<=names,'new live tools discoverable')
 c.edit('editor_select',{'track_id':lead['id'],'note_ids':[]})
 check(c.raw('hamood_audio_context',{'clip_id':clip['id']}).get('isError',False),'empty selection never expands implicitly')
 c.edit('editor_select',{'track_id':lead['id'],'note_ids':[n['id'] for n in lead['clips'][0]['notes'][:3]]})
 selected=c.call('hamood_audio_context',{'clip_id':clip['id']})
 check(selected['to_seconds']-selected['from_seconds']<2,'selected-note context bounded')
 page=c.call('hamood_audio_context',{'clip_id':clip['id'],'whole_clip':True,'limit':3})
 next_page=c.call('hamood_audio_context',{'clip_id':clip['id'],'whole_clip':True,'limit':3,'offset':page['next_offset']})
 check(len(page['chords'])==3 and page['chords'][-1]['end']<=next_page['chords'][0]['start']+1.e-8,'context pagination has no repeated events')
 check(c.raw('hamood_audio_context',{'clip_id':clip['id'],'whole_clip':True,'limit':257}).get('isError',False),'oversized page rejected')
 check(c.raw('hamood_audio_context',{'clip_id':clip['id'],'from_seconds':10,'to_seconds':9}).get('isError',False),'inverted range rejected')
 rev=c.status()['revision'];alignment=c.call('hamood_alignment_preview',{'clip_id':clip['id'],'from_seconds':5,'to_seconds':190})
 check(c.status()['revision']==rev,'alignment preview read only')
 check(abs(alignment['suggested_shift_seconds'])<.01,'already aligned trial stays near zero phase offset')
 (work/'mcp-alignment-preview.json').write_text(json.dumps(alignment,ensure_ascii=False,indent=2),encoding='utf-8')
 opts={'track_id':lead['id'],'whole_track':True,'key_mode':'sections','section_bars':4,'voices':[2]}
 base=c.call('hamood_preview',opts)
 informed=c.call('hamood_preview',dict(opts,audio_clip_id=clip['id']))
 fallback=c.call('hamood_preview',dict(opts,audio_clip_id=clip['id'],minimum_chord_score=1))
 check(informed['audio_covered_notes']>400 and informed['audio_adjusted_targets']>0,'real BTC evidence affects harmony')
 check([n['harmony_midi'] for n in base['notes']]==[n['harmony_midi'] for n in fallback['notes']],'unsupported confidence falls back exactly to original rule')
 check(c.status()['revision']==rev,'all harmony previews read only')
 check(c.raw('hamood_generate',dict(opts,audio_clip_id=clip['id'],expected_revision=rev-1)).get('isError',False),'stale generate rejected')
 check(c.raw('hamood_preview',dict(opts,audio_clip_id=lead['clips'][0]['id'])).get('isError',False),'vocal synthesis clip cannot masquerade as backing analysis')
 first=page['chords'][0]['start']
 c.edit('move_clip',{'clip_id':clip['id'],'start_seconds':clip['start_seconds']+.4})
 moved=c.call('hamood_audio_context',{'clip_id':clip['id'],'whole_clip':True,'limit':3})
 check(abs(moved['chords'][0]['start']-first-.4)<1e-7,'cached events follow current clip placement')
 c.edit('undo');check(c.call('project_snapshot')['tracks']==data['tracks'],'alignment test undo restores exact project')
 # Parameters, region flags and original notes survive the generator's clone.
 source_notes=lead['clips'][0]['notes'];new_notes=harmony['clips'][0]['notes']
 check(len(source_notes)==len(new_notes)==478,'full-song note count preserved')
 preserved=[k for k in source_notes[0] if ('flag' in k or 'consonant' in k or 'overlap' in k or 'stp' in k or 'preutterance' in k)]
 check(all(all(a.get(k)==b.get(k) for k in preserved) for a,b in zip(source_notes,new_notes)),'UTAU flag and timing fields preserved')
 check(all(a['label']==b['label'] and a['start_seconds']==b['start_seconds'] and a['duration_seconds']==b['duration_seconds'] for a,b in zip(source_notes,new_notes)),'lyrics and note timing preserved')
 # Cancel a real worker before it can replace a valid cached analysis.
 job=c.call('hamood_analyse_audio',{'clip_id':clip['id'],'force':True})
 c.call('editor_cancel_job',{'job_id':job['job_id']})
 for _ in range(80):
  cancelled=c.call('editor_job_status',{'job_id':job['job_id']})
  if cancelled['state']!='running':break
  time.sleep(.1)
 check(cancelled['state']=='cancelled','analysis worker cancellation completes')
 check(c.call('hamood_audio_context',{'clip_id':clip['id'],'whole_clip':True})['ok'],'cancelled analysis retains last successful cache')
 c.edit('editor_select',{'track_id':harmony['id'],'note_ids':[]})
 c.edit('project_save',{'path':state['project']})
 c.wait(c.edit('project_open',{'path':state['project']}))
 check(c.call('project_snapshot')['tracks']==data['tracks'],'saved harmony roundtrip exact')
 check(c.call('hamood_audio_context',{'clip_id':clip['id'],'whole_clip':True})['ok'],'cached analysis survives project reopen')
 c.edit('editor_select',{'track_id':harmony['id'],'note_ids':[]})
 (work/'mcp-audio-validation.json').write_text(json.dumps({'passed':True,'checks':checks},ensure_ascii=False,indent=2),encoding='utf-8')
 print('TOTAL',len(checks),flush=True)
finally:c.p.terminate();c.p.wait(10)

from pathlib import Path
import json,numpy as np,sys,shutil,time
sys.path.insert(0,str(Path('tools/hamood_audio').resolve()));from client import Client
work=Path('../HAMOOD-东京泰迪熊试作').resolve();state=json.loads((work/'session.json').read_text(encoding='utf-8'))
a=json.loads(Path('build-integrated/tokyo-harmony/analysis.json').read_text(encoding='utf-8'))
project=Path(state['project']);cache=project.with_name(project.name+'.hamood-cache');cache.mkdir(exist_ok=True)
c=Client(state['exe'],'--mcp-live','--session='+state['session_id']);c.rpc('initialize',{})
try:
 data=c.call('project_snapshot');lead=data['tracks'][0];clip=next(t for t in data['tracks'] if t['accompaniment'])['clips'][0]
 old_generation=json.loads((work/'generation.json').read_text(encoding='utf-8'))
 for old_id in old_generation['created_track_ids']:
  if any(t['id']==old_id for t in data['tracks']):c.edit('remove_track',{'track_id':old_id})
 data=c.call('project_snapshot')
 print('tools',[(t['name']) for t in c.rpc('tools/list')['result']['tools'] if t['name'].startswith('hamood')],flush=True)
 job=c.call('hamood_analyse_audio',{'clip_id':clip['id']});print('accepted',job,flush=True)
 result=c.wait(job,90);print('analysis',result,flush=True)
 (work/'mcp-analysis-summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
 context=c.call('hamood_audio_context',{'clip_id':clip['id'],'whole_clip':True,'limit':256})
 (work/'audio-context-before.json').write_text(json.dumps(context,ensure_ascii=False),encoding='utf-8')
 alignment=c.call('hamood_alignment_preview',{'clip_id':clip['id'],'from_seconds':5,'to_seconds':190})
 assert abs(alignment['suggested_shift_seconds'])<.12,alignment
 c.edit('move_clip',{'clip_id':clip['id'],'start_seconds':alignment['suggested_start_seconds']})
 (work/'alignment-final.json').write_text(json.dumps(alignment,ensure_ascii=False,indent=2),encoding='utf-8')
 c.edit('editor_select',{'track_id':lead['id'],'note_ids':[]})
 opts={'track_id':lead['id'],'whole_track':True,'key_mode':'sections','section_bars':4,'voices':[2],'preserve_pitch':True,'gain_db':-9}
 baseline=c.call('hamood_preview',opts)
 preview=c.call('hamood_preview',dict(opts,audio_clip_id=clip['id']))
 (work/'scale-only-preview.json').write_text(json.dumps(baseline,ensure_ascii=False),encoding='utf-8')
 (work/'chord-guided-preview.json').write_text(json.dumps(preview,ensure_ascii=False),encoding='utf-8')
 print('preview', {k:v for k,v in preview.items() if k not in ['notes','passages','summary']},flush=True)
 created=c.edit('hamood_generate',dict(opts,audio_clip_id=clip['id']))
 (work/'generation.json').write_text(json.dumps(created,ensure_ascii=False),encoding='utf-8')
 after=c.call('project_snapshot');new=next(t for t in after['tracks'] if t['id'] in created['created_track_ids'])
 assert after['tracks'][0]==lead,'Lead modified'
 assert [n['midi'] for cl in new['clips'] for n in cl['notes']]==[n['harmony_midi'][0] for n in preview['notes']]
 beforeUndo=after
 c.edit('undo');assert len(c.call('project_snapshot')['tracks'])==3
 c.edit('redo');assert c.call('project_snapshot')['tracks']==beforeUndo['tracks']
 c.edit('set_track',{'track_id':new['id'],'pan':.2})
 c.edit('editor_select',{'track_id':new['id'],'note_ids':[]})
 c.edit('project_save',{'path':str(project)})
 (work/'generated-snapshot.json').write_text(json.dumps(c.call('project_snapshot'),ensure_ascii=False),encoding='utf-8')
 print('saved',str(project),'harmony_notes',sum(len(cl['notes']) for cl in new['clips']),'adjusted',preview['audio_adjusted_targets'],flush=True)
except Exception:
 import traceback;traceback.print_exc();raise
finally:c.p.terminate();c.p.wait(10)

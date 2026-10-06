from pathlib import Path
import json,sys,numpy as np,soundfile as sf,pretty_midi
sys.path.insert(0,str(Path(__file__).parent));from client import Client
repo=Path(__file__).resolve().parents[2];work=repo.parent/'HAMOOD-原唱参考试作'
s=json.loads((work/'session.json').read_text(encoding='utf-8'));base=json.loads((work/'baseline.json').read_text(encoding='utf-8'))
analysis=json.loads((work/'reference-analysis.json').read_text(encoding='utf-8'))
rows=json.loads((work/'polyphony-candidates.json').read_text(encoding='utf-8'))['rows'];chosen=[]
for row in rows:
 if row['lead_score']<=.4:continue
 eligible=[x for x in row['candidates'] if x['score']>.3 and x['side_score']>.35 and x['occupancy']>.65]
 if eligible:chosen.append(dict(row,chosen=eligible[0],target_midi=eligible[0]['midi']-12))
assert len(chosen)==37
lead=base['tracks'][0];back=next(t for t in base['tracks'] if t['accompaniment'] and 'B三狼' not in t['clips'][0]['source_file'])
reference=next(t for t in base['tracks'] if t['accompaniment'] and 'B三狼' in t['clips'][0]['source_file'])
stem_offset=analysis['stem_to_backing_offset']+back['clips'][0]['start_seconds']
audio,sr=sf.read(analysis['stem_paths']['Vocals'],dtype='float32',always_2d=True)
trim=int(round(-stem_offset*sr));assert 0<trim<2*sr
aligned=work/'原唱人声-已对齐.wav';sf.write(aligned,audio[trim:],sr,subtype='PCM_24')
c=Client(s['executable'],'--mcp-live','--session='+s['session_id'])
try:
 c.rpc('initialize',{});now=c.call('project_snapshot');assert now['tracks']==base['tracks'],'User edited the trial during analysis'
 c.edit('editor_select',{'track_id':lead['id'],'note_ids':[x['id'] for x in chosen]})
 opts={'track_id':lead['id'],'voices':[-2],'gain_db':-9,'preserve_pitch':True}
 c.call('hamood_preview',opts);generated=c.edit('hamood_generate',opts);hid=generated['created_track_ids'][0]
 data=c.call('project_snapshot');track=next(t for t in data['tracks'] if t['id']==hid);notes=track['clips'][0]['notes']
 source={n['id']:n for n in lead['clips'][0]['notes']};commands=[]
 midi=pretty_midi.PrettyMIDI(initial_tempo=base['bpm']);inst=pretty_midi.Instrument(53,name='Reference harmony candidates')
 for row in chosen:
  original=source[row['id']]
  n=next(n for n in notes if abs(n['start_seconds']-original['start_seconds'])<1e-7 and n['label']==original['label'])
  commands.append({'name':'transpose_note','arguments':{'note_id':n['id'],'semitones':row['target_midi']-n['midi']}})
  row['generated_note_id']=n['id']
  inst.notes.append(pretty_midi.Note(80,int(row['target_midi']),float(row['start']),float(row['end'])))
 commands.append({'name':'set_track','arguments':{'track_id':hid,'name':'HAMOOD · 原唱和声候选 37 音（待核对）','pan':-.2}})
 for t in base['tracks']:
  if t['id'] not in [lead['id'],back['id']]:commands.append({'name':'set_track','arguments':{'track_id':t['id'],'muted':True}})
 c.edit('editor_batch',{'commands':commands})
 result=c.edit('add_track',{'name':'原唱人声 · 已对齐（参考）','accompaniment':True});vid=result.split('track_id=')[1].strip()
 c.wait(c.edit('import_audio',{'path':str(aligned),'track_id':vid,'start_seconds':0}))
 c.edit('set_track',{'track_id':vid,'muted':True})
 final=c.call('project_snapshot');assert final['tracks'][0]==lead
 out=next(t for t in final['tracks'] if t['id']==hid)
 for row in chosen:
  n=next(n for n in out['clips'][0]['notes'] if n['id']==row['generated_note_id']);old=source[row['id']]
  assert abs(n['midi']-row['target_midi'])<1e-6
  assert all(n.get(k)==v for k,v in old.items() if 'flag' in k or k in ['label','start_seconds','duration_seconds'])
 c.edit('editor_select',{'track_id':hid,'note_ids':[]})
 c.edit('project_save',{'path':s['project_path']})
 midi.instruments.append(inst);midi.write(str(work/'原唱和声候选-低八度.mid'))
 (work/'candidate-restoration.json').write_text(json.dumps(dict(harmony_track_id=hid,aligned_vocal_track_id=vid,source_trim_seconds=trim/sr,original_reference_shift=analysis['proposed_reference_clip_start']-reference['clips'][0]['start_seconds'],candidates=chosen,complete_restoration=False),ensure_ascii=False,indent=2),encoding='utf-8')
 (work/'final-snapshot.json').write_text(json.dumps(final,ensure_ascii=False),encoding='utf-8')
 print(json.dumps(dict(project=s['project_path'],tracks=len(final['tracks']),candidate_notes=len(chosen),flags_preserved=True,lead_unchanged=True),ensure_ascii=True),flush=True)
finally:c.p.terminate();c.p.wait(10)

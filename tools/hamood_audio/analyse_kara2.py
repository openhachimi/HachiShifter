"""Measure separated harmony evidence; never force every lead note to have harmony."""
from pathlib import Path
import sys,os,json,time,logging
root=Path(__file__).resolve().parent;repo=root.parents[1];work=repo.parent/'HAMOOD-KARA2和声分析'
sys.path[:0]=[str(repo.parent/'HachiShifter-整合版-0.2.3-DiffSinger/engines/diffsinger/packages'),str(root/'reference-deps')]
os.environ['NUMBA_CACHE_DIR']=str(work/'numba');os.environ['NUMBA_CACHE_LOCATOR_CLASSES']='UserProvidedCacheLocator'
logging.disable(logging.WARNING)
import numpy as np,soundfile as sf,onnxruntime as ort
from basic_pitch.inference import Model,run_inference
from basic_pitch.note_creation import model_frames_to_time
from scipy.ndimage import uniform_filter1d
class CpuModel(Model):
 def __init__(self,path):
  options=ort.SessionOptions();options.intra_op_num_threads=4;options.inter_op_num_threads=1
  self.model_type=self.MODEL_TYPES.ONNX;self.model=ort.InferenceSession(str(path),options,providers=['CPUExecutionProvider'])
started=time.monotonic()
while not (work/'separated/separation.json').exists():
 if time.monotonic()-started>900:raise TimeoutError('Separation has not completed')
 time.sleep(2)
model=CpuModel(root/'reference-deps/basic_pitch/saved_models/icassp_2022/nmp.onnx')
paths={'full':repo.parent/'HAMOOD-原唱参考试作/原唱人声-已对齐.wav','lead':work/'separated/lead.wav','backing':work/'separated/backing.wav'}
features={}
for key,path in paths.items():
 dest=work/('pitch-'+key+'.npz')
 if not dest.exists():np.savez_compressed(dest,**run_inference(path,model))
 features[key]=np.load(dest)['note']
 print('Pitch extracted:',key,features[key].shape,flush=True)
full,lead,back=[features[k] for k in ['full','lead','backing']]
count=min(map(len,features.values()));full,lead,back=full[:count],lead[:count],back[:count]
times=model_frames_to_time(count)
data=json.loads((work/'baseline.json').read_text(encoding='utf-8'))
source=data['tracks'][0];clip=source['clips'][0];notes=sorted(clip['notes'],key=lambda n:n['start_seconds'])
# Calibrate feature timestamps against previously measured waveform-based CREPE.
crepe=np.load(repo.parent/'HAMOOD-原唱参考试作/reference-features.npz')
previous=json.loads((repo.parent/'HAMOOD-原唱参考试作/candidate-restoration.json').read_text(encoding='utf-8'))
ct=crepe['time']-previous['source_trim_seconds'];cm=crepe['midi'];cv=crepe['voiced'];cc=crepe['confidence']
observed=lead[:,40:70].argmax(axis=1)+61
strength=lead[np.arange(count),observed-21]
calibration=[]
for shift in np.arange(-.2,.205,.005):
 query=times+shift
 interp=np.interp(query,ct,cm);valid=(strength>.4)&(np.interp(query,ct,cc)>.5)&(query>3)&(query<190)
 error=np.abs(interp-observed)
 calibration.append({'shift':float(shift),'score':float(np.mean(np.exp(-np.minimum(error[valid],12)**2/2))),'frames':int(valid.sum())})
best=max(calibration,key=lambda x:x['score']);shift=best['shift'];times=times+shift
print('Feature alignment:',best,flush=True)
chord_data=json.loads((repo.parent/'HAMOOD-东京泰迪熊试作/audio-context-final.json').read_text(encoding='utf-8'))
chords=chord_data['chords']
def chord_at(t):
 return next((c for c in chords if c['start']<=t<c['end']),None)
def longest(mask):
 edges=np.diff(np.r_[False,mask,False].astype(int));starts=np.where(edges==1)[0];ends=np.where(edges==-1)[0]
 return int(max(ends-starts,default=0))
rows=[]
for n in notes:
 lo=clip['start_seconds']+n['start_seconds'];hi=lo+n['duration_seconds'];margin=min(.06,(hi-lo)*.17)
 mask=(times>lo+margin)&(times<hi-margin)
 if mask.sum()<5:continue
 p=round(n['midi']+12)
 if not 30<=p<=95:continue
 l=lead[mask].mean(axis=0);b=back[mask].mean(axis=0);f=full[mask].mean(axis=0)
 actual=int(np.argmax(l[max(0,p-23):min(88,p-18)])+max(0,p-23)+21)
 candidates=[]
 for q in range(max(45,p-9),min(96,p+10)):
  if abs(q-p)<2 or (q-p)%12==0:continue
  ix=q-21;curve=back[mask,ix];occupancy=float(np.mean(curve>.18));sustain=longest(curve>.18)*float(np.median(np.diff(times)))
  evidence=float(b[ix]);selectivity=float((b[ix]+.03)/(l[ix]+.03));peak=float(np.max(curve))
  eligible=evidence>=.22 and occupancy>=.55 and sustain>=.085 and f[ix]>=.14 and selectivity>=1.15 and l[p-21]>=.25
  c=chord_at((lo+hi)/2)
  candidates.append(dict(midi=q,target_midi=q-12,interval=q-p,backing_score=evidence,lead_score=float(l[ix]),full_score=float(f[ix]),
    occupancy=occupancy,sustain_seconds=sustain,selectivity=selectivity,peak=peak,eligible=bool(eligible),
    chord=c['label'] if c else None,chord_score=c['score'] if c else None,
    in_chord=(q%12 in c['pitch_classes']) if c else None,
    rank_score=evidence*(.6+.4*occupancy)*min(selectivity,3)**.3))
 candidates.sort(key=lambda x:x['rank_score'],reverse=True)
 eligible=[c for c in candidates if c['eligible']]
 chosen=eligible[0] if eligible else None
 row=dict(id=n['id'],lyric=n['label'],start=lo,end=hi,lead_midi=n['midi'],reference_midi=p,observed_lead_midi=actual,
   lead_score=float(l[p-21]),candidates=candidates[:4],chosen=chosen)
 if chosen:
  margin_score=chosen['rank_score']-(eligible[1]['rank_score'] if len(eligible)>1 else 0)
  row['status']='strong' if chosen['backing_score']>=.32 and chosen['occupancy']>=.7 and chosen['selectivity']>=1.6 and margin_score>=.06 and abs(actual-p)<=1 else 'review'
 else:row['status']='no_distinct_harmony_evidence'
 rows.append(row)
old_ids={r['id'] for r in previous['candidates']};strong=[r for r in rows if r['status']=='strong'];review=[r for r in rows if r['status']=='review']
selected={r['id'] for r in strong};retained=[r for r in rows if r['id'] in old_ids and r['chosen']]
summary=dict(source_notes=len(notes),evaluated_notes=len(rows),strong=len(strong),review=len(review),old_candidates=len(old_ids),
 old_supported=len(retained),old_strong=sum(r['id'] in old_ids for r in strong),new_strong=sum(r['id'] not in old_ids for r in strong),
 feature_shift_seconds=shift,feature_alignment=best,calibration=calibration,
 interval_counts={str(i):sum(r['chosen']['interval']==i for r in strong) for i in range(-9,10) if any(r['chosen']['interval']==i for r in strong)},
 complete_restoration=False,seconds=time.monotonic()-started)
(work/'harmony-evidence.json').write_text(json.dumps(dict(summary=summary,rows=rows),ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(summary,ensure_ascii=True),flush=True)
print('STRONG',json.dumps(strong[:15],ensure_ascii=True),flush=True)

from pathlib import Path
import sys,os,json,time,logging
root=Path(__file__).resolve().parent;repo=root.parents[1];work=repo.parent/'HAMOOD-原唱参考试作'
sys.path.insert(0,str(repo.parent/'HachiShifter-整合版-0.2.3-DiffSinger/engines/diffsinger/packages'))
sys.path.insert(0,str(root/'reference-deps'))
os.environ['NUMBA_CACHE_DIR']=str(work/'numba');os.environ['NUMBA_CACHE_LOCATOR_CLASSES']='UserProvidedCacheLocator'
logging.disable(logging.WARNING)
import numpy as np,soundfile as sf,onnxruntime as ort
from basic_pitch.inference import Model,run_inference
from basic_pitch.constants import ANNOTATIONS_FPS
from basic_pitch.note_creation import model_frames_to_time
class CpuModel(Model):
 def __init__(self,path):
  options=ort.SessionOptions();options.intra_op_num_threads=4;options.inter_op_num_threads=1
  self.model_type=self.MODEL_TYPES.ONNX;self.model=ort.InferenceSession(str(path),options,providers=['CPUExecutionProvider'])
report=json.loads((work/'reference-analysis.json').read_text(encoding='utf-8'));data=json.loads((work/'baseline.json').read_text(encoding='utf-8'))
model=CpuModel(root/'reference-deps/basic_pitch/saved_models/icassp_2022/nmp.onnx')
vocal=Path(report['stem_paths']['Vocals']);audio,sr=sf.read(vocal,dtype='float32',always_2d=True)
side=(audio[:,0]-audio[:,1])/2;sf.write(work/'reference-side.wav',side,sr,subtype='FLOAT')
start=time.monotonic()
for key,path in [('full',vocal),('side',work/'reference-side.wav')]:
 dest=work/('basic-pitch-'+key+'.npz')
 if not dest.exists():
  result=run_inference(path,model);np.savez_compressed(dest,**result)
  print(key,'seconds',time.monotonic()-start,flush=True)

full=np.load(work/'basic-pitch-full.npz')['note'];side=np.load(work/'basic-pitch-side.npz')['note']
lead=data['tracks'][0]['clips'][0];back=next(t['clips'][0] for t in data['tracks'] if t['accompaniment'] and 'B三狼' not in t['clips'][0]['source_file'])
offset=report['stem_to_backing_offset']+back['start_seconds']-.11
times=model_frames_to_time(len(full))+offset
rows=[]
for n in lead['notes']:
 lo=lead['start_seconds']+n['start_seconds'];hi=lo+n['duration_seconds'];mask=(times>lo+.035)&(times<hi-.025)
 if mask.sum()<4:continue
 observed=full[mask].mean(axis=0);sides=side[:len(full)][mask].mean(axis=0);p=round(n['midi']+12)
 candidates=[]
 for q in range(max(45,p-9),min(95,p+10)):
  if abs(q-p)<2:continue
  score=float(observed[q-21]);ss=float(sides[q-21]);occupancy=float(np.mean(full[mask,q-21]>.2))
  candidates.append(dict(midi=q,score=score,side_score=ss,occupancy=occupancy))
 candidates.sort(key=lambda c:c['score'],reverse=True)
 rows.append(dict(id=n['id'],start=lo,end=hi,lead_midi=n['midi'],expected_reference=p,
                  lead_score=float(observed[p-21]),candidates=candidates[:3]))
(work/'polyphony-candidates.json').write_text(json.dumps(dict(rows=rows,seconds=time.monotonic()-start,model='Basic Pitch 0.4.0 ONNX'),ensure_ascii=False,indent=2),encoding='utf-8')
strong=[r for r in rows if r['candidates'][0]['score']>.25 and r['candidates'][0]['occupancy']>.5]
print('strong extra-pitch candidates',len(strong),'of',len(rows),flush=True)
print(json.dumps(strong[:16],ensure_ascii=True),flush=True)

from pathlib import Path
import os,json,time
repo=Path(__file__).resolve().parents[2];work=repo.parent/'HAMOOD-KARA2和声分析'
os.environ['NUMBA_CACHE_DIR']=str(work/'numba');os.environ['NUMBA_CACHE_LOCATOR_CLASSES']='UserProvidedCacheLocator'
import numpy as np,soundfile as sf,soxr,torch,torchcrepe,librosa
from scipy.ndimage import uniform_filter1d
torch.set_num_threads(4)
start=time.monotonic()
for key in ['backing','lead']:
 out=work/('crepe-'+key+'.npz')
 if out.exists():continue
 a,sr=sf.read(work/('separated/'+key+'.wav'),dtype='float32',always_2d=True)
 y=soxr.resample(a.mean(axis=1),sr,16000)
 with torch.inference_mode():
  f0,confidence=torchcrepe.predict(torch.tensor(y)[None],16000,320,65,1200,'tiny',batch_size=512,device='cpu',return_periodicity=True)
 midi=librosa.hz_to_midi(np.maximum(f0[0].numpy(),1));conf=confidence[0].numpy();t=np.arange(len(midi))*.02
 rms=librosa.feature.rms(y=y,frame_length=1024,hop_length=320)[0][:len(t)]
 np.savez_compressed(out,time=t,midi=midi,confidence=conf,rms=rms)
 print(key,'CREPE seconds',time.monotonic()-start,flush=True)
d=json.loads((work/'harmony-evidence.json').read_text(encoding='utf-8'));c=np.load(work/'crepe-backing.npz');t=c['time']
for row in d['rows']:
 chosen=row['chosen']
 if not chosen:continue
 lo,hi=row['start'],row['end'];margin=min(.06,(hi-lo)*.17)
 mask=(t>lo+margin)&(t<hi-margin);voiced=mask&(c['confidence']>.3)&(c['rms']>.0015)
 error=np.abs(c['midi']-chosen['midi']);agree=voiced&(error<.8)
 ratio=float(agree.sum()/max(1,mask.sum()))
 row['crepe_support']=dict(agreement=ratio,voiced_frames=int(voiced.sum()),total_frames=int(mask.sum()),
  median_midi=float(np.median(c['midi'][voiced])) if voiced.any() else None)
 # A monophonic corroborator can reject a real secondary line in polyphony;
 # those notes remain inspectable rather than claimed to be absent.
 if row['status']=='strong' and (ratio<.5 or agree.sum()<3):row['status']='review'
summary={k:v for k,v in d['summary'].items() if k not in ['calibration']};strong=[r for r in d['rows'] if r['status']=='strong'];review=[r for r in d['rows'] if r['status']=='review']
summary.update(strong=len(strong),review=len(review),crepe_used=True,raw_strong=d['summary']['strong'],corroboration_seconds=time.monotonic()-start,
 interval_counts={str(i):sum(r['chosen']['interval']==i for r in strong) for i in range(-9,10) if any(r['chosen']['interval']==i for r in strong)})
old=json.loads((repo.parent/'HAMOOD-原唱参考试作/candidate-restoration.json').read_text(encoding='utf-8'));oldmap={r['id']:r for r in old['candidates']}
summary['old_strong']=sum(r['id'] in oldmap for r in strong);summary['new_strong']=sum(r['id'] not in oldmap for r in strong)
summary['old_supported_same_pitch']=sum(bool(r['id'] in oldmap and r['chosen'] and r['chosen']['target_midi']==oldmap[r['id']]['target_midi']) for r in d['rows'])
segments=[]
for lo,hi in [(0,40),(40,75),(75,115),(115,140),(140,175),(175,194.1323)]:
 rs=[r for r in d['rows'] if lo<=r['start']<hi];segments.append(dict(start=lo,end=hi,strong=sum(r['status']=='strong' for r in rs),review=sum(r['status']=='review' for r in rs)))
summary['sections']=segments
(work/'harmony-corroborated.json').write_text(json.dumps(dict(summary=summary,rows=d['rows']),ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(summary,ensure_ascii=True,indent=2),flush=True)
print('Verified candidates:',json.dumps([dict(start=r['start'],end=r['end'],midi=r['chosen']['midi'],interval=r['chosen']['interval'],support=r['crepe_support']['agreement']) for r in strong],ensure_ascii=True),flush=True)

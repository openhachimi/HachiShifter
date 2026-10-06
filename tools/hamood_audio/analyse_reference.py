"""Read-only, octave-independent comparison of the reference and score."""
from pathlib import Path
import os,json,time
work=Path(__file__).resolve().parents[3]/'HAMOOD-原唱参考试作'
os.environ['NUMBA_CACHE_DIR']=str(work/'numba')
os.environ['NUMBA_CACHE_LOCATOR_CLASSES']='UserProvidedCacheLocator'
import numpy as np,soundfile as sf,librosa,soxr,torch,torchcrepe
from scipy import signal
torch.set_num_threads(4)
started=time.monotonic()
def load(p,sr=22050):
 a,r=sf.read(p,dtype='float32',always_2d=True);return soxr.resample(a.mean(axis=1),r,sr) if r!=sr else a.mean(axis=1)
def match_windows(target,template,sr,shift=0,search=1.5):
 rows=[]
 for at in [5,35,70,110,150,180]:
  part=template[int(at*sr):int((at+4)*sr)]
  low=max(0,int((at+shift-search)*sr));high=min(len(target),int((at+shift+search+4)*sr))
  segment=target[low:high]
  if len(segment)<len(part) or np.sqrt(np.mean(part**2))<1e-5:continue
  corr=signal.correlate(segment,part,mode='valid',method='fft')
  energies=signal.fftconvolve(segment**2,np.ones(len(part)),mode='valid')
  scores=corr/np.sqrt(np.maximum(energies*np.sum(part**2),1e-20))
  i=int(np.argmax(scores));rows.append(dict(at=at,offset=float((low+i)/sr-at),correlation=float(scores[i])))
 return rows

ref=load(work/'reference-editor.wav');back=load(work/'backing-editor.wav')
paths={label:next(Path('F:/鬼畜/新UI').glob('*B三狼*('+label+').wav')) for label in ['Vocals','Drums','Bass','Other']}
stems={k:load(v) for k,v in paths.items()};combined=sum(stems.values())
matches=match_windows(ref,combined,22050)
delta=float(np.median([r['offset'] for r in matches if r['correlation']>.7]))
assert np.isfinite(delta),'Existing stems do not match the imported reference'
print('matching stems',matches,flush=True)
instrumental=stems['Drums']+stems['Bass']+stems['Other']
back_matches=match_windows(back,instrumental,22050)
print('backing correlation',back_matches,flush=True)
hop=2048
def chroma(a):
 c=librosa.feature.chroma_cqt(y=a,sr=22050,hop_length=hop,n_octaves=6)
 return np.maximum(c,1e-7)
cr,cb=chroma(instrumental),chroma(back)
cost,path=librosa.sequence.dtw(X=cr,Y=cb,metric='cosine',global_constraints=True,band_rad=.07)
path=path[::-1];mapping=[]
for a in np.unique(path[:,0]):
 b=float(np.median(path[path[:,0]==a,1]));mapping.append([float(a*hop/22050),float(b*hop/22050)])
mapping=np.array(mapping)
align=[]
for start in [5,35,70,110,150,180]:
 mask=(mapping[:,0]>=start)&(mapping[:,0]<start+12)
 align.append(dict(at=start,offset=float(np.median(mapping[mask,1]-mapping[mask,0])),mean_cost=float(np.mean(cost[-1:,-1:]))))
print('DTW instrumental-to-backing',align,flush=True)

# A monophonic tracker supplies the dominant vocal, not a claimed harmony stem.
audio=soxr.resample(stems['Vocals'],22050,16000)
with torch.inference_mode():
 f0,periodicity=torchcrepe.predict(torch.tensor(audio).unsqueeze(0),16000,320,65,1200,'tiny',batch_size=512,device='cpu',return_periodicity=True)
pitch=f0[0].numpy();confidence=periodicity[0].numpy();t=np.arange(len(pitch))*.02
rms=librosa.feature.rms(y=audio,frame_length=1024,hop_length=320)[0][:len(pitch)]
voiced=(confidence>.25)&(rms>.006);midi=librosa.hz_to_midi(np.maximum(pitch,1))
np.savez_compressed(work/'reference-features.npz',time=t,midi=midi,confidence=confidence,voiced=voiced,mapping=mapping)
data=json.loads((work/'baseline.json').read_text(encoding='utf-8'));lead=data['tracks'][0]['clips'][0]
back_clip=next(x['clips'][0] for x in data['tracks'] if x['accompaniment'] and 'B三狼' not in x['clips'][0]['source_file'])
back_offset=float(np.median([x['offset'] for x in back_matches if x['correlation']>.5])) if any(x['correlation']>.5 for x in back_matches) else float(np.median(mapping[(mapping[:,0]>5)&(mapping[:,0]<190),1]-mapping[(mapping[:,0]>5)&(mapping[:,0]<190),0]))
project_times=t+back_offset+back_clip['start_seconds']
notes=[]
for n in lead['notes']:
 lo=lead['start_seconds']+n['start_seconds'];hi=lo+n['duration_seconds']
 mask=(project_times>lo+.03)&(project_times<hi-.02)&voiced
 if mask.sum()<3:continue
 observed=float(np.median(midi[mask]));notes.append(dict(id=n['id'],start=lo,end=hi,score_midi=n['midi'],reference_midi=observed,difference=observed-n['midi'],frames=int(mask.sum())))
report=dict(stem_paths={k:str(v) for k,v in paths.items()},stem_to_reference=matches,stem_to_reference_offset=delta,
 stem_to_backing=back_matches,dtw_offsets=align,stem_to_backing_offset=back_offset,
 proposed_reference_clip_start=back_clip['start_seconds']+back_offset-delta,
 scored_notes=len(notes),median_pitch_difference=float(np.median([n['difference'] for n in notes])),
 near_octave_fraction=float(np.mean([abs(n['difference']-12)<1 for n in notes])),notes=notes,
 processing_seconds=time.monotonic()-started)
(work/'reference-analysis.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k not in ['notes','stem_paths']},ensure_ascii=True),flush=True)

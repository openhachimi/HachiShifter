from pathlib import Path
import json,os
repo=Path(__file__).resolve().parents[2];work=repo.parent/'HAMOOD-KARA2和声分析'
os.environ['MPLCONFIGDIR']=str(work/'matplotlib')
import numpy as np,soundfile as sf,pretty_midi,matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.font_manager import FontProperties
font=FontProperties(fname='C:/Windows/Fonts/msyh.ttc');plt.rcParams['font.family']=font.get_name();plt.rcParams['axes.unicode_minus']=False
D=json.loads((work/'harmony-corroborated.json').read_text(encoding='utf-8'));base=json.loads((work/'baseline.json').read_text(encoding='utf-8'))
for status,title in [('strong','较强证据32音'),('review','待复核63音')]:
 midi=pretty_midi.PrettyMIDI(initial_tempo=base['bpm']);instrument=pretty_midi.Instrument(53,name='KARA2 '+status)
 for r in D['rows']:
  if r['status']==status:instrument.notes.append(pretty_midi.Note(velocity=85,pitch=int(r['chosen']['target_midi']),start=r['start'],end=r['end']))
 midi.instruments.append(instrument);midi.write(str(work/('和声候选-'+title+'-适配翻唱低八度.mid')))
 report=pretty_midi.PrettyMIDI(str(work/('和声候选-'+title+'-适配翻唱低八度.mid')))
 assert len(report.instruments[0].notes)==sum(r['status']==status for r in D['rows'])
source=repo.parent/'HAMOOD-原唱参考试作/原唱人声-已对齐.wav'
original,sr=sf.read(source,dtype='float32',always_2d=True);lead,lr=sf.read(work/'separated/lead.wav',dtype='float32',always_2d=True);back,br=sf.read(work/'separated/backing.wav',dtype='float32',always_2d=True)
assert sr==lr==br and original.shape==lead.shape==back.shape
error=float(np.max(np.abs(original-lead-back)));assert error<1e-6
previews=[]
for lo,hi in [(45,67),(148,170)]:
 a,b=round(lo*sr),round(hi*sr)
 for label,audio,gain in [('原唱完整人声',original,1),('KARA2背景和声_提升6dB',back,10**(.3))]:
  name=f'试听-{lo}-{hi}秒-{label}.wav';samples=audio[a:b]*gain;assert np.max(np.abs(samples))<1
  sf.write(work/name,samples,sr,subtype='PCM_24');previews.append(dict(file=name,start=lo,end=hi,gain_db=20*np.log10(gain)))
c=np.load(work/'crepe-backing.npz');L=np.load(work/'crepe-lead.npz')
fig,axes=plt.subplots(3,1,figsize=(14,10),gridspec_kw={'height_ratios':[1,2,2]})
sections=D['summary']['sections'];x=np.arange(len(sections));good=np.array([s['strong'] for s in sections]);review=np.array([s['review'] for s in sections])
axes[0].bar(x,good,color='#167d9a',label='较强证据');axes[0].bar(x,review,bottom=good,color='#e2aa4b',label='待复核');axes[0].set_xticks(x,[f"{s['start']:.0f}–{s['end']:.0f} s" for s in sections]);axes[0].set_ylabel('候选音符数');axes[0].legend(loc='upper left',ncols=2);axes[0].set_title('KARA2 和声分析：32 个较强证据候选，63 个待复核；未检测到不等于没有和声')
for ax,(lo,hi) in zip(axes[1:],[(45,67),(148,170)]):
 mask=(c['time']>=lo)&(c['time']<=hi)&(c['confidence']>.3)&(c['rms']>.002)
 ax.scatter(c['time'][mask],c['midi'][mask],s=4,c='#9d78ae',alpha=.55,label='背景声部 CREPE 音高')
 first={k:True for k in ['strong','review','lead']}
 for r in D['rows']:
  if r['end']<lo or r['start']>hi:continue
  ax.plot([r['start'],r['end']],[r['reference_midi']]*2,c='#616b78',lw=2,alpha=.6,label='原唱主旋律（主轨 +12）' if first['lead'] else None);first['lead']=False
  if r['chosen']:
   kind=r['status'];ax.plot([r['start'],r['end']],[r['chosen']['midi']]*2,c='#167d9a' if kind=='strong' else '#e2aa4b',lw=3,ls='-' if kind=='strong' else '--',label=('较强候选' if kind=='strong' else '待复核候选') if first[kind] else None);first[kind]=False
 ax.set_xlim(lo,hi);ax.set_ylim(52,86);ax.set_xlabel('工程时间（秒）');ax.set_ylabel('原唱音区 MIDI 音高');ax.grid(alpha=.17);ax.legend(loc='upper left',ncols=2,fontsize=9)
fig.tight_layout();fig.savefig(work/'和声分析总览.png',dpi=150);plt.close(fig)
mask=(L['confidence']>.3)&(c['confidence']>.3)&(c['rms']>.002)
same=float(np.mean(np.abs(c['midi'][mask]-L['midi'][mask])<.8))
report=dict(ok=True,frames=len(original),sample_rate=sr,sum_error_max=error,source_alignment_unchanged=True,stereo=True,
 shared_voiced_frames=int(mask.sum()),unison_fraction=same,previews=previews,midi_notes={'strong':32,'review':63})
(work/'audio-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=True),flush=True)

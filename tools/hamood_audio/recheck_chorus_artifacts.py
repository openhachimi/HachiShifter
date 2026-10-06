"""Matched excerpt inference, diagnostics and audition files for chorus omissions."""
from pathlib import Path
import sys, os, json, time
BASE=Path(__file__).resolve().parents[3]
ROOT=Path(__file__).resolve().parent
WORK=BASE/'HAMOOD-副歌漏检复查'
OLD=BASE/'HAMOOD-KARA2和声分析'
sys.path.insert(0,str(BASE/'HachiShifter-整合版-0.2.3-DiffSinger/engines/diffsinger/packages'))
os.environ['MPLCONFIGDIR']=str(WORK/'matplotlib')
import numpy as np, soundfile as sf, soxr, onnxruntime as ort
from scipy.ndimage import gaussian_filter1d, maximum_filter1d, binary_closing
from recheck_chorus_filters import runs, times

def infer(audio, sr, model):
    # Same window/padding/output contract as Basic Pitch 0.4 run_inference.
    # Direct ONNX avoids unnecessary TensorFlow/Numba import and compilation.
    y=soxr.resample(audio,sr,22050).astype(np.float32)
    original_length=len(y); y=np.pad(y,(3840,0))
    predictions=[]
    for at in range(0,len(y),36164):
        chunk=y[at:at+43844]; chunk=np.pad(chunk,(0,43844-len(chunk)))
        note=model.run(['StatefulPartitionedCall:1'],{'serving_default_input_2:0':chunk[None,:,None]})[0]
        predictions.append(note[:,15:-15,:])
    result=np.concatenate(predictions).reshape(-1,88)[:int(np.floor(original_length*86/22050))]
    assert np.isfinite(result).all()
    return result

def count_evidence(back,lead,lo=2,hi=24):
    t=times(len(back)); dt=float(np.median(np.diff(t)))
    b=gaussian_filter1d(back,1,axis=0)
    peaks=b>=maximum_filter1d(b,3,axis=1)-1e-6
    observed=lead[:,24:70].argmax(axis=1)+45
    active=np.zeros(len(t),dtype=bool);events=[]
    for q in range(45,91):
        curve=b[:,q-21]; mask=binary_closing((curve>.20)&peaks[:,q-21],structure=np.ones(4,dtype=bool))
        for a,z in runs(mask):
            if (z-a)*dt<.09 or float(curve[a:z].max())<.32:continue
            if np.mean(np.abs(observed[a:z]-q)<2)>.75:continue
            active[a:z]=True;events.append(dict(start=float(t[a]),end=float(t[z-1]+dt),midi=q,
                                               mean=float(curve[a:z].mean())))
    sel=(t>=lo)&(t<hi)
    return dict(candidate_seconds=float(np.count_nonzero(active&sel)*dt),
                candidate_events=sum(e['start']<hi and e['end']>lo for e in events),events=events)

def main():
    options=ort.SessionOptions(); options.intra_op_num_threads=4;options.inter_op_num_threads=1
    model=ort.InferenceSession(str(ROOT/'reference-deps/basic_pitch/saved_models/icassp_2022/nmp.onnx'),options,providers=['CPUExecutionProvider'])
    results=[];start=time.monotonic()
    for lo,hi in [(43,69),(146,172)]:
        folder=WORK/f'{lo}-{hi}秒'; features={};channels=[]
        for route in ['旧','控制','新']:
            for stem in ['backing','lead']:
                a,sr=sf.read(folder/f'{route}KARA2-{stem}.wav',dtype='float32',always_2d=True)
                variants={'mid':a.mean(axis=1)}
                if stem=='backing':variants.update(left=a[:,0],right=a[:,1])
                for channel,wave in variants.items():
                    key=f'{route}-{stem}-{channel}';dest=folder/(key+'.npz')
                    if not dest.exists():np.savez_compressed(dest,note=infer(wave,sr,model))
                    features[key]=np.load(dest)['note']
                if stem=='backing':
                    mid=a.mean(axis=1);side=(a[:,0]-a[:,1])/2
                    channels.append(dict(route=route,lr_correlation=float(np.corrcoef(a.T)[0,1]),
                                         side_vs_mid_db=float(20*np.log10(np.sqrt(np.mean(side*side))/np.sqrt(np.mean(mid*mid))))))
                print(lo,route,stem,'inference',round(time.monotonic()-start,1),flush=True)
        section=dict(start=lo+2,end=hi-2,channels=channels,routes={})
        for route in ['旧','控制','新']:
            for version in ['mid','stereo_max']:
                back=features[f'{route}-backing-mid'] if version=='mid' else np.maximum.reduce(
                    [features[f'{route}-backing-'+v] for v in ['mid','left','right']])
                section['routes'][route+'-'+version]=count_evidence(back,features[f'{route}-lead-mid'])
        results.append(section)
        # Each file uses one common gain (+6dB); raw inference files stay FLOAT.
        for prefix,stem in [('A-原有分轨后的背景声部','控制KARA2-backing'),('B-重提人声后的背景声部','新KARA2-backing')]:
            a,sr=sf.read(folder/(stem+'.wav'),dtype='float32',always_2d=True)
            segment=a[2*sr:-2*sr]*10**(.3)
            assert np.abs(segment).max()<1
            sf.write(WORK/f'{lo+2}-{hi-2}秒-{prefix}-共同提升6dB.wav',segment,sr,subtype='PCM_24')
        # Loudness-matched B is extra, not a substitute for the common-gain comparison.
        a,sr=sf.read(folder/'控制KARA2-backing.wav',dtype='float32',always_2d=True);a=a[2*sr:-2*sr]
        b,_=sf.read(folder/'新KARA2-backing.wav',dtype='float32',always_2d=True);b=b[2*sr:-2*sr]
        gain=float(np.sqrt(np.mean(a*a))/np.sqrt(np.mean(b*b))*10**.3)
        assert np.abs(b*gain).max()<1
        sf.write(WORK/f'{lo+2}-{hi-2}秒-B-重提人声后-匹配A音量.wav',b*gain,sr,subtype='PCM_24')
        section['matched_B_gain_db']=float(20*np.log10(gain))
        print('comparison',json.dumps({k:{v:m[v] for v in ['candidate_seconds','candidate_events']} for k,m in section['routes'].items()},ensure_ascii=True),flush=True)
    (WORK/'pitch-comparison.json').write_text(json.dumps(dict(sections=results,seconds=time.monotonic()-start,
        caveat='These are candidate-activity measurements, not precision/recall; separator energy is not a quality metric.'),ensure_ascii=False,indent=2),encoding='utf-8')
    make_artifacts()

def make_artifacts():
    import pretty_midi,matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.font_manager import FontProperties
    plt.rcParams['font.family']=FontProperties(fname='C:/Windows/Fonts/msyh.ttc').get_name()
    plt.rcParams['axes.unicode_minus']=False
    analysis=json.loads((WORK/'filter-ablation.json').read_text(encoding='utf-8'))
    old=json.loads((OLD/'harmony-corroborated.json').read_text(encoding='utf-8'))
    events=analysis['continuous_events']
    midi=pretty_midi.PrettyMIDI(initial_tempo=102)
    track=pretty_midi.Instrument(53,name='Continuous candidates - review required')
    octave=pretty_midi.Instrument(53,name='Octave doubling or octave error - review')
    for e in events:
        if e['type']=='possible_lead_or_unison':continue
        target=octave if e['type']=='octave_doubling_or_octave_error' else track
        target.notes.append(pretty_midi.Note(pitch=e['target_midi'],velocity=75,start=max(0,e['start']),end=e['end']))
    midi.instruments=[track,octave]
    path=WORK/'连续和声候选-待复核-低八度.mid';midi.write(str(path))
    check=pretty_midi.PrettyMIDI(str(path));assert sum(len(i.notes) for i in check.instruments)==sum(len(i.notes) for i in midi.instruments)
    fig,axs=plt.subplots(2,1,figsize=(14,8))
    for ax,(lo,hi) in zip(axs,[(45,67),(148,170)]):
        first={'old':True,'candidate':True,'lead':True}
        for r in old['rows']:
            if r['end']<lo or r['start']>hi:continue
            ax.plot([r['start'],r['end']],[r['reference_midi']]*2,c='#89949d',lw=1.2,alpha=.5,
                    label='主轨 +12（仅作位置参照）' if first['lead'] else None);first['lead']=False
            if r['status']=='strong':
                ax.plot([r['start'],r['end']],[r['chosen']['midi']]*2,c='#087f8c',lw=6,
                        label='旧结果保留' if first['old'] else None);first['old']=False
        for e in events:
            if e['end']<lo or e['start']>hi or e['type']=='possible_lead_or_unison':continue
            ax.plot([e['start'],e['end']],[e['midi']]*2,c='#d58b22',lw=2.5,alpha=.9,
                    label='连续检测线索（待复核）' if first['candidate'] else None);first['candidate']=False
        section=next(s for s in analysis['sections'] if s['start']==lo)
        ax.set_title(f"{lo}–{hi} 秒：旧较强候选覆盖 {section['old_strong_time_seconds']:.1f} 秒；连续线索覆盖 {section['continuous_candidate_time_seconds']:.1f} 秒")
        ax.set_xlim(lo,hi);ax.set_ylim(45,90);ax.set_xlabel('工程时间 / 秒');ax.set_ylabel('原唱音区 / MIDI');ax.grid(alpha=.15)
        ax.legend(loc='upper right',fontsize=9,ncols=3)
    fig.suptitle('副歌漏检复查 · 更多线索不代表全部正确，尚需核对泛音、主唱残留和八度判断',fontsize=12)
    fig.tight_layout();fig.savefig(WORK/'副歌漏检对比.png',dpi=150);plt.close(fig)
    print('MIDI',len(track.notes),len(octave.notes),flush=True)

if __name__=='__main__':main()

"""Reference-informed chorus arrangement, with explicit inferred/composed provenance."""
from pathlib import Path
import json
import numpy as np
from scipy.ndimage import gaussian_filter1d
from recheck_chorus_filters import times

BASE=Path(__file__).resolve().parents[3]
WORK=BASE/'HAMOOD-连续和声试作'
OLD=BASE/'HAMOOD-KARA2和声分析'
CHECK=BASE/'HAMOOD-副歌漏检复查'
SECTIONS=[(42.2,67.1),(98.69,123.6),(132.23,170.66)]

def main():
    WORK.mkdir(exist_ok=True)
    baseline=json.loads((OLD/'baseline.json').read_text(encoding='utf-8'))
    notes=sorted(baseline['tracks'][0]['clips'][0]['notes'],key=lambda n:n['start_seconds'])
    back=np.load(OLD/'pitch-backing.npz')['note']; lead=np.load(OLD/'pitch-lead.npz')['note']
    t=times(len(back)); back=gaussian_filter1d(back,.8,axis=0)
    # Use stereo corroboration in the independently checked excerpts. The full
    # original-vocal route is retained; the new first-stage model lost evidence.
    for lo,hi in [(43,69),(146,172)]:
        folder=CHECK/f'{lo}-{hi}秒'
        channels=[np.load(folder/f'控制-backing-{channel}.npz')['note'] for channel in ['mid','left','right']]
        stereo=np.maximum.reduce(channels); local=times(len(stereo))+lo
        mask=(t>=lo+2)&(t<hi-2)
        for q in range(88):
            extra=np.interp(t[mask],local,stereo[:,q])
            back[mask,q]=np.maximum(back[mask,q],.75*extra+.25*back[mask,q])
    chords=json.loads((BASE/'HAMOOD-东京泰迪熊试作/audio-context-final.json').read_text(encoding='utf-8'))['chords']
    crepe=np.load(OLD/'crepe-backing.npz')
    selected=[n for n in notes if any(a<=n['start_seconds']<b for a,b in SECTIONS)]
    qs=np.arange(57,86); observations=[]
    for n in selected:
        lo=n['start_seconds'];hi=lo+n['duration_seconds'];margin=min(.035,(hi-lo)*.13)
        mask=(t>=lo+margin)&(t<hi-margin)
        if not mask.any():mask[np.argmin(abs(t-(lo+hi)/2))]=True
        quant=np.quantile(back[mask],.7,axis=0); avg=back[mask].mean(axis=0)
        occupancy=(back[mask]>.2).mean(axis=0); l=lead[mask].mean(axis=0)
        pcs=np.zeros(12)
        for c in chords:
            overlap=max(0,min(hi,c['end'])-max(lo,c['start']))
            if overlap:pcs[c['pitch_classes']]+=overlap/(hi-lo)*c['score']
        cmask=(crepe['time']>=lo+margin)&(crepe['time']<hi-margin)&(crepe['confidence']>.3)&(crepe['rms']>.0015)
        support=np.array([float(np.mean(abs(crepe['midi'][cmask]-q)<.8)) if cmask.any() else 0 for q in qs])
        observations.append(dict(quant=quant,avg=avg,occupancy=occupancy,lead=l,pcs=pcs,crepe=support))
    def solve(kind,lower_path=None):
        emission=[]
        for i,(n,o) in enumerate(zip(selected,observations)):
            p=round(n['midi']+12); diff=qs-p
            if kind=='main':valid=(diff<=-2)&(diff>=-19)&(qs<=76)
            else:valid=(diff>=2)&(diff<=12)&(qs>=65)&(qs<=84)
            b=o['quant'][qs-21];occ=o['occupancy'][qs-21]
            evidence=3.0*np.clip((b-.115)/.5,0,1)+.65*np.minimum(occ,.8)
            # A weak second/third harmonic is not automatically another singer.
            for k,q in enumerate(qs):
                below=max(o['quant'][q-21-12],o['quant'][q-21-19])
                if below>.3 and below>b[k]*.85:evidence[k]-=.45
            intervals=abs(diff)%12
            consonance=np.array([{0:.00,1:-1.45,2:-.65,3:.48,4:.48,5:.32,6:-1.30,7:.32,8:.32,9:.32,10:-.45,11:-1.45}[int(x)] for x in intervals])
            score=evidence+consonance+.4*o['pcs'][qs%12]+.35*o['crepe']
            # Leakage remains a soft penalty: no full-vocal or expected-lead gate.
            ratio=(o['avg'][qs-21]+.03)/(o['lead'][qs-21]+.03)
            score-=.25*np.clip(1-ratio,0,1)
            if kind=='main':score-=.018*abs(qs-65)
            if lower_path is not None:
                interval=qs-lower_path[i]
                score-=np.where(interval%12==0,.60,0)  # avoid merely doubling the lower voice
                score-=np.where(np.isin(interval%12,[1,2,6,10,11]),.45,0)
            score[~valid]=-1e5
            emission.append(score)
        emission=np.asarray(emission);cost=np.zeros_like(emission);parent=np.zeros_like(emission,dtype=int)
        cost[0]=emission[0]
        distance=abs(qs[:,None]-qs[None,:])
        transition=.065*np.minimum(distance,12)+.055*np.maximum(distance-5,0)**1.3
        for i in range(1,len(selected)):
            gap=selected[i]['start_seconds']-selected[i-1]['start_seconds']-selected[i-1]['duration_seconds']
            weight=.25 if gap>.4 else 1.0
            scores=cost[i-1][:,None]-weight*transition
            parent[i]=scores.argmax(axis=0);cost[i]=emission[i]+scores.max(axis=0)
        path=np.empty(len(selected),dtype=int);p=int(cost[-1].argmax())
        for i in range(len(selected)-1,-1,-1):path[i]=qs[p];p=parent[i,p]
        return path
    lower=solve('main');upper=solve('upper',lower)
    voices={}
    for kind,path in [('main',lower),('upper',upper)]:
        rows=[]
        for i,(n,o,q) in enumerate(zip(selected,observations,path)):
            q=int(q);b=float(o['quant'][q-21]);occ=float(o['occupancy'][q-21]);avg=float(o['avg'][q-21])
            evidence=b>=.23 and occ>=.2
            # Upper entries require stronger, non-octave evidence so it stays a
            # light additional voice, instead of continuously doubling the lead.
            keep=kind=='main' or (b>=.28 and occ>=.28 and (q-round(n['midi']+12))%12!=0)
            rows.append(dict(source_note_id=n['id'],start=n['start_seconds'],end=n['start_seconds']+n['duration_seconds'],
                             lyric=n['label'],source_midi=n['midi'],reference_midi=q,target_midi=q-12,
                             score=b,occupancy=occ,mean_score=avg,
                             provenance='audio_supported_arrangement' if evidence else 'composed_connection',keep=bool(keep)))
        if kind=='upper':
            # Fill one short missing syllable only when surrounded by supported
            # upper voice, preserving phrase continuity without claiming evidence.
            original=[r['keep'] for r in rows]
            for i in range(1,len(rows)-1):
                r=rows[i]
                if not original[i] and original[i-1] and original[i+1] and r['end']-r['start']<=.31 and rows[i+1]['start']-rows[i-1]['end']<.7:
                    if max(abs(r['reference_midi']-rows[j]['reference_midi']) for j in [i-1,i+1])<=4:
                        r['keep']=True;r['provenance']='composed_connection'
        rows=[r for r in rows if r['keep']]
        # Preserve subtle movement, while rebuilding entry/exit pitches around
        # this voice's own previous/next notes (not copied lead-note transitions).
        byid={n['id']:n for n in selected}
        for i,r in enumerate(rows):
            n=byid[r['source_note_id']];at=r['start'];dur=r['end']-at;q=r['target_midi']
            prev=rows[i-1] if i else None
            nearby=prev and at-prev['end']<.22
            incoming=prev['target_midi'] if nearby else q
            incoming=q+np.clip(incoming-q,-5,5)
            settle=min(.06,dur*.25)
            points=[(at-.025,float(incoming)),(at+settle,float(q))]
            # Retain moderate, within-note original expression only, capped so
            # a copied lead glide cannot bend into the wrong harmony interval.
            contour=n.get('contour',[])
            for rel in np.arange(max(settle+.03,.09),max(settle+.03,dur-.035),.09):
                if contour:
                    nearest=min(contour,key=lambda c:abs(c['time_seconds']-rel))
                    cents=float(nearest.get('rendered_target_cents',0))
                    detune=float(np.clip(cents/100,-.45,.45)*.5)
                else:detune=0.
                points.append((at+rel,q+detune))
            points.append((at+max(settle,dur-.012),float(q)))
            r['pitch_curve']=[dict(time_seconds=float(x),midi=float(y)) for x,y in sorted(set(points))]
        voices[kind]=rows
    summary=dict(sections=SECTIONS,source_notes=len(notes),selected_notes=len(selected),voices={k:dict(notes=len(v),
        audio_supported=sum(r['provenance']=='audio_supported_arrangement' for r in v),
        composed_connections=sum(r['provenance']=='composed_connection' for r in v),
        minimum_midi=min(r['target_midi'] for r in v),maximum_midi=max(r['target_midi'] for r in v)) for k,v in voices.items()},
        exact_original_restoration=False,original_to_cover_shift=-12)
    (WORK/'arrangement.json').write_text(json.dumps(dict(summary=summary,voices=voices),ensure_ascii=False,indent=2),encoding='utf-8')
    import pretty_midi
    midi=pretty_midi.PrettyMIDI(initial_tempo=102)
    for kind,rows in voices.items():
        inst=pretty_midi.Instrument(53,name='Reference-informed '+kind)
        for r in rows:inst.notes.append(pretty_midi.Note(pitch=r['target_midi'],velocity=83 if kind=='main' else 65,start=r['start'],end=r['end']))
        midi.instruments.append(inst)
    midi.write(str(WORK/'东京泰迪熊-连续和声编配.mid'))
    print(json.dumps(summary,ensure_ascii=True,indent=2),flush=True)
    for kind,rows in voices.items():
        print(kind,'FIRST PHRASE',[(round(r['start'],2),r['target_midi'],r['target_midi']-r['source_midi'],round(r['score'],2)) for r in rows if 47.5<=r['start']<53],flush=True)

if __name__=='__main__':main()

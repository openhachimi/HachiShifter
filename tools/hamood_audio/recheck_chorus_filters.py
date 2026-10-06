"""Filter ablation and time-based polyphonic candidates; scores are not ground truth."""
from pathlib import Path
import json, os
import numpy as np
from scipy.ndimage import gaussian_filter1d, maximum_filter1d, binary_closing

BASE = Path(__file__).resolve().parents[3]
OLD = BASE / 'HAMOOD-KARA2和声分析'
WORK = BASE / 'HAMOOD-副歌漏检复查'
WORK.mkdir(exist_ok=True)

def times(n):
    i = np.arange(n)
    return i * 256 / 22050 - np.floor(i/172) * (256/22050*(172-43844/256)+.0018) + .020

def runs(mask):
    edges = np.diff(np.r_[False, mask, False].astype(int))
    return list(zip(np.flatnonzero(edges==1), np.flatnonzero(edges==-1)))

def main():
    d = json.loads((OLD/'harmony-corroborated.json').read_text(encoding='utf-8'))
    full, lead, back = [np.load(OLD/('pitch-'+k+'.npz'))['note'] for k in ['full','lead','backing']]
    t = times(len(back)); dt = float(np.median(np.diff(t)))
    stages = ['旧条件（未经过单音高复核）','只扩大音域到±24半音','再保留八度叠唱候选',
              '再取消完整人声分数硬门槛','再取消主轨指定音高硬门槛','再将主唱泄漏比较改为标记']
    rows = []
    for r in d['rows']:
        p = r['reference_midi']; lo, hi = r['start'], r['end']; margin = min(.06, (hi-lo)*.17)
        mask = (t>lo+margin)&(t<hi-margin)
        if mask.sum()<5: continue
        b, l, f = [arr[mask].mean(axis=0) for arr in [back,lead,full]]
        candidates = []
        for q in range(max(45,p-24), min(96,p+25)):
            if abs(q-p)<2: continue
            ix = q-21; c = back[mask,ix]; occ = float(np.mean(c>.18))
            sustain = max((end-start for start,end in runs(c>.18)),default=0)*dt
            gate = {'narrow_register':abs(q-p)<=9, 'non_octave':(q-p)%12!=0,
                    'backing':bool(b[ix]>=.22), 'occupancy':occ>=.55, 'sustain':bool(sustain>=.085),
                    'full_mix':bool(f[ix]>=.14), 'lead_expected_pitch':bool(l[p-21]>=.25),
                    'selectivity':bool((b[ix]+.03)/(l[ix]+.03)>=1.15)}
            base = gate['backing'] and gate['occupancy'] and gate['sustain']
            keep = [base and all(gate.values()),
                    base and all(gate[k] for k in ['non_octave','full_mix','lead_expected_pitch','selectivity']),
                    base and all(gate[k] for k in ['full_mix','lead_expected_pitch','selectivity']),
                    base and all(gate[k] for k in ['lead_expected_pitch','selectivity']),
                    base and gate['selectivity'], base]
            candidates.append(dict(midi=q,score=float(b[ix]),occupancy=occ,stages=keep, gates=gate))
        rows.append(dict(id=r['id'],start=lo,end=hi,old_status=r['status'],
                         stages=[any(c['stages'][i] for c in candidates) for i in range(len(stages))],
                         candidates=sorted(candidates,key=lambda x:x['score'],reverse=True)[:6]))
    sections = []
    for lo,hi in [(0,194.13229),(45,67),(148,170)]:
        subset=[r for r in rows if lo<=r['start']<hi]
        section=dict(start=lo,end=hi,source_notes=len(subset),old_strong=sum(r['old_status']=='strong' for r in subset),
                     old_review=sum(r['old_status']=='review' for r in subset),
                     ablation={stage:sum(r['stages'][i] for r in subset) for i,stage in enumerate(stages)})
        sections.append(section)
    # A separate high-recall path uses continuous time and permits overlapping pitches.
    # CREPE and lead agreement are annotations, never polyphonic rejection gates.
    smoothed=gaussian_filter1d(back,1.0,axis=0)
    maxima=smoothed>=maximum_filter1d(smoothed,size=3,axis=1)-1e-6
    crepe=np.load(OLD/'crepe-backing.npz'); ctime=crepe['time']
    events=[]
    for q in range(45,91):
        ix=q-21; curve=smoothed[:,ix]
        active=binary_closing((curve>.20)&maxima[:,ix],structure=np.ones(4,dtype=bool))
        for a,z in runs(active):
            start,end=float(t[a]),float(t[z-1]+dt)
            if end-start<.09 or float(curve[a:z].max())<.32:continue
            avg=float(back[a:z,ix].mean()); l=float(lead[a:z,ix].mean())
            cmask=(ctime>=start)&(ctime<end)&(crepe['confidence']>.3)&(crepe['rms']>.0015)
            agreement=float(np.mean(np.abs(crepe['midi'][cmask]-q)<.8)) if cmask.any() else None
            overlaps=[r for r in d['rows'] if r['start']<end and r['end']>start]
            reference=next((r['reference_midi'] for r in overlaps if r['start']<=(start+end)/2<r['end']),None)
            delta=q-reference if reference is not None else None
            label='possible_lead_or_unison' if delta is not None and abs(delta)<2 else (
                  'octave_doubling_or_octave_error' if delta is not None and delta%12==0 else 'harmony_candidate')
            events.append(dict(start=start,end=end,midi=q,target_midi=q-12,mean_score=avg,
                               peak_score=float(curve[a:z].max()),selectivity=(avg+.03)/(l+.03),
                               crepe_agreement=agreement,reference_midi=reference,type=label,
                               source_note_ids=[r['id'] for r in overlaps],verified=False))
    for section in sections:
        lo,hi=section['start'],section['end']; mask=(t>=lo)&(t<hi); active=np.zeros(len(t),dtype=bool)
        for e in events:
            if e['type']=='possible_lead_or_unison':continue
            active|=(t>=e['start'])&(t<e['end'])
        old=np.zeros(len(t),dtype=bool)
        for r in d['rows']:
            if r['status']=='strong':old|=(t>=r['start'])&(t<r['end'])
        section['old_strong_time_seconds']=float(np.count_nonzero(old&mask)*dt)
        section['continuous_candidate_time_seconds']=float(np.count_nonzero(active&mask)*dt)
        section['continuous_candidates']=sum(e['start']<hi and e['end']>lo and e['type']!='possible_lead_or_unison' for e in events)
    output=dict(caveat='Candidate coverage is not measured recall or verified harmony. No annotated ground truth is available.',
                stages=stages,sections=sections,rows=rows,continuous_events=events,manual_confirmation_required=True)
    (WORK/'filter-ablation.json').write_text(json.dumps(output,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(sections,ensure_ascii=True,indent=2),flush=True)

if __name__=='__main__':main()

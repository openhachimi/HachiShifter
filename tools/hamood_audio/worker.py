"""Offline HAMOOD audio evidence worker. stdout is a JSON result, never a log.
BTC implementation/weights: jayg996/BTC-ISMIR19; Beat This!: CPJKU/beat_this.
Score fields are uncalibrated model scores, not accuracy estimates.
"""
from pathlib import Path
import sys, json, hashlib, time, os
ROOT = Path(__file__).resolve().parent
sys.path[:0] = [str(ROOT/'deps'), str(ROOT/'vendor/BTC-ISMIR19')]
VERSION = '036-audio-v2'
NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
QUALITIES = ['min','maj','dim','aug','min6','maj6','min7','minmaj7','maj7','7','dim7','hdim7','sus2','sus4']
INTERVALS = [[0,3,7],[0,4,7],[0,3,6],[0,4,8],[0,3,7,9],[0,4,7,9],[0,3,7,10],[0,3,7,11],[0,4,7,11],[0,4,7,10],[0,3,6,9],[0,3,6,10],[0,2,7],[0,5,7]]
def digest(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def label(index):
    if index>=168:return ('X' if index==168 else 'N'),[]
    root,quality=divmod(int(index),14)
    return NAMES[root]+('' if quality==1 else ':'+QUALITIES[quality]), [(root+p)%12 for p in INTERVALS[quality]]
def analyse(request):
    source=Path(request['audio_path']).resolve()
    cache=Path(request['cache_dir']).resolve();cache.mkdir(parents=True,exist_ok=True)
    weights=ROOT/'vendor/BTC-ISMIR19/test/btc_model_large_voca.pt'
    beat_weights=ROOT/'models/beat-this-small0.ckpt'
    source_hash=digest(source)
    identity={'schema':VERSION,'audio_sha256':source_hash,'decoder':request.get('decoder','libsndfile'),'btc_sha256':digest(weights),'beat_sha256':digest(beat_weights)}
    key=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()
    destination=cache/(key+'.json')
    if destination.exists() and not request.get('force',False):
        data=json.loads(destination.read_text(encoding='utf-8'));data['cache_hit']=True;data['cache_file']=str(destination)
        return data
    os.environ['NUMBA_CACHE_DIR']=str(cache/'numba')
    os.environ['NUMBA_CACHE_LOCATOR_CLASSES']='UserProvidedCacheLocator'
    import faulthandler
    faulthandler.dump_traceback_later(90,repeat=True)
    import numpy as np, soundfile as sf, librosa, soxr, torch, yaml
    from scipy.ndimage import uniform_filter1d
    from beat_this.inference import Audio2Beats
    from btc_model import BTC_model
    torch.set_num_threads(min(4,os.cpu_count() or 1))
    start=time.monotonic()
    stereo,sr=sf.read(request.get('decoded_audio_path',str(source)),dtype='float32',always_2d=True)
    audio=stereo.mean(axis=1);duration=len(audio)/sr
    beat_model=Audio2Beats(checkpoint_path=str(beat_weights),device='cpu',dbn=False)
    with torch.inference_mode(): beats,downbeats=beat_model(audio,sr)
    beats=np.asarray(beats);downbeats=np.asarray(downbeats)
    print("Beat model complete",len(beats),flush=True,file=sys.stderr)
    y=soxr.resample(audio,sr,22050);rate=22050;hop=2048
    config=yaml.safe_load((ROOT/'vendor/BTC-ISMIR19/run_config.yaml').read_text())['model']
    config['num_chords']=170
    model=BTC_model(config).eval()
    # Only accept tensor/scalar checkpoints from the fixed bundled model path.
    with torch.serialization.safe_globals([(np._core.multiarray.scalar,"numpy.core.multiarray.scalar"),np.dtype,np.dtypes.Float64DType,np.dtypes.Float32DType]):
        checkpoint=torch.load(weights,map_location='cpu',weights_only=True)
    model.load_state_dict(checkpoint['model'])
    mean=float(checkpoint['mean']);std=float(checkpoint['std'])
    frame_times=[];frame_probs=[];frame_bass=[]
    # Match the author's 10-second CQT windows, but use the real hop time in
    # each window. Never accumulate the 10/108 approximation across a song.
    for offset in range(0,len(y),rate*10):
        print("BTC",offset/rate,flush=True,file=sys.stderr)
        piece=y[offset:offset+rate*10]
        if len(piece)<2048:piece=np.pad(piece,(0,2048-len(piece)))
        cqt=np.abs(librosa.cqt(piece,sr=rate,n_bins=144,bins_per_octave=24,hop_length=hop))
        feature=(np.log(cqt+1e-6).T-mean)/std
        count=min(108,len(feature));padded=np.pad(feature[:count],((0,108-count),(0,0)))
        with torch.inference_mode():
            encoded,_=model.self_attn_layers(torch.tensor(padded,dtype=torch.float32).unsqueeze(0))
            probabilities=torch.softmax(model.output_layer.output_projection(encoded),dim=-1)[0,:count].numpy()
        # Low-register chroma evidence only: not NNLS and not a bass stem transcription.
        bass=cqt[:48].reshape(24,2,-1).mean(axis=1).reshape(2,12,-1).sum(axis=0).T
        bass=bass/(bass.sum(axis=1,keepdims=True)+1e-9)
        for i in range(count):
            t=offset/rate+i*hop/rate
            if t>=duration:break
            frame_times.append(t);frame_probs.append(probabilities[i]);frame_bass.append(bass[i])
    times=np.asarray(frame_times);prob=np.asarray(frame_probs);bass=np.asarray(frame_bass)
    smooth=uniform_filter1d(prob,size=3,axis=0,mode='nearest');pred=smooth.argmax(axis=1)
    # Remove isolated frame flips without concealing genuinely uncertain scores.
    for i in range(1,len(pred)-1):
        if pred[i-1]==pred[i+1] and pred[i]!=pred[i-1]:pred[i]=pred[i-1]
    boundaries=np.r_[0,np.flatnonzero(np.diff(pred)!=0)+1,len(pred)]
    chords=[]
    for first,last in zip(boundaries[:-1],boundaries[1:]):
        scores=prob[first:last].mean(axis=0);idx=int(pred[first]);name,pcs=label(idx)
        ranked=np.argsort(scores)[-3:][::-1]
        lo=float(times[first]);hi=float(times[last]) if last<len(times) else duration
        low=bass[first:last].mean(axis=0);low_ranks=np.argsort(low)[-3:][::-1]
        chords.append({'start':lo,'end':hi,'label':name,'pitch_classes':pcs,'score':float(scores[idx]),
            'candidates':[{'label':label(k)[0],'score':float(scores[k])} for k in ranked],
            'bass_candidates':[{'pitch_class':int(k),'name':NAMES[k],'relative_strength':float(low[k])} for k in low_ranks]})
    bpm=float(60/np.median(np.diff(beats))) if len(beats)>1 else None
    data={'ok':True,'schema':VERSION,'identity':identity,'source_file':str(source),'source_size':source.stat().st_size,
        'source_mtime_ms':int(source.stat().st_mtime_ns//1000000),'duration':duration,'models':{'beats':'Beat This! small0','chords':'BTC large-vocabulary 170','bass':'CQT low-register chroma (heuristic, not isolated bass)'},
        'bpm':bpm,'beats':beats.tolist(),'downbeats':downbeats.tolist(),'chords':chords,'cache_hit':False,
        'processing_seconds':time.monotonic()-start,'score_semantics':'uncalibrated; low confidence or N/X must not be treated as certain chords'}
    tmp=destination.with_suffix('.json.tmp');tmp.write_text(json.dumps(data,ensure_ascii=False),encoding='utf-8');tmp.replace(destination)
    # Compact evidence for alignment/debugging; MCP consumes the JSON events.
    np.savez_compressed(cache/(key+'.npz'),times=times,probabilities=prob,bass_chroma=bass)
    data['cache_file']=str(destination)
    faulthandler.cancel_dump_traceback_later()
    return data
if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('--request',required=True);parser.add_argument('--output',required=True)
    args=parser.parse_args()
    try:result=analyse(json.loads(Path(args.request).read_text(encoding='utf-8-sig')))
    except Exception as e:
        import traceback
        traceback.print_exc(file=sys.stderr);result={'ok':False,'error':str(e)}
    Path(args.output).write_text(json.dumps(result,ensure_ascii=False),encoding='utf-8')
    sys.exit(0 if result.get('ok') else 1)

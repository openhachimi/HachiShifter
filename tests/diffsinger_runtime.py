"""Sampling/preflight/worker checks and old-vs-new audition timings.
packaged-python -I this.py new_bridge old_bridge bank out
"""
from pathlib import Path
from types import SimpleNamespace
import copy
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from unittest.mock import patch

sys.stdout.reconfigure(encoding='utf-8')
new, old, voice, output = map(lambda p:Path(p).resolve(), sys.argv[1:5])
output.mkdir(parents=True, exist_ok=True)
spec=importlib.util.spec_from_file_location('bridge',new)
b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
np=b.np
import soundfile as sf
c=b.compatibility
checks=[]
def passed(name): checks.append(name);print('PASS',name,flush=True)
def inp(name, dtype='tensor(int64)'):return SimpleNamespace(name=name,type=dtype)

for depth,expected,stride in [(1,1000,50),(.5,500,25),(.53,520,26),(.001,1,1)]:
    actual=c.sampling_inputs({'max_depth':depth},[inp('depth'),inp('speedup')])
    assert actual=={'depth':[expected],'speedup':[stride]},actual
assert c.sampling_inputs({},[inp('speedup')],30)=={'speedup':[25]}
assert c.sampling_inputs({'max_depth':.53},[inp('depth','tensor(float)'),inp('steps')])=={'depth':[.53],'steps':[20]}
for depth in [0,-1,1.1,float('nan')]:
    try:c.sampling_inputs({'max_depth':depth},[inp('depth')])
    except ValueError:pass
    else:raise AssertionError('invalid depth accepted')
passed('continuous and legacy depth, stride divisibility, invalid depth')

# Use the real Model.run conversion rather than testing the helper alone.
class Capture:
    def get_inputs(self):return [inp('depth'),inp('speedup')]
    def get_outputs(self):return [SimpleNamespace(name='out')]
    def run(self,_,feed):self.feed=feed;return [np.zeros((1,1),np.float32)]
model=object.__new__(b.Model);model.root=Path('contract');model.config={'max_depth':.5}
session=Capture();model.session=lambda _:session
model.run('acoustic',{})
assert session.feed['depth'].dtype==np.int64 and session.feed['depth'][0]==500
passed('legacy 0.5 reaches model as int64 500')

# Small synthetic bank: validate actual files, metadata and diagnostics without
# duplicating or modifying any installed voicebank or downloading models.
with tempfile.TemporaryDirectory(dir=output) as temp:
    root=Path(temp);(root/'dsvocoder').mkdir()
    config={'phonemes':'phones.json','acoustic':'acoustic.onnx','vocoder':'dsvocoder'}
    (root/'phones.json').write_text('{"SP":0,"a":1}')
    (root/'acoustic.onnx').write_bytes(b'metadata-substitute')
    (root/'dsvocoder/model.onnx').write_bytes(b'metadata-substitute')
    (root/'dsvocoder/vocoder.yaml').write_text('model: model.onnx\n')
    (root/'dsdict-zh.yaml').write_text('entries:\n- grapheme: a\n  phonemes: [a]\n')
    def write_config(): (root/'dsconfig.yaml').write_text(b.yaml.safe_dump(config))
    write_config()
    class Metadata:
        def __init__(self,names):self.names=names
        def get_inputs(self):return [inp(name,'tensor(float)' if name in ('f0','mel') else 'tensor(int64)') for name in self.names]
        def get_outputs(self):return [SimpleNamespace(name='mel')]
    def inspect():return b.Bank(root).inspect()
    with patch.object(b.Model,'session',lambda *a:Metadata(['tokens','durations','f0','speedup'])),patch.object(b.Bank,'vocoder_session',lambda *a:Metadata(['mel','f0'])):
        result=inspect();assert result['ok'] and not result['has_pitch'] and len(result['warnings'])==2
        passed('absent optional predictors allow acoustic synthesis with warnings')
        (root/'acoustic.onnx').unlink()
        result=inspect();assert not result['ok'] and 'acoustic.onnx' in result['error']
        (root/'acoustic.onnx').write_bytes(b'metadata-substitute')
        (root/'dsvocoder/model.onnx').unlink()
        result=inspect();assert not result['ok'] and 'model.onnx' in result['error']
        (root/'dsvocoder/model.onnx').write_bytes(b'metadata-substitute')
        (root/'dsvocoder/vocoder.yaml').write_text('sample_rate: 48000\n')
        result=inspect();assert not result['ok'] and 'sample_rate' in result['error']
        (root/'dsvocoder/vocoder.yaml').write_text('model: model.onnx\n')
        config.update(speakers=['missing'],hidden_size=2);write_config()
        assert 'missing.emb' in inspect()['error']
        np.array([1],dtype='<f4').tofile(root/'missing.emb')
        assert 'embedding' in inspect()['error']
        config.pop('speakers');config.pop('hidden_size');write_config()
        (root/'dsdur').mkdir()
        assert 'dsconfig.yaml' in inspect()['error']
        (root/'dsdur').rmdir()
        config['use_energy_embed']=True;write_config()
        assert 'dsvariance' in inspect()['error']
        config.pop('use_energy_embed');write_config()
        passed('missing files, bad embedding, vocoder mismatch and incomplete predictors reported')
    with patch.object(b.Model,'session',lambda *a:Metadata(['unknown_input'])),patch.object(b.Bank,'vocoder_session',lambda *a:Metadata(['mel','f0'])):
        assert 'unknown_input' in inspect()['error']
        passed('unsupported ONNX input rejected during preflight')
    request={'voicebank':str(root)}
    loads=[]
    rt=b.runtime.Runtime(lambda *args:loads.append(args) or object())
    a,reused=rt.prepare(request);assert not reused
    same,reused=rt.prepare(request);assert reused and same is a
    (root/'phones.json').write_text('{"SP":0,"a":1,"b":2}')
    changed,reused=rt.prepare(request);assert not reused and changed is not a
    rt.release();assert rt.bank is None
    passed('bank file edits invalidate model reuse; explicit release clears bank')

cache=b.runtime.Cache(limit=10,entries=2)
cache.put('a','A',6);cache.put('b','B',6)
assert cache.get('a') is None and cache.get('b')=='B'
cache.put('c','C',20);assert cache.size<=10 and cache.get('c') is None
passed('cache memory bound and eviction')

notes=[dict(id='n1',start=.3,duration=.7,midi=60,lyric='ni'),
       dict(id='n2',start=1.,duration=.9,midi=64,lyric='hao')]
base=dict(operation='render',voicebank=str(voice),language='zh',duration=2.4,notes=notes)
dyn=copy.deepcopy(base)
for note in dyn['notes']:note['expressions']={'DS:DYN':[[0,-120]]}
times={}
for label,request in [('old-cold',base),('old-dyn-edit',dyn)]:
    source=output/(label+'-request.json');target=output/(label+'.json')
    source.write_text(json.dumps(request),encoding='utf-8')
    start=time.monotonic()
    run=subprocess.run([sys.executable,'-I',str(old),str(source),str(target)],capture_output=True,timeout=180)
    times[label]=time.monotonic()-start
    assert run.returncode==0,(label,run.stderr)
    assert json.loads(target.read_text(encoding='utf-8'))['ok']
    print(label,times[label],flush=True)

ipc=output/'ipc';ipc.mkdir(exist_ok=True)
env=dict(os.environ,HACHI_DS_IDLE_SECONDS='2')
worker=subprocess.Popen([sys.executable,'-I',str(new),'--worker',str(ipc),str(os.getpid())],env=env,
    stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
records=[]
def call(label,request):
    ready=ipc/'result.json';ready.unlink(missing_ok=True)
    payload=dict(request,request_id=label)
    start=time.monotonic();b.runtime.atomic_json(ipc/'request.json',payload)
    while not ready.exists():
        assert worker.poll() is None,'worker crashed'
        assert time.monotonic()-start<180,'worker timeout'
        time.sleep(.01)
    result=json.loads(ready.read_text(encoding='utf-8'))
    assert result['request_id']==label and result['ok'],result
    times[label]=time.monotonic()-start
    records.append(dict(case=label,**result.get('runtime',{})))
    audio=None
    if request['operation']=='render':
        audio,sr=sf.read(ipc/'result.wav',dtype='float32')
        assert sr==44100 and np.isfinite(audio).all() and np.max(np.abs(audio))>.001
        shutil.copyfile(ipc/'result.wav',output/(label+'.wav'))
    print(label,times[label],result.get('runtime'),flush=True)
    return result,audio
try:
    cold,plain=call('new-cold',base)
    warm,quiet=call('new-dyn-edit',dyn)
    assert warm['runtime']['bank_reused'] and warm['runtime']['pid']==cold['runtime']['pid']
    assert warm['runtime']['audio_cache_hits']>cold['runtime']['audio_cache_hits']
    assert np.allclose(quiet,plain*10**(-12/20),atol=1e-7)
    _,restored=call('new-dyn-undo',base);assert np.array_equal(plain,restored)
    passed('real worker reuses models; DYN edit scales cached waveform and undo is exact')
    changed=copy.deepcopy(base)
    for note in changed['notes']:
        note['pitch']=[[0,note['midi']+12],[note['duration'],note['midi']+12]]
    octave,high=call('new-pitch-edit',changed)
    assert octave['runtime']['audio_cache_hits']==warm['runtime']['audio_cache_hits']+1
    assert np.sqrt(np.mean((plain-high)**2))>.005
    passed('pitch edits invalidate raw audio while retaining the model and duration plan')
    inspect,_=call('warm-inspect',dict(base,operation='inspect'))
    assert inspect['runtime']['bank_reused'] and inspect['has_pitch']
    previous_loads=inspect['runtime']['bank_loads']
    time.sleep(2.3)
    idle,_=call('after-idle',dict(base,operation='inspect'))
    assert idle['runtime']['bank_loads']==previous_loads+1 and not idle['runtime']['bank_reused']
    passed('idle expiry releases bank; next request reloads successfully')
finally:
    worker.kill();worker.wait(timeout=10)

report={'ok':True,'checks':checks,'seconds':times,'worker':records,
        'dyn_edit_speedup':times['old-dyn-edit']/times['new-dyn-edit'],
        'benchmark_note':'2.4-second Umidaji example, CPU, wall-clock includes IPC and output WAV. Not a general speed guarantee.'}
(output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2),flush=True)

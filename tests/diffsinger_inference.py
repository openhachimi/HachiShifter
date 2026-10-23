"""DS execution, quality, fallback and real CPU/DirectML regression.
Usage: packaged-python -I this.py bridge.py bank output
"""
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import copy, importlib.util, json, sys, time
sys.stdout.reconfigure(encoding='utf-8')
bridge, voice, out = map(lambda s:Path(s).resolve(), sys.argv[1:4]); out.mkdir(parents=True, exist_ok=True)
spec=importlib.util.spec_from_file_location('bridge',bridge); b=importlib.util.module_from_spec(spec);spec.loader.exec_module(b)
import onnxruntime as ort
checks=[]; records=[]
def passed(s): checks.append(s);print('PASS',s,flush=True)
def inp(n,t='tensor(int64)'):return SimpleNamespace(name=n,type=t)
for opts in [dict(backend='cuda'),dict(quality='bad'),dict(device=-1),dict(device=1.5),dict(quality='custom',depth=0),dict(quality='custom',depth=float('nan')),dict(quality='custom',pitch_steps=0),dict(quality='custom',pitch_steps=3.5)]:
 try:b.inference.options(opts)
 except ValueError:pass
 else:raise AssertionError(opts)
passed('invalid devices, profiles, steps and depths rejected')
sampling=b.compatibility.sampling_inputs
assert sampling({'max_depth':.53},[inp('depth'),inp('speedup')],20,.8)=={'depth':[520],'speedup':[26]}
assert sampling({'max_depth':.53},[inp('depth','tensor(float)'),inp('steps')],8,.7)=={'steps':[8],'depth':[.53]}
assert sampling({},[inp('speedup')],30,.1)=={'speedup':[25]}
assert sampling({},[inp('depth'),inp('speedup')],20,.2)=={'depth':[200],'speedup':[10]}
passed('old integer and continuous sampling; depth clamped only where supported')
class Capture:
 def get_inputs(self):return [inp('steps'),inp('depth','tensor(float)')]
 def get_outputs(self):return [SimpleNamespace(name='out')]
 def run(self,_,feed):self.feed=feed;return [b.np.zeros((1,1),b.np.float32)]
m=object.__new__(b.Model);m.config={'max_depth':.8};m.root=Path('capture');m.execution=b.inference.Context(dict(quality='custom',acoustic_steps=12,pitch_steps=7,variance_steps=9,depth=.4));c=Capture();m.session=lambda _:c
for name,steps,depth in [('pitch',7,.8),('variance',9,.8),('acoustic',12,.4)]:
 m.run(name,{});assert int(c.feed['steps'][0])==steps and abs(float(c.feed['depth'][0])-depth)<1e-6
passed('independent stage steps and acoustic-only depth reach real Model.run feeds')
class Fake:
 def __init__(self,opts,providers): self.providers=providers;self.opts=opts;self.failed=False
 def get_providers(self):return [p[0] if isinstance(p,tuple) else p for p in self.providers]
 def disable_fallback(self):pass
 def run(self,*args):
  if 'DmlExecutionProvider' in self.get_providers():raise RuntimeError('device removed')
  return ['cpu-result']
def fake(path,sess_options,providers):return Fake(sess_options,providers)
with patch.object(ort,'InferenceSession',fake),patch.object(ort,'get_available_providers',lambda:['DmlExecutionProvider','CPUExecutionProvider']):
 ctx=b.inference.Context(dict(backend='auto'));s=ctx.session('model','test');assert not s.inner.opts.enable_mem_pattern and s.inner.opts.execution_mode==ort.ExecutionMode.ORT_SEQUENTIAL
 assert s.run(None,{})==['cpu-result'] and ctx.warnings and ctx.report()['backend']=='CPU'
 explicit=b.inference.Context(dict(backend='directml')).session('model','test')
 try:explicit.run(None,{})
 except RuntimeError:pass
 else:raise AssertionError('explicit GPU silently fell back')
with patch.object(ort,'InferenceSession',fake),patch.object(ort,'get_available_providers',lambda:['CPUExecutionProvider']):
 ctx=b.inference.Context(dict(backend='auto'));s=ctx.session('model','test');assert ctx.report()['backend']=='CPU' and ctx.warnings
 try:b.inference.Context(dict(backend='directml')).session('model','test')
 except ValueError:pass
 else:raise AssertionError('unavailable explicit GPU accepted')
passed('sequential DML sessions, automatic CPU fallback, explicit GPU failures')
request=dict(operation='render',voicebank=str(voice),language='zh',duration=2.4,notes=[dict(id='n1',start=.3,duration=.7,midi=60,lyric='ni'),dict(id='n2',start=1.,duration=.9,midi=64,lyric='hao')])
rt=b.runtime.Runtime(lambda *a,**kw:object());req=dict(request,inference={'backend':'cpu','quality':'fast'});bank,reuse=rt.prepare(req);req['inference']['quality']='high';assert rt.prepare(req)==(bank,True);req['inference']['backend']='directml';assert not rt.prepare(req)[1]
passed('quality switch keeps models; device switch reloads models')
for backend in ['cpu','directml']:
 rt=b.runtime.Runtime(b.Bank)
 for quality in ['fast','standard','high','custom']:
  req=copy.deepcopy(request);req['inference']=dict(backend=backend,quality=quality,acoustic_steps=12,pitch_steps=7,variance_steps=9,depth=.5)
  start=time.monotonic();bank,reused=rt.prepare(req); inspected=bank.inspect();assert inspected['ok'],inspected
  cold=time.monotonic()-start
  req['operation']='pitch';start=time.monotonic();pred=b.execute(req,out/'unused.json',bank);pitch_seconds=time.monotonic()-start
  assert pred['ok'] and pred['curves']; assert pred['inference']['sampling']['pitch']['steps']==[b.inference.options(req['inference'])['pitch_steps']]
  # Keep the same MIDI pitch for quality/cache comparisons; generated pitch
  # is checked independently so it cannot accidentally invalidate the cache.
  req['operation']='render';hits=bank.audio_cache.hits;start=time.monotonic();result=b.execute(req,out/f'{backend}-{quality}.json',bank);seconds=time.monotonic()-start
  assert result['ok'] and bank.audio_cache.hits==hits
  for stage in ['acoustic','variance']:
   if stage in result['inference']['sampling'] and 'steps' in result['inference']['sampling'][stage]:assert result['inference']['sampling'][stage]['steps']==[b.inference.options(req['inference'])[stage+'_steps']]
  start=time.monotonic();b.execute(req,out/'cache.json',bank);repeat=time.monotonic()-start;assert bank.audio_cache.hits==hits+1
  import soundfile as sf
  wav,sr=sf.read(out/f'{backend}-{quality}.wav');assert b.np.isfinite(wav).all() and b.np.sqrt(b.np.mean(wav**2))>.0001
  record=dict(backend=backend,quality=quality,load=cold,pitch=pitch_seconds,render=seconds,repeat=repeat,rms=float(b.np.sqrt(b.np.mean(wav**2))),inference=result['inference']);records.append(record);print(json.dumps(record,ensure_ascii=False),flush=True)
 req['inference']['quality']='fast';b.execute(req,out/'restored-fast.json',bank);assert bank.audio_cache.hits>=4
 rt.release();del bank
passed('real CPU/DirectML four profiles, finite audio, quality cache separation and reuse')
(out/'report.json').write_text(json.dumps(dict(checks=checks,records=records),ensure_ascii=False,indent=2),encoding='utf-8')

"""Expression contracts plus optional real Umidaji model checks.
Usage: packaged-python -I diffsinger_expressions.py bridge.py out [bank]
"""
from pathlib import Path
import sys, importlib.util, json, copy, tempfile
from types import SimpleNamespace
from unittest.mock import patch
sys.stdout.reconfigure(encoding="utf-8")
script, output = map(Path, sys.argv[1:3]); output.mkdir(parents=True, exist_ok=True)
# Permit testing source scripts against the already installed portable runtime.
sys.path.insert(0,str(Path(sys.executable).parent.parent / "diffsinger" / "packages"))
spec = importlib.util.spec_from_file_location("bridge", script)
b = importlib.util.module_from_spec(spec); spec.loader.exec_module(b)
e, np = b.expressions, b.np
checks=[]
def passed(name):
    checks.append(name); print("PASS", name, flush=True)

specs=e.specifications(["a","b"])
notes=[dict(start=0., duration=1., expressions={"DS:DYN":[[0,-120],[1,0]], "DS:GENC":[[0,-100],[1,100]]}),
       dict(start=1., duration=1., expressions={})]
x=e.sample(notes,np.array([-.1,0,.5,.99,1.,1.5]),specs,set(specs))
assert np.allclose(x["DS:DYN"],[-120,-120,-60,-1.2,0,0])
assert np.all(x["DS:VELC"]==100) and np.all(x["DS:VOIC"]==100)
assert np.allclose(x["DS:GENC"],[-100,-100,0,98,0,0])
passed("independent curves, interpolation, defaults and note boundaries")
onset_notes=[dict(start=1.,duration=1.,expressions={"DS:DYN":[[0,-240]]}),
             dict(start=2.,duration=1.,expressions={"DS:DYN":[[0,60]]})]
x=e.sample(onset_notes,np.array([1.79,1.8,1.9,2.]),specs,set(specs),[.85,1.8])
assert np.allclose(x["DS:DYN"],[-240,60,60,60])
passed("next note owns its early consonant expressions, including DYN")
for points in ([[0,0],[0,1]],[[0,float('nan')]],[[0,1,2]]):
    try:e.sample([dict(start=0,expressions={"DS:DYN":points})],np.array([0]),specs,set(specs))
    except ValueError:pass
    else:raise AssertionError("invalid curve accepted")
try:e.sample([dict(start=0,expressions={"DS:BREC":[[0,100]]})],np.array([0]),specs,{"DS:DYN"})
except ValueError:pass
else:raise AssertionError("unsupported expression silently ignored")
passed("invalid curves and unsupported non-neutral edits rejected")
assert np.allclose(e.gender(np.array([-100,0,100]),{"augmentation_args":{"random_pitch_shifting":{"range":[-5,5]}}}),[2.4,0,-2.4])
assert np.allclose(e.variance_delta("energy",np.array([-40]*3),np.array([-100,0,100])),[-52,-40,-28])
assert np.allclose(e.variance_delta("breathiness",np.array([-90,-40,-5]),np.array([-100,0,100])),[-96,-40,0])
assert np.allclose(e.variance_delta("voicing",np.array([-20]*3),np.array([0,50,100])),[-32,-26,-20])
assert np.allclose(e.variance_delta("tension",np.array([-9,0,9]),np.array([-100,0,100])),[-10,0,10])
w={"DS:DYN":np.zeros(3),"DS:CLR:a":np.array([0,50,100]),"DS:CLR:b":np.array([0,0,100])}
assert np.allclose(e.speaker_weights(w,["a","b"],"b"),[[0,1],[.5,.5],[.5,.5]])
passed("OpenUtau unit conversions, clamping and speaker normalization")

# Exercise actual bridge wiring, with deterministic substitute models to expose
# unsupported-on-Umidaji variance and pitch-controllable vocoder inputs.
n=5; captured={}
cfg=dict(sample_rate=44100,hop_size=512,use_key_shift_embed=True,use_speed_embed=True,
         **{f"use_{v}_embed":True for v in e.VARIANCE.values()})
main=SimpleNamespace(config=cfg,tokens={"a":0},languages={},dt=512/44100)
def acoustic(name,values,frames):
    captured["acoustic"]=values
    return {"mel":np.zeros((1,frames,128),np.float32)}
main.run=acoustic
variance=SimpleNamespace(config={f"predict_{v}":True for v in e.VARIANCE.values()},
    linguistic=lambda *args:{})
def variance_run(name,values,frames):
    captured["variance"]=values
    return {v+"_pred":np.full((1,frames),0 if v=="tension" else -40,np.float32) for v in e.VARIANCE.values()}
variance.run=variance_run
pitchmodel=SimpleNamespace(config={"use_expr":True},linguistic=lambda *args:{})
def pitch_run(name,values,frames):
    captured["pitch"]=values
    return {"pitch":np.full((1,frames),60,np.float32)}
pitchmodel.run=pitch_run
bank=b.Bank.__new__(b.Bank);bank.main=main
bank.execution=SimpleNamespace(options=b.inference.options({}))
bank.audio_cache=b.runtime.Cache();bank._vocoder_session=None
bank.model=lambda folder: variance if folder=="dsvariance" else pitchmodel
bank.vocoder_config=lambda required=True:(Path('.'),dict(model="unused.onnx",pitch_controllable=True))
plan=dict(total=n,phones=["a"],ph_dur=[n],word_div=[1],word_dur=[n],pitch=np.full(n,60),
          note_midi=[[60]],note_dur=[[n]],note_rest=[[False]],note_glide=[[0]])
x={key:np.full(n,sp["default"],np.float32) for key,sp in e.specifications([]).items()}
x["DS:GENC"]=np.linspace(-100,100,n);x["DS:VELC"]=np.linspace(0,200,n)
x["DS:SHFC"]=np.linspace(0,1200,n);x["DS:PEXP"]=np.linspace(0,100,n)
for key in ("ENE","BREC","TENC"):x["DS:"+key]=np.linspace(-100,100,n)
x["DS:VOIC"]=np.linspace(0,100,n);x["DS:DYN"]=np.linspace(-120,0,n)
class Vocoder:
    def get_inputs(self):return [SimpleNamespace(name="mel"),SimpleNamespace(name="f0")]
    def run(self,_,feed):
        captured["vocoder"]=feed;return [np.ones((1,n*512),np.float32)]
import onnxruntime as ort
bank.vocoder_session=lambda:Vocoder()
with patch.object(ort,"InferenceSession",lambda *a,**k:Vocoder()):
    wav=bank.render(plan,plan["pitch"],x)
bank.pitch(plan,x)
a=captured["acoustic"]
assert np.allclose(a["gender"],[-(-1),.5,0,-.5,-1])
assert np.allclose(a["velocity"],np.exp2(np.linspace(-1,1,n)))
assert np.allclose(a["energy"],np.linspace(-52,-28,n))
assert np.allclose(a["breathiness"],np.linspace(-52,-28,n))
assert np.allclose(a["tension"],np.linspace(-5,5,n))
assert np.allclose(a["voicing"],np.linspace(-52,-40,n))
assert np.allclose(captured["variance"]["pitch"],np.linspace(60,72,n))
assert np.allclose(np.array(a["f0"])[0]/captured["vocoder"]["f0"][0],np.exp2(np.linspace(0,1,n)))
assert np.allclose(captured["pitch"]["expr"],np.linspace(0,1,n))
assert abs(wav[0]-10**(-12/20))<1e-6 and wav[-1]==1
passed("bridge: simultaneous acoustic/variance/PEXP/SHFC/DYN wiring")

# Frame-wise embeddings go to acoustic, pitch and variance; linguistic/dur
# continue to use their phoneme-length default speaker.
with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);(root/'dsconfig.yaml').write_text('phonemes: phones.json\nhidden_size: 2\nspeakers: [a, b]\n')
    (root/'phones.json').write_text('{"a":0}')
    np.array([1,0],dtype='<f4').tofile(root/'a.emb');np.array([0,1],dtype='<f4').tofile(root/'b.emb')
    model=b.Model(root);model.speaker='b';model.expression_curves=w
    class Session:
        def get_inputs(self):return [SimpleNamespace(name='spk_embed',type='tensor(float)')]
        def get_outputs(self):return [SimpleNamespace(name='output')]
        def run(self,_,feed):return [feed['spk_embed']]
    model.session=lambda name:Session()
    for name in ('acoustic','pitch','variance'):
        assert np.allclose(model.run(name,{},3)['output'][0],[[0,1],[.5,.5],[.5,.5]])
    assert np.allclose(model.run('dur',{},2)['output'][0],[[0,1],[0,1]])
passed("frame speaker mixtures across all three predictors")

report=dict(ok=True,contracts=checks)
if len(sys.argv)>3:
    import soundfile as sf
    bank=b.Bank(Path(sys.argv[3])); caps=bank.inspect()['expressions']
    (output/'capabilities.json').write_text(json.dumps(caps,ensure_ascii=False,indent=2),encoding='utf-8')
    supported={v['key'] for v in caps if v['supported']}
    assert 'DS:GENC' in supported and 'DS:VELC' in supported and 'DS:PEXP' in supported
    assert 'DS:BREC' not in supported and 'DS:SHFC' not in supported
    notes=[dict(id='n1',start=.3,duration=1.5,midi=60,lyric='ni'),dict(id='n2',start=1.8,duration=.9,midi=64,lyric='hao')]
    plan=bank.plan(notes)
    baseline=bank.expression_curves(notes,plan)
    actual={};original=bank.main.run
    def capture(name,values,frames):
        actual[name]=copy.deepcopy(values);return original(name,values,frames)
    bank.main.run=capture
    sr=bank.main.config['sample_rate'];peak={};difference={}
    def render(name,curves):
        wav=bank.render(plan,plan['pitch'],curves)
        assert np.isfinite(wav).all() and np.max(np.abs(wav))>.001
        sf.write(output/(name+'.wav'),wav,sr,subtype='FLOAT');peak[name]=float(np.max(np.abs(wav)))
        return wav
    ref=render('01-default',baseline)
    variants={
        '02-gender':{'DS:GENC':[[0,-80],[1.5,80]]},
        '03-velocity':{'DS:VELC':[[0,0],[1.5,200]]},
        '04-colour':{'DS:CLR:embeds/umidaji-rainbow':[[0,0],[1.5,100]]},
        '05-combined':{'DS:GENC':[[0,-60],[1.5,60]],'DS:VELC':[[0,60],[1.5,160]],
            'DS:DYN':[[0,-120],[1.5,0]],'DS:CLR:embeds/umidaji-rainbow':[[0,0],[1.5,100]]}}
    for name,edited in variants.items():
        changed=copy.deepcopy(notes);changed[0]['expressions']=edited
        curves=bank.expression_curves(changed,plan)
        wav=render(name,curves);difference[name]=float(np.sqrt(np.mean((wav-ref)**2)))
        assert difference[name]>1e-5
        if 'DS:GENC' in edited:assert np.ptp(actual['acoustic']['gender'])>1
        if 'DS:VELC' in edited:assert np.ptp(actual['acoustic']['velocity'])>.5
    original_pitch=bank.model('dspitch').run
    def capture_pitch(name,values,frames=None):
        actual[name]=copy.deepcopy(values);return original_pitch(name,values,frames)
    bank.model('dspitch').run=capture_pitch
    pitch100=bank.pitch(plan,baseline)
    changed=copy.deepcopy(notes);changed[0]['expressions']={'DS:PEXP':[[0,0],[1.5,100]]}
    pitchcurve=bank.pitch(plan,bank.expression_curves(changed,plan))
    assert np.ptp(actual['pitch']['expr'])==1
    assert np.isfinite(pitchcurve).all() and np.max(np.abs(pitch100-pitchcurve))>.01
    (output/'pitch-expression.json').write_text(json.dumps({'default':pitch100.tolist(),'curve':pitchcurve.tolist()}))
    passed('real Umidaji: GENC, VELC, colour, four simultaneous curves, PEXP')
    report.update(real_bank=str(sys.argv[3]),peaks=peak,waveform_rms_differences=difference,
        caveat='Variance and SHFC tested with deterministic contract substitutes; Umidaji does not support those inputs.')
(output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))

"""Actual-parameter contracts and optional real ZhiBin render validation."""
from pathlib import Path
import sys, runpy, json, copy
sys.stdout.reconfigure(encoding='utf-8')
args=sys.argv[:]
script,out=map(Path,args[1:3]);out.mkdir(parents=True,exist_ok=True)
sys.argv=[str(Path(__file__).with_name('diffsinger_expressions.py')),str(script),str(out/'legacy')]
ns=runpy.run_path(sys.argv[0]);sys.argv=args
b,np,e=ns['b'],ns['np'],ns['e'];bank,plan,x,captured=ns['bank'],ns['plan'],ns['x'],ns['captured']
checks=[]
def check(name,condition):
    assert condition,name
    checks.append(name);print('PASS',name,flush=True)
actual=copy.deepcopy(x)
for code in e.VARIANCE: actual['DS:ABS:'+code]=np.full(plan['total'],2 if code=='TENC' else -25,np.float32)
bank.predict_parameters=lambda *a: (_ for _ in ()).throw(AssertionError('frozen base re-predicted'))
bank._render_raw(plan,plan['pitch'],actual)
for code,name in e.VARIANCE.items():
    check('actual plus offset '+code,np.allclose(captured['acoustic'][name],e.variance_delta(name,actual['DS:ABS:'+code],actual['DS:'+code])))
notes=[dict(start=0,parameters={'BREC':[[-.2,-40],[1,-20]]}),dict(start=1,parameters={})]
sample=e.sample_absolute(notes,np.array([-.1,.5,.9,1.1]),{'DS:BREC'},[-.2,.9])
check('consonant ownership and live fallback',np.isfinite(sample['DS:ABS:BREC'][:2]).all() and np.isnan(sample['DS:ABS:BREC'][2:]).all())
try:e.sample_absolute(notes,np.array([0]),set(),[0,1])
except ValueError:check('unsupported actual rejected',True)
else:raise AssertionError('unsupported actual accepted')
for bad in ([[0,float('nan')]],[[0,-30],[0,-20]],[]):
    try:e.sample_absolute([dict(start=0,parameters={'BREC':bad})],np.array([0]),{'DS:BREC'},[0])
    except ValueError:pass
    else:raise AssertionError('invalid actual accepted')
check('invalid actual data rejected',True)
if len(args)>3:
    voice=Path(args[3]);vocoders=Path(args[4])
    bank=b.Bank(voice,vocoders=vocoders,settings=dict(backend='cpu',quality='fast'))
    request=dict(operation='parameters',voicebank=str(voice),duration=2.3,inference=dict(backend='cpu',quality='fast'),
        notes=[dict(id='a',start=.5,duration=.7,midi=60,lyric='ni',context='one'),dict(id='b',start=1.2,duration=.7,midi=62,lyric='hao',context='one')])
    result=b.execute(request,out/'prediction.json',bank)
    (out/'prediction.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    check('real three supported predictions',len(result['parameters'])==2 and set(result['parameters'][0]['curves'])=={'BREC','TENC','VOIC'})
    frozen=copy.deepcopy(request);frozen['operation']='render'
    for note,row in zip(frozen['notes'],result['parameters']):note['parameters']=row['curves']
    original=bank.main.run;feeds=[]
    def capture(name,values,frames):
        if name=='acoustic':feeds.append(copy.deepcopy(values))
        return original(name,values,frames)
    bank.main.run=capture
    bank.predict_parameters=lambda *a: (_ for _ in ()).throw(AssertionError('saved base re-predicted'))
    b.execute(frozen,out/'01-frozen.json',bank)
    base=feeds[-1]
    shifted=copy.deepcopy(frozen)
    for note in shifted['notes']:note['expressions']={'DS:BREC':[[0,50]],'DS:TENC':[[0,20]]}
    b.execute(shifted,out/'02-offset.json',bank)
    changed=feeds[-1]
    check('real acoustic +6dB BREC',np.allclose(changed['breathiness'],np.clip(np.asarray(base['breathiness'])+6,-96,0),atol=2e-5))
    check('real acoustic +1 tension',np.allclose(changed['tension'],np.clip(np.asarray(base['tension'])+1,-10,10),atol=2e-5))
    check('real other parameter unchanged',np.array_equal(changed['voicing'],base['voicing']))
    edited=copy.deepcopy(frozen)
    for note in edited['notes']:note['parameters']['BREC']=[[p[0],-20] for p in note['parameters']['BREC']]
    b.execute(edited,out/'03-actual-edited.json',bank)
    check('real absolute reaches acoustic',np.allclose(feeds[-1]['breathiness'],-20))
    import soundfile as sf
    a,sr=sf.read(out/'01-frozen.wav');z,_=sf.read(out/'03-actual-edited.wav')
    check('real output audible and changed',len(a)==len(z) and np.max(np.abs(a))>.001 and np.mean(np.abs(a-z))>1e-5)
    # Regenerate selection while ignoring saved actual values and variance offsets.
    bank.predict_parameters=b.Bank.predict_parameters.__get__(bank)
    selective=copy.deepcopy(shifted);selective['operation']='parameters';selective['retake_ids']=['b']
    rr=b.execute(selective,out/'selected.json',bank)
    check('real selected regeneration only',len(rr['parameters'])==1 and rr['parameters'][0]['id']=='b')
    (out/'selected.json').write_text(json.dumps(rr,ensure_ascii=False),encoding='utf-8')
(out/'report.json').write_text(json.dumps(dict(ok=True,checks=checks),ensure_ascii=False,indent=2),encoding='utf-8')

"""Variance context/retake contracts and optional real voicebank integration."""
from pathlib import Path
from types import SimpleNamespace
import sys, importlib.util, copy, json
sys.stdout.reconfigure(encoding='utf-8')
script, out = map(Path, sys.argv[1:3]); out.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(Path(sys.executable).parent.parent/'diffsinger'/'packages'))
spec = importlib.util.spec_from_file_location('bridge', script)
b = importlib.util.module_from_spec(spec); spec.loader.exec_module(b)
v, np, e = b.variance_retake, b.np, b.expressions
checks = []
def check(name, value):
    assert value, name
    checks.append(name); print('PASS', name, flush=True)
def rejects(fn):
    try: fn()
    except ValueError: return True
    return False

class Variance:
    def __init__(self):
        self.config = dict(speakers=['a','b'], **{'predict_'+k:True for k in v.ORDER})
        self.speaker = 'a'; self.calls=[]; self.supported=True; self.bad=False
    def session(self, name):
        return SimpleNamespace(get_inputs=lambda:[SimpleNamespace(name=n) for n in
            (['retake', *v.ORDER] if self.supported else ['pitch'])])
    def linguistic(self, *args): return {}
    def run(self, name, values, n):
        self.calls.append(copy.deepcopy(values))
        # Deliberately ignores locks to verify the bridge enforces them, too.
        result={k+'_pred':np.full((1,n), (0 if k=='tension' else -40)+len(self.calls), np.float32)
                +np.arange(n,dtype=np.float32)[None]*.01 for k in v.ORDER}
        if self.bad: result['breathiness_pred'][0,0]=np.nan
        return result

def fixture():
    model=Variance()
    bank=b.Bank.__new__(b.Bank)
    bank.main=SimpleNamespace(config={'use_'+k+'_embed':True for k in v.ORDER})
    bank.model=lambda _:model
    bank.execution=SimpleNamespace(options={'variance_steps':5}, warnings=[])
    bank.parameter_events=[]
    plan=dict(total=20,phones=['SP','a','b','SP'],ph_dur=[2,8,8,2],word_div=[1,1,1,1],word_dur=[2,8,8,2],origin=0)
    pitch=np.full(20,60,np.float32)
    curves={k:np.full(20,s['default'],np.float32) for k,s in e.specifications(['a','b']).items()}
    return bank,model,plan,pitch,curves

bank,m,p,pit,x=fixture()
initial=v.predict(bank,p,pit,x)
check('initial all four channels and frame mask', m.calls[-1]['retake'].shape==(1,20,4) and m.calls[-1]['retake'].all())
again=v.predict(bank,p,pit,x)
check('identical preview cached',len(m.calls)==1 and all(np.array_equal(initial[k],again[k]) for k in initial))
pitch2=pit.copy();pitch2[7:11]+=2
changed=v.predict(bank,p,pitch2,x)
mask=m.calls[-1]['retake'][0]
check('pitch edit retakes only changed frames',mask[7:11].all() and mask.sum()==16)
check('outside context bit exact despite model drift',all(np.array_equal(initial[k][:,:7],changed[k][:,:7]) and np.array_equal(initial[k][:,11:],changed[k][:,11:]) for k in initial))
check('raw previous channel values passed in export order',all(np.array_equal(m.calls[-1][k],initial[k]) for k in v.ORDER))
v.predict(bank,p,pit,x)
check('undo restores exact prior prediction without inference',len(m.calls)==2)
offsets=copy.deepcopy(x)
for code in e.VARIANCE:offsets['DS:'+code][:]=70
offsets['DS:DYN'][:]=-120
v.predict(bank,p,pit,offsets)
check('FLAG and dynamics offsets never trigger or bake into variance',len(m.calls)==2)
color=copy.deepcopy(x);color['DS:CLR:b'][12:15]=100
v.predict(bank,p,pit,color)
check('speaker edit retakes changed frames only',m.calls[-1]['retake'].sum()==12 and m.calls[-1]['retake'][0,12:15].all())
v.predict(bank,p,pit,x)

known={ 'DS:ABS:'+code:np.full(20,2 if code=='TENC' else -23,np.float32) for code in e.VARIANCE }
selected=np.zeros(20,bool);selected[4:6]=True;selected[13:16]=True
local=v.predict(bank,p,pit,offsets,existing=known,retake=selected)
check('noncontiguous manual selection locks all other frames',np.array_equal(m.calls[-1]['retake'][0,:,0],selected) and all(np.array_equal(local[name][0,~selected],known['DS:ABS:'+code][~selected]) for code,name in e.VARIANCE.items()))
check('manual context ignores FLAG deltas',np.array_equal(m.calls[-1]['breathiness'][0],known['DS:ABS:BREC']))
partial=copy.deepcopy(x)
for key,value in known.items():partial[key]=value.copy();partial[key][10:]=np.nan
v.predict(bank,p,pit,partial)
pitch3=pit+3
frozen=v.predict(bank,p,pitch3,partial)
check('frozen half preserved on pitch edit, live half predicted',not m.calls[-1]['retake'][0,:10].any() and m.calls[-1]['retake'][0,10:].all() and np.all(frozen['breathiness'][0,:10]==-23))
v.predict(bank,p,pitch3,x)
check('clearing frozen data recalculates released frames',m.calls[-1]['retake'][0,:10].all() and not m.calls[-1]['retake'][0,10:].any())
before=len(m.calls);v.predict(bank,p,pitch3,x,force_full=True)
check('explicit full regeneration bypasses old prediction',len(m.calls)==before+1 and m.calls[-1]['retake'].all())
bank.execution.options['variance_steps']=8;v.predict(bank,p,pitch3,x)
check('quality change invalidates context',m.calls[-1]['retake'].all())
other=copy.deepcopy(p);other['phones'][1]='z';v.predict(bank,other,pitch3,x)
check('pronunciation change invalidates context',m.calls[-1]['retake'].all())
other['ph_dur']=[2,7,9,2];v.predict(bank,other,pitch3,x)
check('timing change invalidates context',m.calls[-1]['retake'].all())
other['parameter_context']=[('other-clip','0',.5,.7)];v.predict(bank,other,pitch3,x)
check('other phrase has independent context',m.calls[-1]['retake'].all())
before=len(m.calls);m.bad=True
badpitch=pitch3+1
check('invalid model output rejected',rejects(lambda:v.predict(bank,other,badpitch,x)))
m.bad=False;v.predict(bank,other,badpitch,x)
check('failed result cannot poison cache',len(m.calls)==before+2)
check('invalid pitch rejected',rejects(lambda:v.predict(bank,p,np.full(20,np.nan),x)))
check('invalid selection shape rejected',rejects(lambda:v.predict(bank,p,pit,x,retake=[True])))
for i in range(40):
    other['parameter_context']=[str(i)];v.predict(bank,other,pit,x)
check('prediction caches bounded',len(bank.variance_states.items)<=16 and len(bank.variance_exact.items)<=32 and bank.variance_states.size<=16*1024*1024)

bank,m,p,pit,x=fixture();v.predict(bank,p,pit,x);m.supported=False
check('unsupported manual retake fails explicitly',rejects(lambda:v.predict(bank,p,pit,x,retake=selected)))
v.predict(bank,p,pitch2,x)
check('legacy automatic fallback reported',bank.parameter_events[-1]['fallback_full'] and len(bank.execution.warnings)==1)
bank,m,p,pit,x=fixture();v.predict(bank,p,pit,x,existing=known,retake=selected)
check('saved context works after worker restart',np.array_equal(m.calls[-1]['retake'][0,:,0],selected))
bank,m,p,pit,x=fixture();v.predict(bank,p,pit,x,retake=selected)
check('missing initial context initialized once',m.calls[-1]['retake'].all() and bank.parameter_events[-1]['initialized_values']==80)
notes=[dict(id='a',start=.5,duration=.5),dict(id='b',start=1,duration=.5),dict(id='c',start=1.5,duration=.5)]
time=np.arange(0,2.2,.01);timing=dict(note_onsets=[.4,.9,1.4])
sel=v.selection_mask(notes,timing,time,{'b'})
check('selection includes pre consonant, excludes next consonant',np.array_equal(sel,(time>=.9)&(time<1.4)))
check('invalid or subframe selection rejected',rejects(lambda:v.selection_mask(notes,timing,np.array([0.,3.]),{'b'})))

if len(sys.argv)>3:
    voice,vocoders=map(Path,sys.argv[3:5])
    bank=b.Bank(voice,vocoders=vocoders,settings=dict(backend='cpu',quality='fast'))
    request=dict(operation='parameters',voicebank=str(voice),duration=2.3,inference=dict(backend='cpu',quality='fast'),notes=[
        dict(id='a',start=.5,duration=.7,midi=60,lyric='ni',context='one'),
        dict(id='b',start=1.2,duration=.7,midi=62,lyric='hao',context='one')])
    base=b.execute(request,out/'real-full.json',bank)
    frozen=copy.deepcopy(request)
    for note,row in zip(frozen['notes'],base['parameters']):note['parameters']=row['curves']
    model=bank.model('dsvariance'); original=model.run; feeds=[]
    def capture(name,values,n=None):
        if name=='variance':feeds.append(copy.deepcopy(values))
        return original(name,values,n)
    model.run=capture
    local=copy.deepcopy(frozen);local['retake_ids']=['b']
    local['notes'][1]['pitch']=[[0,63],[.7,64]]
    local['notes'][0]['expressions']={'DS:BREC':[[0,50]]}
    result=b.execute(local,out/'real-selected.json',bank)
    check('real selected result writes only requested note',len(result['parameters'])==1 and result['parameters'][0]['id']=='b')
    check('real ONNX partial three-channel retake',len(feeds)==1 and feeds[0]['retake'].shape[2]==3 and 0<feeds[0]['retake'].sum()<feeds[0]['retake'].size)
    event=result['parameter_retake'][0]
    check('real selected lock reported without fallback',event['mode']=='selected' and event['locked_values']>0 and not event['fallback_full'])
    bank2=b.Bank(voice,vocoders=vocoders,settings=dict(backend='cpu',quality='fast'))
    restarted=b.execute(local,out/'real-restarted.json',bank2)
    check('real saved context supports fresh process',restarted['parameter_retake'][0]['locked_values']==event['locked_values'])
    request['operation']='render'
    first=b.execute(request,out/'01-live.wav',bank)
    livepitch=copy.deepcopy(request);livepitch['notes'][1]['pitch']=[[0,65],[.7,65]]
    after=b.execute(livepitch,out/'02-live-pitch.wav',bank)
    event2=after['parameter_retake'][0]
    check('real live pitch edit uses automatic partial retake',event2['mode']=='automatic' and 0<event2['changed_frames']<event2['frames'] and not event2['fallback_full'])
    before=len(feeds)
    replay=b.execute(livepitch,out/'03-repeat.wav',bank)
    check('real repeated audition skips variance',len(feeds)==before and replay['parameter_retake'][0]['changed_frames']==0)
    import soundfile as sf
    a,sr=sf.read(out/'02-live-pitch.wav');c,_=sf.read(out/'03-repeat.wav')
    check('real cached audio exact and audible',np.array_equal(a,c) and np.isfinite(a).all() and np.max(np.abs(a))>.001)
    frozen['operation']='render';b.execute(frozen,out/'04-frozen.wav',bank)
    check('real frozen actual curves skip inference',len(feeds)==before)
    for name,report in [('full',base),('selected',result),('auto',after)]:
        (out/('real-'+name+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
(out/'report.json').write_text(json.dumps(dict(ok=True,checks=checks),ensure_ascii=False,indent=2),encoding='utf-8')

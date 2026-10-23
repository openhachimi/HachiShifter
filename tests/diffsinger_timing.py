"""Real voicebank timing, phrase conservation, cache and inference regression.
python -I this.py bridge.py packaged-packages bank output
"""
import copy
import importlib.util
import json
from pathlib import Path
import sys

sys.stdout.reconfigure(encoding='utf-8')
source, packages, voice, out = map(Path, sys.argv[1:5])
out.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(packages.resolve()))
spec = importlib.util.spec_from_file_location('bridge', source.resolve())
b = importlib.util.module_from_spec(spec); spec.loader.exec_module(b)
np = b.np
bank = b.Bank(voice)
notes = [dict(id='0', start=.3, duration=.7, midi=60, lyric='ni'),
         dict(id='1', start=1., duration=.9, midi=62, lyric='hao')]
checks = []
def check(name, condition=True):
    assert condition, name
    checks.append(name); print('PASS', name, flush=True)

request = dict(operation='timing', voicebank=str(voice), language='zh', duration=2.4, notes=notes)
response = b.execute(request, out/'timing.json', bank)
assert response['ok'], response
base = bank.plan(notes)
original = copy.deepcopy(base)
rows = response['phonemes']
check('real model token ownership and negative consonant offset',
      [r['token'] for r in rows if r['id']] == ['zh/n','zh/i','zh/h','zh/ao']
      and next(r for r in rows if r['token']=='zh/n')['start'] < .3)
target = next(r for r in rows if r['token']=='zh/h')
edited = copy.deepcopy(notes)
edited[1]['timing'] = dict(context=target['context'], tokens=target['tokens'],
                         starts=[target['start']-1.-.08, None])
plan, moved, ignored, clamped = b.timing.apply(bank, edited, base)
check('shared boundary, beat preservation and exact frame conservation',
      sum(plan['ph_dur']) == plan['total'] == sum(plan['note_dur'][0])
      and sum(plan['word_dur']) == plan['total']
      and moved[3]['start'] == moved[2]['end']
      and moved[3]['start'] < rows[3]['start']
      and moved[4]['start'] == rows[4]['start']
      and plan['ph_dur'] != base['ph_dur'] and not ignored and not clamped)
check('prediction cache is immutable', base['ph_dur']==original['ph_dur']
      and np.array_equal(base['voiced'], original['voiced']))

# Pitch, acoustic and variance all consume this same overridden plan.
real_pitch = bank.pitch
seen=[]
def capture_pitch(p, curves):
    seen.append(p['ph_dur']); return real_pitch(p, curves)
bank.pitch=capture_pitch
pitch=b.execute(dict(request, operation='pitch',notes=edited),out/'pitch.json',bank)
bank.pitch=real_pitch
check('real pitch prediction uses manual durations',pitch['ok'] and seen==[plan['ph_dur']])
for name, values in [('baseline',notes),('edited',edited),('undo',notes)]:
    result=b.execute(dict(request, operation='render',notes=values),out/(name+'.json'),bank)
    assert result['ok'], result
import soundfile as sf
a,sr=sf.read(out/'baseline.wav'); c,_=sf.read(out/'edited.wav'); restored,_=sf.read(out/'undo.wav')
check('acoustic output changes, undo exactly reuses original audio',
      len(a)==len(c) and np.max(np.abs(a-c))>.001 and np.array_equal(a,restored))

extreme=copy.deepcopy(edited);extreme[1]['timing']['starts'][0]=-30
p,r,_,clamp=b.timing.apply(bank,extreme,base)
check('crossing boundaries clamp to one frame without negative duration',clamp and min(p['ph_dur'])>=1)
for invalid in [float('nan'),float('inf'),True,'0']:
    corrupt=copy.deepcopy(edited);corrupt[1]['timing']['starts'][0]=invalid
    try:b.timing.apply(bank,corrupt,base)
    except ValueError:continue
    raise AssertionError('invalid boundary accepted')
check('invalid boundary values rejected')
stale=copy.deepcopy(edited);stale[1]['timing']['context']='other bank or lyric'
p,_,ignored,_=b.timing.apply(bank,stale,base)
check('changed bank/language/lyric does not reuse incompatible override',ignored==['1'] and p['ph_dur']==base['ph_dur'])

slur=copy.deepcopy(notes);slur[1]['lyric']='+';slur[1].pop('timing',None)
p=bank.plan(slur);_,r,_,_=b.timing.apply(bank,slur,p)
check('slur extends original vowel, no duplicate tokens',
      [x['token'] for x in r if x['id']]==['zh/n','zh/i'] and sum(p['ph_dur'])==p['total'])
gaps=copy.deepcopy(notes);gaps[1]['start']=1.2
p=bank.plan(gaps);_,r,_,_=b.timing.apply(bank,gaps,p)
check('internal rests remain explicit and unowned',len([x for x in r if x['token']=='SP'])==3)
short=copy.deepcopy(edited);short[0]['duration']=.08;short[1]['start']=.38;short[1]['duration']=.08
p,r,_,_=b.timing.apply(bank,short,bank.plan(short))
check('note shortening retains valid ordered timing',min(p['ph_dur'])>=1 and sum(p['ph_dur'])==p['total'])

saved_model=bank.model
bank.model=lambda folder: None if folder=='dsdur' else saved_model(folder)
fallback=bank._plan(notes)
p,r,_,_=b.timing.apply(bank,notes,fallback)
bank.model=saved_model
check('banks without duration predictor still expose editable rule timings',len(r)==6 and min(p['ph_dur'])>=1)
(out/'report.json').write_text(json.dumps(dict(checks=checks,dt=bank.main.dt,phonemes=rows,
    edited=moved,peak_difference=float(np.max(np.abs(a-c)))),ensure_ascii=False,indent=2),encoding='utf-8')

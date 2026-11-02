#!/usr/bin/env python3
"""Scoped UI-only fromUTF8 adapter migration. Does not touch preset data or IDs."""
import re,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=ROOT/'src/PianoRollComponent.cpp'
s=p.read_text(encoding='utf-8')
changes=[];token='juce::String::fromUTF8('
for m in list(re.finditer(re.escape(token),s))[::-1]:
    line=s.count('\n',0,m.start())+1
    if line<1000:continue
    at=m.end();depth=1;quote=False;escaped=False
    while at<len(s) and depth:
        c=s[at]
        if quote:
            if escaped:escaped=False
            elif c=='\\':escaped=True
            elif c=='"':quote=False
        elif c=='"':quote=True
        elif c=='(':depth+=1
        elif c==')':depth-=1
        at+=1
    arg=s[m.end():at-1]
    if '.c_str()' in arg:continue
    if not ('"' in arg or arg.strip() in ('kind.label','field.label')):continue
    literals=[]
    for raw in re.findall(r'"((?:[^"\\]|\\.)*)"',arg):
        try:literals.append(json.loads('"'+raw+'"'))
        except Exception:literals.append(raw)
    changes.append({'location':str(p.relative_to(ROOT))+':'+str(line),'literals':literals,'argument':arg})
    s=s[:m.start()]+'I18n::uiText('+s[m.end():]
p.write_text(s,encoding='utf-8',newline='\n')
out=ROOT.parent.parent/'validation/translation-file-only/piano-migrated-literals.json';out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(changes[::-1],ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(f'migrated UI calls={len(changes)} report={out}')

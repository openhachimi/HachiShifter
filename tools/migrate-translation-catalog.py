#!/usr/bin/env python3
"""One-time move of legacy C++ catalog into external TSV, preserving user cells.
No runtime component imports generated language values back into C++.
"""
import argparse,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from importlib.machinery import SourceFileLoader
legacy=SourceFileLoader('catalog_audit',str(ROOT/'tools/audit-ui-translations.py')).load_module()
LANGS=['zh-CN','zh-TW','ja-JP','ko-KR','en-US']
def unescape(text):
    return re.sub(r'\\(.)',lambda m:{'n':'\n','r':'\r','t':'\t','\\':'\\'}.get(m[1],m[1]),text)
def escape(text):return text.replace('\\','\\\\').replace('\t','\\t').replace('\r','\\r').replace('\n','\\n')
def read(path):
    lines=path.read_text(encoding='utf-8-sig').splitlines();head=lines[0].split('\t')
    assert head[:6]==['key']+LANGS
    result={}
    for line in lines[1:]:
        if not line or line.startswith('#'):continue
        cells=[unescape(x) for x in line.split('\t')];row=dict(zip(head,cells))
        assert row['key'] not in result
        row.setdefault('note','');row.setdefault('aliases','[]');result[row['key']]=row
    return result
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('existing',type=Path);p.add_argument('destination',type=Path);p.add_argument('snapshot',type=Path);args=p.parse_args()
    rows=read(args.existing)
    source=(ROOT/'src/I18n.cpp').read_text(encoding='utf-8')
    lit=legacy.LIT
    pattern=re.compile(r'\{\s*'+lit+r'\s*,\s*\{\s*'+lit+r'\s*,\s*'+lit+r'\s*,\s*'+lit+r'\s*,\s*'+lit+r'\s*,\s*'+lit+r'\s*\}\s*\}')
    originals=[]
    for match in pattern.findall(source):
        key,*values=[legacy.cpp(x) for x in match];row=rows.setdefault(key,dict(key=key,note='',aliases='[]'))
        for lang,value in zip(LANGS,values):
            if not row.get(lang):row[lang]=value
        aliases=set(json.loads(row['aliases']));aliases.update((key,values[0],values[-1]));row['aliases']=json.dumps(sorted(aliases),ensure_ascii=False,separators=(',',':'))
        originals.append({'key':key,**dict(zip(LANGS,values))})
    # Keep source literals stable when the user later edits any language cell.
    for key,row in rows.items():
        aliases=set(json.loads(row.get('aliases','[]')));aliases.update(x for x in (key,row.get('zh-CN',''),row.get('en-US','')) if x)
        row['aliases']=json.dumps(sorted(aliases),ensure_ascii=False,separators=(',',':'))
    head=['key']+LANGS+['note','aliases']
    args.destination.parent.mkdir(parents=True,exist_ok=True)
    args.destination.write_text('\n'.join(['\t'.join(head)]+['\t'.join(escape(row.get(c,'')) for c in head) for key,row in sorted(rows.items())])+'\n',encoding='utf-8')
    args.snapshot.parent.mkdir(parents=True,exist_ok=True);args.snapshot.write_text(json.dumps(originals,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(rows=len(rows),legacy_rows=len(originals),missing_cells=sum(not row.get(lang) for row in rows.values() for lang in LANGS),destination=str(args.destination)),ensure_ascii=False,indent=2))

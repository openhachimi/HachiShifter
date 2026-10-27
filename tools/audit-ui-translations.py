#!/usr/bin/env python3
"""Read-only catalog audit; emits JSON reports, never changes source/catalogs."""
import csv,json,re,argparse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
LIT=r'"((?:[^"\\]|\\.)*)"'
def cpp(value):
    try:return json.loads('"'+value+'"')
    except Exception:return value

def audit(catalog):
    rows=list(csv.DictReader(catalog.read_text(encoding='utf-8-sig').splitlines(),delimiter='\t'))
    languages=['zh-CN','zh-TW','ja-JP','ko-KR','en-US']
    keys={};duplicates=[];missing=[]
    for row in rows:
        key=row['key']
        if key in keys:duplicates.append(key)
        keys[key]=row
        blanks=[lang for lang in languages if not row.get(lang,'').strip()]
        if blanks:missing.append({'key':key,'missing':blanks,'row':row})
    src=(ROOT/'src/I18n.cpp').read_text(encoding='utf-8')
    pattern=re.compile(r'\{\s*'+LIT+r'\s*,\s*\{\s*'+LIT+r'\s*,\s*'+LIT+r'\s*,\s*'+LIT+r'\s*,\s*'+LIT+r'\s*,\s*'+LIT+r'\s*\}\s*\}')
    embedded={cpp(m[0]):dict(zip(languages,[cpp(x) for x in m[1:]])) for m in pattern.findall(src)}
    absent=[{'key':key,'translations':value} for key,value in embedded.items() if key not in keys]
    aliases={x for key,row in keys.items() for x in (key,row.get('zh-CN',''),row.get('en-US','')) if x}
    requests={};pairs={}
    single=re.compile(r'(?<![\w.])(?:utf8|I18n::uiText|u)\(\s*'+LIT+r'\s*\)')
    pair=re.compile(r'(?:materialText|(?:\b\w+\.)?tr|I18n::pairText)\(\s*'+LIT+r'\s*,\s*'+LIT)
    for path in sorted((ROOT/'src').rglob('*')):
        if path.suffix not in ('.cpp','.h','.inc') or 'tests' in path.parts or path.name in ('I18n.cpp','UiLiterals.inc'):continue
        text=path.read_text(encoding='utf-8',errors='replace')
        for m in single.finditer(text):
            value=cpp(m.group(1))
            if value and value not in aliases:requests.setdefault(value,[]).append(f'{path.relative_to(ROOT)}:{text.count(chr(10),0,m.start())+1}')
        for m in pair.finditer(text):
            zh,en=map(cpp,m.groups())
            if en and en not in aliases:pairs.setdefault(en,{'zh-CN':zh,'en-US':en,'locations':[]})['locations'].append(f'{path.relative_to(ROOT)}:{text.count(chr(10),0,m.start())+1}')
    return dict(catalog=str(catalog),rows=len(rows),embedded_rows=len(embedded),duplicates=duplicates,missing_cells=missing,missing_embedded_keys=absent,unmapped_literal_requests=requests,unmapped_pairs=pairs)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('catalog',type=Path);p.add_argument('report',type=Path);args=p.parse_args()
    result=audit(args.catalog);args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:(len(v) if isinstance(v,(list,dict)) else v) for k,v in result.items()},ensure_ascii=False,indent=2))

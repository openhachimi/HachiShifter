#!/usr/bin/env python3
"""Preserve user's edited cells while preparing an external catalog deployment."""
import argparse,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from importlib.machinery import SourceFileLoader
m=SourceFileLoader('deployment_catalog_tools',str(ROOT/'tools/migrate-translation-catalog.py')).load_module()
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('installed',type=Path);p.add_argument('legacy_snapshot',type=Path);p.add_argument('output',type=Path);args=p.parse_args()
    current=m.read(args.installed);next_rows=m.read(ROOT/'resources/lang/translations.tsv')
    baseline={row['key']:row for row in json.loads(args.legacy_snapshot.read_text(encoding='utf-8-sig'))}
    preserved=[]
    for key,row in current.items():
        target=next_rows.setdefault(key,dict(key=key,note=row.get('note',''),aliases='[]'))
        for lang in m.LANGS:
            value=row.get(lang,'')
            if not value or value==target.get(lang,''):continue
            default=baseline.get(key,{}).get(lang)
            generated_literal=key not in baseline and value==key
            if value==default or generated_literal:continue
            target[lang]=value;preserved.append({'key':key,'language':lang,'user_value':value})
        aliases=set(json.loads(target.get('aliases','[]')));aliases.update(json.loads(row.get('aliases','[]')));aliases.add(key)
        target['aliases']=json.dumps(sorted(aliases),ensure_ascii=False,separators=(',',':'))
    header=['key']+m.LANGS+['note','aliases'];args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text('\n'.join(['\t'.join(header)]+['\t'.join(m.escape(row.get(c,'')) for c in header) for key,row in sorted(next_rows.items())])+'\n',encoding='utf-8')
    args.output.with_suffix('.merge.json').write_text(json.dumps({'rows':len(next_rows),'preserved_nondefault_cells':preserved},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'rows':len(next_rows),'preserved_nondefault_cells':len(preserved),'output':str(args.output)},ensure_ascii=False,indent=2))

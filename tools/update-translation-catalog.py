#!/usr/bin/env python3
"""Apply reviewed translations and add discovered UI rows to the external file."""
import argparse,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from importlib.machinery import SourceFileLoader
migration=SourceFileLoader('catalog_migration',str(ROOT/'tools/migrate-translation-catalog.py')).load_module()
LANGS=migration.LANGS
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--fill',type=Path,action='append',default=[]);p.add_argument('--discover',type=Path,action='append',default=[]);p.add_argument('--replace-default-english',action='store_true');args=p.parse_args()
    path=ROOT/'resources/lang/translations.tsv';rows=migration.read(path)
    for file in args.fill:
        data=json.loads(file.read_text(encoding='utf-8-sig'))
        for rawkey,values in data.items():
            key=rawkey if rawkey in rows else migration.unescape(rawkey);row=rows.setdefault(key,dict(key=key,note='',aliases='[]'))
            for lang in LANGS:
                if not row.get(lang) or (args.replace_default_english and lang!='en-US' and row.get(lang)==key and re.search('[A-Za-z]{3}',key)):
                    if values.get(lang):row[lang]=values[lang]
            if values.get('note'):row['note']=values['note']
    existing_aliases={alias:key for key,row in rows.items() for alias in json.loads(row.get('aliases','[]'))}
    for file in args.discover:
        data=json.loads(file.read_text(encoding='utf-8-sig'))
        for item in data:
            literals=item.get('literals') or [item.get('key','')]
            for key in literals:
                if not key or key in existing_aliases:continue
                row=rows.setdefault(key,dict(key=key,note='needs translation',aliases='[]'))
                for lang in LANGS:
                    if not row.get(lang) and item.get(lang):row[lang]=item[lang]
                # Punctuation/units/algorithm labels require no semantic translation.
                if not re.search(r'[A-Za-z\u3400-\u9fff\u3040-\u30ff\uac00-\ud7af]{2}',key):
                    for lang in LANGS:row[lang]=key
                    row['note']='nonlinguistic'
    for key,row in rows.items():
        aliases=set(json.loads(row.get('aliases','[]')));aliases.add(key);row['aliases']=json.dumps(sorted(aliases),ensure_ascii=False,separators=(',',':'))
    header=['key']+LANGS+['note','aliases']
    path.write_text('\n'.join(['\t'.join(header)]+['\t'.join(migration.escape(row.get(c,'')) for c in header) for key,row in sorted(rows.items())])+'\n',encoding='utf-8')
    pending=[{'key':key,**{lang:row.get(lang,'') for lang in LANGS},'note':row.get('note','')} for key,row in rows.items() if any(not row.get(lang) for lang in LANGS)]
    out=ROOT.parent.parent/'validation/translation-file-only/pending-catalog-rows.json';out.write_text(json.dumps(pending,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(rows=len(rows),pending_rows=len(pending),missing_cells=sum(not row.get(lang) for row in rows.values() for lang in LANGS)),indent=2))

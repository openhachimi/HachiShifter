#!/usr/bin/env python3
"""Validate the EXTERNAL source of all UI language values, not a C++ fallback."""
import argparse,json,re,collections,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from importlib.machinery import SourceFileLoader
migration=SourceFileLoader('catalog_validate_tools',str(ROOT/'tools/migrate-translation-catalog.py')).load_module()
def validate(path):
    rows=migration.read(path);errors=[];aliases={}
    for key,row in rows.items():
        for lang in migration.LANGS:
            if not row.get(lang):errors.append(f'{key}: empty {lang}')
        reference=collections.Counter(re.findall(r'\{([A-Za-z0-9_]+)\}',row.get('en-US','')))
        for lang in migration.LANGS:
            if collections.Counter(re.findall(r'\{([A-Za-z0-9_]+)\}',row.get(lang,'')))!=reference:errors.append(f'{key}: inconsistent {lang} placeholders')
        try:
            names=json.loads(row.get('aliases','[]'))
            assert isinstance(names,list) and all(isinstance(n,str) for n in names)
            for name in names:aliases.setdefault(name,key)
        except Exception:errors.append(f'{key}: invalid aliases')
    # Descriptors are model-stable names; only their UI presentation is translated.
    model=(ROOT/'src/ProjectModel.cpp').read_text(encoding='utf-8')
    start=model.index('const std::vector<FlagCurveKind>& flagCurveKinds()');end=model.index('const FlagCurveKind& flagCurveKindFor(',start)
    labels=[migration.legacy.cpp(s) for s in re.findall(r'\{\s*"(?:[^"\\]|\\.)*"\s*,\s*"((?:[^"\\]|\\.)*)"',model[start:end])]
    missing_flags=[label for label in labels if label not in rows and label not in aliases]
    runtime=(ROOT/'src/I18n.cpp').read_text(encoding='utf-8')
    no_embedded='unordered_map<std::string, Row> strings' not in runtime and 'uiSourceLiterals' not in runtime
    result={'file':str(path),'rows':len(rows),'language_cells':len(rows)*5,'errors':errors,'missing_flag_descriptors':missing_flags,'compiled_translation_table_removed':no_embedded}
    return result
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('catalog',type=Path);p.add_argument('--report',type=Path);args=p.parse_args();result=validate(args.catalog)
    if args.report:args.report.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False,indent=2));sys.exit(1 if result['errors'] or not result['compiled_translation_table_removed'] else 0)

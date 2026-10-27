#!/usr/bin/env python3
"""Discover actual C++ UI string arguments, respecting adjacent literals/UTF-8 hex."""
import re,json,argparse,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from importlib.machinery import SourceFileLoader
migration=SourceFileLoader('external_catalog_tools',str(ROOT/'tools/migrate-translation-catalog.py')).load_module()
LIT=r'"((?:[^"\\]|\\.)*)"'
RUN=r'"(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*'
def decode(raw):
    raw=re.sub(r'(?:\\x[0-9a-fA-F]{2})+',lambda m:bytes(int(n,16) for n in re.findall(r'\\x([0-9a-fA-F]{2})',m[0])).decode('utf-8'),raw)
    return json.loads('"'+raw+'"')
def literals(arg):return [''.join(decode(raw) for raw in re.findall(LIT,run)) for run in re.findall(RUN,arg)]

def main(report):
    rows=migration.read(ROOT/'resources/lang/translations.tsv')
    aliases={alias for row in rows.values() for alias in json.loads(row.get('aliases','[]'))}
    discovered={};calls=re.compile(r'(?<![\w])(?:I18n::uiText|utf8|materialText|tr|u|text)\s*\(')
    files=['PianoRollComponent.cpp','AdvancedEnvelopePanel.h','MainComponentHamood.cpp','HamoodTimelinePanel.h','PronunciationCandidateBox.cpp','MainComponentDiffSinger.cpp','TrackListComponent.cpp','TimelineComponent.cpp','OtoWaveformEditorComponent.cpp','MaterialAnnotationEditor.cpp','SettingsComponent.cpp','AssetManagerComponent.cpp','JapaneseAutoAnnotationDialog.cpp','MainComponent.cpp','VoicebankSettingsComponent.cpp','DiffSingerPhonemeEditor.h','DiffSingerPronunciationEditor.h']
    for name in files:
        p=ROOT/'src'/name;s=p.read_text(encoding='utf-8')
        for match in calls.finditer(s):
            # Only string-leading calls, never declarations or arbitrary std text().
            tail=s[match.end():]
            if not tail.lstrip().startswith('"'):continue
            i=match.end();depth=1;quoted=False;escape=False
            while i<len(s) and depth:
                c=s[i]
                if quoted:
                    if escape:escape=False
                    elif c=='\\':escape=True
                    elif c=='"':quoted=False
                elif c=='"':quoted=True
                elif c=='(':depth+=1
                elif c==')':depth-=1
                i+=1
            arg=s[match.end():i-1]
            try:values=literals(arg)
            except Exception:continue
            # Paired tr/materialText key is English half (not both fallbacks).
            if match[0].split('(')[0].strip() in ('tr','materialText') and re.match(r'^\s*'+RUN+r'\s*,\s*'+RUN,arg,re.S) and len(values)>=2:
                source=values[0];values=values[1:2]
            else:source=''
            for key in values:
                if not key or key in aliases:continue
                item=discovered.setdefault(key,{'key':key,'zh-CN':source if source else (key if re.search('[\u3400-\u9fff]',key) else ''),'en-US':key if re.search('[A-Za-z]{3}',key) and not re.search('[\u3400-\u9fff]',key) else '', 'locations':[]})
                item['locations'].append(f'src/{name}:{s.count(chr(10),0,match.start())+1}')
    report.parent.mkdir(parents=True,exist_ok=True);report.write_text(json.dumps(list(discovered.values()),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('new actual literal keys',len(discovered),report)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('report',type=Path);main(p.parse_args().report)

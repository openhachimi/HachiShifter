import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
work=ROOT.parent.parent/'validation/translation-file-only'
piano=json.loads((work/'piano-migrated-literals.json').read_text(encoding='utf-8'))
known={key for item in piano for key in item['literals']}
rows=json.loads((work/'pending-catalog-rows.json').read_text(encoding='utf-8'))
rows=[row for row in rows if row['key'] not in known]
cut=(len(rows)+1)//2
for tag,chunk in [('A',rows[:cut]),('B',rows[cut:])]:
    path=work/f'ui-extra-{tag}.json';path.write_text(json.dumps(chunk,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(tag,len(chunk),path)

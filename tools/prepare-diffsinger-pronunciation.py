"""Install pinned, offline G2P resources from downloaded wheels (no NLTK downloads)."""
import ast
import hashlib
import json
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parents[1]
wheels = root / 'integration-tests/diffsinger-update009/wheels'
target = root / 'juce/engines/diffsinger'
assets = target / 'pronunciation-data'
assets.mkdir(exist_ok=True)
records = []
for wheel in sorted(wheels.glob('*.whl')):
    records.append(dict(file=wheel.name, sha256=hashlib.sha256(wheel.read_bytes()).hexdigest()))
    with zipfile.ZipFile(wheel) as archive:
        if wheel.name.startswith('pypinyin-'):
            for name in archive.namelist():
                if name.startswith('pypinyin/') and not name.endswith('/'):
                    dest = assets / name
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes(archive.read(name))
            name = next(n for n in archive.namelist() if n.endswith('/LICENSE.txt'))
            (assets / 'LICENSE-pypinyin.txt').write_bytes(archive.read(name))
        elif wheel.name.startswith('cmudict-'):
            for source, dest in [('cmudict/data/cmudict.dict', 'cmudict.dict'),
                                  ('cmudict/data/LICENSE', 'LICENSE-cmudict.txt')]:
                (assets / dest).write_bytes(archive.read(source))
        elif wheel.name.startswith('g2p_en-'):
            (assets / 'checkpoint20.npz').write_bytes(archive.read('g2p_en/checkpoint20.npz'))
            (assets / 'LICENSE-g2p-en.txt').write_bytes(archive.read('g2p_en-2.1.0.dist-info/LICENSE.txt'))
            # Keep the original NumPy predictor, excluding NLTK/POS and network
            # side effects. Dictionary lookup and normalization are handled here.
            tree = ast.parse(archive.read('g2p_en/g2p.py').decode('utf-8'))
            cls = next(n for n in tree.body if isinstance(n, ast.ClassDef))
            cls.name = 'EnglishPredictor'
            cls.body = [n for n in cls.body if getattr(n, 'name', '') != '__call__']
            init = next(n for n in cls.body if getattr(n, 'name', '') == '__init__')
            init.body = [n for n in init.body if not (isinstance(n, ast.Assign)
                and any(isinstance(t, ast.Attribute) and t.attr in ('cmu', 'homograph2features') for t in n.targets))]
            code = ('# Adapted from g2p-en 2.1.0, Kyubyong Park and Jongseok Kim.\n'
                    '# Apache-2.0; see pronunciation-data/LICENSE-g2p-en.txt.\n'
                    '# Changes: offline predictor only; no NLTK imports/downloads.\n'
                    'import os\nimport numpy as np\n'
                    'dirname = os.path.join(os.path.dirname(__file__), "pronunciation-data")\n\n')
            (target / 'english_g2p.py').write_text(code + ast.unparse(cls) + '\n', encoding='utf-8')
(assets / 'sources.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
print(json.dumps(records, indent=2))

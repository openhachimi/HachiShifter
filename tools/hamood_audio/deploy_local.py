"""Update the existing local 0.2.3 install without bundling a second PyTorch."""
from pathlib import Path
import shutil, json, hashlib, datetime

repo = Path(__file__).resolve().parents[2]
source = Path(__file__).resolve().parent
dist = repo.parent / 'HachiShifter-整合版-0.2.3-DiffSinger'
trial = repo.parent / 'HAMOOD-东京泰迪熊试作'
runtime = dist / 'engines/hamood'
runtime.mkdir(parents=True, exist_ok=True)
for name in ['worker.py', 'runtime.json']:
    shutil.copy2(source / name, runtime / name)
for name in ['deps', 'models']:
    shutil.copytree(source / name, runtime / name, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('__pycache__', '*.pyc', 'bin'))
vendor = Path('vendor/BTC-ISMIR19')
(runtime / vendor / 'test').mkdir(parents=True, exist_ok=True)
for name in ['btc_model.py', 'run_config.yaml', 'LICENSE', 'test/btc_model_large_voca.pt']:
    shutil.copy2(source / vendor / name, runtime / vendor / name)
shutil.copytree(source / vendor / 'utils', runtime / vendor / 'utils', dirs_exist_ok=True,
                ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
for name in ['hamood-audio.md', 'hamood.md', 'mcp-live.md', 'updates.md']:
    shutil.copy2(repo / 'docs' / name, dist / 'docs' / name)
shutil.copy2(trial / 'mcp-audio-validation.json', dist / 'hamood-audio-validation.json')
shutil.copy2(repo / 'build-integrated/hamood-audio-core-regression/report.json', dist / 'hamood-core-regression-036.json')
shutil.copy2(repo / 'build-integrated/hamood-audio-live-regression/report.json', dist / 'hamood-live-regression-036.json')
exe = repo / 'build-integrated/HachiShifterNext_artefacts/RelWithDebInfo/HachiShifter Next.exe'
shutil.copy2(exe, dist / exe.name)
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert digest(exe) == digest(dist / exe.name)
info_path = dist / 'build-info.json'
info = json.loads(info_path.read_text(encoding='utf-8-sig'))
analysis = json.loads((trial / 'mcp-analysis-summary.json').read_text(encoding='utf-8'))
preview = json.loads((trial / 'chord-guided-preview.json').read_text(encoding='utf-8'))
info.update(exe_sha256=digest(exe), built_at=datetime.datetime.now().astimezone().isoformat(),
            current_documentation_revision='036', documentation_revision='036',
            edition='HAMOOD accompaniment-aware harmony and live MCP audio analysis')
info['hamood_audio_update_036'] = dict(
    version='0.2.3', exe_sha256=digest(exe), deployment='complete',
    core_checks=99, live_regression_checks=28, real_project_checks=22,
    analysis=analysis, audio_covered_notes=preview['audio_covered_notes'],
    audio_adjusted_targets=preview['audio_adjusted_targets'],
    same_decoder_as_editor=True, source_project_modified=False,
    optional_runtime_bytes=sum(p.stat().st_size for p in runtime.rglob('*') if p.is_file()),
    python_external='D:/anaconda/python.exe',
    runtime_portable=False)
info_path.write_text(json.dumps(info, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
readme = dist / '使用说明.txt'
content = readme.read_text(encoding='utf-8-sig').replace('目前为 035', '目前为 036')
entry = '\n更新 036：窗口 MCP 支持伴奏节拍、和弦、低频候选分析，拍点对齐预览，以及参考伴奏和弦生成独立和声轨。入口和限制见 docs/hamood-audio.md。本机复用 D:/anaconda/python.exe，移至其他电脑需配置 engines/hamood/runtime.json；未重复打包 PyTorch。\n'
if '更新 036：' not in content:
    content += entry
readme.write_text(content, encoding='utf-8')
print(json.dumps({'exe':str(dist/exe.name), 'sha256':digest(exe),
                 'runtime_bytes':info['hamood_audio_update_036']['optional_runtime_bytes']},ensure_ascii=True))

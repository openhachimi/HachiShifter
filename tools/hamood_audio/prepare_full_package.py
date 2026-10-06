"""Stage a portable public distribution without user projects or machine paths."""
from pathlib import Path
import json, hashlib, shutil, datetime

repo = Path(__file__).resolve().parents[2]
work = repo.parent
source = work/'HachiShifter-整合版-0.2.3-DiffSinger'
target = work/'配布预检-037/HachiShifter-0.2.3-Windows-x64'
target.mkdir(parents=True, exist_ok=True)
excluded = {'__pycache__', '.cache', 'backups', 'verification', '.git'}

def include(p):
    q = p.relative_to(source)
    if any(x in excluded for x in q.parts) or p.suffix.lower() in {'.log','.pyc','.pyo','.tmp'}:
        return False
    if q.parts[0] in {'mcp-live.json','build-info.json'} or p.name in {'wcsndm_lastcall.txt','input.npy'}:
        return False
    if len(q.parts) == 1 and p.suffix == '.json':
        return False
    return True

for p in source.rglob('*'):
    if p.is_file() and include(p):
        out = target/p.relative_to(source)
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p,out)
exe = repo/'build-integrated/HachiShifterNext_artefacts/RelWithDebInfo/HachiShifter Next.exe'
shutil.copy2(exe,target/exe.name)
(target/'engines/hamood/runtime.json').write_text(json.dumps({'python':'../python/python.exe'},indent=2)+'\n',encoding='utf-8')
for name in ['kara2.md','hamood-audio.md','updates.md']:
    shutil.copy2(repo/'docs'/name,target/'docs'/name)
(target/'使用说明.txt').write_text('HachiShifter Next 0.2.3 / 内部文档 037\n\n'
    '将整个目录解压后运行 HachiShifter Next.exe，无需安装 Python 或下载内置模型。\n'
    '包含 WCSNDM 0.0803、HF / NSF-HiFiGAN、DiffSinger 运行库、HAMOOD Beat This! / BTC / KARA2。音源库需另行添加。\n'
    'KARA2 用法见 docs/kara2.md；HAMOOD 伴奏分析通过当前窗口 MCP 调用，见 docs/hamood-audio.md。\n'
    '配置 MCP 时将 command 设置为解压后的 HachiShifter Next.exe 绝对路径，args 为 ["--mcp-live"]。\n',encoding='utf-8')
info = dict(version='0.2.3',engine_version='0.0803',documentation_revision='037',current_documentation_revision='037',
    exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),built_at=datetime.datetime.now().astimezone().isoformat(),
    edition='Complete portable runtime with HAMOOD and KARA2',runtime_portable=True,
    python='engines/python/python.exe',models=['NSF-HiFiGAN ONNX','WCSNDM HF checkpoint','Beat This! small0','BTC 170','UVR MDX-NET KARA2'])
(target/'build-info.json').write_text(json.dumps(info,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
files=[p for p in target.rglob('*') if p.is_file()]
print(json.dumps(dict(path=str(target),files=len(files),bytes=sum(p.stat().st_size for p in files)),ensure_ascii=True),flush=True)

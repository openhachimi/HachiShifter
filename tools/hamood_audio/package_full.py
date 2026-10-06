"""Zip the complete portable distribution and verify every file's CRC and SHA256."""
from pathlib import Path
from collections import defaultdict
import json, hashlib, zipfile
repo=Path(__file__).resolve().parents[2]
source=repo.parent/'配布预检-037/HachiShifter-0.2.3-Windows-x64'
target=repo.parent/'HachiShifter-0.2.3-Windows-x64-完整配布包-20260929.zip'
assert not target.exists(),target
records=[]
with zipfile.ZipFile(target,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as z:
    files=sorted(p for p in source.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix not in {'.pyc','.log'})
    for i,p in enumerate(files):
        name=p.relative_to(source).as_posix()
        before=p.stat()
        with p.open('rb') as stream: sha=hashlib.file_digest(stream,'sha256').hexdigest()
        archive_name=source.name+'/'+name
        z.write(p,archive_name)
        after=p.stat()
        assert (before.st_size,before.st_mtime_ns)==(after.st_size,after.st_mtime_ns),p
        records.append(dict(path=name,bytes=after.st_size,compressed_bytes=z.getinfo(archive_name).compress_size,sha256=sha))
        if i%1500==0:print(f'Packed {i+1}/{len(files)}',flush=True)
    z.writestr(source.name+'/package-manifest.json',json.dumps(dict(version='0.2.3',documentation_revision='037',files=records),ensure_ascii=False,indent=2))
print('Verifying every archived file...',flush=True)
with zipfile.ZipFile(target) as z:
    for r in records:
        with z.open(source.name+'/'+r['path']) as f:
            assert hashlib.file_digest(f,'sha256').hexdigest()==r['sha256'],r['path']
    assert z.testzip() is None
groups=defaultdict(lambda:dict(bytes=0,compressed_bytes=0,files=0))
for r in records:
    parts=r['path'].split('/')
    group='/'.join(parts[:2]) if parts[0] in {'engines','models'} else 'editor-and-docs'
    groups[group]['bytes']+=r['bytes'];groups[group]['compressed_bytes']+=r['compressed_bytes'];groups[group]['files']+=1
with target.open('rb') as f:sha=hashlib.file_digest(f,'sha256').hexdigest()
report=dict(package=str(target),bytes=target.stat().st_size,uncompressed_bytes=sum(r['bytes'] for r in records),
            files=len(records),sha256=sha,crc_verified=True,all_file_hashes_verified=True,groups=dict(groups))
target.with_suffix('.package.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
target.with_suffix('.zip.sha256').write_text(sha+'  '+target.name+'\n',encoding='utf-8')
print(json.dumps(report,ensure_ascii=True,indent=2),flush=True)

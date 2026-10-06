"""Snapshot the verified portable build before adding another backend."""
import hashlib
import json
from pathlib import Path
import zipfile
import argparse

WORKSPACE = Path(__file__).resolve().parents[2]
SOURCE = WORKSPACE / 'HachiShifter-整合版-0.2.2'
TARGET = WORKSPACE / 'HachiShifter-0.2.2-Windows-x64-配布包-20260925.zip'
PREFIX = 'HachiShifter-0.2.2-Windows-x64'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def include(relative):
    if any(part in {'backups', 'verification', '__pycache__', '.cache', '.git'} for part in relative.parts):
        return False
    return relative.suffix.lower() not in {'.pyc', '.pyo', '.log', '.tmp'} and relative.name not in {
        'wcsndm_lastcall.txt', 'input.npy'
    }


def main():
    global SOURCE, TARGET, PREFIX
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, default=SOURCE)
    parser.add_argument('--target', type=Path, default=TARGET)
    args = parser.parse_args()
    SOURCE, TARGET = args.source, args.target
    assert not TARGET.exists(), f'Existing package will not be overwritten: {TARGET}'
    info = json.loads((SOURCE / 'build-info.json').read_text(encoding='utf-8-sig'))
    version = info['version']
    baseline = version == '0.2.2'
    PREFIX = f'HachiShifter-{version}-Windows-x64'
    assert digest(SOURCE / 'HachiShifter Next.exe') == info['exe_sha256']
    files = sorted(p for p in SOURCE.rglob('*') if p.is_file() and include(p.relative_to(SOURCE)))
    records = []
    with zipfile.ZipFile(TARGET, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=3, allowZip64=True) as archive:
        for index, path in enumerate(files):
            before = path.stat()
            checksum = digest(path)
            relative = path.relative_to(SOURCE).as_posix()
            archive.write(path, PREFIX + '/' + relative)
            after = path.stat()
            assert (before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns), path
            records.append(dict(path=relative, size=after.st_size, sha256=checksum))
            if index % 1000 == 0:
                print(f'Packed {index + 1}/{len(files)} files', flush=True)
        manifest = dict(version=version, engine='0.0803', baseline_before_diffsinger=baseline, files=records)
        archive.writestr(PREFIX + '/package-manifest.json', json.dumps(manifest, ensure_ascii=False, indent=2))
        archive.writestr(PREFIX + '/配布说明.txt',
            f'HachiShifter Next {version} — Windows x64\n'
            '解压整个文件夹后运行 HachiShifter Next.exe。\n'
            '包含 WCSNDM 0.0803、HF 运行库及当前 NSF-HiFiGAN 模型。\n'
            + ('此包保存添加 DiffSinger 兼容前的版本，尚未包含 DiffSinger。\n' if baseline else
               '含 DiffSinger 初版兼容，参见 docs/DiffSinger使用说明.md。\n') +
            '音源库需要用户另行添加；历史备份和内部验证文件不包含在此配布包中。\n')
    print('Checking archive CRC...', flush=True)
    with zipfile.ZipFile(TARGET) as archive:
        assert archive.testzip() is None
        assert hashlib.sha256(archive.read(PREFIX + '/HachiShifter Next.exe')).hexdigest() == info['exe_sha256']
    checksum = digest(TARGET)
    TARGET.with_suffix('.zip.sha256').write_text(checksum + '  ' + TARGET.name + '\n', encoding='utf-8')
    report = dict(package=str(TARGET), files=len(files), bytes=TARGET.stat().st_size,
                  uncompressed_bytes=sum(r['size'] for r in records), sha256=checksum, crc_verified=True,
                  executable_matches_current=True)
    TARGET.with_suffix('.package.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False), flush=True)


if __name__ == '__main__':
    main()

"""Consolidate portable diagnostics and old executables without moving runtime files."""
import argparse
import hashlib
import json
import re
from datetime import datetime, timezone
from pathlib import Path


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def inside(root, path):
    path.resolve().relative_to(root)
    return path


def organize(folder, apply=False):
    root = folder.resolve(strict=True)
    if not (root / 'HachiShifter Next.exe').is_file():
        raise ValueError('Choose a portable distribution containing HachiShifter Next.exe')
    files = sorted(p for p in root.iterdir() if p.is_file())
    moves = []
    for source in files:
        if source.suffix.lower() == '.json' and re.search(r'(validation|regression|audio-info)', source.stem):
            destination = root / 'verification' / 'reports' / source.name
        elif re.fullmatch(r'HachiShifter Next\.previous-update\d+\.bin', source.name):
            destination = root / 'backups' / 'previous-executables' / source.name
        else:
            continue
        inside(root, source)
        inside(root, destination)
        if destination.exists():
            raise FileExistsError(f'Refusing to overwrite: {destination}')
        moves.append((source, destination))
    summary = {'root': str(root), 'apply': apply, 'root_files_before': len(files),
               'files_to_move': len(moves), 'root_files_after': len(files) - len(moves)}
    if not apply:
        return summary
    if not moves:
        summary['status'] = 'already_organized'
        return summary
    moved_names = {source.name for source, _ in moves}
    retained = {p.name: digest(p) for p in files if p.name not in moved_names}
    resources = ('engines', 'models', 'third_party')
    for name in resources:
        if not inside(root, root / name).is_dir():
            raise FileNotFoundError(root / name)
    records = [{'from': source.relative_to(root).as_posix(),
                'to': destination.relative_to(root).as_posix(),
                'bytes': source.stat().st_size, 'sha256': digest(source)}
               for source, destination in moves]
    report = inside(root, root / 'verification' / 'directory-layout.json')
    if report.exists():
        raise FileExistsError(f'Refusing to overwrite previous migration record: {report}')
    completed = []
    try:
        for source, destination in moves:
            destination.parent.mkdir(parents=True, exist_ok=True)
            source.rename(destination)
            completed.append((source, destination))
        for record in records:
            destination = root / record['to']
            if digest(destination) != record['sha256']:
                raise RuntimeError(f'Content changed: {destination}')
        for name, checksum in retained.items():
            if digest(root / name) != checksum:
                raise RuntimeError(f'Runtime or configuration changed: {name}')
        summary.update(status='organized', timestamp_utc=datetime.now(timezone.utc).isoformat(),
                       moved_files=records, retained_file_sha256=retained,
                       all_moved_contents_verified=True, retained_contents_verified=True,
                       resource_directories_unchanged=list(resources))
        report.parent.mkdir(parents=True, exist_ok=True)
        # Exclusive creation preserves any concurrently created audit record.
        with report.open('x', encoding='utf-8') as stream:
            json.dump(summary, stream, ensure_ascii=False, indent=2)
            stream.write('\n')
    except Exception:
        for source, destination in reversed(completed):
            if source.exists():
                raise RuntimeError(f'Cannot roll back over existing file: {source}')
            destination.rename(source)
        raise
    return {**{k: v for k, v in summary.items() if k not in ('moved_files', 'retained_file_sha256')},
            'record': str(report)}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('distribution', type=Path)
    parser.add_argument('--apply', action='store_true', help='Move files; without this flag only preview counts')
    args = parser.parse_args()
    print(json.dumps(organize(args.distribution, args.apply), ensure_ascii=False, indent=2))

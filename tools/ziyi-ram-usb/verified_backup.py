"""Create an exclusive, hashed backup and independently check its restore copy.

Never overwrites a destination. All input paths must be inside the workspace.
Use before replacing local port files; this does not back up phone partitions.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def inside(path, parent):
    resolved = path.resolve()
    if not resolved.is_relative_to(parent.resolve()):
        raise ValueError(f'Path outside intended directory: {path}')
    return resolved


def copy_new(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    with source.open('rb') as src, target.open('xb') as dst:
        shutil.copyfileobj(src, dst, 1024 * 1024)


def verify(folder, current=False):
    folder = inside(folder, ROOT)
    manifest = folder / 'manifest.json'
    if digest(manifest) != (folder / 'manifest.sha256').read_text().strip():
        raise ValueError('Backup manifest digest mismatch')
    data = json.loads(manifest.read_text())
    expected = set()
    if data['workspace'] != str(ROOT) or not data['files']:
        raise ValueError('Backup workspace or inventory mismatch')
    for entry in data['files']:
        name = entry['path']
        for prefix in ('files', 'restore-check'):
            file = inside(folder / prefix / name, folder / prefix)
            if file.stat().st_size != entry['bytes'] or digest(file) != entry['sha256']:
                raise ValueError(f'Backup/restore content mismatch: {prefix}/{name}')
            expected.add(file)
        if current and digest(inside(ROOT / name, ROOT)) != entry['sha256']:
            raise ValueError(f'Live source changed since backup: {name}')
    actual = {p.resolve() for prefix in ('files', 'restore-check')
              for p in (folder / prefix).rglob('*') if p.is_file()}
    if actual != expected:
        raise ValueError('Unexpected or missing files in backup inventory')
    return data


def create(folder, names, trees):
    folder = inside(folder, ROOT)
    if folder.exists():
        raise FileExistsError('Backup destination already exists; select a new name')
    inputs = set()
    for name in names:
        path = inside(ROOT / name, ROOT)
        if not path.is_file():
            raise ValueError(f'Not a file: {name}')
        inputs.add(path)
    for name in trees:
        tree = inside(ROOT / name, ROOT)
        if not tree.is_dir() or folder.is_relative_to(tree):
            raise ValueError(f'Invalid/recursive input tree: {name}')
        for path in tree.rglob('*'):
            if path.is_symlink():
                raise ValueError(f'Symlink in backup scope: {path}')
            if path.is_file() and '.git' not in path.relative_to(tree).parts:
                inputs.add(inside(path, ROOT))
    if not inputs:
        raise ValueError('Empty backup scope')
    folder.mkdir(parents=True, exist_ok=False)
    entries = []
    for source in sorted(inputs):
        name = source.relative_to(ROOT).as_posix()
        before = digest(source)
        size = source.stat().st_size
        dest = folder / 'files' / name
        copy_new(source, dest)
        if digest(dest) != before or digest(source) != before or dest.stat().st_size != size:
            raise ValueError(f'Source changed or backup copy failed: {name}')
        # Restore from the backup, not from the live source, into a separate tree.
        restored = folder / 'restore-check' / name
        copy_new(dest, restored)
        if digest(restored) != before:
            raise ValueError(f'Restore check failed: {name}')
        entries.append(dict(path=name, bytes=size, sha256=before))
    data = dict(version=1, workspace=str(ROOT), createdUtc=datetime.now(timezone.utc).isoformat(),
                scope='Local files only; no phone partition backup', files=entries)
    manifest = folder / 'manifest.json'
    with manifest.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(data, indent=2, ensure_ascii=False) + '\n')
    with (folder / 'manifest.sha256').open('x') as stream:
        stream.write(digest(manifest) + '\n')
    verify(folder, current=True)
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('create', 'verify'))
    parser.add_argument('folder', type=Path)
    parser.add_argument('--file', action='append', default=[])
    parser.add_argument('--tree', action='append', default=[])
    parser.add_argument('--require-current', action='store_true')
    args = parser.parse_args()
    data = (create(args.folder, args.file, args.tree) if args.mode == 'create'
            else verify(args.folder, args.require_current))
    print(json.dumps(dict(result='PASS', files=len(data['files']),
                          bytes=sum(e['bytes'] for e in data['files']),
                          backup=str(args.folder.resolve()),
                          restoreCopiesVerified=True, liveSourcesChecked=args.mode == 'create' or args.require_current)))


if __name__ == '__main__':
    main()

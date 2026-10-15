#!/usr/bin/env python3
"""Inventory supplied retail bytes and stage only the existing tested edition."""
import argparse
from contextlib import nullcontext
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import sys

FILES = {
    'd1': ('descent.hog', 'descent.pig'),
    'd2': ('descent2.hog', 'descent2.ham', 'descent2.s22', 'groupa.pig',
           'alien1.pig', 'alien2.pig', 'fire.pig', 'ice.pig', 'water.pig'),
}


def baseline(repo):
    source = (Path(repo) / 'android/helpers/standard_game_data.ps1').read_text(encoding='utf-8')
    blocks = re.findall(
        r'(?ms)^function Get-StandardGameDataDeps\s*\{\s*(?:#[^\n]*\n\s*)*return\s+@\(\s*\n(.*?)^\s*\)\s*\n\}',
        source)
    if len(blocks) != 1:
        raise ValueError('Cannot identify the reviewed standard retail identity list')
    pairs = []
    for line in blocks[0].splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        match = re.fullmatch(r'@\{\s*file\s*=\s*"([^"]+)";\s*sha256\s*=\s*"([0-9a-f]{64})"\s*\}', line)
        if not match:
            raise ValueError('Unrecognized standard retail identity entry')
        pairs.append(match.groups())
    expected = {name for names in FILES.values() for name in names}
    if len(pairs) != 11 or {name for name, _ in pairs} != expected:
        raise ValueError('Standard retail identity list differs from the reviewed eleven-file contract')
    return dict(pairs)


def identity_digest(records):
    identity = sorted(({'path': row['path'], 'sha256': row['sha256']} for row in records),
                      key=lambda row: row['path'])
    return hashlib.sha256(json.dumps(identity, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()


def baseline_identity(repo):
    expected = baseline(repo)
    return identity_digest([{'path': game + '/' + name, 'sha256': expected[name]}
                            for game, names in FILES.items() for name in names])


def reject_links(path):
    path = Path(path).absolute()
    for component in (path, *path.parents):
        try:
            info = component.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(info.st_mode) or (getattr(info, 'st_file_attributes', 0)
                                        & getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0)):
            raise ValueError('Retail paths must not contain symbolic links or junctions')


def file_identity(path, target=None):
    """Stream bytes with bounded memory and reject replacement or in-place changes."""
    reject_links(path)
    before = path.stat()
    if not stat.S_ISREG(before.st_mode):
        raise ValueError('Retail input must contain regular files')
    flags = os.O_RDONLY | getattr(os, 'O_BINARY', 0) | getattr(os, 'O_NOFOLLOW', 0) | getattr(os, 'O_NONBLOCK', 0)
    with os.fdopen(os.open(path, flags), 'rb') as stream:
        opened = os.fstat(stream.fileno())
        if not stat.S_ISREG(opened.st_mode) or not os.path.samestat(before, opened):
            raise ValueError('Retail file changed while opening')
        digest = hashlib.sha256()
        size = 0
        with target.open('xb') if target else nullcontext() as output:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                size += len(chunk)
                digest.update(chunk)
                if output is not None:
                    output.write(chunk)
        reject_links(path)
        after = path.stat()
        # Check the pathname as well as the open descriptor to catch replacement
        for current in (os.fstat(stream.fileno()), after):
            if (not os.path.samestat(before, current) or before.st_size != current.st_size
                    or before.st_mtime_ns != current.st_mtime_ns or before.st_ctime_ns != current.st_ctime_ns):
                raise ValueError('Retail file changed while reading')
    return size, digest.hexdigest()


def checked_source_paths(source_paths=None):
    paths = [game + '/' + name for game, names in FILES.items() for name in names]
    if source_paths is None:
        return {path: path for path in paths}
    if not isinstance(source_paths, dict) or set(source_paths) != set(paths):
        raise ValueError('Retail source map must cover exactly the eleven canonical paths')
    for source in source_paths.values():
        if (not isinstance(source, str) or re.search(r'[\\:\x00-\x1f]', source)
                or any(part in ('', '.', '..') or part.endswith(('.', ' ')) for part in source.split('/'))):
            raise ValueError('Retail source paths must be plain relative paths without traversal')
    if len({source.casefold() for source in source_paths.values()}) != len(paths):
        raise ValueError('Retail source map must not contain duplicate source paths')
    return dict(source_paths)


def unique_source_keys(pairs):
    result = dict(pairs)
    if len(result) != len(pairs):
        raise ValueError('Retail source map contains duplicate JSON keys')
    return result


def inventory(root, expected, source_paths=None):
    if (set(expected) != {name for names in FILES.values() for name in names}
            or any(not isinstance(value, str) or not re.fullmatch('[0-9a-f]{64}', value)
                   for value in expected.values())):
        raise ValueError('Expected identities must cover exactly eleven retail files')
    root = Path(root)
    reject_links(root)
    if not root.is_dir():
        raise ValueError('Retail input must be a real directory')
    mapped = source_paths is not None
    sources = checked_source_paths(source_paths)
    records = []
    for game, names in FILES.items():
        for name in names:
            relative = game + '/' + name
            source = root / sources[relative]
            reject_links(source.parent)
            if not source.parent.is_dir():
                raise ValueError('Missing retail source directory')
            size, actual = file_identity(source)
            row = {'path': relative, 'size': size, 'sha256': actual,
                   'matches_tested_edition': actual == expected[name]}
            if mapped:
                row['source_path'] = sources[relative]
            records.append(row)
    return {'schema': 1, 'files': records, 'manifest_sha256': identity_digest(records),
            'matches_tested_edition': all(row['matches_tested_edition'] for row in records)}


def stage(root, destination, manifest):
    if manifest.get('matches_tested_edition') is not True:
        raise ValueError('Supplied retail edition differs from existing test fixtures; review identities before enabling tests')
    records = manifest.get('files', [])
    paths = [game + '/' + name for game, names in FILES.items() for name in names]
    if (manifest.get('schema') != 1 or not isinstance(records, list) or len(records) != 11
            or any(not isinstance(row, dict) for row in records)
            or [row.get('path') for row in records] != paths
            or any(row.get('matches_tested_edition') is not True or type(row.get('size')) is not int
                   or row['size'] < 0 or not isinstance(row.get('sha256'), str)
                   or not re.fullmatch('[0-9a-f]{64}', row['sha256']) for row in records)
            or manifest.get('manifest_sha256') != identity_digest(records)):
        raise ValueError('Invalid verified retail inventory')
    sources = checked_source_paths({row['path']: row.get('source_path', row['path']) for row in records})
    root, destination = Path(root), Path(destination)
    reject_links(root)
    reject_links(destination)
    if destination.exists():
        raise ValueError('Retail stage already exists; refusing to overwrite it')
    if destination.resolve().is_relative_to(root.resolve()) or root.resolve().is_relative_to(destination.resolve()):
        raise ValueError('Retail stage must be separate from the input tree')
    destination.mkdir(parents=True)
    try:
        for row in records:
            target = destination / row['path']
            reject_links(target)
            target.parent.mkdir(exist_ok=True)
            size, digest = file_identity(root / sources[row['path']], target)
            if size != row['size'] or digest != row['sha256']:
                raise ValueError('Retail file changed during staging')
            if file_identity(target) != (row['size'], row['sha256']):
                raise ValueError('Staged retail file differs from verified inventory')
    except BaseException:
        shutil.rmtree(destination)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', required=True, type=Path)
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path, help='New metadata-only report path')
    parser.add_argument('--stage', type=Path)
    parser.add_argument('--source-map', type=Path, help='Reviewed JSON map from canonical paths to supplied relative paths')
    args = parser.parse_args()
    reject_links(args.output)
    if args.output.resolve().is_relative_to(args.root.resolve()) or (
            args.stage and args.output.resolve().is_relative_to(args.stage.resolve())):
        raise ValueError('Retail inventory report must be outside the input and stage trees')
    if args.output.exists():
        raise ValueError('Retail inventory report already exists; refusing to overwrite it')
    source_paths = None
    if args.source_map:
        reject_links(args.source_map)
        source_paths = json.loads(args.source_map.read_text(encoding='utf-8'), object_pairs_hook=unique_source_keys)
        if source_paths is None:
            raise ValueError('Retail source map must be a JSON object')
    result = inventory(args.root, baseline(args.repo), source_paths)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8', newline='\n') as output:
        output.write(json.dumps(result, indent=2) + '\n')
    if not result['matches_tested_edition']:
        print('Retail edition mismatch; inspect file identities in the inventory report', file=sys.stderr)
        return 1
    if args.stage:
        stage(args.root, args.stage, result)
    print('Verified eleven retail files against the existing tested edition')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)

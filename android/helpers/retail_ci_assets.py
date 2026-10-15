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


# Staged identities match the existing Vertigo and Obsidian fixture pins
# Only the reviewed LF descriptor is converted, exclusively in the new stage
MISSION_FILES = (
    {'path': 'missions/d2x.hog', 'source_path': 'descent_2_vertigo/d2x.hog',
     'source_size': 4371791, 'source_sha256': '97a4433833ec1f2fbdd2becc144a1e9726ca78b1210a71a49ddbec6182665fe6',
     'size': 4371791, 'sha256': '97a4433833ec1f2fbdd2becc144a1e9726ca78b1210a71a49ddbec6182665fe6', 'transform': 'none'},
    {'path': 'missions/d2x.mn2', 'source_path': 'descent_2_vertigo/d2x.mn2',
     'source_size': 381, 'source_sha256': '7b7e2445304d5541c42af7d2033ad2cbf693a2db080270853f46ecfd26825811',
     'size': 408, 'sha256': '42d1c04c96f4dad7e8d03a6fdf342ac9ccd22672c0db5886f965638bd325764d',
     'transform': 'd2x-mn2-lf-to-crlf'},
    {'path': 'missions/obsidian.zip', 'source_path': 'missions/Obsidian.zip',
     'source_size': 7676896, 'source_sha256': '441c970b8ebab45f9544c00cd44843f1fce7cda649ee402ef6e4e407f94ef5ce',
     'size': 7676896, 'sha256': '441c970b8ebab45f9544c00cd44843f1fce7cda649ee402ef6e4e407f94ef5ce', 'transform': 'none'},
)


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


def identity_digest(records, include_missions=False):
    identity = sorted(({'path': row['path'], 'sha256': row['sha256']} for row in records),
                      key=lambda row: row['path'])
    if include_missions:
        identity = sorted(({'path': row['path'], 'sha256': row['sha256'],
                            'source_sha256': row.get('source_sha256', row['sha256']),
                            'transform': row.get('transform', 'none')} for row in records), key=lambda row: row['path'])
    return hashlib.sha256(json.dumps(identity, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()


def baseline_identity(repo, include_missions=False):
    expected = baseline(repo)
    records = [{'path': game + '/' + name, 'sha256': expected[name]}
               for game, names in FILES.items() for name in names]
    if include_missions:
        records.extend(MISSION_FILES)
    return identity_digest(records, include_missions)


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


def file_identity(path, target=None, normalize_descriptor=False):
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
        descriptor = bytearray()
        with target.open('xb') if target and not normalize_descriptor else nullcontext() as output:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                size += len(chunk)
                digest.update(chunk)
                if normalize_descriptor:
                    if size > MISSION_FILES[1]['source_size']:
                        raise ValueError('Unapproved Vertigo descriptor input')
                    descriptor.extend(chunk)
                if output is not None:
                    output.write(chunk)
        reject_links(path)
        after = path.stat()
        # Compare each API with itself; stat/fstat timestamps can differ on Windows
        # Keep both pathname and descriptor checks to catch replacement and mutation
        for previous, current in ((opened, os.fstat(stream.fileno())), (before, after)):
            if (not os.path.samestat(previous, current) or previous.st_size != current.st_size
                    or previous.st_mtime_ns != current.st_mtime_ns or previous.st_ctime_ns != current.st_ctime_ns):
                raise ValueError('Retail file changed while reading')
    if normalize_descriptor:
        pin = MISSION_FILES[1]
        if ((size, digest.hexdigest()) != (pin['source_size'], pin['source_sha256']) or b'\r' in descriptor):
            raise ValueError('Unapproved Vertigo descriptor input')
        normalized = descriptor.replace(b'\n', b'\r\n')
        identity = len(normalized), hashlib.sha256(normalized).hexdigest()
        if identity != (pin['size'], pin['sha256']):
            raise ValueError('Normalized Vertigo descriptor differs from existing fixture pin')
        if target is not None:
            with target.open('xb') as output:
                output.write(normalized)
        return identity
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


def inventory(root, expected, source_paths=None, include_missions=False):
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
    if include_missions:
        for pin in MISSION_FILES:
            source_size, source_digest = file_identity(root / pin['source_path'])
            matches = (source_size, source_digest) == (pin['source_size'], pin['source_sha256'])
            size, digest = source_size, source_digest
            if pin['transform'] != 'none':
                # Never transform unreviewed input; report its raw identity on mismatch
                size, digest = file_identity(root / pin['source_path'], normalize_descriptor=True) if matches else (None, None)
            records.append({'path': pin['path'], 'source_path': pin['source_path'],
                            'source_size': source_size, 'source_sha256': source_digest,
                            'size': size, 'sha256': digest, 'transform': pin['transform'],
                            'matches_tested_edition': matches and (size, digest) == (pin['size'], pin['sha256'])})
    result = {'schema': 1, 'files': records, 'manifest_sha256': identity_digest(records, include_missions),
              'matches_tested_edition': all(row['matches_tested_edition'] for row in records)}
    if include_missions:
        result['includes_missions'] = True
    return result


def stage_inventory(root, expected, include_missions=False):
    """Verify canonical staged bytes without claiming to recheck their raw sources."""
    result = inventory(root, expected)
    if include_missions:
        for pin in MISSION_FILES:
            size, digest = file_identity(Path(root) / pin['path'])
            result['files'].append({'path': pin['path'], 'size': size, 'sha256': digest,
                                    'matches_tested_edition': (size, digest) == (pin['size'], pin['sha256'])})
        result['manifest_sha256'] = identity_digest(result['files'])
        result['matches_tested_edition'] = all(row['matches_tested_edition'] for row in result['files'])
        result['includes_missions'] = True
        result['verification_scope'] = 'staged-only'
    return result


def stage(root, destination, manifest, include_missions=False):
    if manifest.get('matches_tested_edition') is not True:
        raise ValueError('Supplied retail edition differs from existing test fixtures; review identities before enabling tests')
    records = manifest.get('files', [])
    paths = [game + '/' + name for game, names in FILES.items() for name in names]
    if include_missions:
        paths.extend(pin['path'] for pin in MISSION_FILES)
    if (manifest.get('schema') != 1 or manifest.get('includes_missions', False) is not include_missions
            or not isinstance(records, list) or len(records) != len(paths)
            or any(not isinstance(row, dict) for row in records)
            or [row.get('path') for row in records] != paths
            or any(row.get('transform', 'none') != 'none' for row in records[:11])
            or any(row.get('matches_tested_edition') is not True or type(row.get('size')) is not int
                   or row['size'] < 0 or not isinstance(row.get('sha256'), str)
                   or not re.fullmatch('[0-9a-f]{64}', row['sha256']) for row in records)
            or manifest.get('manifest_sha256') != identity_digest(records, include_missions)):
        raise ValueError('Invalid verified retail inventory')
    if include_missions and any(any(row.get(key) != value for key, value in pin.items())
                                for row, pin in zip(records[11:], MISSION_FILES)):
        raise ValueError('Unapproved supplemental retail identity or transform')
    sources = checked_source_paths({row['path']: row.get('source_path', row['path']) for row in records[:11]})
    if include_missions:
        sources.update({pin['path']: pin['source_path'] for pin in MISSION_FILES})
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
            source = root / sources[row['path']]
            if row.get('transform') == 'd2x-mn2-lf-to-crlf':
                size, digest = file_identity(source, target, normalize_descriptor=True)
            else:
                size, digest = file_identity(source, target)
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
    parser.add_argument('--include-missions', action='store_true', help='Include the three reviewed Vertigo/Obsidian files')
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
    result = inventory(args.root, baseline(args.repo), source_paths, args.include_missions)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8', newline='\n') as output:
        output.write(json.dumps(result, indent=2) + '\n')
    if not result['matches_tested_edition']:
        print('Retail edition mismatch; inspect file identities in the inventory report', file=sys.stderr)
        return 1
    if args.stage:
        stage(args.root, args.stage, result, args.include_missions)
    print('Verified ' + ('fourteen' if args.include_missions else 'eleven') + ' retail files against the existing tested edition')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)

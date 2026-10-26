"""Discover bounded regression owners from existing checked-in mission baselines."""
import hashlib
import json
import re
from pathlib import Path
import sys
import subprocess

CORPUS_COMMIT = 'd9fbeaf6eb059fd4504308bfd6364734775b7cba'
HISTORICAL_CORPUS_COMMIT = '5fb5b199f9f8c670cff7b0b4ea7086c8505bbefa'
VERTIGO_BASELINE = 'CD - Descent II - The Vertigo Series (USA).json'


def vertigo_campaign(relative, mission):
    return (relative == VERTIGO_BASELINE and mission.get('target_index', 0) == 0
            and mission.get('mission_filename', '').casefold() == 'd2x.mn2'
            and mission.get('game') == 'd2')


def unavailable_level(level):
    return bool(re.search(r'level file is missing|unsupported level version [0-9]+|invalid level header',
                         str(level.get('route_problem') or level.get('problems', '')), re.I))

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'android/helpers'))


def baselines(repo=ROOT):
    root = Path(repo) / 'game_data/mission_files'
    result = {}
    for path in sorted(root.rglob('*.json')):
        if path.name.endswith(('.simulation.json', '.tracklist.json')):
            continue
        relative = path.relative_to(root).as_posix()
        # These are the two existing archive-source owners, plus built-in missions
        if len(path.relative_to(root).parts) > 2 or ('/' in relative and not relative.startswith('d2xxl_downloads/')):
            continue
        try:
            value = json.loads(path.read_text(encoding='utf-8-sig'))
        except (OSError, ValueError) as error:
            raise ValueError('Unreadable committed mission JSON: ' + relative) from error
        rows = value if isinstance(value, list) else [value]
        if not rows or not all(isinstance(row, dict) and 'levels' in row and 'mission_filename' in row for row in rows):
            continue
        result[hashlib.sha256(relative.encode()).hexdigest()[:20]] = relative
    return result


def suites(repo=ROOT):
    result = []
    for key, relative in baselines(repo).items():
        common = {'kind': 'retail-corpus', 'baseline': relative, 'fixture': 'retail-corpus',
                  'executables': ['pwsh', 'cmake'], 'python_modules': [], 'constraints': []}
        result.append({**common, 'id': f'corpus:metadata:{key}', 'profile': 'metadata',
                       'estimated_seconds': 180, 'coverage_scope': 'Full archive metadata comparison'})
        document = json.loads((Path(repo) / 'game_data/mission_files' / relative).read_text(encoding='utf-8-sig'))
        missions = document if isinstance(document, list) else [document]
        if any('/' in row.get('mission_path', '').replace('\\', '/') for row in missions):
            result[-1]['unavailable_reason'] = 'Nested mission variant requires a variant-aware regression stage'
        missing_source = relative.startswith('CD - ') or relative in ('castaway_redux.json', 'ewithin-versions.json')
        if missing_source:
            result[-1]['unavailable_reason'] = 'Archive or CD-source fixture unavailable'
        for mission in missions:
            if mission.get('game') not in ('d1', 'd2'):
                continue
            for level in mission['levels']:
                target = mission.get('target_index', 0)
                identity = f"{relative}|{target}|{level['level_num']}|{level['level_file']}"
                leaf = hashlib.sha256(identity.encode()).hexdigest()[:20]
                unavailable = {}
                if missing_source and not vertigo_campaign(relative, mission):
                    unavailable['unavailable_reason'] = 'Archive or CD-source fixture unavailable'
                if unavailable_level(level):
                    unavailable['unavailable_reason'] = 'Committed level is explicitly missing or unsupported'
                result.append({**common, **unavailable, 'id': f'corpus:route:{leaf}', 'profile': 'route',
                               'target_index': target, 'level_num': level['level_num'], 'level_file': level['level_file'],
                               'identity': identity, 'estimated_seconds': 360,
                               'coverage_scope': 'One exact route level; receipts traverse every admitted level before cycle completion'})
    return result


def policy_suites(repo=ROOT):
    return [{'id': row['id'], 'owner': 'host-adapter', 'lane': 'host',
             'args': {'suite': row['id']}, 'estimated_seconds': row['estimated_seconds'],
             'fixture': 'retail-corpus', 'category': 'mission-' + row['profile'],
             **({'unavailable_reason': row['unavailable_reason']} if row.get('unavailable_reason') else {})}
            for row in suites(repo)]


def stage_archives(source, repo, commit):
    """Copy only baseline-matched archives from the reviewed immutable checkout."""
    from xl_private_assets import ASSETS_COMMIT as XL_ASSETS_COMMIT
    if commit not in (CORPUS_COMMIT, HISTORICAL_CORPUS_COMMIT, XL_ASSETS_COMMIT):
        raise ValueError('Mission corpus requires the reviewed immutable asset revision')
    source, repo = Path(source).resolve(), Path(repo).resolve()
    actual = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != commit:
        raise ValueError('Mission corpus checkout revision mismatch')
    from retail_ci_assets import reject_links, file_identity
    tree = subprocess.check_output(['git', '-C', str(source), 'ls-tree', '-rz', commit, '--', 'missions'])
    tracked = {}
    for record in tree.decode().split('\0'):
        if record:
            header, path = record.split('\t', 1)
            mode, kind, digest = header.split()
            if mode == '100644' and kind == 'blob':
                tracked[path] = digest
    candidates = {}
    for path in (source / 'missions').rglob('*'):
        if path.suffix.lower() not in ('.zip', '.7z', '.rar'):
            continue
        reject_links(path)
        if path.is_file():
            key = path.relative_to(source / 'missions').with_suffix('').as_posix().casefold()
            candidates.setdefault(key, []).append(path)
    staged = 0
    for relative in baselines(repo).values():
        target = repo / 'game_data/mission_files' / relative
        found = candidates.get(Path(relative).with_suffix('').as_posix().casefold(), [])
        if len(found) > 1:
            raise ValueError('Ambiguous mission archive basename')
        if not found:
            continue
        destination = target.with_suffix(found[0].suffix.lower())
        reject_links(destination)
        if destination.exists():
            raise ValueError('Refusing to overwrite an existing mission archive')
        source_key = found[0].relative_to(source).as_posix()
        if source_key not in tracked:
            raise ValueError('Mission archive is not a regular tracked file at the pinned revision')
        file_identity(found[0], target=destination)
        actual_blob = subprocess.check_output(['git', 'hash-object', '--no-filters', str(destination)], text=True).strip()
        if actual_blob != tracked[source_key]:
            destination.unlink()
            raise ValueError('Mission archive bytes differ from the pinned revision')
        staged += 1
    return staged


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    print('Staged baseline-matched mission archives:', stage_archives(args.source, args.repo, args.commit))

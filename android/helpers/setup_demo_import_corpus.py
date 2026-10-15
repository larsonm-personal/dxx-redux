#!/usr/bin/env python3
"""Stage the complete SHA-pinned public demo import corpus, without extracting it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
from urllib.parse import quote

RELEASE = 'https://github.com/larsonm-personal/dxx-redux/releases/download/demo_installers/'
# Verified release asset names; do not guess URLs or substitute equivalent extracted data
PUBLIC_ASSETS = {
    'desc14sw.exe': 'desc14sw.exe',
    'd2demo10.zip': 'd2demo10.zip',
    'Descent Shareware.sit': 'Descent.Shareware.sit',
    'Descent II Preview.sit': 'Descent.II.Preview.sit',
}


def packages(repo: Path) -> list[dict]:
    oracles = json.loads((repo / 'android/tests/fixtures/demo_import_oracles.json').read_text())['archives']
    manifest = json.loads((repo / 'game_data/test_data_manifest.json').read_text())['files']
    result = []
    seen = set()
    for oracle in oracles:
        name = oracle['archive']
        if not isinstance(name, str) or not name or name in ('.', '..') or '/' in name or '\\' in name or name in seen:
            raise ValueError('Invalid or duplicate demo corpus archive name')
        seen.add(name)
        matches = [entry for entry in manifest if entry['path'] == 'game_data/demo installers/' + name]
        if len(matches) != 1:
            raise ValueError(f'Missing unique source manifest entry: {name}')
        entry = matches[0]
        if (entry['sha256'] != oracle['sha256'] or not re.fullmatch('[0-9a-f]{64}', entry['sha256']) or
                type(entry['size']) is not int or not 0 < entry['size'] <= 32 * 1024 * 1024):
            raise ValueError(f'Invalid pinned demo identity: {name}')
        result.append({'archive': name, 'bytes': entry['size'], 'sha256': entry['sha256'],
                       'url': RELEASE + quote(PUBLIC_ASSETS[name], safe='') if name in PUBLIC_ASSETS else None})
    if not result or sum(entry['bytes'] for entry in result) > 128 * 1024 * 1024:
        raise ValueError('Demo corpus exceeds reviewed download budget')
    return result


def verify(path: Path, entry: dict) -> None:
    if path.stat().st_size != entry['bytes'] or hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']:
        raise ValueError(f'Demo corpus size/SHA256 mismatch: {entry["archive"]}')


def stage(repo: Path, output: Path, download: bool = False) -> dict:
    entries = packages(repo)
    output.mkdir(parents=True, exist_ok=True)
    unavailable = [entry['archive'] for entry in entries
                   if not (output / entry['archive']).exists() and entry['url'] is None]
    if download and unavailable:
        raise ValueError('Original pinned packages have no reviewed public URL; provide verified cache: ' + ', '.join(unavailable))
    for entry in entries:
        destination = output / entry['archive']
        if destination.exists():
            verify(destination, entry)  # Never replace an unverified user-provided package
            continue
        if not download:
            raise ValueError(f'Missing public package: {destination}; pass --download to provision it')
        with tempfile.TemporaryDirectory(prefix='.demo-corpus-', dir=output) as scratch:
            temporary = Path(scratch) / 'package'
            subprocess.run(['curl', '--fail', '--location', '--proto', '=https', '--proto-redir', '=https',
                            '--silent', '--show-error', '--max-time', '120', '--max-filesize', str(entry['bytes']),
                            '--output', str(temporary), entry['url']], check=True, timeout=130)
            verify(temporary, entry)
            # Atomic, exclusive same-filesystem publication; interruption cannot leave a partial package
            os.link(temporary, destination)
    result = {'schema': 1, 'source': 'complete checked-in import oracle + source manifest',
              'archive_count': len(entries), 'total_bytes': sum(entry['bytes'] for entry in entries),
              'archives': entries}
    (output / 'nightly-demo-corpus-provenance.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--download', action='store_true')
    args = parser.parse_args()
    print(json.dumps(stage(args.repo, args.repo / 'game_data/demo installers', args.download), indent=2))

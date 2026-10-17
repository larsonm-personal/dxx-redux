#!/usr/bin/env python3
"""Cold ordinary-save matcen continuation using a real display or Xvfb.

Inputs JSON pins mission, level, trigger, expected_bindings, and exact asset maps
(relative POSIX filename -> SHA-256). No game data is bundled or downloaded.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess


class EvidenceError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise EvidenceError(message)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read_json(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, f'duplicate JSON key: {key}')
            result[key] = value
        return result
    return json.loads(Path(path).read_text(encoding='utf-8'), object_pairs_hook=unique)


def validate_manifest(manifest):
    require(isinstance(manifest, dict), 'inputs must be an object')
    require(isinstance(manifest.get('mission'), str) and
            re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9_-]{0,63}', manifest['mission']), 'invalid mission stem')
    require(type(manifest.get('level')) is int and -127 <= manifest['level'] <= 127 and manifest['level'] != 0, 'invalid level')
    require(type(manifest.get('trigger')) is int and 0 <= manifest['trigger'] <= 255, 'invalid trigger')
    bindings = manifest.get('expected_bindings')
    validate_bindings(bindings)
    for key in ('base_files', 'extra_files'):
        files = manifest.get(key)
        require(isinstance(files, dict) and files, f'missing {key}')
        for name, sha in files.items():
            path = PurePosixPath(name)
            require(name and path.parts and '\x00' not in name and not path.is_absolute() and str(path) == name and
                    '..' not in path.parts and '\\' not in name and ':' not in name,
                    f'unsafe asset path: {name}')
            require(isinstance(sha, str) and re.fullmatch('[0-9a-f]{64}', sha),
                    f'invalid SHA-256: {name}')


BINDING_FIELDS = ('ordinal', 'station', 'segment', 'mask')


def validate_bindings(bindings):
    require(isinstance(bindings, list) and bindings, 'missing expected bindings')
    for binding in bindings:
        require(isinstance(binding, dict) and set(binding) == set(BINDING_FIELDS), 'invalid binding fields')
        require(all(type(binding[key]) is int and binding[key] >= 0
                    for key in BINDING_FIELDS[:-1]), 'invalid binding ID')
        mask = binding['mask']
        require(isinstance(mask, list) and mask and
                all(type(word) is int and 0 <= word <= 0xffffffff for word in mask), 'invalid binding mask')
    for key in BINDING_FIELDS[:-1]:
        require(len({binding[key] for binding in bindings}) == len(bindings), f'duplicate binding {key}')


def verify_assets(root, expected):
    root = Path(root)
    require(root.is_dir() and not root.is_symlink(), f'invalid asset directory: {root}')
    actual = {}
    for path in root.rglob('*'):
        require(not path.is_symlink(), f'symlink in assets: {path}')
        if path.is_dir():
            continue
        require(path.is_file(), f'non-file asset: {path}')
        actual[path.relative_to(root).as_posix()] = digest(path)
    require(actual == expected, f'asset file set or bytes differ: {root}')


def validate_output(output, base, extra, executable, provenance):
    output = Path(output).resolve()
    require(not output.exists(), 'output must be a new directory')
    for protected in (base, extra, executable, provenance):
        protected = Path(protected).resolve()
        require(output != protected and output not in protected.parents and
                protected not in output.parents, f'output overlaps protected path: {protected}')
    return output


CENTER_FIELDS = {'ordinal', 'station', 'segment', 'mask', 'enabled', 'lives',
                 'capacity', 'timer', 'flag', 'disable_time'}
BIRTH_FIELDS = {'frame', 'ordinal', 'station', 'segment', 'type', 'shields'}


def validate_report(report, manifest):
    require(isinstance(report, dict), 'report must be an object')
    identity = report.get('identity')
    require(isinstance(identity, dict) and identity.get('mission') == manifest['mission'] and
            identity.get('level') == manifest['level'] and
            isinstance(identity.get('level_file'), str) and identity['level_file'], 'wrong mission/level identity')
    for phase in ('checkpoint', 'terminal'):
        state = report.get(phase)
        require(isinstance(state, dict) and type(state.get('sim_state')) is int, f'missing {phase} SIM state')
        centers = state.get('centers')
        require(isinstance(centers, list) and centers, f'missing {phase} centers')
        for center in centers:
            require(isinstance(center, dict) and CENTER_FIELDS <= center.keys(), f'incomplete {phase} center')
            require(all(type(center[key]) is int for key in CENTER_FIELDS - {'mask'}), f'invalid {phase} center values')
            mask = center['mask']
            require(isinstance(mask, list) and mask and
                    all(type(word) is int and 0 <= word <= 0xffffffff for word in mask), 'invalid robot mask')
            require(all(center[key] >= 0 for key in ('ordinal', 'station', 'segment')), 'negative center ID')
        for key in ('ordinal', 'station', 'segment'):
            require(len({center[key] for center in centers}) == len(centers), f'duplicate center {key}')
    require([{key: center[key] for key in BINDING_FIELDS}
             for center in report['checkpoint']['centers']] == manifest['expected_bindings'],
            'checkpoint differs from pinned authored bindings')
    births = report.get('births')
    require(isinstance(births, list), 'missing births')
    for birth in births:
        require(isinstance(birth, dict) and BIRTH_FIELDS <= birth.keys() and
                all(type(birth[key]) is int for key in BIRTH_FIELDS), 'incomplete birth')
        require(0 <= birth['frame'] < 1200, 'birth frame outside continuation')
        center = next((center for center in report['checkpoint']['centers']
                       if all(birth[key] == center[key] for key in ('ordinal', 'station', 'segment'))), None)
        require(center is not None, 'birth center mapping differs')
        robot = birth['type']
        require(0 <= robot < 32 * len(center['mask']) and
                center['mask'][robot // 32] & (1 << (robot % 32)), 'birth type outside authored mask')


def compare_reports(write, restored, repeated, negative, manifest):
    for report in (write, restored, repeated, negative):
        validate_report(report, manifest)
        require(report['identity'] == write['identity'], 'mission/map identity differs')
    require(write['births'], 'empty positive birth witness')
    expected_ordinals = {binding['ordinal'] for binding in manifest['expected_bindings']}
    for report in (write, restored, repeated):
        require({birth['ordinal'] for birth in report['births']} == expected_ordinals,
                'positive births do not cover every pinned station')
    require(all(center['enabled'] == 1 for center in write['checkpoint']['centers']),
            'positive checkpoint station disabled')
    for report in (restored, repeated):
        for key in ('checkpoint', 'births', 'terminal'):
            require(report[key] == write[key], f'cold restore {key} differs')
    require(not negative['births'], 'no-dispatch produced births')
    for phase in ('checkpoint', 'terminal'):
        require(all(center['enabled'] == 0 for center in negative[phase]['centers']),
                f'no-dispatch {phase} station enabled')
        keys = ('ordinal', 'station', 'segment', 'mask')
        require([{key: c[key] for key in keys} for c in negative[phase]['centers']] ==
                [{key: c[key] for key in keys} for c in write[phase]['centers']],
                'no-dispatch center mapping differs')


def run(args):
    base, extra, executable, provenance = (Path(p).absolute() for p in
                                          (args.base, args.extra, args.executable, args.inputs))
    manifest = read_json(provenance)
    validate_manifest(manifest)
    require(executable.is_file(), 'missing executable')
    require(args.timeout > 0, 'timeout must be positive')
    output = validate_output(args.output, base, extra, executable, provenance)
    verify_assets(base, manifest['base_files'])
    verify_assets(extra, manifest['extra_files'])
    pinned_exe, pinned_inputs = digest(executable), digest(provenance)
    output.mkdir(parents=True)
    results = {'executable_sha256': pinned_exe, 'inputs_sha256': pinned_inputs,
               'inputs': manifest, 'invocations': [], 'passed': False}
    save = output / 'authored-matcen.sav'
    reports = []
    try:
        for mode, label in (('write', 'write'), ('restore', 'restore-first'),
                            ('restore', 'restore-repeat'), ('no-dispatch', 'no-dispatch')):
            verify_assets(base, manifest['base_files'])
            verify_assets(extra, manifest['extra_files'])
            require(digest(executable) == pinned_exe and digest(provenance) == pinned_inputs,
                    'executable/provenance changed')
            if mode != 'write':
                require(digest(save) == results['save_sha256'], 'save changed between invocations')
            report_path = output / (mode + '.json')
            if report_path.exists():
                report_path.unlink()
            command = [str(executable), '--authored-matcen-save', str(base), str(extra),
                       manifest['mission'], str(manifest['level']), str(manifest['trigger']), mode]
            invocation = {'mode': mode, 'label': label, 'command': command, 'log': label + '.log'}
            results['invocations'].append(invocation)
            with (output / invocation['log']).open('wb') as log:
                try:
                    process = subprocess.run(command, cwd=output, stdout=log, stderr=subprocess.STDOUT,
                                             timeout=args.timeout, check=False)
                    invocation['exit'] = process.returncode
                except subprocess.TimeoutExpired:
                    invocation['exit'] = 124
                    raise EvidenceError(f'{label} timed out')
            require(invocation['exit'] == 0, f'{label} exited {invocation["exit"]}')
            require(report_path.is_file() and not report_path.is_symlink(), f'{label} missing native report')
            archived = output / (label + '.json')
            if archived != report_path:
                shutil.copyfile(report_path, archived)
            invocation['report'] = archived.name
            invocation['report_sha256'] = digest(archived)
            report = read_json(archived)
            validate_report(report, manifest)
            reports.append(report)
            require(save.is_file() and not save.is_symlink(), 'missing ordinary save')
            if mode == 'write':
                require(save.stat().st_size > 0, 'empty ordinary save')
                results['save_sha256'] = digest(save)
            else:
                require(digest(save) == results['save_sha256'], 'consumer changed ordinary save')
        compare_reports(*reports, manifest)
        verify_assets(base, manifest['base_files'])
        verify_assets(extra, manifest['extra_files'])
        require(digest(executable) == pinned_exe and digest(provenance) == pinned_inputs,
                'executable/provenance changed')
        results['passed'] = True
    except (EvidenceError, OSError, ValueError) as error:
        results['error'] = str(error)
    finally:
        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('executable', 'base', 'extra', 'inputs', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--timeout', type=float, default=180)
    args = parser.parse_args()
    try:
        results = run(args)
    except (EvidenceError, OSError, ValueError) as error:
        parser.exit(1, f'FAIL: {error}\n')
    print('PASS' if results['passed'] else 'FAIL: ' + results['error'])
    return int(not results['passed'])


if __name__ == '__main__':
    raise SystemExit(main())

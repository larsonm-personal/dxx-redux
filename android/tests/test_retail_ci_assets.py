"""Exercise retail inventory/staging using only disposable synthetic bytes."""
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[2]
HELPER = REPO / 'android/helpers/retail_ci_assets.py'
SPEC = importlib.util.spec_from_file_location('retail_ci_assets', HELPER)
assets = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(assets)


class RetailAssetTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='retail-ci-test-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name).resolve()
        self.root = self.base / 'input'
        self.destination = self.base / 'stage'
        self.report = self.base / 'report.json'
        self.repo = self.base / 'repo'
        self.source = self.repo / 'android/helpers/standard_game_data.ps1'
        self.source.parent.mkdir(parents=True)
        self.data = {}
        self.expected = {}
        for game, names in assets.FILES.items():
            (self.root / game).mkdir(parents=True)
            for name in names:
                relative = game + '/' + name
                data = ('SYNTHETIC ONLY - never retail data: ' + relative + '\n').encode('ascii')
                (self.root / relative).write_bytes(data)
                self.data[relative] = data
                self.expected[name] = hashlib.sha256(data).hexdigest()
        # The isolated mock repository is used only for positive synthetic tests
        self.pin_source = 'function Get-StandardGameDataDeps {\n    return @(\n' + ''.join(
            '        @{file = "' + name + '"; sha256 = "' + digest + '" }\n'
            for name, digest in self.expected.items()) + '    )\n}\n'
        self.source.write_text(self.pin_source, encoding='utf-8')

    def inventory(self):
        return assets.inventory(self.root, assets.baseline(self.repo))

    def cli(self, *extra, repo=None, output=None):
        return subprocess.run([sys.executable, '-B', str(HELPER), '--root', str(self.root),
                               '--repo', str(repo or self.repo), '--output', str(output or self.report),
                               *map(str, extra)], capture_output=True, text=True, timeout=10)

    def symlink(self, path, target, directory=False):
        try:
            path.symlink_to(target, target_is_directory=directory)
        except (OSError, NotImplementedError) as error:
            self.skipTest('Symlink creation is unavailable: ' + str(error))

    def mapped_fixture(self):
        sources = {}
        folders = {'d1': 'descent_1.4a_retail', 'd2': 'descent_2_1.1_retail'}
        for relative in self.data:
            game, name = relative.split('/')
            source = folders[game] + '/' + name.upper()
            target = self.root / source
            target.parent.mkdir(exist_ok=True)
            (self.root / relative).rename(target)
            sources[relative] = source
        for game in assets.FILES:
            (self.root / game).rmdir()
        self.map_file = self.base / 'source-map.json'
        self.map_file.write_text(json.dumps(sources), encoding='utf-8')
        return sources

    def test_explicit_source_map_preserves_uploaded_paths_and_canonical_identity(self):
        canonical = self.inventory()
        sources = self.mapped_fixture()
        extra = self.root / 'descent_2_1.1_retail/DESCENT2.S11'
        extra.write_bytes(b'optional, deliberately not staged')
        original_files = {path.relative_to(self.root).as_posix(): path.read_bytes()
                          for path in self.root.rglob('*') if path.is_file()}
        result = assets.inventory(self.root, self.expected, sources)
        self.assertEqual(result['manifest_sha256'], canonical['manifest_sha256'])
        self.assertEqual(result['manifest_sha256'], assets.baseline_identity(self.repo))
        self.assertEqual({row['path']: row['source_path'] for row in result['files']}, sources)
        for row in result['files']:
            self.assertEqual(row['size'], len(self.data[row['path']]))
            self.assertIs(row['matches_tested_edition'], True)
        process = self.cli('--source-map', self.map_file, '--stage', self.destination)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(json.loads(self.report.read_text()), result)
        staged = {path.relative_to(self.destination).as_posix(): path.read_bytes()
                  for path in self.destination.rglob('*') if path.is_file()}
        self.assertEqual(staged, self.data)
        self.assertEqual({path.relative_to(self.root).as_posix(): path.read_bytes()
                          for path in self.root.rglob('*') if path.is_file()}, original_files)
        # Host-stage checks retain the unmapped canonical contract
        self.assertEqual(assets.inventory(self.destination, self.expected), canonical)
        with self.assertRaises(ValueError):
            self.inventory()

    def test_source_map_requires_exact_keys_and_unique_portable_relative_paths(self):
        good = assets.checked_source_paths()
        malformed = [[], {}, {**good, 'd1/extra.pig': 'extra.pig'}]
        missing = dict(good)
        missing.pop('d1/descent.hog')
        malformed.append(missing)
        for value in ('../outside.hog', '/outside.hog', 'C:/outside.hog', 'C:outside.hog',
                      r'folder\DESCENT.HOG', 'folder/../DESCENT.HOG', './DESCENT.HOG',
                      'folder//DESCENT.HOG', 'folder/', '', 'folder/DESCENT.HOG.',
                      'folder/DESCENT.HOG ', 'folder/DESCENT.HOG:stream', 'bad\x00path', 1,
                      good['d1/descent.pig'], good['d1/descent.pig'].upper()):
            malformed.append({**good, 'd1/descent.hog': value})
        for source_paths in malformed:
            with self.subTest(source_paths=source_paths), self.assertRaises(ValueError):
                assets.inventory(self.root, self.expected, source_paths)

    def test_cli_rejects_duplicate_source_map_keys_and_nonobject_json(self):
        good = assets.checked_source_paths()
        self.map_file = self.base / 'source-map.json'
        duplicate = json.dumps(good)[:-1] + ', "d1/descent.hog": "other.hog"}'
        for text in (duplicate, 'null', '[]', '{invalid json'):
            self.map_file.write_text(text, encoding='utf-8')
            process = self.cli('--source-map', self.map_file, '--stage', self.destination)
            self.assertEqual(process.returncode, 1, process.stderr)
            self.assertFalse(self.report.exists())
            self.assertFalse(self.destination.exists())

    def test_mapped_missing_file_and_wrong_edition_never_fall_back_to_canonical(self):
        sources = assets.checked_source_paths()
        sources['d1/descent.hog'] = 'd1/missing.hog'
        with self.assertRaises(OSError):
            assets.inventory(self.root, self.expected, sources)
        (self.root / 'd1/missing.hog').write_bytes(b'different edition')
        result = assets.inventory(self.root, self.expected, sources)
        self.assertIs(result['matches_tested_edition'], False)
        with self.assertRaises(ValueError):
            assets.stage(self.root, self.destination, result)
        self.assertFalse(self.destination.exists())

    def test_stage_revalidates_manifest_source_paths_before_copying(self):
        sources = self.mapped_fixture()
        manifest = assets.inventory(self.root, self.expected, sources)
        for source in ('../outside', '/outside', 'C:/outside', r'd1\descent.hog',
                       sources['d1/descent.pig'], sources['d1/descent.pig'].upper()):
            bad = copy.deepcopy(manifest)
            bad['files'][0]['source_path'] = source
            with self.subTest(source=source), self.assertRaises(ValueError):
                assets.stage(self.root, self.destination, bad)
        self.assertFalse(self.destination.exists())

    def test_mapped_source_change_is_detected_and_partial_stage_removed(self):
        sources = self.mapped_fixture()
        result = assets.inventory(self.root, self.expected, sources)
        (self.root / sources['d2/water.pig']).write_bytes(b'changed mapped file')
        with self.assertRaisesRegex(ValueError, 'changed during staging'):
            assets.stage(self.root, self.destination, result)
        self.assertFalse(self.destination.exists())

    def test_mapped_symlink_ancestor_and_linked_source_map_are_rejected(self):
        sources = self.mapped_fixture()
        directory = self.root / 'descent_1.4a_retail'
        real = directory.with_name('real')
        directory.rename(real)
        self.symlink(directory, real, directory=True)
        with self.assertRaises(ValueError):
            assets.inventory(self.root, self.expected, sources)
        alias = self.base / 'linked-source-map.json'
        self.symlink(alias, self.map_file)
        process = self.cli('--source-map', alias)
        self.assertEqual(process.returncode, 1, process.stderr)
        self.assertFalse(self.report.exists())

    def mission_fixture(self):
        pins = copy.deepcopy(assets.MISSION_FILES)
        data = (b'synthetic Vertigo HOG', b'name = Synthetic\nnum_levels = 1\nexample.rl2\n\x1a',
                b'synthetic Obsidian ZIP')
        self.mission_data = {}
        for pin, source in zip(pins, data):
            target = self.root / pin['source_path']
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source)
            staged = source.replace(b'\n', b'\r\n') if pin['transform'] != 'none' else source
            pin.update(source_size=len(source), source_sha256=hashlib.sha256(source).hexdigest(),
                       size=len(staged), sha256=hashlib.sha256(staged).hexdigest())
            self.mission_data[pin['source_path']] = source
        patched = patch.object(assets, 'MISSION_FILES', pins)
        patched.start()
        self.addCleanup(patched.stop)
        return pins

    def test_missions_are_explicit_opt_in_with_unchanged_base_contract(self):
        base = self.inventory()
        base_identity = assets.baseline_identity(self.repo)
        pins = self.mission_fixture()
        self.assertEqual(self.inventory(), base)
        self.assertEqual(assets.baseline_identity(self.repo), base_identity)
        expanded = assets.inventory(self.root, self.expected, include_missions=True)
        self.assertEqual(len(expanded['files']), 14)
        self.assertIs(expanded['matches_tested_edition'], True)
        self.assertEqual(expanded['manifest_sha256'], assets.baseline_identity(self.repo, include_missions=True))
        self.assertNotEqual(expanded['manifest_sha256'], base_identity)
        for actual, expected in zip(expanded['files'][11:], pins):
            self.assertEqual({key: actual[key] for key in expected}, expected)
        assets.stage(self.root, self.destination, expanded, include_missions=True)
        expected_data = dict(self.data)
        for pin in pins:
            source = self.mission_data[pin['source_path']]
            expected_data[pin['path']] = source.replace(b'\n', b'\r\n') if pin['transform'] != 'none' else source
            self.assertEqual((self.root / pin['source_path']).read_bytes(), source)
        staged = {path.relative_to(self.destination).as_posix(): path.read_bytes()
                  for path in self.destination.rglob('*') if path.is_file()}
        self.assertEqual(staged, expected_data)
        descriptor = staged['missions/d2x.mn2']
        self.assertTrue(descriptor.endswith(b'\r\n\x1a'))
        self.assertEqual(assets.stage_inventory(self.destination, self.expected), base)
        checked = assets.stage_inventory(self.destination, self.expected, include_missions=True)
        self.assertIs(checked['matches_tested_edition'], True)
        self.assertEqual(len(checked['files']), 14)
        self.assertEqual(checked['verification_scope'], 'staged-only')
        self.assertNotEqual(checked['manifest_sha256'], expanded['manifest_sha256'])
        self.assertTrue(all('source_sha256' not in row for row in checked['files']))
        with self.assertRaises(OSError):
            assets.inventory(self.destination, self.expected, include_missions=True)

    def test_expanded_identity_binds_both_hashes_paths_and_transform(self):
        self.mission_fixture()
        result = assets.inventory(self.root, self.expected, include_missions=True)
        for field in ('path', 'source_sha256', 'sha256', 'transform'):
            records = copy.deepcopy(result['files'])
            records[12][field] += '.changed'
            with self.subTest(field=field):
                self.assertNotEqual(assets.identity_digest(records, include_missions=True), result['manifest_sha256'])

    def test_supplemental_hog_and_zip_mismatch_records_actual_bytes_and_refuses_stage(self):
        pins = self.mission_fixture()
        for pin in (pins[0], pins[2]):
            path = self.root / pin['source_path']
            path.write_bytes(b'wrong package')
            result = assets.inventory(self.root, self.expected, include_missions=True)
            row = next(row for row in result['files'] if row['path'] == pin['path'])
            self.assertIs(result['matches_tested_edition'], False)
            self.assertEqual(row['source_size'], len(b'wrong package'))
            self.assertEqual(row['source_sha256'], hashlib.sha256(b'wrong package').hexdigest())
            with self.assertRaises(ValueError):
                assets.stage(self.root, self.destination, result, include_missions=True)
            self.assertFalse(self.destination.exists())
            path.write_bytes(self.mission_data[pin['source_path']])

    def test_descriptor_edits_and_unreviewed_line_endings_never_normalize(self):
        pin = self.mission_fixture()[1]
        path = self.root / pin['source_path']
        original = self.mission_data[pin['source_path']]
        variants = (original.replace(b'Synthetic', b'Changed!!'), original.replace(b'\n', b'\r\n'),
                    original.replace(b'\n', b'\r\n', 1), original.replace(b'\n', b'\r'),
                    b'\xef\xbb\xbf' + original, original + b'extra')
        for changed in variants:
            with self.subTest(changed=changed):
                path.write_bytes(changed)
                result = assets.inventory(self.root, self.expected, include_missions=True)
                row = result['files'][12]
                self.assertIs(result['matches_tested_edition'], False)
                self.assertEqual(row['source_size'], len(changed))
                self.assertEqual(row['source_sha256'], hashlib.sha256(changed).hexdigest())
                self.assertIsNone(row['size'])
                self.assertIsNone(row['sha256'])
                with self.assertRaises(ValueError):
                    assets.stage(self.root, self.destination, result, include_missions=True)
                with self.assertRaisesRegex(ValueError, 'Unapproved Vertigo descriptor input'):
                    assets.file_identity(path, self.base / 'normalized.mn2', normalize_descriptor=True)
                self.assertFalse((self.base / 'normalized.mn2').exists())
                self.assertFalse(self.destination.exists())
                self.assertEqual(path.read_bytes(), changed)

    def test_descriptor_normalization_must_match_existing_destination_pin(self):
        pins = self.mission_fixture()
        pins[1]['sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'differs from existing fixture pin'):
            assets.file_identity(self.root / pins[1]['source_path'], self.base / 'normalized.mn2', normalize_descriptor=True)
        self.assertFalse((self.base / 'normalized.mn2').exists())

    def test_supplemental_manifest_cannot_change_membership_sources_pins_or_transform(self):
        self.mission_fixture()
        good = assets.inventory(self.root, self.expected, include_missions=True)
        with self.assertRaises(ValueError):
            assets.stage(self.root, self.destination, good)
        mutations = [lambda m: m['files'].pop(), lambda m: m['files'].append(m['files'][-1]),
                     lambda m: m['files'][12].update(transform='arbitrary-text-normalization'),
                     lambda m: m['files'][12].update(source_path='../outside.mn2'),
                     lambda m: m['files'][12].update(source_sha256='0' * 64),
                     lambda m: m['files'][12].update(source_size=0),
                     lambda m: m['files'][12].update(sha256='0' * 64),
                     lambda m: m['files'][12].update(size=0),
                     lambda m: m['files'][0].update(transform='d2x-mn2-lf-to-crlf')]
        for mutate in mutations:
            bad = copy.deepcopy(good)
            mutate(bad)
            bad['manifest_sha256'] = assets.identity_digest(bad['files'], include_missions=True)
            with self.assertRaises(ValueError):
                assets.stage(self.root, self.destination, bad, include_missions=True)
            self.assertFalse(self.destination.exists())
        sources = assets.checked_source_paths()
        sources['missions/d2x.hog'] = 'descent_2_vertigo/d2x.hog'
        with self.assertRaises(ValueError):
            assets.inventory(self.root, self.expected, sources, include_missions=True)

    def test_missing_supplemental_files_are_not_silently_omitted(self):
        pins = self.mission_fixture()
        for pin in pins:
            path = self.root / pin['source_path']
            path.unlink()
            with self.assertRaises(OSError):
                assets.inventory(self.root, self.expected, include_missions=True)
            path.write_bytes(self.mission_data[pin['source_path']])
        self.assertFalse(self.destination.exists())

    def test_supplemental_source_changes_after_inventory_remove_entire_stage(self):
        pins = self.mission_fixture()
        manifest = assets.inventory(self.root, self.expected, include_missions=True)
        for pin in pins:
            path = self.root / pin['source_path']
            path.write_bytes(b'changed after inventory')
            with self.assertRaises(ValueError):
                assets.stage(self.root, self.destination, manifest, include_missions=True)
            self.assertFalse(self.destination.exists())
            self.assertEqual(path.read_bytes(), b'changed after inventory')
            path.write_bytes(self.mission_data[pin['source_path']])

    def test_staged_mission_recheck_rejects_changed_destination_bytes(self):
        pins = self.mission_fixture()
        manifest = assets.inventory(self.root, self.expected, include_missions=True)
        assets.stage(self.root, self.destination, manifest, include_missions=True)
        for pin in pins:
            path = self.destination / pin['path']
            original = path.read_bytes()
            path.write_bytes(b'changed stage')
            result = assets.stage_inventory(self.destination, self.expected, include_missions=True)
            self.assertIs(result['matches_tested_edition'], False)
            self.assertEqual([row['path'] for row in result['files'] if not row['matches_tested_edition']], [pin['path']])
            path.write_bytes(original)

    def test_supplemental_symlink_rejected_before_reading_and_during_stage(self):
        pin = self.mission_fixture()[0]
        manifest = assets.inventory(self.root, self.expected, include_missions=True)
        path = self.root / pin['source_path']
        moved = path.with_name('real.hog')
        path.rename(moved)
        self.symlink(path, moved)
        with self.assertRaises(ValueError):
            assets.inventory(self.root, self.expected, include_missions=True)
        with self.assertRaises(ValueError):
            assets.stage(self.root, self.destination, manifest, include_missions=True)
        self.assertFalse(self.destination.exists())
        self.assertEqual(moved.read_bytes(), self.mission_data[pin['source_path']])

    def test_cli_missions_switch_keeps_reviewed_pins_and_reports_raw_mismatch(self):
        self.mission_fixture()
        # The subprocess retains production pins, never the patched synthetic pins
        process = self.cli('--include-missions', '--stage', self.destination)
        self.assertEqual(process.returncode, 1, process.stderr)
        result = json.loads(self.report.read_text())
        self.assertEqual(len(result['files']), 14)
        self.assertIs(result['matches_tested_edition'], False)
        self.assertFalse(self.destination.exists())
        self.assertEqual(result['files'][12]['source_size'], len(self.mission_data['descent_2_vertigo/d2x.mn2']))
        self.assertIsNone(result['files'][12]['sha256'])

    def test_repository_baseline_remains_exactly_eleven_source_pins(self):
        expected = assets.baseline(REPO)
        self.assertEqual(len(expected), 11)
        self.assertEqual(set(expected), set(self.expected))
        self.assertNotEqual(expected, self.expected)
        for digest in expected.values():
            self.assertRegex(digest, r'^[0-9a-f]{64}$')

    def test_changed_missing_duplicate_and_extra_pin_contracts_fail_closed(self):
        lines = self.pin_source.splitlines(keepends=True)
        cases = {
            'missing': ''.join(lines[:2] + lines[3:]),
            'duplicate': ''.join(lines[:2] + [lines[3]] + lines[3:]),
            'extra': ''.join(lines[:2] + [lines[2]] + lines[2:]),
            'unknown': self.pin_source.replace('descent.hog', 'unknown.hog'),
            'path': self.pin_source.replace('descent.hog', '../descent.hog'),
            'malformed hash': self.pin_source.replace(self.expected['descent.hog'], 'invalid'),
            'unpinned entry': self.pin_source.replace('    )', '        @{file = "extra.pig" }\n    )'),
            'unrecognized entry': self.pin_source.replace('    )', '        Get-OtherPins\n    )'),
            'missing function': self.pin_source.replace('Get-StandardGameDataDeps', 'Other'),
            'duplicate function': self.pin_source * 2,
        }
        for label, source in cases.items():
            with self.subTest(label=label):
                self.source.write_text(source, encoding='utf-8')
                with self.assertRaises(ValueError):
                    assets.baseline(self.repo)

    def test_inventory_records_measured_sizes_hashes_and_no_raw_bytes(self):
        result = self.inventory()
        self.assertEqual(result['schema'], 1)
        self.assertIs(result['matches_tested_edition'], True)
        self.assertEqual(len(result['files']), 11)
        for row in result['files']:
            self.assertEqual(set(row), {'path', 'size', 'sha256', 'matches_tested_edition'})
            data = self.data[row['path']]
            self.assertEqual(row['size'], len(data))
            self.assertEqual(row['sha256'], hashlib.sha256(data).hexdigest())
            self.assertIs(row['matches_tested_edition'], True)
        text = json.dumps(result)
        self.assertNotIn('SYNTHETIC ONLY', text)
        self.assertNotIn(str(self.root), text)
        self.assertEqual(result, self.inventory())

    def test_canonical_digest_binds_paths_hashes_and_matches_trusted_baseline(self):
        result = self.inventory()
        canonical = [{'path': name, 'sha256': hashlib.sha256(data).hexdigest()}
                     for name, data in sorted(self.data.items())]
        digest = hashlib.sha256(json.dumps(canonical, sort_keys=True, separators=(',', ':')).encode('ascii')).hexdigest()
        self.assertEqual(result['manifest_sha256'], digest)
        self.assertEqual(assets.baseline_identity(self.repo), digest)
        altered = copy.deepcopy(result['files'])
        altered.reverse()
        altered[0]['size'] += 1
        self.assertEqual(assets.identity_digest(altered), digest)
        altered[0]['path'] += '.changed'
        self.assertNotEqual(assets.identity_digest(altered), digest)
        (self.root / 'd1/descent.hog').write_bytes(b'different edition')
        self.assertNotEqual(self.inventory()['manifest_sha256'], digest)

    def test_stage_copies_only_exact_verified_files(self):
        (self.root / 'd1/unrelated.secret').write_bytes(b'never copy this')
        (self.root / 'private-notes.txt').write_bytes(b'never copy this either')
        assets.stage(self.root, self.destination, self.inventory())
        staged = {path.relative_to(self.destination).as_posix(): path.read_bytes()
                  for path in self.destination.rglob('*') if path.is_file()}
        self.assertEqual(staged, self.data)

    def test_missing_input_root_game_file_and_nonregular_input_are_rejected(self):
        with self.assertRaises(ValueError):
            assets.inventory(self.base / 'missing', self.expected)
        (self.root / 'd1').rename(self.root / 'missing-d1')
        with self.assertRaises(ValueError):
            self.inventory()
        (self.root / 'missing-d1').rename(self.root / 'd1')
        path = self.root / 'd1/descent.hog'
        path.unlink()
        with self.assertRaises(OSError):
            self.inventory()
        path.mkdir()
        with self.assertRaises(ValueError):
            self.inventory()

    def test_incomplete_or_malformed_expected_pins_are_rejected(self):
        for expected in ({}, {**self.expected, 'extra.pig': '0' * 64},
                         {**self.expected, 'descent.hog': ''}, {**self.expected, 'descent.hog': None}):
            with self.subTest(expected=expected):
                with self.assertRaises(ValueError):
                    assets.inventory(self.root, expected)

    def test_mismatched_edition_is_reported_but_never_staged(self):
        (self.root / 'd2/water.pig').write_bytes(b'different edition')
        result = self.inventory()
        self.assertIs(result['matches_tested_edition'], False)
        self.assertEqual([row['path'] for row in result['files'] if not row['matches_tested_edition']], ['d2/water.pig'])
        with self.assertRaises(ValueError):
            assets.stage(self.root, self.destination, result)
        self.assertFalse(self.destination.exists())
        process = self.cli('--stage', self.destination)
        self.assertEqual(process.returncode, 1, process.stderr)
        self.assertIn('edition mismatch', process.stderr)
        self.assertIs(json.loads(self.report.read_text())['matches_tested_edition'], False)
        self.assertFalse(self.destination.exists())

    def test_cli_refuses_synthetic_bytes_against_real_repository_pins(self):
        process = self.cli('--stage', self.destination, repo=REPO)
        self.assertEqual(process.returncode, 1, process.stderr)
        report = json.loads(self.report.read_text())
        self.assertEqual(len(report['files']), 11)
        self.assertTrue(all(not row['matches_tested_edition'] for row in report['files']))
        self.assertFalse(self.destination.exists())

    def test_cli_stages_verified_synthetic_fixture_and_metadata_report(self):
        process = self.cli('--stage', self.destination)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn('Verified eleven retail files', process.stdout)
        self.assertEqual(json.loads(self.report.read_text()), self.inventory())
        self.assertEqual((self.destination / 'd1/descent.hog').read_bytes(), self.data['d1/descent.hog'])
        self.assertNotIn(b'\r\n', self.report.read_bytes())

    def test_existing_stage_is_preserved(self):
        self.destination.mkdir()
        marker = self.destination / 'keep.txt'
        marker.write_text('keep', encoding='utf-8')
        with self.assertRaises(ValueError):
            assets.stage(self.root, self.destination, self.inventory())
        self.assertEqual(marker.read_text(), 'keep')

    def test_input_stage_overlap_is_rejected(self):
        with self.assertRaises(ValueError):
            assets.stage(self.root, self.root / 'stage', self.inventory())
        self.assertFalse((self.root / 'stage').exists())

    def test_manifest_paths_duplicates_missing_rows_and_unverified_rows_are_rejected(self):
        good = self.inventory()
        for path in ('../outside', '/outside', 'C:/outside', 'd1\\descent.hog', 'd2/water.pig'):
            bad = copy.deepcopy(good)
            bad['files'][0]['path'] = path
            bad['manifest_sha256'] = assets.identity_digest(bad['files'])
            with self.subTest(path=path), self.assertRaises(ValueError):
                assets.stage(self.root, self.destination, bad)
        mutations = [lambda m: m['files'].pop(), lambda m: m['files'].append(m['files'][0]),
                     lambda m: m['files'][0].update(matches_tested_edition=False),
                     lambda m: m['files'][0].update(size=-1), lambda m: m['files'][0].update(size=True),
                     lambda m: m['files'][0].update(sha256='invalid'),
                     lambda m: m.update(manifest_sha256='0' * 64), lambda m: m.update(schema=2)]
        for mutate in mutations:
            bad = copy.deepcopy(good)
            mutate(bad)
            with self.assertRaises(ValueError):
                assets.stage(self.root, self.destination, bad)
        self.assertFalse(self.destination.exists())

    def test_source_change_after_inventory_removes_partial_stage(self):
        manifest = self.inventory()
        (self.root / 'd2/water.pig').write_bytes(b'changed last file')
        with self.assertRaisesRegex(ValueError, 'changed during staging'):
            assets.stage(self.root, self.destination, manifest)
        self.assertFalse(self.destination.exists())
        self.assertEqual((self.root / 'd2/water.pig').read_bytes(), b'changed last file')

    def test_measured_size_is_checked_during_stage(self):
        manifest = self.inventory()
        manifest['files'][-1]['size'] += 1
        with self.assertRaisesRegex(ValueError, 'changed during staging'):
            assets.stage(self.root, self.destination, manifest)
        self.assertFalse(self.destination.exists())

    def snapshot(self, info, **changes):
        fields = ('st_mode', 'st_ino', 'st_dev', 'st_size', 'st_mtime_ns', 'st_ctime_ns')
        return SimpleNamespace(**{field: changes.get(field, getattr(info, field)) for field in fields})

    def test_stat_and_fstat_timestamps_can_differ_for_unchanged_files(self):
        original_fstat = os.fstat

        def descriptor_snapshot(fd):
            info = original_fstat(fd)
            return self.snapshot(info, st_mtime_ns=info.st_mtime_ns + 123,
                                 st_ctime_ns=info.st_ctime_ns + 456)

        # Different API representations must not become false mutation reports
        with patch.object(assets.os, 'fstat', side_effect=descriptor_snapshot):
            manifest = self.inventory()
            assets.stage(self.root, self.destination, manifest)
        self.assertIs(manifest['matches_tested_edition'], True)
        self.assertEqual(assets.inventory(self.destination, self.expected), manifest)

    def test_each_metadata_stream_still_rejects_real_changes(self):
        path = self.root / 'd1/descent.hog'
        original_stat = Path.stat
        before = path.stat()
        opened = self.snapshot(before, st_mtime_ns=before.st_mtime_ns + 123,
                               st_ctime_ns=before.st_ctime_ns + 456)
        for stream in ('pathname', 'descriptor'):
            for field in ('st_ino', 'st_dev', 'st_size', 'st_mtime_ns', 'st_ctime_ns'):
                after = self.snapshot(before)
                closed = self.snapshot(opened)
                changed = after if stream == 'pathname' else closed
                setattr(changed, field, getattr(changed, field) + 1)
                path_observations = iter((before, after))

                def pathname_snapshot(candidate, **kwargs):
                    if candidate == path and kwargs.get('follow_symlinks', True):
                        return next(path_observations)
                    return original_stat(candidate, **kwargs)

                with self.subTest(stream=stream, field=field):
                    with patch.object(Path, 'stat', pathname_snapshot), patch.object(
                            assets.os, 'fstat', side_effect=[opened, closed]):
                        with self.assertRaisesRegex(ValueError, 'changed while reading'):
                            assets.file_identity(path)

    def test_open_descriptor_must_still_match_pathname_identity(self):
        path = self.root / 'd1/descent.hog'
        before = path.stat()
        for field in ('st_ino', 'st_dev'):
            opened = self.snapshot(before, **{field: getattr(before, field) + 1})
            with self.subTest(field=field), patch.object(assets.os, 'fstat', return_value=opened):
                with self.assertRaisesRegex(ValueError, 'changed while opening'):
                    assets.file_identity(path)

    def test_midstream_change_or_path_replacement_is_rejected_and_stage_removed(self):
        path = self.root / 'd1/descent.hog'
        original_sha256 = hashlib.sha256
        # Windows prevents deleting an open input; in-place mutation is still tested
        for replacement in ((False,) if os.name == 'nt' else (False, True)):
            path.write_bytes(self.data['d1/descent.hog'])
            manifest = self.inventory()

            class ChangingDigest:
                def __init__(self):
                    self.digest = original_sha256()

                def update(self, data):
                    self.digest.update(data)
                    if replacement:
                        path.unlink()
                    path.write_bytes(b'X' * len(data))

                def hexdigest(self):
                    return self.digest.hexdigest()

            # identity_digest uses a positional argument; streamed hashing does not
            def sha256(data=None):
                return original_sha256(data) if data is not None else ChangingDigest()

            with self.subTest(replacement=replacement), patch.object(assets.hashlib, 'sha256', side_effect=sha256):
                with self.assertRaisesRegex(ValueError, 'changed while reading'):
                    assets.stage(self.root, self.destination, manifest)
            self.assertFalse(self.destination.exists())

    def test_staged_bytes_are_independently_verified(self):
        manifest = self.inventory()
        original = assets.file_identity

        def corrupt_copy(path, target=None):
            result = original(path, target)
            if target is not None:
                target.write_bytes(b'corrupted output')
            return result

        with patch.object(assets, 'file_identity', side_effect=corrupt_copy):
            with self.assertRaisesRegex(ValueError, 'Staged retail file differs'):
                assets.stage(self.root, self.destination, manifest)
        self.assertFalse(self.destination.exists())

    def test_copy_failure_removes_partial_stage(self):
        manifest = self.inventory()
        original = assets.file_identity

        def fail_last(path, target=None):
            if path.name == 'water.pig':
                raise OSError('synthetic write failure')
            return original(path, target)

        with patch.object(assets, 'file_identity', side_effect=fail_last):
            with self.assertRaisesRegex(OSError, 'synthetic write failure'):
                assets.stage(self.root, self.destination, manifest)
        self.assertFalse(self.destination.exists())

    def test_symlinked_root_game_file_and_input_ancestor_are_rejected(self):
        alias = self.base / 'alias'
        self.symlink(alias, self.root, directory=True)
        with self.assertRaises(ValueError):
            assets.inventory(alias, self.expected)
        with self.assertRaises(ValueError):
            assets.inventory(alias / '..' / 'input', self.expected)
        for relative in ('d1/descent.hog', 'd2'):
            path = self.root / relative
            moved = path.with_name(path.name + '.real')
            path.rename(moved)
            self.symlink(path, moved, directory=moved.is_dir())
            with self.subTest(relative=relative), self.assertRaises(ValueError):
                self.inventory()
            path.unlink()
            moved.rename(path)

    def test_symlinked_stage_and_stage_ancestor_are_rejected(self):
        manifest = self.inventory()
        alias = self.base / 'alias'
        self.symlink(alias, self.root, directory=True)
        for destination in (alias, alias / 'stage'):
            with self.subTest(destination=destination), self.assertRaises(ValueError):
                assets.stage(self.root, destination, manifest)
        self.assertFalse((self.root / 'stage').exists())

    def test_symlink_inserted_after_inventory_is_rejected_and_stage_removed(self):
        manifest = self.inventory()
        path = self.root / 'd2/water.pig'
        moved = path.with_name('water.real')
        path.rename(moved)
        self.symlink(path, moved)
        with self.assertRaises(ValueError):
            assets.stage(self.root, self.destination, manifest)
        self.assertFalse(self.destination.exists())
        self.assertEqual(moved.read_bytes(), self.data['d2/water.pig'])

    def test_windows_reparse_points_are_rejected_without_symlink_privileges(self):
        info = type('FileInfo', (), {'st_mode': 0, 'st_file_attributes': 0x400})()
        with patch.object(Path, 'lstat', return_value=info), patch.object(assets.stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0x400, create=True):
            with self.assertRaises(ValueError):
                assets.reject_links(self.root)

    def test_cli_report_cannot_overwrite_inputs_existing_reports_or_stage_content(self):
        original = (self.root / 'd1/descent.hog').read_bytes()
        self.report.write_text('keep', encoding='utf-8')
        for output in (self.report, self.root / 'd1/descent.hog', self.root / 'new-report.json', self.destination / 'report.json'):
            process = self.cli('--stage', self.destination, output=output)
            self.assertEqual(process.returncode, 1, process.stderr)
        self.assertEqual(self.report.read_text(), 'keep')
        self.assertEqual((self.root / 'd1/descent.hog').read_bytes(), original)
        self.assertFalse(self.destination.exists())

    def test_cli_rejects_symlinked_report_paths(self):
        self.symlink(self.report, self.root / 'd1/descent.hog')
        process = self.cli()
        self.assertEqual(process.returncode, 1, process.stderr)
        self.assertEqual((self.root / 'd1/descent.hog').read_bytes(), self.data['d1/descent.hog'])

    def test_missing_file_does_not_publish_a_success_report(self):
        (self.root / 'd2/water.pig').unlink()
        process = self.cli('--stage', self.destination)
        self.assertEqual(process.returncode, 1, process.stderr)
        self.assertFalse(self.report.exists())
        self.assertFalse(self.destination.exists())


if __name__ == '__main__':
    unittest.main()

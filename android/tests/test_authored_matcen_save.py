"""Source-only rejection tests; no game assets, SDL, or native binary needed."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import run_authored_matcen_save as probe


class AuthoredMatcenSaveTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.base, self.extra = self.root / 'base', self.root / 'extra'
        for directory in (self.base, self.extra):
            directory.mkdir()
            (directory / 'data.hog').write_bytes(b'pinned fixture bytes')
        self.manifest = {'base_files': {'data.hog': probe.digest(self.base / 'data.hog')},
                         'extra_files': {'data.hog': probe.digest(self.extra / 'data.hog')},
                         'mission': 'authored', 'level': 1, 'trigger': 2,
                         'expected_bindings': [dict(ordinal=0, station=3, segment=5, mask=[1, 0])]}
        self.exe = self.root / 'native'
        self.exe.write_bytes(b'placeholder executable')
        self.inputs = self.root / 'inputs.json'
        self.inputs.write_text(json.dumps(self.manifest))
        self.args = argparse.Namespace(base=self.base, extra=self.extra, executable=self.exe,
                                       inputs=self.inputs, output=self.root / 'output', timeout=1)
        center = dict(ordinal=0, station=3, segment=5, mask=[1, 0], enabled=1,
                      lives=2, capacity=65536, timer=100, flag=0, disable_time=300)
        state = {'centers': [center], 'sim_state': 12345}
        self.positive = {'identity': {'mission': 'authored', 'level': 1, 'level_file': 'level01.rl2'},
                         'checkpoint': state, 'births': [dict(frame=4, ordinal=0, station=3,
                         segment=5, type=0, shields=65536)], 'terminal': copy.deepcopy(state)}
        self.negative = copy.deepcopy(self.positive)
        self.negative['births'] = []
        for phase in ('checkpoint', 'terminal'):
            self.negative[phase]['centers'][0]['enabled'] = 0

    def check(self, write=None, restore=None, repeat=None, negative=None):
        probe.compare_reports(write if write is not None else self.positive,
                              restore if restore is not None else self.positive,
                              repeat if repeat is not None else self.positive,
                              negative if negative is not None else self.negative, self.manifest)

    def test_valid_oracle(self):
        self.check()

    def test_missing_or_empty_births_rejected(self):
        for value in (None, []):
            changed = copy.deepcopy(self.positive)
            if value is None:
                del changed['births']
            else:
                changed['births'] = value
            with self.subTest(value=value), self.assertRaises(probe.EvidenceError):
                self.check(write=changed)

    def test_identity_map_timing_and_terminal_mutations_rejected(self):
        mutations = [('identity', 'mission', 'wrong'), ('identity', 'level', 2),
                     ('identity', 'level_file', 'wrong.rl2'), ('checkpoint', 'sim_state', 0)]
        for section, key, value in mutations:
            changed = copy.deepcopy(self.positive)
            changed[section][key] = value
            with self.subTest(section=section, key=key), self.assertRaises(probe.EvidenceError):
                self.check(restore=changed)
        for section in ('checkpoint', 'terminal'):
            for key in ('ordinal', 'station', 'segment', 'timer', 'disable_time'):
                changed = copy.deepcopy(self.positive)
                changed[section]['centers'][0][key] += 1
                with self.subTest(section=section, key=key), self.assertRaises(probe.EvidenceError):
                    self.check(restore=changed)
        changed = copy.deepcopy(self.positive)
        del changed['terminal']
        with self.assertRaises(probe.EvidenceError):
            self.check(restore=changed)
        changed = copy.deepcopy(self.positive)
        changed['births'][0]['frame'] += 1
        with self.assertRaises(probe.EvidenceError):
            self.check(repeat=changed)

    def test_bounded_manifest_admission(self):
        for key, values in (('level', (-128, 0, 128, True, '1')),
                            ('trigger', (-1, 256, False, '2')),
                            ('mission', ('../x', '/x', 'x/y', 'x.msn', '', 'é', 'x' * 65))):
            for value in values:
                changed = copy.deepcopy(self.manifest)
                changed[key] = value
                with self.subTest(key=key, value=value), self.assertRaises(probe.EvidenceError):
                    probe.validate_manifest(changed)
        for level in (-127, -1, 1, 127):
            changed = dict(self.manifest, level=level, trigger=255)
            probe.validate_manifest(changed)

    def test_joint_mapping_drift_rejected(self):
        positive, negative = copy.deepcopy(self.positive), copy.deepcopy(self.negative)
        for report in (positive, negative):
            for phase in ('checkpoint', 'terminal'):
                report[phase]['centers'][0]['segment'] = 6
            for birth in report['births']:
                birth['segment'] = 6
        with self.assertRaisesRegex(probe.EvidenceError, 'pinned authored bindings'):
            self.check(write=positive, restore=positive, repeat=positive, negative=negative)

    def test_every_pinned_station_requires_positive_births(self):
        second = dict(ordinal=1, station=4, segment=6, mask=[1, 0])
        self.manifest['expected_bindings'].append(second)
        for report in (self.positive, self.negative):
            for phase in ('checkpoint', 'terminal'):
                center = dict(report[phase]['centers'][0], **second)
                report[phase]['centers'].append(center)
        # All three positives agree, but the second station never produces a robot.
        with self.assertRaisesRegex(probe.EvidenceError, 'every pinned station'):
            self.check()
        self.positive['births'].append(dict(frame=5, ordinal=1, station=4,
                                           segment=6, type=0, shields=65536))
        self.check()

    def test_invalid_pinned_bindings_rejected(self):
        for bindings in (None, [], [{}], [dict(ordinal=-1, station=3, segment=5, mask=[1])],
                         [dict(ordinal=0, station=3, segment=5, mask=[])],
                         self.manifest['expected_bindings'] * 2):
            changed = dict(self.manifest, expected_bindings=bindings)
            with self.subTest(bindings=bindings), self.assertRaises(probe.EvidenceError):
                probe.validate_manifest(changed)

    def test_center_and_birth_admission(self):
        for key, values in (('enabled', (0, 2)), ('ordinal', (-1,)), ('station', (-1,)),
                            ('segment', (-1,)), ('mask', (0, [], [-1], [2**32], [True]))):
            for value in values:
                changed = copy.deepcopy(self.positive)
                changed['checkpoint']['centers'][0][key] = value
                with self.subTest(key=key, value=value), self.assertRaises(probe.EvidenceError):
                    self.check(write=changed, restore=changed, repeat=changed)
        for key, values in (('frame', (-1, 1200)), ('type', (-1, 1, 64))):
            for value in values:
                changed = copy.deepcopy(self.positive)
                changed['births'][0][key] = value
                with self.subTest(key=key, value=value), self.assertRaises(probe.EvidenceError):
                    self.check(write=changed, restore=changed, repeat=changed)
        for key in ('ordinal', 'station', 'segment'):
            changed = copy.deepcopy(self.positive)
            original = changed['checkpoint']['centers'][0]
            other = dict(original, ordinal=1, station=4, segment=6)
            other[key] = original[key]
            changed['checkpoint']['centers'].append(other)
            with self.subTest(duplicate=key), self.assertRaises(probe.EvidenceError):
                probe.validate_report(changed, self.manifest)

    def test_no_dispatch_activity_rejected(self):
        with self.assertRaises(probe.EvidenceError):
            self.check(negative=self.positive)
        for phase in ('checkpoint', 'terminal'):
            changed = copy.deepcopy(self.negative)
            changed[phase]['centers'][0]['enabled'] = 1
            with self.assertRaises(probe.EvidenceError):
                self.check(negative=changed)

    def test_changed_extra_missing_assets_rejected(self):
        probe.verify_assets(self.base, self.manifest['base_files'])
        path = self.base / 'data.hog'
        path.write_bytes(b'changed')
        with self.assertRaises(probe.EvidenceError):
            probe.verify_assets(self.base, self.manifest['base_files'])
        path.unlink()
        with self.assertRaises(probe.EvidenceError):
            probe.verify_assets(self.base, self.manifest['base_files'])
        path.write_bytes(b'pinned fixture bytes')
        (self.base / 'unlisted').write_bytes(b'extra')
        with self.assertRaises(probe.EvidenceError):
            probe.verify_assets(self.base, self.manifest['base_files'])

    def test_unsafe_manifest_paths_rejected(self):
        for name in ('../escape', '/absolute', 'a/../b', './b', 'a//b', 'a\\b', 'C:/a', '.'):
            changed = copy.deepcopy(self.manifest)
            changed['base_files'] = {name: '0' * 64}
            with self.subTest(name=name), self.assertRaises(probe.EvidenceError):
                probe.validate_manifest(changed)

    def test_unsafe_output_and_symlinks_rejected(self):
        for output in (self.base, self.base / 'out', self.extra / 'out', self.root,
                       self.exe / 'out', self.inputs / 'out'):
            with self.subTest(output=output), self.assertRaises(probe.EvidenceError):
                probe.validate_output(output, self.base, self.extra, self.exe, self.inputs)
        (self.base / 'link').symlink_to(self.extra / 'data.hog')
        with self.assertRaises(probe.EvidenceError):
            probe.verify_assets(self.base, self.manifest['base_files'])

    def test_zero_exit_without_report_fails_and_preserves_log(self):
        with patch.object(probe.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0)):
            result = probe.run(self.args)
        self.assertFalse(result['passed'])
        self.assertIn('missing native report', result['error'])
        self.assertTrue((self.args.output / 'write.log').exists())
        self.assertTrue((self.args.output / 'results.json').exists())

    def native(self, command, cwd, **kwargs):
        mode = command[-1]
        if mode == 'write':
            (cwd / 'authored-matcen.sav').write_bytes(b'ordinary save')
        report = self.negative if mode == 'no-dispatch' else self.positive
        (cwd / (mode + '.json')).write_text(json.dumps(report))
        return subprocess.CompletedProcess(command, 0)

    def test_runner_four_cold_invocations_and_hashes(self):
        with patch.object(probe.subprocess, 'run', side_effect=self.native) as native:
            result = probe.run(self.args)
        self.assertTrue(result['passed'], result)
        self.assertEqual(native.call_count, 4)
        self.assertEqual([row['mode'] for row in result['invocations']],
                         ['write', 'restore', 'restore', 'no-dispatch'])
        self.assertEqual(result['save_sha256'], probe.digest(self.args.output / 'authored-matcen.sav'))
        self.assertEqual(result['executable_sha256'], probe.digest(self.exe))
        self.assertTrue((self.args.output / 'restore-first.json').exists())
        self.assertTrue((self.args.output / 'restore-repeat.json').exists())

    def test_assets_rechecked_before_every_invocation(self):
        def mutate(command, cwd, **kwargs):
            result = self.native(command, cwd, **kwargs)
            (self.extra / 'data.hog').write_bytes(b'changed during native execution')
            return result
        with patch.object(probe.subprocess, 'run', side_effect=mutate) as native:
            result = probe.run(self.args)
        self.assertFalse(result['passed'])
        self.assertEqual(native.call_count, 1)
        self.assertIn('asset file set or bytes differ', result['error'])

    def test_final_invocation_mutations_rejected(self):
        for index, target in enumerate((self.base / 'data.hog', self.extra / 'data.hog',
                                        self.exe, self.inputs)):
            original = target.read_bytes()
            self.args.output = self.root / f'final-mutation-{index}'
            def mutate(command, cwd, **kwargs):
                result = self.native(command, cwd, **kwargs)
                if command[-1] == 'no-dispatch':
                    target.write_bytes(b'changed during final invocation')
                return result
            with self.subTest(target=target), patch.object(probe.subprocess, 'run', side_effect=mutate) as native:
                result = probe.run(self.args)
                self.assertFalse(result['passed'])
                self.assertEqual(native.call_count, 4)
                self.assertRegex(result['error'], 'asset file set or bytes differ|executable/provenance changed')
            target.write_bytes(original)

    def test_timeout_fails(self):
        with patch.object(probe.subprocess, 'run', side_effect=subprocess.TimeoutExpired('native', 1)):
            result = probe.run(self.args)
        self.assertFalse(result['passed'])
        self.assertEqual(result['invocations'][0]['exit'], 124)


if __name__ == '__main__':
    unittest.main()

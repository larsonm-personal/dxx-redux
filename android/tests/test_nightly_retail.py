"""Synthetic private-fixture boundary checks; no token, network or game data."""
import copy
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import nightly_retail as retail
import nightly_tooling_sweep as sweep

ROOT = Path(__file__).resolve().parents[2]
SHA = 'a' * 40
ENV = {'GITHUB_REPOSITORY': retail.REPOSITORY, 'GITHUB_REF': 'refs/heads/cmake',
       'GITHUB_WORKFLOW_REF': retail.WORKFLOW, 'GITHUB_EVENT_NAME': 'schedule',
       'RETAIL_ASSETS_COMMIT': 'b' * 40}


class RetailBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.policy = {'schema': 1, 'suites': [
            {'id': 'master:retail', 'owner': 'master', 'lane': 'android-single',
             'args': {'suite': 'retail'}, 'fixture': 'retail-base', 'estimated_seconds': 100}], 'exclusions': []}
        self.catalog = {'schema': 1, 'tests': [{'name': 'retail'}]}
        self.identity = retail.configured_identity(ENV)

    def plan(self, identity=None, state=None):
        return sweep.plan_batch(SHA, self.catalog, self.policy, state, retail_assets=identity)

    def test_only_exact_trusted_cmake_entry_can_enable_assets(self):
        self.assertTrue(retail.trusted_context(ENV))
        self.assertTrue(retail.trusted_context({**ENV, 'GITHUB_EVENT_NAME': 'workflow_dispatch'}))
        for key, value in [('GITHUB_REPOSITORY', 'other/repo'), ('GITHUB_REF', 'refs/pull/1/merge'),
                           ('GITHUB_REF', 'refs/heads/feature'), ('GITHUB_REF', 'refs/tags/cmake'),
                           ('GITHUB_EVENT_NAME', 'pull_request'), ('GITHUB_EVENT_NAME', 'pull_request_target'),
                           ('GITHUB_EVENT_NAME', 'workflow_call'), ('GITHUB_WORKFLOW_REF', retail.WORKFLOW.replace('nightly-tooling-sweep', 'other'))]:
            with self.subTest(key=key, value=value):
                self.assertIsNone(retail.configured_identity({**ENV, key: value}))

    def test_missing_config_pending_not_excluded_or_completed(self):
        self.assertIsNone(retail.configured_identity({**ENV, 'RETAIL_ASSETS_COMMIT': ''}))
        plan, state = self.plan()
        self.assertEqual(plan['tests'], [])
        self.assertEqual(plan['unavailable'][0]['id'], 'master:retail')
        self.assertNotIn('master:retail', {row['id'] for row in plan['exclusions']})
        self.assertEqual(state['records'], {})
        available, _ = self.plan(self.identity, state)
        self.assertEqual(len(available['tests']), 1)

    def test_configuration_requires_immutable_commit(self):
        for commit in ('main', 'b' * 39, 'B' * 40, '../secret'):
            with self.subTest(commit=commit), self.assertRaises(ValueError):
                retail.configured_identity({**ENV, 'RETAIL_ASSETS_COMMIT': commit})

    def test_identity_binds_manifest_repository_commit_and_batch(self):
        plan, _ = self.plan(self.identity)
        for key, value in [('repository', 'other/repo'), ('manifest_sha256', '0' * 64), ('commit', 'c' * 40)]:
            altered = copy.deepcopy(plan)
            altered['retail_assets'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                sweep.validate_plan(altered, self.policy)
        del plan['retail_assets']
        with self.assertRaises(ValueError):
            sweep.validate_plan(plan, self.policy)

    def test_changed_data_commit_cannot_reuse_pass(self):
        plan, state = self.plan(self.identity)
        state['records']['master:retail'] = {'status': 'PASS', 'run_url': 'https://github.com/o/r/actions/runs/1'}
        changed = {**self.identity, 'commit': 'c' * 40}
        other, reset = self.plan(changed, state)
        self.assertNotEqual(plan['catalog_hash'], other['catalog_hash'])
        self.assertEqual(reset['records'], {})

    def test_projection_removes_all_arbitrary_test_strings(self):
        marker = 'ASSET_BYTES_MUST_NOT_ESCAPE'
        plan, state = self.plan(self.identity)
        row = {'id': 'master:retail', 'status': 'FAIL', 'phase': 'tests', 'attempted_tests': True,
               'reason': marker, 'run_url': marker, 'exclusions': [marker], 'traceback': marker,
               'cases': [{'id': marker, 'status': 'FAIL', 'reason': marker}], 'blocker_kind': marker,
               'exit_code': 1, 'coverage_complete': False}
        receipt = {key: plan[key] for key in ('schema', 'target_sha', 'catalog_hash', 'batch_id', 'retail_assets')}
        receipt.update(lane='android-single', results=[row])
        with patch.dict(os.environ, {'GITHUB_RUN_ID': '123'}):
            clean = retail.public_receipt(receipt)
            ledger = sweep.reconcile(plan, self.policy, state, [receipt])
        self.assertNotIn(marker, json.dumps(clean))
        self.assertNotIn(marker, json.dumps(ledger))
        self.assertEqual(ledger['records']['master:retail']['status'], 'FAIL')
        self.assertEqual(ledger['records']['master:retail']['case_counts']['FAIL'], 1)

    def test_failed_setup_remains_pending_after_projection(self):
        plan, state = self.plan(self.identity)
        with tempfile.TemporaryDirectory() as directory:
            result = sweep.collect(plan, self.policy, 'android-single', directory, 'secret raw setup detail')
            self.assertNotIn('secret raw', json.dumps(result))
            self.assertEqual(result['results'][0]['status'], 'BLOCKED')
            self.assertFalse(result['results'][0]['attempted_tests'])
            self.assertEqual(sweep.reconcile(plan, self.policy, state, [result])['records'], {})

    def test_checkout_cli_rejects_untrusted_or_changed_configuration(self):
        plan, _ = self.plan(self.identity)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sweep.write(root / 'plan', plan); sweep.write(root / 'policy', self.policy)
            argv = ['sweep', 'retail-check', '--plan', str(root / 'plan'), '--policy', str(root / 'policy'), '--lane', 'android-single']
            for changes in ({'GITHUB_EVENT_NAME': 'pull_request'}, {'RETAIL_ASSETS_COMMIT': 'c' * 40}, {'RETAIL_ASSETS_COMMIT': ''}):
                with patch.dict(os.environ, {**ENV, **changes}, clear=True), patch('sys.argv', argv), self.assertRaises(ValueError):
                    sweep.main()
            with patch.dict(os.environ, ENV, clear=True), patch('sys.argv', argv), patch('builtins.print'):
                self.assertEqual(sweep.main(), 0)

    def test_workflow_secret_and_artifact_boundary_is_explicit(self):
        for name in ('nightly-tooling-sweep.yml', 'android-nightly-tooling.yml'):
            source = (ROOT / '.github/workflows' / name).read_text()
            self.assertIn("'retail-ci' || 'asset-free'", source)
            self.assertNotIn('secrets: inherit', source)
            self.assertIn('repository: larsonm-personal/dxx-redux-test-assets', source)
            self.assertIn('token: ${{ secrets.RETAIL_ASSETS_READ_TOKEN }}', source)
            self.assertIn('unset RETAIL_READ_TOKEN', source)
            self.assertIn('--stage target/game_data/retail-ci', source)
            self.assertIn('results/retail-verification.json', source)
            self.assertIn('rm -rf retail-input', source)
        android = (ROOT / '.github/workflows/android-nightly-tooling.yml').read_text()
        first_step = android.split('      - name: Check out trusted sweep controller')[0]
        self.assertIn('$GITHUB_WORKFLOW_REF', first_step)
        self.assertIn('$GITHUB_EVENT_NAME', first_step)
        self.assertIn('github.workflow_ref ==', first_step)
        self.assertIn('github.repository ==', first_step)
        for step in android.split('      - name: '):
            if 'uses: actions/cache/save@' in step:
                self.assertIn('!inputs.retail_assets', step)
            if 'uses: actions/upload-artifact@' in step and 'retail-safe' not in step:
                self.assertIn('!inputs.retail_assets', step)


if __name__ == '__main__':
    unittest.main()

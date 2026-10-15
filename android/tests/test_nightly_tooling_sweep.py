"""Pure planner/reconciliation controls; never invokes PowerShell or GitHub."""
import copy
import json
import os
import subprocess
import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import nightly_tooling_sweep as sweep

SHA = 'a' * 40


class SweepTests(unittest.TestCase):
    def setUp(self):
        self.policy = {'schema': 1, 'suites': [
            {'id': 'master:' + name, 'owner': 'master', 'lane': 'host', 'args': {'suite': name}, 'estimated_seconds': 100}
            for name in ('first', 'second', 'third')], 'exclusions': []}
        self.catalog = {'schema': 1, 'tests': [{'name': x} for x in ('first', 'second', 'third')]}
        self.plan, self.state = sweep.plan_batch(SHA, self.catalog, self.policy, budget=200)

    def receipt(self, status='PASS', ident='master:first'):
        return {**{k: self.plan[k] for k in ('schema', 'target_sha', 'catalog_hash', 'batch_id')}, 'lane': 'host',
                'results': [{'id': ident, 'status': status, 'run_url': 'https://github.com/o/r/actions/runs/1'}]}

    def test_planning_does_not_advance_and_repeats_identically(self):
        old = copy.deepcopy(self.state)
        again, _ = sweep.plan_batch(SHA, self.catalog, self.policy, self.state, 200)
        self.assertEqual(again, self.plan)
        self.assertEqual(old, self.state)

    def test_partial_interruption_advances_only_completed(self):
        state = sweep.reconcile(self.plan, self.policy, self.state,
            [self.receipt(), self.receipt('RUNNING', 'master:second')])
        plan, _ = sweep.plan_batch(SHA, self.catalog, self.policy, state, 200)
        self.assertEqual([e['id'] for e in plan['tests']], ['master:second', 'master:third'])

    def test_failures_are_completed_but_remain_failed(self):
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt('FAIL')])
        self.assertEqual(state['records']['master:first']['status'], 'FAIL')
        self.assertNotIn('master:first', [e['id'] for e in sweep.plan_batch(SHA, self.catalog, self.policy, state)[0]['tests']])

    def test_test_timeout_is_failed_attempt_not_success(self):
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt('TIMEOUT')])
        self.assertEqual(state['records']['master:first']['status'], 'TIMEOUT')

    def test_transient_blockage_retries_without_coverage(self):
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt('BLOCKED')])
        self.assertNotIn('master:first', state['records'])
        self.assertEqual(state['retryable']['master:first']['attempts'], 1)
        self.assertIn('master:first', [e['id'] for e in sweep.plan_batch(SHA, self.catalog, self.policy, state)[0]['tests']])
        self.assertEqual(sweep.reconcile(self.plan, self.policy, state, [self.receipt('BLOCKED')]), state)

    def test_all_done_has_no_wrap(self):
        plan, state = sweep.plan_batch(SHA, self.catalog, self.policy)
        receipts = []
        for entry in plan['tests']:
            r = self.receipt(ident=entry['id']); r['batch_id'] = plan['batch_id']; receipts.append(r)
        state = sweep.reconcile(plan, self.policy, state, receipts)
        self.assertEqual(sweep.plan_batch(SHA, self.catalog, self.policy, state)[0]['tests'], [])

    def test_new_sha_starts_fresh(self):
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt()])
        plan, new = sweep.plan_batch('b' * 40, self.catalog, self.policy, state)
        self.assertEqual(len(plan['tests']), 3)
        self.assertEqual(new['records'], {})

    def test_changed_catalog_reconsiders_blockages(self):
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt('BLOCKED')])
        policy = copy.deepcopy(self.policy); policy['suites'][0]['estimated_seconds'] = 101
        self.assertEqual(sweep.plan_batch(SHA, self.catalog, policy, state)[1]['records'], {})

    def test_malformed_ledger_never_silently_resets(self):
        for bad in ({}, {'schema': 9}, {**self.state, 'records': {'x': {'status': 'PASS'}}}):
            with self.assertRaises(ValueError):
                sweep.plan_batch(SHA, self.catalog, self.policy, bad)

    def test_mismatched_revision_catalog_and_batch_rejected(self):
        for key in ('target_sha', 'catalog_hash', 'batch_id'):
            receipt = self.receipt(); receipt[key] = 'bad'
            with self.assertRaises(ValueError):
                sweep.reconcile(self.plan, self.policy, self.state, [receipt])

    def test_unknown_and_wrong_lane_results_rejected(self):
        for receipt in (self.receipt(ident='unknown'), {**self.receipt(), 'lane': 'jvm'}):
            with self.assertRaises(ValueError):
                sweep.reconcile(self.plan, self.policy, self.state, [receipt])

    def test_conflicting_duplicates_rejected_identical_idempotent(self):
        with self.assertRaises(ValueError):
            sweep.reconcile(self.plan, self.policy, self.state, [self.receipt(), self.receipt('FAIL')])
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt(), self.receipt()])
        self.assertEqual(sweep.reconcile(self.plan, self.policy, state, [self.receipt()]), state)

    def test_plan_cannot_inject_commands_or_unknown_owner(self):
        for mutate in (lambda p: p['tests'][0]['args'].update(command='rm -rf /'),
                       lambda p: p['tests'][0].update(owner='shell')):
            plan = copy.deepcopy(self.plan); mutate(plan)
            with self.assertRaises(ValueError): sweep.validate_plan(plan, self.policy)

    def test_oversized_owner_rejected_instead_of_starving(self):
        self.policy['suites'][0]['estimated_seconds'] = 2701
        with self.assertRaises(ValueError): sweep.plan_batch(SHA, self.catalog, self.policy)

    def test_missing_journal_remains_pending(self):
        with tempfile.TemporaryDirectory() as d:
            result = sweep.collect(self.plan, self.policy, 'host', d)
            self.assertTrue(all(r['status'] == 'NOT_RUN' for r in result['results']))

    def test_partial_journal_retains_terminal_and_ignores_torn_append(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, 'journal.jsonl').write_text(json.dumps({'name': 'first', 'commit': SHA, 'status': 'PASS'}) + '\n{"name":')
            result = sweep.collect(self.plan, self.policy, 'host', d)
            self.assertEqual([r['status'] for r in result['results']], ['PASS', 'NOT_RUN'])

    def test_no_receipts_no_progress(self):
        self.assertEqual(sweep.reconcile(self.plan, self.policy, self.state, []), self.state)

    def test_unknown_catalog_suites_are_visible_exclusions(self):
        self.catalog['tests'].append({'name': 'retail-only', 'requires': 'emulator'})
        plan, _ = sweep.plan_batch(SHA, self.catalog, self.policy)
        self.assertIn('retail-only', [e['id'] for e in plan['exclusions']])

    def test_retryable_infrastructure_does_not_starve_untried(self):
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt('BLOCKED')])
        plan, _ = sweep.plan_batch(SHA, self.catalog, self.policy, state, 100)
        self.assertEqual(plan['tests'][0]['id'], 'master:second')

    def test_prelaunch_failure_is_retryable_not_test_failure(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, 'journal.jsonl').write_text(json.dumps({'name': 'first', 'commit': SHA, 'status': 'FAIL', 'attempted_tests': False}) + '\n')
            result = sweep.collect(self.plan, self.policy, 'host', d)
            self.assertEqual(result['results'][0]['status'], 'BLOCKED')
            self.assertFalse(result['results'][0]['attempted_tests'])

    def test_stable_target_capability_is_exclusion_not_coverage(self):
        receipt = self.receipt('BLOCKED')
        receipt['results'][0].update(retryable=False, blocker_kind='target-capability', reason='suite absent in immutable target')
        state = sweep.reconcile(self.plan, self.policy, self.state, [receipt])
        self.assertEqual(state['records'], {})
        self.assertIn('master:first', state['excluded'])
        self.assertNotIn('master:first', [e['id'] for e in sweep.plan_batch(SHA, self.catalog, self.policy, state)[0]['tests']])

    def test_large_leaf_tracebacks_stay_out_of_permanent_ledger(self):
        receipt = self.receipt()
        receipt['results'][0].update(cases=[{'id': 'leaf', 'status': 'FAIL', 'reason': 'x' * 100000}], coverage_complete=False)
        state = sweep.reconcile(self.plan, self.policy, self.state, [receipt])
        self.assertLess(len(json.dumps(state)), 2000)
        self.assertEqual(state['records']['master:first']['case_counts']['FAIL'], 1)
        self.assertFalse(state['records']['master:first']['coverage_complete'])

    def test_success_receipt_is_not_labelled_infrastructure(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, 'journal.jsonl').write_text(json.dumps({'name': 'first', 'status': 'PASS', 'commit': SHA, 'attempted_tests': True}) + '\n')
            with patch.dict(os.environ, {'GITHUB_SERVER_URL': 'https://github.com', 'GITHUB_REPOSITORY': 'o/r', 'GITHUB_RUN_ID': '1'}):
                result = sweep.collect(self.plan, self.policy, 'host', d)
            row = result['results'][0]
            self.assertEqual(row['status'], 'PASS')
            self.assertFalse(row['retryable'])
            self.assertEqual(row['blocker_kind'], '')

    def test_timeout_cleanup_is_bounded(self):
        with tempfile.TemporaryDirectory() as d, open(os.devnull, 'w') as log:
            with self.assertRaises(subprocess.TimeoutExpired):
                sweep.run_command([sys.executable, '-c', 'import time; time.sleep(60)'], d, log, timeout=0.1)

    def test_source_hooks_do_not_use_wildcard_or_random_target(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / 'android/run_all_tests.ps1').read_text()
        self.assertIn('$name -cnotin $allTests.Name', source)
        self.assertIn('Write-SuiteEvidenceJournal -Observation $observation', source)
        self.assertIn('if (-not $SelectedTestsPath -and', source)
        workflow = (root / '.github/workflows/nightly-tooling-sweep.yml').read_text()
        self.assertIn('cancel-in-progress: false', workflow)
        self.assertIn('persist-credentials: false', workflow)
        self.assertIn('Recover completed receipts', workflow)
        self.assertEqual(workflow.count('> target/dependency_base.txt'), 2)
        self.assertLess(workflow.index('> target/dependency_base.txt'), workflow.index('Export authoritative master catalog'))
        host = workflow[workflow.index('  host:'):workflow.index('  android:')]
        self.assertLess(host.index('> target/dependency_base.txt'), host.index('Execute admitted host owners'))


if __name__ == '__main__':
    unittest.main()

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

SHA = '0000000000000000000000000000000000000008'  # Stable first/second/third fixture ordering.


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

    def attempt(self, output, exit_code=None):
        sweep.write(Path(output, 'attempt.json'), {
            **sweep.attempt_identity(self.plan, 'host'),
            'processes': {'master': {'completed': exit_code is not None, 'exit_code': exit_code}}})

    def test_representative_validation_covers_four_lanes_within_budgets(self):
        policy = sweep.read(Path(__file__).with_name('nightly_tooling_policy.json'))
        entries = sweep.policy_entries(policy)
        selected = [entries[ident] for ident in sweep.VALIDATION_IDS]
        self.assertEqual(len(selected), 10)
        self.assertEqual({entry['lane'] for entry in selected}, set(sweep.LANES))
        for lane in sweep.LANES:
            self.assertLessEqual(sum(entry['estimated_seconds'] for entry in selected if entry['lane'] == lane), 2700)
        self.assertIn('master:test_lan_lobby_discovery', sweep.VALIDATION_IDS)
        self.assertIn('master:test_android_music_completion', sweep.VALIDATION_IDS)
        self.assertIn('master:test_launcher_media_controls_generated', sweep.VALIDATION_IDS)

    def test_planning_does_not_advance_and_repeats_identically(self):
        old = copy.deepcopy(self.state)
        again, _ = sweep.plan_batch(SHA, self.catalog, self.policy, self.state, 200)
        self.assertEqual(again, self.plan)
        self.assertEqual(old, self.state)

    def test_sha_seed_changes_order_without_reusing_certification(self):
        full, _ = sweep.plan_batch(SHA, self.catalog, self.policy, budget=300)
        certified = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt()])
        self.assertIn('master:first', certified['records'])
        other, state = sweep.plan_batch('b' * 40, self.catalog, self.policy, certified, 300)
        self.assertNotEqual([e['id'] for e in full['tests']], [e['id'] for e in other['tests']])
        self.assertEqual(state['records'], {})
        self.assertEqual(other, sweep.plan_batch('b' * 40, self.catalog, self.policy, budget=300)[0])

    def test_seeded_sweep_resume_is_finite_and_never_repeats_terminal(self):
        state = self.state
        seen = []
        while True:
            plan, state = sweep.plan_batch(SHA, self.catalog, self.policy, state, 100)
            if not plan['tests']:
                break
            ident = plan['tests'][0]['id']
            self.assertNotIn(ident, seen)
            seen.append(ident)
            receipt = {**{k: plan[k] for k in ('schema', 'target_sha', 'catalog_hash', 'batch_id')},
                       'lane': 'host', 'results': [{'id': ident, 'status': 'PASS', 'run_url': 'https://github.com/o/r/actions/runs/1'}]}
            state = sweep.reconcile(plan, self.policy, state, [receipt])
        self.assertEqual(set(seen), set(sweep.policy_entries(self.policy)))

    def test_each_of_four_lanes_has_its_own_bounded_budget(self):
        policy = {'schema': 1, 'suites': []}
        catalog = {'schema': 1, 'tests': []}
        for lane in sweep.LANES:
            for i in range(2):
                name = lane.replace('-', '_') + str(i)
                policy['suites'].append({'id': 'master:' + name, 'owner': 'master', 'lane': lane,
                                         'args': {'suite': name}, 'estimated_seconds': 100})
                catalog['tests'].append({'name': name})
        plan, _ = sweep.plan_batch(SHA, catalog, policy, budget=100)
        self.assertEqual(len(plan['tests']), 4)
        self.assertEqual(plan['estimated_seconds'], 400)
        self.assertEqual(plan['budget_scope'], 'per-lane')
        sweep.validate_plan(plan, policy)
        for lane in sweep.LANES:
            self.assertEqual(sum(e['estimated_seconds'] for e in plan['tests'] if e['lane'] == lane), 100)
        extra = next(e for e in policy['suites'] if e['lane'] == 'host' and e not in plan['tests'])
        plan['tests'].append(extra)
        plan['batch_id'] = sweep.digest({k: plan[k] for k in ('target_sha', 'catalog_hash', 'tests')})
        with self.assertRaises(ValueError):
            sweep.validate_plan(plan, policy)

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

    def test_failed_master_without_receipts_is_pending_with_diagnostic(self):
        with tempfile.TemporaryDirectory() as d:
            self.attempt(d, 1)
            result = sweep.collect(self.plan, self.policy, 'host', d)
            self.assertTrue(all(row['status'] == 'NOT_RUN' for row in result['results']))
            self.assertTrue(all('Master runner exited 1' in row['reason'] for row in result['results']))
            self.assertTrue(all(not row['attempted_tests'] for row in result['results']))
            self.assertEqual(sweep.reconcile(self.plan, self.policy, self.state, [result])['records'], {})

    def test_shareware_refresh_precedes_each_owner_and_failure_stays_pending(self):
        policy = {'schema': 1, 'suites': [
            {'id': 'shareware:test_death#' + game, 'owner': 'shareware', 'lane': 'android-single',
             'args': {'suite': 'test_death', 'game': game}, 'estimated_seconds': 100}
            for game in ('d1', 'd2')], 'exclusions': []}
        plan, state = sweep.plan_batch(SHA, {'schema': 1, 'tests': []}, policy, budget=200)
        for failure in (False, True, 'timeout'):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as d:
                calls = []
                def run(command, *args, **kwargs):
                    calls.append(command)
                    if command[0] == 'bash' and failure == 'timeout':
                        raise subprocess.TimeoutExpired(command, 60)
                    return subprocess.CompletedProcess(command, 1 if command[0] == 'bash' and failure else 0)
                with patch.dict(os.environ, {'GITHUB_ACTIONS': 'true'}), patch.object(sweep.subprocess, 'check_output', return_value=SHA), patch.object(sweep, 'run_command', side_effect=run):
                    result = sweep.execute(plan, policy, 'android-single', d, Path(d, 'out'))
                self.assertEqual([c[0] for c in calls], ['bash', 'bash'] if failure else ['bash', 'pwsh', 'bash', 'pwsh'])
                self.assertTrue(all(c[1].endswith('refresh_nightly_kvm_access.sh') for c in calls if c[0] == 'bash'))
                receipt = sweep.collect(plan, policy, 'android-single', Path(d, 'out'))
                if failure:
                    self.assertTrue(result['runner_failed'])
                    self.assertTrue(all(r['status'] == 'BLOCKED' and r['retryable'] and not r['attempted_tests'] for r in receipt['results']))
                    self.assertEqual(sweep.reconcile(plan, policy, state, [receipt])['records'], {})
                else:
                    self.assertTrue(all(r['status'] == 'PASS' for r in receipt['results']))

    def test_nonzero_master_exit_fails_lane_even_without_test_failures(self):
        argv = ['sweep', 'execute', '--plan', 'plan', '--policy', 'policy', '--repo', 'repo', '--output', 'out', '--lane', 'host']
        with patch.object(sys, 'argv', argv), patch.object(sweep, 'read', return_value={}), patch.object(sweep, 'execute', return_value={'runner_failed': True, 'results': []}):
            self.assertEqual(sweep.main(), 1)

    def test_zero_runner_missing_required_receipts_fails_only_pr_validation(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            for name, value in (('plan', self.plan), ('policy', self.policy), ('state', self.state)):
                sweep.write(root / name, value)
            self.attempt(root, 0)
            receipt = sweep.collect(self.plan, self.policy, 'host', root)
            self.assertTrue(all(row['status'] == 'NOT_RUN' for row in receipt['results']))
            argv = ['sweep', 'reconcile', '--plan', str(root / 'plan'), '--policy', str(root / 'policy'), '--state', str(root / 'state'), '--output', str(root / 'out'), '--receipts', str(root / 'results.json')]
            with patch.object(sys, 'argv', argv), patch('builtins.print'):
                self.assertEqual(sweep.main(), 0)
            with patch.object(sys, 'argv', argv + ['--require-selected']), patch('builtins.print'):
                self.assertEqual(sweep.main(), 1)
            self.assertEqual(sweep.read(root / 'out')['records'], {})

    def test_required_selection_rejects_missing_blocked_and_failed_receipts(self):
        self.assertEqual(set(sweep.selected_without_pass(self.plan, self.state)), {e['id'] for e in self.plan['tests']})
        for status in ('FAIL', 'BLOCKED', 'TIMEOUT'):
            state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt(status)])
            self.assertIn('master:first', sweep.selected_without_pass(self.plan, state))
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt()])
        self.assertEqual(sweep.selected_without_pass(self.plan, state), ['master:second'])
        state = sweep.reconcile(self.plan, self.policy, state, [self.receipt(ident='master:second')])
        self.assertEqual(sweep.selected_without_pass(self.plan, state), [])

    def test_partial_journal_retains_terminal_and_ignores_torn_append(self):
        with tempfile.TemporaryDirectory() as d:
            self.attempt(d)
            Path(d, 'journal.jsonl').write_text(json.dumps({'name': 'first', 'commit': SHA, 'status': 'PASS'}) + '\n{"name":')
            result = sweep.collect(self.plan, self.policy, 'host', d)
            self.assertEqual([r['status'] for r in result['results']], ['PASS', 'NOT_RUN'])

    def run_owner_control(self, owner, rows, exit_code=0, stale=False):
        entry = {'id': owner + ':probe', 'owner': owner, 'lane': 'host',
                 'args': {'suite': 'probe'}, 'estimated_seconds': 100}
        policy = {'schema': 1, 'suites': [entry], 'exclusions': []}
        plan, state = sweep.plan_batch(SHA, {'schema': 1, 'tests': [{'name': 'probe'}]}, policy)
        with tempfile.TemporaryDirectory() as d:
            output = Path(d, 'out')
            output.mkdir()
            destination = output / ('journal.jsonl' if owner == 'master' else owner + '_probe.json')
            def emit():
                if rows is None:
                    return
                if owner == 'master':
                    destination.write_text(''.join(json.dumps({'name': 'probe', 'commit': SHA, **row}) + '\n' for row in rows))
                else:
                    sweep.write(destination, {'schema': 1, 'suite': 'probe', **rows[0]})
            if stale:
                emit()
                sweep.write(output / 'results.json', {'stale': True})
                sweep.write(output / 'attempt.json', {**sweep.attempt_identity(plan, 'host'),
                    'processes': {owner if owner == 'master' else entry['id']: {'completed': True, 'exit_code': 0}}})
            def run(*args, **kwargs):
                self.assertFalse((output / 'results.json').exists())
                if not stale:
                    emit()
                return subprocess.run([sys.executable, '-c', 'raise SystemExit(' + str(exit_code) + ')'], check=False)
            with patch.object(sweep.subprocess, 'check_output', return_value=SHA), patch.object(sweep, 'run_command', side_effect=run):
                execution = sweep.execute(plan, policy, 'host', d, output)
            receipt = sweep.collect(plan, policy, 'host', output)
            result = sweep.reconcile(plan, policy, state, [receipt])
            for name, value in (('plan', plan), ('policy', policy), ('state', state)):
                sweep.write(output / name, value)
            argv = ['sweep', 'reconcile', '--plan', str(output / 'plan'), '--policy', str(output / 'policy'),
                    '--state', str(output / 'state'), '--output', str(output / 'final'),
                    '--receipts', str(output / 'results.json'), '--require-selected']
            with patch.object(sys, 'argv', argv), patch('builtins.print'):
                final_exit = sweep.main()
            return execution, receipt['results'][0], result, final_exit

    def test_real_nonzero_process_cannot_certify_pass(self):
        for owner in ('master', 'host-adapter'):
            with self.subTest(owner=owner):
                execution, row, state, final_exit = self.run_owner_control(owner, [{'status': 'PASS'}], 17)
                self.assertTrue(execution['runner_failed'])
                self.assertEqual(row['status'], 'FAIL')
                self.assertEqual(state['records'][owner + ':probe']['status'], 'FAIL')
                self.assertEqual(final_exit, 1)

    def test_previous_raw_receipts_are_cleared_before_launch(self):
        for owner in ('master', 'host-adapter'):
            with self.subTest(owner=owner):
                _, row, state, final_exit = self.run_owner_control(owner, [{'status': 'PASS'}], 17, stale=True)
                self.assertEqual(row['status'], 'NOT_RUN')
                self.assertEqual(state['records'], {})
                self.assertEqual(final_exit, 1)

    def test_zero_host_exit_without_receipt_is_pending(self):
        execution, row, state, final_exit = self.run_owner_control('host-adapter', None)
        self.assertFalse(execution['runner_failed'])
        self.assertEqual(row['status'], 'NOT_RUN')
        self.assertEqual(state['records'], {})
        self.assertEqual(final_exit, 1)

    def test_conflicting_terminal_journal_cannot_be_overwritten(self):
        with self.assertRaisesRegex(ValueError, 'Conflicting terminal'):
            self.run_owner_control('master', [{'status': 'FAIL'}, {'status': 'PASS'}])
        _, row, _, final_exit = self.run_owner_control('master', [{'status': 'RUNNING'}, {'status': 'PASS'}])
        self.assertEqual(row['status'], 'PASS')
        self.assertEqual(final_exit, 0)

    def test_structured_contradictions_and_no_pass_stay_red(self):
        for detail, expected in (({'attempted_tests': False}, 'BLOCKED'),
                                 ({'phase': 'preflight'}, 'BLOCKED'),
                                 ({'exit_code': 17}, 'FAIL'),
                                 ({'cases': [{'status': 'FAIL'}]}, 'FAIL'),
                                 ({'cases': [{'status': 'SKIP'}]}, 'BLOCKED'),
                                 ({'cases': [{'status': 'BLOCKED'}]}, 'BLOCKED'),
                                 ({'counts': {'total': 0, 'passed': 0, 'failed': 0}}, 'BLOCKED'),
                                 ({'counts': {'passed': 1, 'failed': 1}}, 'FAIL')):
            with self.subTest(detail=detail):
                _, row, _, final_exit = self.run_owner_control('host-adapter', [{'status': 'PASS', **detail}])
                self.assertEqual(row['status'], expected)
                self.assertEqual(final_exit, 1)
                receipt = self.receipt()
                receipt['results'][0].update(detail)
                state = sweep.reconcile(self.plan, self.policy, self.state, [receipt])
                self.assertIn('master:first', sweep.selected_without_pass(self.plan, state))

    def test_real_partial_pass_and_unknown_script_coverage_remain_valid(self):
        for detail in ({'cases': [{'status': 'PASS'}] * 54 + [{'status': 'SKIP'}] * 3}, {}):
            _, row, state, final_exit = self.run_owner_control('host-adapter', [{'status': 'PASS', 'coverage_complete': False, **detail}])
            self.assertEqual(row['status'], 'PASS')
            record = state['records']['host-adapter:probe']
            self.assertFalse(record['coverage_complete'])
            self.assertEqual(record['case_counts']['PASS'], 54 if detail else 0)
            self.assertNotIn('host-adapter:probe', state['retryable'])
            self.assertEqual(final_exit, 0)

    def test_unbound_journal_never_certifies(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, 'journal.jsonl').write_text(json.dumps({'name': 'first', 'commit': SHA, 'status': 'PASS'}) + '\n')
            self.assertTrue(all(row['status'] == 'NOT_RUN' for row in sweep.collect(self.plan, self.policy, 'host', d)['results']))

    def test_attempt_must_match_batch_lane_and_github_run(self):
        with tempfile.TemporaryDirectory() as d:
            for key in ('target_sha', 'catalog_hash', 'batch_id', 'lane', 'run_id', 'run_attempt', 'repository'):
                with self.subTest(key=key):
                    self.attempt(d)
                    attempt = sweep.read(Path(d, 'attempt.json'))
                    attempt[key] = 'wrong'
                    sweep.write(Path(d, 'attempt.json'), attempt)
                    with self.assertRaisesRegex(ValueError, 'attempt provenance'):
                        sweep.collect(self.plan, self.policy, 'host', d)

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
            self.attempt(d)
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
        self.assertEqual(state['records']['master:first']['status'], 'FAIL')
        self.assertFalse(state['records']['master:first']['coverage_complete'])

    def test_success_receipt_is_not_labelled_infrastructure(self):
        with tempfile.TemporaryDirectory() as d:
            self.attempt(d)
            Path(d, 'journal.jsonl').write_text(json.dumps({'name': 'first', 'status': 'PASS', 'commit': SHA, 'attempted_tests': True}) + '\n')
            with patch.dict(os.environ, {'GITHUB_SERVER_URL': 'https://github.com', 'GITHUB_REPOSITORY': 'o/r', 'GITHUB_RUN_ID': '1'}):
                self.attempt(d)
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

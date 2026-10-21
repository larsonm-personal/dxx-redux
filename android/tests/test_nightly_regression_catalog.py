"""Synthetic discovery/staging controls; no private fixtures or network access."""
import json
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest
from unittest.mock import patch
import nightly_regression_catalog as corpus
import nightly_tooling_sweep as sweep
import nightly_retail as retail


class CorpusCatalogTests(unittest.TestCase):
    def baseline(self, root, name='mission.json'):
        path = root / 'game_data/mission_files' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps([{'mission_filename': 'mine.mn2', 'game': 'd2', 'levels': [{'level_num': 1, 'level_file': 'one.rl2'}]}]))
        return path

    def test_existing_baselines_create_distinct_bounded_typed_owners(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.baseline(root)
            self.baseline(root, 'd2xxl_downloads/mission.json')
            self.baseline(root, 'ignored.simulation.json')
            rows = corpus.policy_suites(root)
            self.assertEqual(len(rows), 4)
            self.assertEqual(len({r['id'] for r in rows}), 4)
            for row in rows:
                self.assertEqual(row['fixture'], 'retail-corpus')
                self.assertEqual(row['args'], {'suite': row['id']})
                self.assertLessEqual(row['estimated_seconds'], 2700)

    def test_nested_variants_remain_pending_without_spending_runner_budget(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = self.baseline(root)
            document = json.loads(path.read_text())
            document[0]["mission_path"] = "missions/nested/mine.mn2"
            path.write_text(json.dumps(document))
            entries = corpus.policy_suites(root)
            self.assertEqual(len(entries), 2)
            policy = {"schema": 1, "suites": entries}
            plan, state = sweep.plan_batch("b" * 40, {"schema": 1, "tests": []}, policy)
            self.assertEqual(plan["tests"], [])
            self.assertEqual(len(plan["unavailable"]), 2)
            self.assertEqual(sum(row.get("blocker_kind") == "target-capability" for row in plan["unavailable"]), 1)
            self.assertIn("unavailable_reason", entries[0])
            self.assertNotIn("unavailable_reason", entries[1])
            self.assertEqual(state["records"], {})
            self.assertEqual(plan["estimated_seconds"], 0)

    def test_stage_requires_exact_reviewed_checkout_and_never_overwrites(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, repo = root / 'assets', root / 'repo'
            self.baseline(repo)
            (source / 'missions').mkdir(parents=True)
            (source / 'missions/mission.ZIP').write_bytes(b'synthetic archive')
            (source / 'missions/unreviewed.zip').write_bytes(b'unrelated')
            with patch.object(corpus.subprocess, 'check_output', side_effect=lambda args, **kw: ('100644 blob '+ 'c' * 40 + '\tmissions/mission.ZIP\0').encode() if 'ls-tree' in args else 'c' * 40 if 'hash-object' in args else corpus.CORPUS_COMMIT):
                with self.assertRaises(ValueError):
                    corpus.stage_archives(source, repo, 'a' * 40)
                self.assertEqual(corpus.stage_archives(source, repo, corpus.CORPUS_COMMIT), 1)
                self.assertFalse((repo / 'game_data/mission_files/unreviewed.zip').exists())
                with self.assertRaises(ValueError):
                    corpus.stage_archives(source, repo, corpus.CORPUS_COMMIT)
            with patch.object(corpus.subprocess, 'check_output', return_value='a' * 40), self.assertRaises(ValueError):
                corpus.stage_archives(source, repo, corpus.CORPUS_COMMIT)

    def test_standalone_helper_import_and_dirty_blob_rejection(self):
        subprocess.run([sys.executable, '-I', '-c',
                        'import sys; sys.path.insert(0, ' + repr(str(Path(corpus.__file__).parent)) +
                        '); import nightly_regression_catalog; import retail_ci_assets'], check=True)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.baseline(root / 'repo')
            source = root / 'assets'
            (source / 'missions').mkdir(parents=True)
            (source / 'missions/mission.zip').write_bytes(b'dirty synthetic bytes')
            def git(args, **kwargs):
                if 'ls-tree' in args:
                    return ('100644 blob ' + 'c' * 40 + '\tmissions/mission.zip\0').encode()
                return 'd' * 40 if 'hash-object' in args else corpus.CORPUS_COMMIT
            with patch.object(corpus.subprocess, 'check_output', side_effect=git), self.assertRaises(ValueError):
                corpus.stage_archives(source, root / 'repo', corpus.CORPUS_COMMIT)
            self.assertFalse((root / 'repo/game_data/mission_files/mission.zip').exists())

    def test_every_level_has_its_own_owner_including_zero_and_negative(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = self.baseline(root)
            document = json.loads(path.read_text())
            document[0]['levels'] = [{'level_num': number, 'level_file': str(number) + '.rl2'} for number in (0, 1, -1)]
            path.write_text(json.dumps(document))
            routes = [row for row in corpus.suites(root) if row['profile'] == 'route']
            self.assertEqual(len(routes), 3)
            self.assertEqual({row['level_num'] for row in routes}, {0, 1, -1})
            self.assertEqual(len({row['id'] for row in routes}), 3)
            path.write_text('malformed')
            with self.assertRaises(ValueError):
                corpus.baselines(root)

    def test_missing_corpus_stays_pending_and_cannot_reuse_old_asset_receipts(self):
        entry = {'id': 'corpus:metadata:abc', 'owner': 'host-adapter', 'lane': 'host',
                 'args': {'suite': 'corpus:metadata:abc'}, 'estimated_seconds': 180, 'fixture': 'retail-corpus'}
        policy = {'schema': 1, 'suites': [entry]}
        catalog = {'schema': 1, 'tests': []}
        identity = {'repository': retail.ASSET_REPOSITORY, 'commit': 'a' * 40,
                    'manifest_sha256': retail.baseline_identity(corpus.ROOT, include_missions=True)}
        plan, state = sweep.plan_batch('b' * 40, catalog, policy, retail_assets=identity)
        self.assertEqual(plan['tests'], [])
        self.assertEqual(len(plan['unavailable']), 1)
        identity['commit'] = corpus.CORPUS_COMMIT
        plan, state = sweep.plan_batch('b' * 40, catalog, policy, state, retail_assets=identity)
        self.assertEqual(plan['tests'], [entry])
        self.assertEqual(state['records'], {})
        sweep.validate_plan(plan, policy)

    def test_same_revision_traverses_every_route_leaf_then_stops(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = self.baseline(root)
            document = json.loads(path.read_text())
            document[0]['levels'] = [{'level_num': number, 'level_file': str(number) + '.rl2'} for number in (0, 1, -1)]
            path.write_text(json.dumps(document))
            policy = {'schema': 1, 'suites': [row for row in corpus.policy_suites(root) if row['id'].startswith('corpus:route:')]}
            catalog = {'schema': 1, 'tests': []}
            identity = {'repository': retail.ASSET_REPOSITORY, 'commit': corpus.CORPUS_COMMIT,
                        'manifest_sha256': retail.baseline_identity(corpus.ROOT, include_missions=True)}
            state = None
            seen = set()
            for index in range(3):
                plan, state = sweep.plan_batch('b' * 40, catalog, policy, state, budget=360, retail_assets=identity)
                self.assertEqual(len(plan['tests']), 1)
                entry = plan['tests'][0]
                self.assertNotIn(entry['id'], seen)
                again, _ = sweep.plan_batch('b' * 40, catalog, policy, state, budget=360, retail_assets=identity)
                self.assertEqual(again['tests'], plan['tests'])
                receipt = {key: plan[key] for key in ('schema', 'target_sha', 'catalog_hash', 'batch_id', 'retail_assets')}
                receipt.update(lane='host', results=[{'id': entry['id'], 'status': 'PASS', 'coverage_complete': True,
                                                     'run_url': 'https://github.com/o/r/actions/runs/' + str(index)}])
                state = sweep.reconcile(plan, policy, state, [receipt])
                seen.add(entry['id'])
            done, _ = sweep.plan_batch('b' * 40, catalog, policy, state, budget=360, retail_assets=identity)
            self.assertEqual(done['tests'], [])
            restarted, fresh = sweep.plan_batch('c' * 40, catalog, policy, state, budget=360, retail_assets=identity)
            self.assertEqual(len(restarted['tests']), 1)
            self.assertEqual(fresh['records'], {})

    def test_private_corpus_rejects_control_target_sha_mismatch(self):
        argv = ['nightly', 'plan', '--target-sha', 'b' * 40, '--catalog', 'unused',
                '--policy', 'unused', '--output', 'unused', '--state-output', 'unused']
        with patch.object(sys, 'argv', argv), patch.object(retail, 'configured_identity',
                return_value={'commit': corpus.CORPUS_COMMIT}), patch.object(
                sweep.subprocess, 'check_output', return_value='a' * 40), patch.object(sweep, 'read') as read:
            with self.assertRaisesRegex(ValueError, 'differs from exact target'):
                sweep.main()
            read.assert_not_called()

    def test_grouped_sample_and_public_basic_use_campaign(self):
        entries = [{'id': 'master:' + name, 'owner': 'master', 'lane': 'host',
                    'args': {'suite': name}, 'estimated_seconds': 100, 'category': category}
                   for category in ('metadata', 'route', 'audio') for name in (category + '1', category + '2')]
        policy = {'schema': 1, 'suites': entries}
        catalog = {'schema': 1, 'tests': [{'name': e['args']['suite']} for e in entries]}
        plan, state = sweep.plan_batch('b' * 40, catalog, policy, budget=300)
        self.assertEqual({e['category'] for e in plan['tests']}, {'metadata', 'route', 'audio'})
        again, _ = sweep.plan_batch('b' * 40, catalog, policy, state, budget=300)
        self.assertEqual(again, plan)
        real = sweep.read(corpus.ROOT / 'android/tests/nightly_tooling_policy.json')
        catalog['tests'] = [{'name': e['args']['suite']} for e in real['suites']]
        plan, _ = sweep.plan_batch('b' * 40, catalog, real)
        basic = sweep.select_public_campaign(plan, real, catalog, 'basic-use')
        self.assertEqual({e['id'] for e in basic['tests']}, sweep.BASIC_USE_IDS)
        self.assertNotIn('retail_assets', basic)
        self.assertTrue(all(not e.get('fixture') for e in basic['tests']))
        for lane in sweep.LANES:
            self.assertLessEqual(sum(e['estimated_seconds'] for e in basic['tests'] if e['lane'] == lane), 2700)


if __name__ == '__main__':
    unittest.main()

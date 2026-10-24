"""Pure planner/reconciliation controls; never invokes PowerShell or GitHub."""

import copy
import io
import json
import os
import subprocess
import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import nightly_tooling_sweep as sweep

SHA = "0000000000000000000000000000000000000008"  # Stable first/second/third fixture ordering.


class SweepTests(unittest.TestCase):
    def test_public_jvm_failure_exposes_bounded_owner_log(self):
        plan = {"tests": [{"id": "master:test_gradle_unit_tests", "lane": "jvm"}]}
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "master").mkdir()
            (root / "master.log").write_text("owner failed")
            (root / "master/test_gradle_unit_tests_1.log").write_text(
                "old line\n" * 10000 + "ExampleTest > assertion FAILED\n::error::untrusted\n##[error]legacy\n"
            )
            (root / "master/private.log").write_text("unrelated private output")
            output = io.StringIO()
            with patch("sys.stdout", output):
                sweep.print_public_jvm_failure(plan, "jvm", root)
            lines = output.getvalue().splitlines()
            self.assertEqual(len(lines), 3)
            payload = json.loads(lines[-1])
            self.assertEqual(len(payload["tail"]), 200)
            self.assertIn("ExampleTest > assertion FAILED", payload["tail"])
            self.assertIn("##[error]legacy", payload["tail"])
            self.assertNotIn("##[", output.getvalue())
            self.assertIn(r"\u0023\u0023[error]legacy", output.getvalue())
            self.assertNotIn("unrelated private output", output.getvalue())
            for lane, private_plan in [("host", plan), ("jvm", {**plan, "retail_assets": {"commit": SHA}})]:
                with patch("sys.stdout", io.StringIO()) as hidden:
                    sweep.print_public_jvm_failure(private_plan, lane, root)
                    self.assertEqual(hidden.getvalue(), "")
            with patch.dict(os.environ, {"RETAIL_REQUESTED": "true"}), patch("sys.stdout", io.StringIO()) as hidden:
                sweep.print_public_jvm_failure(plan, "jvm", root)
                self.assertEqual(hidden.getvalue(), "")

    def test_online_failure_summary_exports_only_phase_and_fixed_category(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "master").mkdir()
            canary = 'secret-canary-short {"send_key":"credential-canary"}'
            owner = root / "master/test_mp_public_shareware_1.log"
            owner.write_text("\x1b[31m--- Phase 3: Connect ---\x1b[0m\nFAIL: " + canary)
            self.assertIn("last-numbered-phase-3, owner-assertion", sweep.online_failure_summary(root))
            owner.write_text(owner.read_text() + "\nEXCEPTION: " + canary)
            result = sweep.online_failure_summary(root)
            self.assertIn("last-numbered-phase-3, owner-exception", result)
            self.assertNotIn("canary", result)
            owner.unlink()
            (root / "master/unrelated.log").write_text(canary)
            self.assertIn("unknown, owner-failure", sweep.online_failure_summary(root))

    def test_online_failed_journal_gains_safe_reason_without_changing_status(self):
        entry = {
            "id": "master:test_mp_public_shareware",
            "owner": "master",
            "lane": "android-dual",
            "args": {"suite": "test_mp_public_shareware"},
            "estimated_seconds": 1800,
        }
        policy = {"schema": 1, "suites": [entry]}
        catalog = {
            "schema": 1,
            "tests": [{"name": "test_mp_public_shareware"}, {"name": "test_mp_public_shareware_d2"}],
        }
        plan, _ = sweep.plan_batch(SHA, catalog, policy, budget=1800)
        plan = sweep.select_public_campaign(plan, policy, catalog, "online-preview")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sweep.write(
                root / "attempt.json",
                {
                    **sweep.attempt_identity(plan, "android-dual"),
                    "processes": {"master": {"completed": True, "exit_code": 1}},
                },
            )
            (root / "journal.jsonl").write_text(
                json.dumps({"name": "test_mp_public_shareware", "commit": SHA, "status": "FAIL", "reason": ""}) + "\n"
            )
            (root / "master").mkdir()
            (root / "master/test_mp_public_shareware_1.log").write_text(
                "--- Phase 8: Wait for game launch ---\nEXCEPTION: secret-canary"
            )
            result = sweep.collect(plan, policy, "android-dual", root)["results"][0]
            self.assertEqual(result["status"], "FAIL")
            self.assertIn("last-numbered-phase-8, owner-exception", result["reason"])
            self.assertNotIn("canary", result["reason"])

    def test_online_native_event_reader_exports_only_fixed_counts(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        body = workflow.split("      - name: Report bounded sanitized evidence", 1)[1].split("  plan:", 1)[0]
        code = body.split("python3 - <<'PYTHON'", 1)[1].rsplit("          PYTHON", 1)[0]
        import textwrap

        code = textwrap.dedent(code)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "evidence/setup").mkdir(parents=True)
            (root / "evidence/results.json").write_text(json.dumps({"target_sha": SHA, "results": []}))
            (root / "evidence/setup/emulator-5554-logcat.log").write_text(
                "refuse_stuff: prompting secret-short-canary\nrefuse_stuff: TIMEOUT secret-short-canary\n"
            )
            result = subprocess.run(
                [sys.executable, "-c", code],
                cwd=root,
                env={**os.environ, "EVIDENCE_KIND": "online"},
                capture_output=True,
                text=True,
                check=True,
            )
            self.assertNotIn("canary", result.stdout)
            row = json.loads(
                next(
                    line.removeprefix("ONLINE_NATIVE_EVENTS ")
                    for line in result.stdout.splitlines()
                    if line.startswith("ONLINE_NATIVE_EVENTS ")
                )
            )
            self.assertEqual(row["retained_tail_counts"]["host_approval_pending"], 1)
            self.assertEqual(row["retained_tail_counts"]["host_approval_timeout"], 1)
            self.assertEqual(row["retained_tail_counts"]["native_identity_rejected"], 0)

    def setUp(self):
        self.policy = {
            "schema": 1,
            "suites": [
                {
                    "id": "master:" + name,
                    "owner": "master",
                    "lane": "host",
                    "args": {"suite": name},
                    "estimated_seconds": 100,
                }
                for name in ("first", "second", "third")
            ],
            "exclusions": [],
        }
        self.catalog = {"schema": 1, "tests": [{"name": x} for x in ("first", "second", "third")]}
        self.plan, self.state = sweep.plan_batch(SHA, self.catalog, self.policy, budget=200)

    def receipt(self, status="PASS", ident="master:first"):
        return {
            **{k: self.plan[k] for k in ("schema", "target_sha", "catalog_hash", "batch_id")},
            "lane": "host",
            "results": [{"id": ident, "status": status, "run_url": "https://github.com/o/r/actions/runs/1"}],
        }

    def attempt(self, output, exit_code=None):
        sweep.write(
            Path(output, "attempt.json"),
            {
                **sweep.attempt_identity(self.plan, "host"),
                "processes": {"master": {"completed": exit_code is not None, "exit_code": exit_code}},
            },
        )

    def test_representative_validation_covers_four_lanes_within_budgets(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        entries = sweep.policy_entries(policy)
        selected = [entries[ident] for ident in sweep.VALIDATION_IDS]
        self.assertEqual(len(selected), 12)
        self.assertEqual({entry["lane"] for entry in selected}, set(sweep.LANES))
        for lane in sweep.LANES:
            self.assertLessEqual(sum(entry["estimated_seconds"] for entry in selected if entry["lane"] == lane), 2700)
        self.assertIn("master:test_lan_launch_preparation_public_shareware", sweep.VALIDATION_IDS)
        self.assertNotIn("master:test_lan_lobby_discovery", sweep.VALIDATION_IDS)
        self.assertIn("master:test_lan_lobby_discovery", entries)
        self.assertEqual(sum(e["estimated_seconds"] for e in selected if e["lane"] == "android-dual"), 1500)
        self.assertTrue(all(e.get("fixture") not in ("retail-base", "retail-missions") for e in selected))
        self.assertIn("master:test_android_music_completion", sweep.VALIDATION_IDS)
        self.assertIn("master:test_launcher_media_controls_generated", sweep.VALIDATION_IDS)
        self.assertIn("python:test_music_spectra.py", sweep.VALIDATION_IDS)
        self.assertIn("shareware:test_auto_level_cheats#d2", sweep.VALIDATION_IDS)

    def test_host_installs_isolated_pinned_spectral_runtime(self):
        root = Path(__file__).resolve().parents[2]
        requirements = (root / "android/tests/music_spectral/requirements.txt").read_text().splitlines()
        self.assertTrue(all("==" in line for line in requirements if line and not line.startswith("#")))
        packages = {line.split("==", 1)[0] for line in requirements}
        self.assertTrue({"numpy", "scipy", "matplotlib", "imageio-ffmpeg", "contourpy"} <= packages)
        workflow = (root / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        self.assertIn('python3 -m venv "$RUNNER_TEMP/dxx-nightly-python"', workflow)
        self.assertNotIn("--system-site-packages", workflow)
        self.assertIn(" -m pip install -r control/android/tests/music_spectral/requirements.txt", workflow)
        self.assertIn('"$RUNNER_TEMP/dxx-nightly-python/bin/python" -m pip check', workflow)

    def test_public_network_recovery_is_bounded_and_not_default_pr_work(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        entries = sweep.policy_entries(policy)
        ident = "master:test_lan_host_migration_public_shareware"
        self.assertNotIn(ident, sweep.VALIDATION_IDS)
        self.assertIn(ident, sweep.FIRST_PLAY_IDS)
        self.assertEqual(entries[ident]["args"], {"suite": "test_lan_host_migration_public_shareware"})
        self.assertEqual(entries[ident]["lane"], "android-dual")
        self.assertNotIn("fixture", entries[ident])
        self.assertEqual(entries["master:test_lan"]["fixture"], "retail-base")
        self.assertEqual(
            sum(entries[i]["estimated_seconds"] for i in sweep.FIRST_PLAY_IDS if entries[i]["lane"] == "android-dual"),
            2400,
        )

    def test_network_paths_trigger_existing_pr_sample(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        paths = workflow.split("    paths:", 1)[1].split("  workflow_dispatch:", 1)[0]
        for expected in (
            "d[12]/main/net*",
            "d[12]/main/multi*",
            "android/app/src/main/cpp/shared/net/**",
            "android/app/src/main/cpp/shared/coop/**",
            "android/app/src/main/java/com/dxxredux/app/lobby/**",
            "android/app/src/main/java/com/dxxredux/app/multiplayer/**",
            "android/tests/test_lan*",
        ):
            self.assertIn('"' + expected + '"', paths)
        self.assertNotIn('"**"', paths)

    def test_first_play_is_fixed_public_and_within_each_lane_budget(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        catalog = {
            "schema": 1,
            "tests": [{"name": e["args"]["suite"]} for e in policy["suites"] if e["owner"] == "master"],
        }
        plan, _ = sweep.plan_batch(SHA, catalog, policy)
        chosen = sweep.select_public_campaign(plan, policy, catalog, "first-play")
        self.assertEqual({e["id"] for e in chosen["tests"]}, sweep.FIRST_PLAY_IDS)
        self.assertNotIn("retail_assets", chosen)
        for lane in sweep.LANES:
            self.assertLessEqual(sum(e["estimated_seconds"] for e in chosen["tests"] if e["lane"] == lane), 2700)
        self.assertTrue(all(e.get("fixture") not in ("retail-base", "retail-missions") for e in chosen["tests"]))

    def test_first_play_rejects_missing_owner_and_retail_reclassification(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        catalog = {
            "schema": 1,
            "tests": [{"name": e["args"]["suite"]} for e in policy["suites"] if e["owner"] == "master"],
        }
        plan, _ = sweep.plan_batch(SHA, catalog, policy)
        with self.assertRaisesRegex(ValueError, "absent"):
            sweep.select_public_campaign(plan, policy, {"tests": []}, "first-play")
        bad = copy.deepcopy(policy)
        next(e for e in bad["suites"] if e["id"] in sweep.FIRST_PLAY_IDS)["fixture"] = "retail-base"
        with self.assertRaisesRegex(ValueError, "asset-free"):
            sweep.select_public_campaign(plan, bad, catalog, "first-play")
        bad["suites"] = [e for e in bad["suites"] if e["id"] not in sweep.FIRST_PLAY_IDS]
        with self.assertRaisesRegex(ValueError, "Missing"):
            sweep.select_public_campaign(plan, bad, catalog, "first-play")

    def test_manual_first_play_requires_selected_receipts_and_exact_target(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        self.assertIn(
            "REQUIRE_SELECTED: ${{ inputs.campaign == 'online-preview' || inputs.campaign == 'online-preview-d2' || github.event_name == 'pull_request' || inputs.campaign == 'first-play' || inputs.campaign == 'graphics-capabilities' || inputs.campaign == 'xl-browser' || inputs.campaign == 'basic-use' }}",
            workflow,
        )
        self.assertIn(
            "(inputs.campaign == 'first-play' || inputs.campaign == 'graphics-capabilities' || inputs.campaign == 'xl-browser' || inputs.campaign == 'basic-use' || inputs.campaign == 'online-preview' || inputs.campaign == 'online-preview-d2') && github.sha",
            workflow,
        )
        self.assertIn(
            "if: ${{ !inputs.evidence_run_id && (github.event.action != 'labeled' || github.event.label.name == 'release-first-play') }}",
            workflow,
        )
        self.assertIn("if: github.event_name == 'workflow_dispatch' && inputs.evidence_run_id != ''", workflow)
        self.assertIn("types: [opened, synchronize, reopened, labeled]", workflow)
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        catalog = {
            "schema": 1,
            "tests": [{"name": e["args"]["suite"]} for e in policy["suites"] if e["owner"] == "master"],
        }
        plan, _ = sweep.plan_batch(SHA, catalog, policy)
        plan = sweep.select_public_campaign(plan, policy, catalog, "first-play")
        state = sweep.fresh_state(plan["target_sha"], plan["catalog_hash"])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, value in (("plan", plan), ("policy", policy), ("state", state)):
                sweep.write(root / name, value)
            argv = [
                "sweep",
                "reconcile",
                "--require-selected",
                "--plan",
                str(root / "plan"),
                "--policy",
                str(root / "policy"),
                "--state",
                str(root / "state"),
                "--output",
                str(root / "out"),
            ]
            with patch.object(sys, "argv", argv), patch("builtins.print"):
                self.assertEqual(sweep.main(), 1)
            self.assertEqual(sweep.read(root / "out")["records"], {})

    def test_xl_browser_campaign_is_exact_and_asset_free(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        catalog = {"schema": 1, "tests": [{"name": "test_xl_feature_browser"}]}
        plan, _ = sweep.plan_batch(SHA, catalog, policy)
        chosen = sweep.select_public_campaign(plan, policy, catalog, "xl-browser")
        self.assertEqual([e["id"] for e in chosen["tests"]], ["master:test_xl_feature_browser"])
        self.assertEqual(chosen["lanes"], ["android-single"])
        self.assertEqual(chosen["estimated_seconds"], 120)
        self.assertNotIn("retail_assets", chosen)

    def test_graphics_campaign_is_exact_public_and_within_lane_budget(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        names = ("test_graphics_capabilities", "test_graphics_context_loss", "test_graphics_msaa_allocation_failure")
        catalog = {"schema": 1, "tests": [{"name": name} for name in names]}
        plan, _ = sweep.plan_batch(SHA, catalog, policy)
        chosen = sweep.select_public_campaign(plan, policy, catalog, "graphics-capabilities")
        expected = {"shareware:" + name + "#" + game for name in names for game in ("d1", "d2")}
        self.assertEqual(sweep.GRAPHICS_CAPABILITIES_IDS, expected)
        self.assertEqual({e["id"] for e in chosen["tests"]}, expected)
        self.assertEqual(chosen["lanes"], ["android-single"])
        self.assertEqual(chosen["estimated_seconds"], 1800)
        self.assertLessEqual(chosen["estimated_seconds"], chosen["budget_seconds"])
        self.assertLessEqual(chosen["budget_seconds"], 2700)
        self.assertNotIn("retail_assets", chosen)
        for entry in chosen["tests"]:
            self.assertEqual(entry["owner"], "shareware")
            self.assertEqual(entry["estimated_seconds"], 300)
            self.assertNotIn("fixture", entry)
            self.assertEqual(entry["id"], "shareware:" + entry["args"]["suite"] + "#" + entry["args"]["game"])

    def test_graphics_campaign_rejects_missing_owner_retail_and_overbudget(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        catalog = {"schema": 1, "tests": [{"name": e["args"]["suite"]} for e in policy["suites"]]}
        plan, _ = sweep.plan_batch(SHA, catalog, policy)
        with self.assertRaisesRegex(ValueError, "absent"):
            sweep.select_public_campaign(plan, policy, {"tests": []}, "graphics-capabilities")
        for fixture in ("retail-base", "retail-missions"):
            bad = copy.deepcopy(policy)
            next(e for e in bad["suites"] if e["id"] in sweep.GRAPHICS_CAPABILITIES_IDS)["fixture"] = fixture
            with self.assertRaisesRegex(ValueError, "asset-free"):
                sweep.select_public_campaign(plan, bad, catalog, "graphics-capabilities")
        bad = copy.deepcopy(policy)
        bad["suites"] = [e for e in bad["suites"] if e["id"] != "shareware:test_graphics_capabilities#d1"]
        with self.assertRaisesRegex(ValueError, "Missing"):
            sweep.select_public_campaign(plan, bad, catalog, "graphics-capabilities")
        bad = copy.deepcopy(policy)
        for entry in bad["suites"]:
            if entry["id"] in sweep.GRAPHICS_CAPABILITIES_IDS:
                entry["estimated_seconds"] = 500
        with self.assertRaisesRegex(ValueError, "lane budget"):
            sweep.select_public_campaign(plan, bad, catalog, "graphics-capabilities")
        with (
            patch.object(sys, "argv", ["sweep", "plan", "--campaign", "shareware:test_death#d1"]),
            patch("sys.stderr"),
            self.assertRaises(SystemExit) as error,
        ):
            sweep.main()
        self.assertEqual(error.exception.code, 2)

    def test_graphics_campaign_discards_old_pass_and_requires_all_fresh_receipts(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        catalog = {"schema": 1, "tests": [{"name": e["args"]["suite"]} for e in policy["suites"]]}
        _, state = sweep.plan_batch(SHA, catalog, policy)
        state["records"] = {
            ident: {"status": "PASS", "run_url": "https://github.com/o/r/actions/runs/1"}
            for ident in sweep.GRAPHICS_CAPABILITIES_IDS
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, value in (("policy", policy), ("catalog", catalog), ("previous", state)):
                sweep.write(root / name, value)
            argv = [
                "sweep",
                "plan",
                "--campaign",
                "graphics-capabilities",
                "--target-sha",
                SHA,
                "--catalog",
                str(root / "catalog"),
                "--policy",
                str(root / "policy"),
                "--state",
                str(root / "previous"),
                "--output",
                str(root / "plan"),
                "--state-output",
                str(root / "state"),
            ]
            with (
                patch.object(sys, "argv", argv),
                patch.object(sweep.retail, "configured_identity", return_value=None),
                patch("builtins.print"),
            ):
                self.assertEqual(sweep.main(), 0)
            plan = sweep.read(root / "plan")
            self.assertEqual(plan["target_sha"], SHA)
            self.assertEqual({e["id"] for e in plan["tests"]}, sweep.GRAPHICS_CAPABILITIES_IDS)
            self.assertEqual(sweep.read(root / "state")["records"], {})
            receipt = {key: plan[key] for key in ("schema", "target_sha", "catalog_hash", "batch_id")}
            receipt["lane"] = "android-single"
            argv = [
                "sweep",
                "reconcile",
                "--require-selected",
                "--plan",
                str(root / "plan"),
                "--policy",
                str(root / "policy"),
                "--state",
                str(root / "state"),
                "--output",
                str(root / "out"),
                "--receipts",
                str(root / "receipt"),
            ]
            for status in (None, "BLOCKED", "NOT_RUN", "RUNNING", "TIMEOUT", "FAIL", "PASS"):
                receipt["results"] = [
                    {"id": e["id"], "status": "PASS", "run_url": "https://github.com/o/r/actions/runs/2"}
                    for e in plan["tests"]
                ]
                if status is None:
                    receipt["results"].pop()
                else:
                    receipt["results"][0]["status"] = status
                sweep.write(root / "receipt", receipt)
                with self.subTest(status=status), patch.object(sys, "argv", argv), patch("builtins.print"):
                    self.assertEqual(sweep.main(), 0 if status == "PASS" else 1)

    def test_lan_smoke_discards_old_pass_and_requires_all_fresh_receipts(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        catalog = {"schema": 1, "tests": [{"name": e["args"]["suite"]} for e in policy["suites"]]}
        _, state = sweep.plan_batch(SHA, catalog, policy)
        state["records"] = {
            ident: {"status": "PASS", "run_url": "https://github.com/o/r/actions/runs/1"}
            for ident in sweep.PUBLIC_LAN_IDS
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, value in (("policy", policy), ("catalog", catalog), ("previous", state)):
                sweep.write(root / name, value)
            argv = [
                "sweep",
                "plan",
                "--campaign",
                "lan-smoke",
                "--target-sha",
                SHA,
                "--catalog",
                str(root / "catalog"),
                "--policy",
                str(root / "policy"),
                "--state",
                str(root / "previous"),
                "--output",
                str(root / "plan"),
                "--state-output",
                str(root / "state"),
            ]
            with (
                patch.object(sys, "argv", argv),
                patch.object(sweep.retail, "configured_identity", return_value=None),
                patch("builtins.print"),
            ):
                self.assertEqual(sweep.main(), 0)
            plan = sweep.read(root / "plan")
            self.assertEqual(plan["target_sha"], SHA)
            self.assertEqual({e["id"] for e in plan["tests"]}, sweep.PUBLIC_LAN_IDS)
            self.assertEqual(sweep.read(root / "state")["records"], {})
            receipt = {key: plan[key] for key in ("schema", "target_sha", "catalog_hash", "batch_id")}
            receipt["lane"] = "android-dual"
            argv = [
                "sweep",
                "reconcile",
                "--require-selected",
                "--plan",
                str(root / "plan"),
                "--policy",
                str(root / "policy"),
                "--state",
                str(root / "state"),
                "--output",
                str(root / "out"),
                "--receipts",
                str(root / "receipt"),
            ]
            for status in (None, "BLOCKED", "NOT_RUN", "RUNNING", "TIMEOUT", "FAIL", "PASS"):
                receipt["results"] = [
                    {"id": e["id"], "status": "PASS", "run_url": "https://github.com/o/r/actions/runs/2"}
                    for e in plan["tests"]
                ]
                if status is None:
                    receipt["results"].pop()
                else:
                    receipt["results"][0]["status"] = status
                sweep.write(root / "receipt", receipt)
                with self.subTest(status=status), patch.object(sys, "argv", argv), patch("builtins.print"):
                    self.assertEqual(sweep.main(), 0 if status == "PASS" else 1)

    def test_extended_pr_omits_only_unconditional_lan(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        catalog = {"schema": 1, "tests": [{"name": e["args"]["suite"]} for e in policy["suites"]]}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, value in (("policy", policy), ("catalog", catalog)):
                sweep.write(root / name, value)
            for selection, expected in (
                (["--validation"], sweep.VALIDATION_IDS),
                (["--campaign", "first-play"], sweep.FIRST_PLAY_IDS),
            ):
                argv = [
                    "sweep",
                    "plan",
                    *selection,
                    "--omit-pr-public-lan",
                    "--target-sha",
                    SHA,
                    "--catalog",
                    str(root / "catalog"),
                    "--policy",
                    str(root / "policy"),
                    "--output",
                    str(root / "plan"),
                    "--state-output",
                    str(root / "state"),
                ]
                with (
                    self.subTest(selection=selection),
                    patch.object(sys, "argv", argv),
                    patch("builtins.print"),
                    patch.object(sweep.retail, "configured_identity", return_value=None),
                ):
                    self.assertEqual(sweep.main(), 0)
                    plan = sweep.read(root / "plan")
                    self.assertEqual({e["id"] for e in plan["tests"]}, expected - sweep.PUBLIC_LAN_IDS)
                    self.assertEqual(set(plan["lanes"]), {e["lane"] for e in plan["tests"]})
                    sweep.validate_plan(plan, policy)

    def test_lan_smoke_rejects_absent_or_private_suite(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        catalog = {"schema": 1, "tests": [{"name": "test_lan_launch_preparation_public_shareware"}]}
        plan, _ = sweep.plan_batch(SHA, catalog, policy)
        chosen = sweep.select_public_campaign(plan, policy, catalog, "lan-smoke")
        self.assertEqual(chosen["lanes"], ["android-dual"])
        self.assertEqual(len(chosen["tests"]), 1)
        self.assertNotIn("retail_assets", chosen)
        with self.assertRaisesRegex(ValueError, "absent"):
            sweep.select_public_campaign(plan, policy, {"tests": []}, "lan-smoke")
        next(e for e in policy["suites"] if e["id"] in sweep.PUBLIC_LAN_IDS)["fixture"] = "retail-base"
        with self.assertRaisesRegex(ValueError, "asset-free"):
            sweep.select_public_campaign(plan, policy, catalog, "lan-smoke")

    def test_online_campaign_rejects_absent_or_private_suite(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        catalog = {
            "schema": 1,
            "tests": [{"name": "test_mp_public_shareware"}, {"name": "test_mp_public_shareware_d2"}],
        }
        plan, _ = sweep.plan_batch(SHA, catalog, policy)
        chosen = sweep.select_public_campaign(plan, policy, catalog, "online-preview")
        self.assertEqual(chosen["lanes"], ["android-dual"])
        self.assertEqual({e["id"] for e in chosen["tests"]}, sweep.PUBLIC_ONLINE_IDS)
        d2 = sweep.select_public_campaign(plan, policy, catalog, "online-preview-d2")
        self.assertEqual({e["id"] for e in d2["tests"]}, sweep.PUBLIC_ONLINE_D2_IDS)
        workflow = (Path(__file__).parents[2] / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        self.assertIn(
            'elif [[ "$ONLINE_PREVIEW_D2" == true ]]; then\n            validation=(--campaign online-preview-d2)',
            workflow,
        )
        self.assertNotIn("retail_assets", chosen)
        with self.assertRaisesRegex(ValueError, "absent"):
            sweep.select_public_campaign(plan, policy, {"tests": []}, "online-preview")
        next(e for e in policy["suites"] if e["id"] in sweep.PUBLIC_ONLINE_IDS)["fixture"] = "retail-base"
        with self.assertRaisesRegex(ValueError, "asset-free"):
            sweep.select_public_campaign(plan, policy, catalog, "online-preview")

    def three_player_plan(self):
        policy = sweep.read(Path(__file__).with_name("nightly_tooling_policy.json"))
        catalog = {"schema": 1, "tests": [{"name": e["args"]["suite"]} for e in policy["suites"]]}
        plan, _ = sweep.plan_batch(SHA, catalog, policy)
        plan = sweep.select_public_campaign(plan, policy, catalog, "lan-three-player")
        return policy, plan, sweep.fresh_state(plan["target_sha"], plan["catalog_hash"])

    def three_player_summary(self):
        return {
            "schema": 1,
            "owner": "test_lan_three_player_startup",
            "revision": SHA,
            "category": "three_player_network",
            "passed": True,
            "coverage_complete": True,
            "results": [{"id": name, "status": "PASS"} for name in sorted(sweep.THREE_PLAYER_CASES)],
        }

    def test_three_player_default_nightly_selection_has_its_own_resource_category(self):
        policy, campaign, _ = self.three_player_plan()
        (entry,) = campaign["tests"]
        self.assertEqual(entry["id"], "master:test_lan_three_player_startup")
        self.assertEqual(sweep.owner_category(entry), "three_player_network")
        self.assertEqual(entry["requires"], "three_emulators")
        self.assertEqual(entry["lane"], "android-dual")
        self.assertEqual(entry["estimated_seconds"], 900)
        self.assertNotIn("fixture", entry)
        self.assertEqual(campaign["lanes"], ["android-dual"])
        self.assertNotIn("retail_assets", campaign)
        catalog = {"schema": 1, "tests": [{"name": e["args"]["suite"]} for e in policy["suites"]]}
        # The ordinary scheduled planner, without any campaign or recovery opt-in, admits it
        for number in range(32):
            plan, _ = sweep.plan_batch(f"{number:040x}", catalog, policy)
            self.assertTrue(sweep.THREE_PLAYER_IDS <= {e["id"] for e in plan["tests"]})
            self.assertFalse((sweep.PUBLIC_ONLINE_IDS | sweep.PUBLIC_ONLINE_D2_IDS) & {e["id"] for e in plan["tests"]})
            sweep.validate_plan(plan, policy)
        for ids in (sweep.VALIDATION_IDS, sweep.FIRST_PLAY_IDS, sweep.PUBLIC_LAN_IDS):
            self.assertFalse(sweep.THREE_PLAYER_IDS & ids)

    def test_three_player_policy_rejects_resource_category_and_budget_drift(self):
        policy, _, _ = self.three_player_plan()
        for key, value in (
            ("requires", None),
            ("requires", "two_emulators"),
            ("category", "network"),
            ("lane", "android-single"),
            ("estimated_seconds", 901),
        ):
            bad = copy.deepcopy(policy)
            next(e for e in bad["suites"] if e["id"] in sweep.THREE_PLAYER_IDS)[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                sweep.policy_entries(bad)

    def test_three_player_public_lane_defers_retail_without_losing_or_starving_it(self):
        full_policy, campaign, _ = self.three_player_plan()
        public = campaign["tests"][0]
        retail = next(e for e in full_policy["suites"] if e["id"] == "master:test_lan")
        host = {
            "id": "master:retail-host",
            "owner": "master",
            "lane": "host",
            "args": {"suite": "retail-host"},
            "estimated_seconds": 100,
            "fixture": "retail-base",
        }
        policy = {"schema": 1, "suites": [public, retail, host], "exclusions": []}
        catalog = {"schema": 1, "tests": [{"name": e["args"]["suite"]} for e in policy["suites"]]}
        observed_groups = set()
        with patch.object(sweep.retail, "validate_identity"):
            for number in range(32):
                sha = f"{number:040x}"
                plan, state = sweep.plan_batch(sha, catalog, policy, retail_assets={"pinned-test-fixture": True})
                selected = {e["id"] for e in plan["tests"]}
                self.assertIn(host["id"], selected)
                self.assertEqual(len(selected & {public["id"], retail["id"]}), 1)
                self.assertEqual(plan["exclusions"], [])
                self.assertEqual(state["excluded"], {})
                self.assertEqual(plan["estimated_seconds"], 1000)
                self.assertEqual(plan["lanes"], ["host", "android-dual"])
                sweep.validate_plan(plan, policy)
                observed_groups.update(selected)
                completed = next(ident for ident in selected if ident != host["id"])
                deferred = retail["id"] if completed == public["id"] else public["id"]
                # A completed group yields the lane to pending work in the next batch
                next_state = copy.deepcopy(state)
                next_state["records"][completed] = {
                    "status": "PASS",
                    "run_url": "https://github.com/o/r/actions/runs/1",
                }
                next_plan, _ = sweep.plan_batch(
                    sha, catalog, policy, next_state, retail_assets={"pinned-test-fixture": True}
                )
                self.assertIn(deferred, {e["id"] for e in next_plan["tests"]})
                # A setup-blocked group also yields to untried work rather than starving it
                next_state = copy.deepcopy(state)
                next_state["retryable"][completed] = {"attempts": 1}
                next_plan, _ = sweep.plan_batch(
                    sha, catalog, policy, next_state, retail_assets={"pinned-test-fixture": True}
                )
                self.assertIn(deferred, {e["id"] for e in next_plan["tests"]})
            self.assertEqual(observed_groups, {public["id"], retail["id"], host["id"]})
            ordinary = {**policy, "suites": [retail, host]}
            plan, _ = sweep.plan_batch(SHA, catalog, ordinary, retail_assets={"pinned-test-fixture": True})
            self.assertEqual({e["id"] for e in plan["tests"]}, {retail["id"], host["id"]})
            mixed = copy.deepcopy(plan)
            mixed["tests"].append(public)
            with self.assertRaisesRegex(ValueError, "asset-free lane"):
                sweep.validate_plan(mixed, policy)

    def test_three_player_campaign_is_exact_and_resets_prior_progress(self):
        policy, plan, state = self.three_player_plan()
        catalog = {"schema": 1, "tests": [{"name": "test_lan_three_player_startup"}]}
        with self.assertRaisesRegex(ValueError, "absent"):
            sweep.select_public_campaign(plan, policy, {"tests": []}, "lan-three-player")
        bad = copy.deepcopy(policy)
        next(e for e in bad["suites"] if e["id"] in sweep.THREE_PLAYER_IDS)["fixture"] = "retail-base"
        with self.assertRaisesRegex(ValueError, "asset-free"):
            sweep.select_public_campaign(plan, bad, catalog, "lan-three-player")
        state["records"] = {
            ident: {"status": "PASS", "run_url": "https://github.com/o/r/actions/runs/1"}
            for ident in sweep.THREE_PLAYER_IDS
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, value in (("policy", policy), ("catalog", catalog), ("previous", state)):
                sweep.write(root / name, value)
            common = [
                "sweep",
                "plan",
                "--target-sha",
                SHA,
                "--catalog",
                str(root / "catalog"),
                "--policy",
                str(root / "policy"),
                "--state",
                str(root / "previous"),
                "--output",
                str(root / "plan"),
                "--state-output",
                str(root / "state"),
            ]
            with (
                patch.object(sys, "argv", common + ["--campaign", "lan-three-player"]),
                patch("builtins.print"),
                patch.object(sweep.retail, "configured_identity", return_value=None),
            ):
                self.assertEqual(sweep.main(), 0)
            self.assertEqual({e["id"] for e in sweep.read(root / "plan")["tests"]}, sweep.THREE_PLAYER_IDS)
            self.assertEqual(sweep.read(root / "state")["records"], {})
            with (
                patch.object(sys, "argv", common + ["--validation"]),
                patch("builtins.print"),
                patch.object(sweep.retail, "configured_identity", return_value=None),
            ):
                self.assertEqual(sweep.main(), 0)
            self.assertFalse(sweep.THREE_PLAYER_IDS & {e["id"] for e in sweep.read(root / "plan")["tests"]})

    def run_three_player_control(self, summary, journal_status="PASS", exit_code=0, stale=False):
        policy, plan, state = self.three_player_plan()
        with tempfile.TemporaryDirectory() as directory:
            # Match the executor's canonical root, including Windows short-name temp paths
            root = Path(directory).resolve()
            output = root / "out"
            source = root / "android/temp/lan-three-player-startup/three-player-summary.json"
            destination = output / "three-player/three-player-summary.json"
            if stale:
                sweep.write(source, self.three_player_summary())
                sweep.write(destination, self.three_player_summary())

            def run(command, *args, **kwargs):
                self.assertFalse(source.exists())
                self.assertFalse(destination.exists())
                self.assertEqual(command[:4], ["pwsh", "-NoProfile", "-File", str(root / "android/run_all_tests.ps1")])
                self.assertEqual(
                    sweep.read(command[command.index("-SelectedTestsPath") + 1]), ["test_lan_three_player_startup"]
                )
                self.assertEqual(command[command.index("-TestBudgetSeconds") + 1], "900")
                self.assertIn("-FullSuite", command)
                self.assertIn("-ExtendedMultiplayer", command)
                self.assertNotIn("-HostOnly", command)
                self.assertNotIn("run_parallel_startup_checks.ps1", " ".join(command))
                if summary is not None:
                    sweep.write(source, summary)
                if journal_status is not None:
                    (output / "journal.jsonl").write_text(
                        json.dumps(
                            {
                                "name": "test_lan_three_player_startup",
                                "commit": SHA,
                                "status": journal_status,
                                "attempted_tests": True,
                                "phase": "tests",
                            }
                        )
                        + "\n"
                    )
                return subprocess.CompletedProcess(command, exit_code)

            with (
                patch.object(sweep.subprocess, "check_output", return_value=SHA),
                patch.object(sweep, "run_command", side_effect=run),
            ):
                execution = sweep.execute(plan, policy, "android-dual", root, output)
            receipt = sweep.collect(plan, policy, "android-dual", output)
            reconciled = sweep.reconcile(plan, policy, state, [receipt])
            for name, value in (("plan", plan), ("policy", policy), ("state", state)):
                sweep.write(output / name, value)
            argv = [
                "sweep",
                "reconcile",
                "--require-selected",
                "--plan",
                str(output / "plan"),
                "--policy",
                str(output / "policy"),
                "--state",
                str(output / "state"),
                "--output",
                str(output / "final"),
                "--receipts",
                str(output / "results.json"),
            ]
            with patch.object(sys, "argv", argv), patch("builtins.print"):
                result = sweep.main()
            return execution, receipt, reconciled, result

    def test_three_player_normal_entrypoint_requires_current_four_case_evidence(self):
        execution, receipt, state, result = self.run_three_player_control(self.three_player_summary(), stale=True)
        self.assertFalse(execution["runner_failed"])
        self.assertEqual(receipt["results"][0]["status"], "PASS")
        self.assertEqual({case["id"] for case in receipt["results"][0]["cases"]}, sweep.THREE_PLAYER_CASES)
        self.assertEqual(state["records"]["master:test_lan_three_player_startup"]["case_counts"]["PASS"], 4)
        self.assertEqual(result, 0)

    def test_three_player_missing_skipped_partial_or_stale_evidence_never_passes(self):
        for status in (None, "SKIP", "NOT_RUN", "BLOCKED", "FAIL"):
            with self.subTest(journal_status=status):
                _, _, _, result = self.run_three_player_control(self.three_player_summary(), journal_status=status)
                self.assertEqual(result, 1)
        for change in (
            "missing",
            "wrong-revision",
            "wrong-owner",
            "wrong-category",
            "missing-case",
            "skip-case",
            "duplicate-case",
            "not-complete",
            "not-passed",
            "failed-case",
        ):
            summary = self.three_player_summary()
            if change == "missing":
                summary = None
            elif change.startswith("wrong-"):
                summary[change.removeprefix("wrong-")] = "wrong"
            elif change == "missing-case":
                summary["results"].pop()
            elif change == "skip-case":
                summary["results"][0]["status"] = "SKIP"
            elif change == "duplicate-case":
                summary["results"][0] = summary["results"][1]
            elif change == "not-complete":
                summary["coverage_complete"] = False
            elif change == "not-passed":
                summary["passed"] = False
            elif change == "failed-case":
                summary["results"][0]["status"] = "FAIL"
            with self.subTest(change=change):
                _, receipt, _, result = self.run_three_player_control(summary, stale=True)
                self.assertEqual(receipt["results"][0]["status"], "FAIL")
                self.assertEqual(result, 1)
        _, receipt, _, result = self.run_three_player_control(self.three_player_summary(), exit_code=17)
        self.assertEqual(receipt["results"][0]["status"], "FAIL")
        self.assertEqual(result, 1)

    def test_three_player_reconcile_independently_requires_complete_named_leaves(self):
        policy, plan, state = self.three_player_plan()
        receipt = {key: plan[key] for key in ("schema", "target_sha", "catalog_hash", "batch_id")}
        receipt.update(
            lane="android-dual",
            results=[
                {
                    "id": "master:test_lan_three_player_startup",
                    "status": "PASS",
                    "run_url": "https://github.com/o/r/actions/runs/2",
                }
            ],
        )
        self.assertEqual(
            sweep.selected_without_pass(plan, sweep.reconcile(plan, policy, state, [receipt])),
            ["master:test_lan_three_player_startup"],
        )
        receipt["results"][0].update(cases=self.three_player_summary()["results"], coverage_complete=True)
        self.assertEqual(sweep.selected_without_pass(plan, sweep.reconcile(plan, policy, state, [receipt])), [])

    def test_three_player_workflow_binding_preserves_light_pr_and_independent_label_run(self):
        root = Path(__file__).parents[2]
        public = (root / ".github/workflows/public-lan-smoke.yml").read_text()
        self.assertIn("types: [opened, synchronize, reopened, labeled]", public)
        self.assertIn("options: [lan-smoke, lan-three-player]", public)
        self.assertIn("default: lan-smoke", public)
        self.assertIn(
            "github.event.action == 'labeled' && github.event.label.name == 'test-three-player-network'", public
        )
        concurrency = public.split("concurrency:", 1)[1].split("jobs:", 1)[0]
        self.assertIn("'lan-three-player' || 'ignored-label') || inputs.campaign || 'lan-smoke'", concurrency)
        self.assertIn("'Require three-player LAN pass' || 'Require public LAN pass'", public)
        # Label-only runs, even failed/skipped plans, cannot replace the ordinary PR check
        self.assertIn(
            "name: ${{ (github.event.action == 'labeled' || inputs.campaign == 'lan-three-player') && 'Require three-player LAN pass'",
            public,
        )
        self.assertIn('--campaign "$CAMPAIGN"', public)
        self.assertIn("'lan-three-player': ['master:test_lan_three_player_startup']", public)
        self.assertIn("--require-selected --receipts", public)
        self.assertIn("assert p['target_sha'] == os.environ['TARGET_SHA']", public)
        self.assertIn("needs.plan.outputs.campaign != 'lan-three-player' && inputs.recovery_checks == true", public)
        self.assertIn("ref: ${{ github.event.pull_request.head.sha || github.sha }}", public)
        android = (root / ".github/workflows/android-nightly-tooling.yml").read_text()
        for requirement in ("public_fixtures", "three_emulators"):
            expression = next(
                line.split(":", 1)[1].strip().rstrip(",")
                for line in android.splitlines()
                if "'" + requirement + "':" in line
            )
            _, plan, _ = self.three_player_plan()
            self.assertTrue(eval(expression, {"selected": plan["tests"]}))
            self.assertFalse(eval(expression, {"selected": []}))
        self.assertIn(
            "validate_plan(plan, json.load(open('control/android/tests/nightly_tooling_policy.json')))", android
        )
        self.assertIn("if plan['target_sha'] != os.environ['TARGET_SHA']:", android)
        boot = android.split("      - name: Prepare isolated headless emulators", 1)[1].split("      - name:", 1)[0]
        self.assertIn("DXX_SELECTED_THREE_EMULATORS: ${{ steps.selection.outputs.three_emulators }}", boot)
        self.assertIn("$env:DXX_SELECTED_THREE_EMULATORS -eq 'true' -or $experimentalThird", boot)
        self.assertEqual(boot.count("Name = 'Nexus5X_Light_3'"), 1)
        self.assertIn("DXX_THIRD_EMULATOR_SERIAL=emulator-5558", boot)
        self.assertIn("$avds.Count", boot)
        self.assertIn("pathlib.Path('results/three-player')", android)
        nightly = (root / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        self.assertIn("  schedule:", nightly)
        self.assertIn("default: nightly", nightly)
        self.assertIn("validation=()", nightly)

    def test_graphics_campaign_is_manual_only_and_never_publishes_progress(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        self.assertIn(
            "options: [nightly, first-play, graphics-capabilities, xl-browser, basic-use, online-preview, online-preview-d2]",
            workflow,
        )
        self.assertIn("default: nightly", workflow)
        self.assertIn(
            "GRAPHICS_CAPABILITIES: ${{ github.event_name == 'workflow_dispatch' && inputs.campaign == 'graphics-capabilities' }}",
            workflow,
        )
        self.assertIn(
            'elif [[ "$GRAPHICS_CAPABILITIES" == true ]]; then\n            validation=(--campaign graphics-capabilities)',
            workflow,
        )
        self.assertNotIn("release-graphics-capabilities", workflow)
        publish = workflow.split("  publish:", 1)[1]
        self.assertIn("inputs.campaign != 'first-play' && inputs.campaign != 'graphics-capabilities'", publish)

    @unittest.skipUnless(sys.platform == "linux", "Hosted receipt discovery runs in the Linux coordinator")
    def test_receipt_discovery_handles_single_and_multiple_artifact_layouts(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        discovery = [line.strip() for line in workflow.splitlines() if "mapfile -t receipts" in line]
        self.assertEqual(len(discovery), 2)
        self.assertEqual(discovery[0], discovery[1])
        for multiple in (False, True):
            with self.subTest(multiple=multiple), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                policy = {"schema": 1, "suites": self.policy["suites"][: 2 if multiple else 1]}
                plan, state = sweep.plan_batch(SHA, self.catalog, policy)
                expected = []
                for i, entry in enumerate(plan["tests"]):
                    # download-artifact v7 flattens a single pattern match, even without merge-multiple
                    relative = f"receipts/nightly-results-{i}/results.json" if multiple else "receipts/results.json"
                    receipt = {key: plan[key] for key in ("schema", "target_sha", "catalog_hash", "batch_id")}
                    receipt.update(
                        lane=entry["lane"],
                        results=[
                            {"id": entry["id"], "status": "PASS", "run_url": "https://github.com/o/r/actions/runs/2"}
                        ],
                    )
                    sweep.write(root / relative, receipt)
                    expected.append(relative)
                for relative in (
                    "receipts/other/results.json",
                    "receipts/nightly-results-host/nested/results.json",
                    "receipts/nightly-results-host/owner.json",
                    "receipts/not-results.json",
                ):
                    sweep.write(root / relative, {"unrelated": True})
                linked = root / "receipts/nightly-results-linked"
                linked.mkdir()
                (linked / "results.json").symlink_to(root / expected[0])
                command = discovery[0] + '\nprintf "%s\\n" "${receipts[@]}"'
                found = subprocess.check_output(["bash", "-c", command], cwd=root, text=True).splitlines()
                self.assertEqual(found, sorted(expected))
                reconciled = sweep.reconcile(plan, policy, state, [sweep.read(root / path) for path in found])
                self.assertEqual(sweep.selected_without_pass(plan, reconciled), [])

    def test_first_play_public_variants_provision_originals_on_both_lanes(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/android-nightly-tooling.yml").read_text()
        fixture_step = workflow.split("      - name: Provision verified public fixtures", 1)[1].split(
            "      - name:", 1
        )[0]
        self.assertIn("if: steps.selection.outputs.public_fixtures == 'true'", fixture_step)
        self.assertNotIn("inputs.lane == 'android-single'", fixture_step)
        self.assertIn("-Game both -Download -RequireArchives", fixture_step)
        for name in (
            "test_demo_import_public_basic_use",
            "test_demo_import_public_first_play",
            "test_lan_launch_preparation_public_shareware",
            "test_lan_host_migration_public_shareware",
        ):
            self.assertIn(name, workflow)
            expression = next(
                line.split(":", 1)[1].strip().rstrip(",")
                for line in workflow.splitlines()
                if "'public_fixtures':" in line
            )
            self.assertTrue(eval(expression, {"selected": [{"owner": "master", "args": {"suite": name}}]}))
        self.assertFalse(eval(expression, {"selected": [{"owner": "master", "args": {"suite": "test_demo_import"}}]}))
        entries = sweep.policy_entries(sweep.read(Path(__file__).with_name("nightly_tooling_policy.json")))
        self.assertEqual(entries["master:test_lan_launch_preparation"]["fixture"], "retail-base")
        self.assertNotIn("fixture", entries["master:test_lan_launch_preparation_public_shareware"])

    def test_only_selected_auto_level_owners_require_native_replay_setup(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/android-nightly-tooling.yml").read_text()
        expression = next(
            line.split(":", 1)[1].strip().rstrip(",") for line in workflow.splitlines() if "'native_replay':" in line
        )
        for owner in ("shareware", "master"):
            for name in ("test_auto_level_cheats", "test_auto_level_demos", "test_death"):
                selected = [{"owner": owner, "args": {"suite": name}}]
                self.assertEqual(
                    eval(expression, {"selected": selected}), owner == "shareware" and name != "test_death"
                )
        self.assertFalse(eval(expression, {"selected": []}))
        setup = workflow.split("      - name: Provision selected auto-level replay prerequisites", 1)[1].split(
            "      - name:", 1
        )[0]
        self.assertIn("if: steps.selection.outputs.native_replay == 'true'", setup)
        self.assertIn("bash android/get_deps/helpers/get_linux_build_prereqs.sh", setup)
        self.assertIn("xvfb xauth libgl1-mesa-dri", setup)
        self.assertIn("timeout-minutes: 15", setup)

    @unittest.skipUnless(sys.platform == "linux", "Native replay setup uses Bash")
    def test_native_replay_setup_builds_only_selected_games_before_emulator_boot(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/android-nightly-tooling.yml").read_text()
        name = "      - name: Build selected native replay executables before test budget"
        self.assertLess(workflow.index(name), workflow.index("      - name: Prepare isolated headless emulators"))
        step = workflow.split(name, 1)[1].split("      - name:", 1)[0]
        script = "\n".join(line[10:] for line in step.split("        run: |\n", 1)[1].splitlines())

        def entry(game, lane="android-single", suite="test_auto_level_cheats"):
            return {"lane": lane, "owner": "shareware", "args": {"game": game, "suite": suite}}

        cases = [
            ([entry("d2"), entry("d1", lane="android-dual"), entry("d1", suite="test_death")], ["d2"]),
            ([entry("d1"), entry("d1-in-d2"), entry("d2")], ["d1", "d2"]),
            ([], None),
        ]
        for selected, expected in cases:
            with self.subTest(expected=expected), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                for path in ("target", "plan", "results/setup"):
                    (root / path).mkdir(parents=True)
                sweep.write(root / "plan/plan.json", {"tests": selected})
                (root / "target/run-linux-build.sh").write_text('printf "%s\\n" "$*" >> ../build-calls\n')
                result = subprocess.run(
                    ["bash", "-c", script],
                    cwd=root / "target",
                    env={**os.environ, "SWEEP_LANE": "android-single"},
                    capture_output=True,
                    text=True,
                )
                if expected is None:
                    self.assertNotEqual(result.returncode, 0)
                    self.assertFalse((root / "build-calls").exists())
                else:
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(
                        (root / "build-calls").read_text().splitlines(),
                        [f"--target {game} --game-only --jobs 2" for game in expected],
                    )

    @unittest.skipUnless(sys.platform == "linux", "Native replay display setup runs in the Linux Android job")
    def test_native_replay_display_wraps_unchanged_executor_only_when_selected(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/android-nightly-tooling.yml").read_text()
        step = workflow.split("      - name: Execute reviewed test selection", 1)[1].split("      - name:", 1)[0]
        self.assertIn("NATIVE_REPLAY: ${{ steps.selection.outputs.native_replay }}", step)
        script = "\n".join(line[10:] for line in step.split("        run: |\n", 1)[1].splitlines())
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            helpers = root / "control/android/helpers"
            helpers.mkdir(parents=True)
            (helpers / "refresh_nightly_kvm_access.sh").write_text("exit 0\n")
            binary = root / "bin"
            binary.mkdir()
            python = binary / "python3"
            python.write_text(
                f"#!{sys.executable}\nimport json, os, sys\n"
                'json.dump({"argv": sys.argv[1:], "env": {k: os.environ.get(k) for k in '
                '("DISPLAY", "LIBGL_ALWAYS_SOFTWARE", "SDL_VIDEODRIVER", "SDL_AUDIODRIVER")}}, '
                'open(os.environ["OBSERVATION"], "w"))\n'
                'sys.exit(int(os.environ.get("EXECUTOR_EXIT", "0")))\n'
            )
            python.chmod(0o755)
            xvfb = binary / "xvfb-run"
            xvfb.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$XVFB_ARGUMENTS"\nshift 3\nDISPLAY=:91 exec "$@"\n')
            xvfb.chmod(0o755)
            for selected in ("false", "true"):
                with self.subTest(native_replay=selected):
                    env = {
                        key: value
                        for key, value in os.environ.items()
                        if key not in ("DISPLAY", "LIBGL_ALWAYS_SOFTWARE", "SDL_VIDEODRIVER", "SDL_AUDIODRIVER")
                    }
                    env.update(
                        PATH=str(binary) + os.pathsep + os.environ["PATH"],
                        NATIVE_REPLAY=selected,
                        SWEEP_LANE="android-single",
                        GITHUB_WORKSPACE=str(root),
                        OBSERVATION=str(root / "observed.json"),
                        XVFB_ARGUMENTS=str(root / "xvfb.txt"),
                    )
                    subprocess.run(["bash", "-c", script], cwd=root, env=env, check=True)
                    failed = subprocess.run(["bash", "-c", script], cwd=root, env={**env, "EXECUTOR_EXIT": "17"})
                    self.assertEqual(failed.returncode, 17)
                    observed = json.loads((root / "observed.json").read_text())
                    self.assertEqual(
                        observed["argv"],
                        [
                            "control/android/tests/nightly_tooling_sweep.py",
                            "execute",
                            "--plan",
                            "plan/plan.json",
                            "--policy",
                            "control/android/tests/nightly_tooling_policy.json",
                            "--lane",
                            "android-single",
                            "--repo",
                            str(root / "target"),
                            "--output",
                            str(root / "results"),
                        ],
                    )
                    self.assertEqual(
                        observed["env"],
                        dict(
                            zip(
                                ("DISPLAY", "LIBGL_ALWAYS_SOFTWARE", "SDL_VIDEODRIVER", "SDL_AUDIODRIVER"),
                                (":91", "1", "x11", "dummy") if selected == "true" else (None,) * 4,
                            )
                        ),
                    )
                    if selected == "true":
                        self.assertEqual(
                            (root / "xvfb.txt").read_text().splitlines()[:3],
                            ["-a", "-s", "-screen 0 1280x720x24 -nolisten tcp"],
                        )
                    else:
                        self.assertFalse((root / "xvfb.txt").exists())

    def test_first_play_evidence_is_public_only_and_keeps_receipt_root(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/android-nightly-tooling.yml").read_text()
        collect = workflow.split("      - name: Collect public first-play diagnostics", 1)[1].split("      - name:", 1)[
            0
        ]
        self.assertIn("if: always() && !inputs.retail_assets", collect)
        self.assertIn(
            "('public-first-play-*', 'lan-launch-preparation-public-shareware', 'lan-host-migration-public-shareware')",
            collect,
        )
        self.assertIn("pathlib.Path('results/first-play')", collect)
        self.assertNotIn("game_data", collect)
        upload = workflow.split("      - name: Upload execution evidence", 1)[1].split("      - name:", 1)[0]
        self.assertIn("path: results/", upload)
        self.assertIn("if: always() && !inputs.retail_assets", upload)

    def test_graphics_failure_collector_is_exact_bounded_and_rejects_links(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/android-nightly-tooling.yml").read_text()
        collect = workflow.split("      - name: Collect public first-play diagnostics", 1)[1].split("      - name:", 1)[
            0
        ]
        code = collect.split("          python3 - <<'PYTHON'\n", 1)[1].rsplit("          PYTHON", 1)[0]
        code = "\n".join(line[10:] for line in code.splitlines())
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            temporary = root / "target/android/temp"
            source = temporary / "public-graphics-lifecycle-d2"
            source.mkdir(parents=True)
            (source / "manifest.json").write_text('{"run_id":"current"}')
            (source / "logcat-crash.txt").write_bytes(b"c" * 262144)
            (source / "logcat-main.txt").write_bytes(b"m" * 262145)
            (source / "unreviewed.txt").write_text("excluded")
            (source / "window.txt").symlink_to(source / "manifest.json")
            (temporary / "public-graphics-lifecycle-d1").symlink_to(source, target_is_directory=True)
            unrelated = temporary / "private-owner"
            unrelated.mkdir()
            (unrelated / "manifest.json").write_text("excluded")
            result = subprocess.run([sys.executable, "-c", code], cwd=root, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            files = {
                p.relative_to(root / "results/first-play").as_posix()
                for p in (root / "results/first-play").rglob("*")
                if p.is_file()
            }
            self.assertEqual(
                files, {"public-graphics-lifecycle-d2/manifest.json", "public-graphics-lifecycle-d2/logcat-crash.txt"}
            )

    def test_graphics_failure_snapshot_precedes_cleanup_without_changing_oracle(self):
        root = Path(__file__).parents[2]
        runner = (root / "android/helpers/run_test.ps1").read_text()
        failure = runner.split("        if (-not $passed) {", 1)[1]
        self.assertLess(failure.index("Save-PublicGraphicsLifecycleFailure"), failure.index("Get-GameIntrospection"))
        self.assertLess(failure.index("Save-PublicGraphicsLifecycleFailure"), failure.index("exit 1"))
        helpers = (root / "android/helpers/test_helpers.ps1").read_text()
        probe = helpers.split("                    $processDiagnostic = $null", 1)[1].split("# Native UI state", 1)[0]
        self.assertIn('$observedProcess -ne "$($graphicsBefore.attempt.owner_pid)"', probe)
        self.assertIn("-DiagnosticResult ([ref]$processDiagnostic)", probe)
        self.assertIn("same-process survival could not be verified", probe)
        self.assertIn("return $false", probe)
        self.assertIn("-Game $gameId -Initialize", runner)
        smoke = (root / ".github/workflows/engine-smoke.yml").read_text()
        self.assertIn("python3 android/tests/test_android_audio_lifecycle.py", smoke)
        tooling = (root / "android/tests/run_tooling_smoke.ps1").read_text()
        self.assertIn("'test_automation_result_evidence'", tooling)
        snapshot = helpers.split("function Save-PublicGraphicsLifecycleFailure {", 1)[1].split(
            "function Wait-ProcessDead", 1
        )[0]
        for restriction in (
            "$DataEdition -cne 'shareware'",
            "$ownerPath",
            "'^emulator-[0-9]+$'",
            "$script:PACKAGE -cne 'com.dxxredux.app'",
            "-Seconds 3",
            "-lt 45",
            "[Math]::Min($bytes.Length, 262144)",
            "Select-Object -First 2",
        ):
            self.assertIn(restriction, snapshot)
        for forbidden in ("bugreport", "/data/tombstones", "root", "grant", "logcat -c"):
            self.assertNotIn(forbidden, snapshot)

    def test_planning_does_not_advance_and_repeats_identically(self):
        old = copy.deepcopy(self.state)
        again, _ = sweep.plan_batch(SHA, self.catalog, self.policy, self.state, 200)
        self.assertEqual(again, self.plan)
        self.assertEqual(old, self.state)

    def test_sha_seed_changes_order_without_reusing_certification(self):
        full, _ = sweep.plan_batch(SHA, self.catalog, self.policy, budget=300)
        certified = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt()])
        self.assertIn("master:first", certified["records"])
        other, state = sweep.plan_batch("b" * 40, self.catalog, self.policy, certified, 300)
        self.assertNotEqual([e["id"] for e in full["tests"]], [e["id"] for e in other["tests"]])
        self.assertEqual(state["records"], {})
        self.assertEqual(other, sweep.plan_batch("b" * 40, self.catalog, self.policy, budget=300)[0])

    def test_seeded_sweep_resume_is_finite_and_never_repeats_terminal(self):
        state = self.state
        seen = []
        while True:
            plan, state = sweep.plan_batch(SHA, self.catalog, self.policy, state, 100)
            if not plan["tests"]:
                break
            ident = plan["tests"][0]["id"]
            self.assertNotIn(ident, seen)
            seen.append(ident)
            receipt = {
                **{k: plan[k] for k in ("schema", "target_sha", "catalog_hash", "batch_id")},
                "lane": "host",
                "results": [{"id": ident, "status": "PASS", "run_url": "https://github.com/o/r/actions/runs/1"}],
            }
            state = sweep.reconcile(plan, self.policy, state, [receipt])
        self.assertEqual(set(seen), set(sweep.policy_entries(self.policy)))

    def test_each_of_four_lanes_has_its_own_bounded_budget(self):
        policy = {"schema": 1, "suites": []}
        catalog = {"schema": 1, "tests": []}
        for lane in sweep.LANES:
            for i in range(2):
                name = lane.replace("-", "_") + str(i)
                policy["suites"].append(
                    {
                        "id": "master:" + name,
                        "owner": "master",
                        "lane": lane,
                        "args": {"suite": name},
                        "estimated_seconds": 100,
                    }
                )
                catalog["tests"].append({"name": name})
        plan, _ = sweep.plan_batch(SHA, catalog, policy, budget=100)
        self.assertEqual(len(plan["tests"]), 4)
        self.assertEqual(plan["estimated_seconds"], 400)
        self.assertEqual(plan["budget_scope"], "per-lane")
        sweep.validate_plan(plan, policy)
        for lane in sweep.LANES:
            self.assertEqual(sum(e["estimated_seconds"] for e in plan["tests"] if e["lane"] == lane), 100)
        extra = next(e for e in policy["suites"] if e["lane"] == "host" and e not in plan["tests"])
        plan["tests"].append(extra)
        plan["batch_id"] = sweep.digest({k: plan[k] for k in ("target_sha", "catalog_hash", "tests")})
        with self.assertRaises(ValueError):
            sweep.validate_plan(plan, policy)

    def test_partial_interruption_advances_only_completed(self):
        state = sweep.reconcile(
            self.plan, self.policy, self.state, [self.receipt(), self.receipt("RUNNING", "master:second")]
        )
        plan, _ = sweep.plan_batch(SHA, self.catalog, self.policy, state, 200)
        self.assertEqual([e["id"] for e in plan["tests"]], ["master:second", "master:third"])

    def test_failures_are_completed_but_remain_failed(self):
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt("FAIL")])
        self.assertEqual(state["records"]["master:first"]["status"], "FAIL")
        self.assertNotIn(
            "master:first", [e["id"] for e in sweep.plan_batch(SHA, self.catalog, self.policy, state)[0]["tests"]]
        )

    def test_test_timeout_is_failed_attempt_not_success(self):
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt("TIMEOUT")])
        self.assertEqual(state["records"]["master:first"]["status"], "TIMEOUT")

    def test_transient_blockage_retries_without_coverage(self):
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt("BLOCKED")])
        self.assertNotIn("master:first", state["records"])
        self.assertEqual(state["retryable"]["master:first"]["attempts"], 1)
        self.assertIn(
            "master:first", [e["id"] for e in sweep.plan_batch(SHA, self.catalog, self.policy, state)[0]["tests"]]
        )
        self.assertEqual(sweep.reconcile(self.plan, self.policy, state, [self.receipt("BLOCKED")]), state)

    def test_all_done_has_no_wrap(self):
        plan, state = sweep.plan_batch(SHA, self.catalog, self.policy)
        receipts = []
        for entry in plan["tests"]:
            r = self.receipt(ident=entry["id"])
            r["batch_id"] = plan["batch_id"]
            receipts.append(r)
        state = sweep.reconcile(plan, self.policy, state, receipts)
        self.assertEqual(sweep.plan_batch(SHA, self.catalog, self.policy, state)[0]["tests"], [])

    def test_new_sha_starts_fresh(self):
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt()])
        plan, new = sweep.plan_batch("b" * 40, self.catalog, self.policy, state)
        self.assertEqual(len(plan["tests"]), 3)
        self.assertEqual(new["records"], {})

    def test_changed_catalog_reconsiders_blockages(self):
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt("BLOCKED")])
        policy = copy.deepcopy(self.policy)
        policy["suites"][0]["estimated_seconds"] = 101
        self.assertEqual(sweep.plan_batch(SHA, self.catalog, policy, state)[1]["records"], {})

    def test_malformed_ledger_never_silently_resets(self):
        for bad in ({}, {"schema": 9}, {**self.state, "records": {"x": {"status": "PASS"}}}):
            with self.assertRaises(ValueError):
                sweep.plan_batch(SHA, self.catalog, self.policy, bad)

    def test_mismatched_revision_catalog_and_batch_rejected(self):
        for key in ("target_sha", "catalog_hash", "batch_id"):
            receipt = self.receipt()
            receipt[key] = "bad"
            with self.assertRaises(ValueError):
                sweep.reconcile(self.plan, self.policy, self.state, [receipt])

    def test_unknown_and_wrong_lane_results_rejected(self):
        for receipt in (self.receipt(ident="unknown"), {**self.receipt(), "lane": "jvm"}):
            with self.assertRaises(ValueError):
                sweep.reconcile(self.plan, self.policy, self.state, [receipt])

    def test_conflicting_duplicates_rejected_identical_idempotent(self):
        with self.assertRaises(ValueError):
            sweep.reconcile(self.plan, self.policy, self.state, [self.receipt(), self.receipt("FAIL")])
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt(), self.receipt()])
        self.assertEqual(sweep.reconcile(self.plan, self.policy, state, [self.receipt()]), state)

    def test_plan_cannot_inject_commands_or_unknown_owner(self):
        for mutate in (
            lambda p: p["tests"][0]["args"].update(command="rm -rf /"),
            lambda p: p["tests"][0].update(owner="shell"),
        ):
            plan = copy.deepcopy(self.plan)
            mutate(plan)
            with self.assertRaises(ValueError):
                sweep.validate_plan(plan, self.policy)

    def test_oversized_owner_rejected_instead_of_starving(self):
        self.policy["suites"][0]["estimated_seconds"] = 2701
        with self.assertRaises(ValueError):
            sweep.plan_batch(SHA, self.catalog, self.policy)

    def test_missing_journal_remains_pending(self):
        with tempfile.TemporaryDirectory() as d:
            result = sweep.collect(self.plan, self.policy, "host", d)
            self.assertTrue(all(r["status"] == "NOT_RUN" for r in result["results"]))

    def test_adapter_diagnostics_survive_collection_and_interruption(self):
        entry = {
            "id": "corpus:metadata:fixture",
            "owner": "host-adapter",
            "lane": "host",
            "args": {"suite": "corpus:metadata:fixture"},
            "estimated_seconds": 10,
        }
        policy = {"schema": 1, "suites": [entry]}
        plan, state = sweep.plan_batch(SHA, {"schema": 1, "tests": []}, policy)
        for completed in (True, False):
            with self.subTest(completed=completed), tempfile.TemporaryDirectory() as directory:
                output = Path(directory)
                sweep.write(
                    output / "attempt.json",
                    {
                        **sweep.attempt_identity(plan, "host"),
                        "processes": {entry["id"]: {"completed": completed, "exit_code": 1 if completed else None}},
                    },
                )
                sweep.write(
                    output / "corpus_metadata_fixture.json",
                    {
                        "schema": 1,
                        "suite": entry["id"],
                        "status": "FAIL",
                        "exit_code": 7,
                        "reason": "Full projected metadata differs from committed baseline",
                        "commands": [{"command": "pwsh", "outcome": "completed", "exit_code": 7}],
                        "mismatches": [{"path": [0, "levels", 1, "segment_count"], "kind": "value"}],
                    },
                )
                receipt = sweep.collect(plan, policy, "host", output)
                row = receipt["results"][0]
                self.assertEqual(row["exit_code"], 7)
                self.assertEqual(row["commands"][0]["exit_code"], 7)
                self.assertEqual(row["mismatches"][0]["path"], [0, "levels", 1, "segment_count"])
                self.assertEqual(row["status"], "FAIL" if completed else "NOT_RUN")
                if not completed:
                    self.assertEqual(sweep.reconcile(plan, policy, state, [receipt])["records"], {})

    def test_failure_summary_preserves_inner_worker_categories(self):
        receipt = self.receipt()
        row = receipt["results"][0]
        row.update(status="FAIL", workers=[{"categories": ["assertion"], "exit_code": -6}])
        state = sweep.reconcile(self.plan, self.policy, self.state, [receipt])
        self.assertEqual(state["records"][row["id"]]["failure_categories"], ["assertion"])
        self.assertEqual(state["records"][row["id"]]["worker_exit_codes"], [-6])

    def test_interrupted_inner_worker_records_are_collected_without_coverage(self):
        entry = {
            "id": "corpus:metadata:fixture",
            "owner": "host-adapter",
            "lane": "host",
            "args": {"suite": "corpus:metadata:fixture"},
            "estimated_seconds": 10,
        }
        policy = {"schema": 1, "suites": [entry]}
        plan, state = sweep.plan_batch(SHA, {"schema": 1, "tests": []}, policy)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            worker_id = "a" * 32
            sweep.write(
                output / "attempt.json",
                {
                    **sweep.attempt_identity(plan, "host"),
                    "processes": {entry["id"]: {"completed": False, "exit_code": None}},
                },
            )
            sweep.write(
                output / "corpus_metadata_fixture.json",
                {"schema": 1, "suite": entry["id"], "status": "BLOCKED", "worker_diagnostics_id": worker_id},
            )
            workers = output / "corpus_metadata_fixture.artifacts" / ("metadata-workers-" + worker_id)
            sweep.write(workers / "123.start.json", {"executable": "PRIVATE_PATH"})
            receipt = sweep.collect(plan, policy, "host", output)
            row = receipt["results"][0]
            self.assertEqual(row["status"], "NOT_RUN")
            self.assertTrue(row["workers"][0]["shutdown_missing"])
            self.assertNotIn("PRIVATE", json.dumps(receipt))
            self.assertEqual(sweep.reconcile(plan, policy, state, [receipt])["records"], {})

    def test_failed_master_without_receipts_is_pending_with_diagnostic(self):
        with tempfile.TemporaryDirectory() as d:
            self.attempt(d, 1)
            result = sweep.collect(self.plan, self.policy, "host", d)
            self.assertTrue(all(row["status"] == "NOT_RUN" for row in result["results"]))
            self.assertTrue(all("Master runner exited 1" in row["reason"] for row in result["results"]))
            self.assertTrue(all(not row["attempted_tests"] for row in result["results"]))
            self.assertEqual(sweep.reconcile(self.plan, self.policy, self.state, [result])["records"], {})

    def test_shareware_refresh_precedes_each_owner_and_failure_stays_pending(self):
        policy = {
            "schema": 1,
            "suites": [
                {
                    "id": "shareware:test_death#" + game,
                    "owner": "shareware",
                    "lane": "android-single",
                    "args": {"suite": "test_death", "game": game},
                    "estimated_seconds": 100,
                }
                for game in ("d1", "d2")
            ],
            "exclusions": [],
        }
        plan, state = sweep.plan_batch(SHA, {"schema": 1, "tests": []}, policy, budget=200)
        for failure in (False, True, "timeout"):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as d:
                calls = []

                def run(command, *args, **kwargs):
                    calls.append(command)
                    if command[0] == "bash" and failure == "timeout":
                        raise subprocess.TimeoutExpired(command, 60)
                    return subprocess.CompletedProcess(command, 1 if command[0] == "bash" and failure else 0)

                with (
                    patch.dict(os.environ, {"GITHUB_ACTIONS": "true"}),
                    patch.object(sweep.subprocess, "check_output", return_value=SHA),
                    patch.object(sweep, "run_command", side_effect=run),
                ):
                    result = sweep.execute(plan, policy, "android-single", d, Path(d, "out"))
                self.assertEqual(
                    [c[0] for c in calls], ["bash", "bash"] if failure else ["bash", "pwsh", "bash", "pwsh"]
                )
                self.assertTrue(all(c[1].endswith("refresh_nightly_kvm_access.sh") for c in calls if c[0] == "bash"))
                receipt = sweep.collect(plan, policy, "android-single", Path(d, "out"))
                if failure:
                    self.assertTrue(result["runner_failed"])
                    self.assertTrue(
                        all(
                            r["status"] == "BLOCKED" and r["retryable"] and not r["attempted_tests"]
                            for r in receipt["results"]
                        )
                    )
                    self.assertEqual(sweep.reconcile(plan, policy, state, [receipt])["records"], {})
                else:
                    self.assertTrue(all(r["status"] == "PASS" for r in receipt["results"]))

    def test_master_child_exit_is_not_replaced_by_failing_sibling_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            self.attempt(directory, 1)
            Path(directory, "journal.jsonl").write_text(
                "\n".join(
                    json.dumps(row)
                    for row in (
                        {"name": "first", "commit": SHA, "status": "PASS", "exit_code": 0},
                        {"name": "second", "commit": SHA, "status": "FAIL", "exit_code": 7},
                    )
                )
            )
            rows = sweep.collect(self.plan, self.policy, "host", directory)["results"]
            self.assertEqual([(r["status"], r["exit_code"]) for r in rows], [("PASS", 0), ("FAIL", 7)])
            with patch.object(sweep.retail, "master_failure_diagnostics", side_effect=OSError("private path")):
                failed_read = sweep.collect(self.plan, self.policy, "host", directory)["results"]
            self.assertEqual([(r["status"], r["exit_code"]) for r in failed_read], [("PASS", 0), ("FAIL", 7)])
            self.assertNotIn("private path", str(failed_read))

    def test_nonzero_master_exit_fails_lane_even_without_test_failures(self):
        argv = [
            "sweep",
            "execute",
            "--plan",
            "plan",
            "--policy",
            "policy",
            "--repo",
            "repo",
            "--output",
            "out",
            "--lane",
            "host",
        ]
        with (
            patch.object(sys, "argv", argv),
            patch.object(sweep, "read", return_value={}),
            patch.object(sweep, "execute", return_value={"runner_failed": True, "results": []}),
        ):
            self.assertEqual(sweep.main(), 1)

    def test_zero_runner_missing_required_receipts_fails_only_pr_validation(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            for name, value in (("plan", self.plan), ("policy", self.policy), ("state", self.state)):
                sweep.write(root / name, value)
            self.attempt(root, 0)
            receipt = sweep.collect(self.plan, self.policy, "host", root)
            self.assertTrue(all(row["status"] == "NOT_RUN" for row in receipt["results"]))
            argv = [
                "sweep",
                "reconcile",
                "--plan",
                str(root / "plan"),
                "--policy",
                str(root / "policy"),
                "--state",
                str(root / "state"),
                "--output",
                str(root / "out"),
                "--receipts",
                str(root / "results.json"),
            ]
            with patch.object(sys, "argv", argv), patch("builtins.print"):
                self.assertEqual(sweep.main(), 0)
            with patch.object(sys, "argv", argv + ["--require-selected"]), patch("builtins.print"):
                self.assertEqual(sweep.main(), 1)
            self.assertEqual(sweep.read(root / "out")["records"], {})

    def test_required_selection_rejects_missing_blocked_and_failed_receipts(self):
        self.assertEqual(set(sweep.selected_without_pass(self.plan, self.state)), {e["id"] for e in self.plan["tests"]})
        for status in ("FAIL", "BLOCKED", "TIMEOUT"):
            state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt(status)])
            self.assertIn("master:first", sweep.selected_without_pass(self.plan, state))
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt()])
        self.assertEqual(sweep.selected_without_pass(self.plan, state), ["master:second"])
        state = sweep.reconcile(self.plan, self.policy, state, [self.receipt(ident="master:second")])
        self.assertEqual(sweep.selected_without_pass(self.plan, state), [])

    def test_partial_journal_retains_terminal_and_ignores_torn_append(self):
        with tempfile.TemporaryDirectory() as d:
            self.attempt(d)
            Path(d, "journal.jsonl").write_text(
                json.dumps({"name": "first", "commit": SHA, "status": "PASS"}) + '\n{"name":'
            )
            result = sweep.collect(self.plan, self.policy, "host", d)
            self.assertEqual([r["status"] for r in result["results"]], ["PASS", "NOT_RUN"])

    def run_owner_control(self, owner, rows, exit_code=0, stale=False):
        entry = {
            "id": owner + ":probe",
            "owner": owner,
            "lane": "host",
            "args": {"suite": "probe"},
            "estimated_seconds": 100,
        }
        policy = {"schema": 1, "suites": [entry], "exclusions": []}
        plan, state = sweep.plan_batch(SHA, {"schema": 1, "tests": [{"name": "probe"}]}, policy)
        with tempfile.TemporaryDirectory() as d:
            output = Path(d, "out")
            output.mkdir()
            destination = output / ("journal.jsonl" if owner == "master" else owner + "_probe.json")

            def emit():
                if rows is None:
                    return
                if owner == "master":
                    destination.write_text(
                        "".join(json.dumps({"name": "probe", "commit": SHA, **row}) + "\n" for row in rows)
                    )
                else:
                    sweep.write(destination, {"schema": 1, "suite": "probe", **rows[0]})

            if stale:
                emit()
                sweep.write(output / "results.json", {"stale": True})
                sweep.write(
                    output / "attempt.json",
                    {
                        **sweep.attempt_identity(plan, "host"),
                        "processes": {owner if owner == "master" else entry["id"]: {"completed": True, "exit_code": 0}},
                    },
                )

            def run(*args, **kwargs):
                self.assertFalse((output / "results.json").exists())
                if not stale:
                    emit()
                return subprocess.run([sys.executable, "-c", "raise SystemExit(" + str(exit_code) + ")"], check=False)

            with (
                patch.object(sweep.subprocess, "check_output", return_value=SHA),
                patch.object(sweep, "run_command", side_effect=run),
            ):
                execution = sweep.execute(plan, policy, "host", d, output)
            receipt = sweep.collect(plan, policy, "host", output)
            result = sweep.reconcile(plan, policy, state, [receipt])
            for name, value in (("plan", plan), ("policy", policy), ("state", state)):
                sweep.write(output / name, value)
            argv = [
                "sweep",
                "reconcile",
                "--plan",
                str(output / "plan"),
                "--policy",
                str(output / "policy"),
                "--state",
                str(output / "state"),
                "--output",
                str(output / "final"),
                "--receipts",
                str(output / "results.json"),
                "--require-selected",
            ]
            with patch.object(sys, "argv", argv), patch("builtins.print"):
                final_exit = sweep.main()
            return execution, receipt["results"][0], result, final_exit

    def test_real_nonzero_process_cannot_certify_pass(self):
        for owner in ("master", "host-adapter"):
            with self.subTest(owner=owner):
                execution, row, state, final_exit = self.run_owner_control(owner, [{"status": "PASS"}], 17)
                self.assertTrue(execution["runner_failed"])
                self.assertEqual(row["status"], "FAIL")
                self.assertEqual(state["records"][owner + ":probe"]["status"], "FAIL")
                self.assertEqual(final_exit, 1)

    def test_previous_raw_receipts_are_cleared_before_launch(self):
        for owner in ("master", "host-adapter"):
            with self.subTest(owner=owner):
                _, row, state, final_exit = self.run_owner_control(owner, [{"status": "PASS"}], 17, stale=True)
                self.assertEqual(row["status"], "NOT_RUN")
                self.assertEqual(state["records"], {})
                self.assertEqual(final_exit, 1)

    def test_zero_host_exit_without_receipt_is_pending(self):
        execution, row, state, final_exit = self.run_owner_control("host-adapter", None)
        self.assertFalse(execution["runner_failed"])
        self.assertEqual(row["status"], "NOT_RUN")
        self.assertEqual(state["records"], {})
        self.assertEqual(final_exit, 1)

    def test_conflicting_terminal_journal_cannot_be_overwritten(self):
        with self.assertRaisesRegex(ValueError, "Conflicting terminal"):
            self.run_owner_control("master", [{"status": "FAIL"}, {"status": "PASS"}])
        _, row, _, final_exit = self.run_owner_control("master", [{"status": "RUNNING"}, {"status": "PASS"}])
        self.assertEqual(row["status"], "PASS")
        self.assertEqual(final_exit, 0)

    def test_structured_contradictions_and_no_pass_stay_red(self):
        for detail, expected in (
            ({"attempted_tests": False}, "BLOCKED"),
            ({"phase": "preflight"}, "BLOCKED"),
            ({"exit_code": 17}, "FAIL"),
            ({"cases": [{"status": "FAIL"}]}, "FAIL"),
            ({"cases": [{"status": "SKIP"}]}, "BLOCKED"),
            ({"cases": [{"status": "BLOCKED"}]}, "BLOCKED"),
            ({"counts": {"total": 0, "passed": 0, "failed": 0}}, "BLOCKED"),
            ({"counts": {"passed": 1, "failed": 1}}, "FAIL"),
        ):
            with self.subTest(detail=detail):
                _, row, _, final_exit = self.run_owner_control("host-adapter", [{"status": "PASS", **detail}])
                self.assertEqual(row["status"], expected)
                self.assertEqual(final_exit, 1)
                receipt = self.receipt()
                receipt["results"][0].update(detail)
                state = sweep.reconcile(self.plan, self.policy, self.state, [receipt])
                self.assertIn("master:first", sweep.selected_without_pass(self.plan, state))

    def test_real_partial_pass_and_unknown_script_coverage_remain_valid(self):
        for detail in ({"cases": [{"status": "PASS"}] * 54 + [{"status": "SKIP"}] * 3}, {}):
            _, row, state, final_exit = self.run_owner_control(
                "host-adapter", [{"status": "PASS", "coverage_complete": False, **detail}]
            )
            self.assertEqual(row["status"], "PASS")
            record = state["records"]["host-adapter:probe"]
            self.assertFalse(record["coverage_complete"])
            self.assertEqual(record["case_counts"]["PASS"], 54 if detail else 0)
            self.assertNotIn("host-adapter:probe", state["retryable"])
            self.assertEqual(final_exit, 0)

    def test_unbound_journal_never_certifies(self):
        with tempfile.TemporaryDirectory() as d:
            Path(d, "journal.jsonl").write_text(json.dumps({"name": "first", "commit": SHA, "status": "PASS"}) + "\n")
            self.assertTrue(
                all(row["status"] == "NOT_RUN" for row in sweep.collect(self.plan, self.policy, "host", d)["results"])
            )

    def test_attempt_must_match_batch_lane_and_github_run(self):
        with tempfile.TemporaryDirectory() as d:
            for key in ("target_sha", "catalog_hash", "batch_id", "lane", "run_id", "run_attempt", "repository"):
                with self.subTest(key=key):
                    self.attempt(d)
                    attempt = sweep.read(Path(d, "attempt.json"))
                    attempt[key] = "wrong"
                    sweep.write(Path(d, "attempt.json"), attempt)
                    with self.assertRaisesRegex(ValueError, "attempt provenance"):
                        sweep.collect(self.plan, self.policy, "host", d)

    def test_no_receipts_no_progress(self):
        self.assertEqual(sweep.reconcile(self.plan, self.policy, self.state, []), self.state)

    def test_unknown_catalog_suites_are_visible_exclusions(self):
        self.catalog["tests"].append({"name": "retail-only", "requires": "emulator"})
        plan, _ = sweep.plan_batch(SHA, self.catalog, self.policy)
        self.assertIn("retail-only", [e["id"] for e in plan["exclusions"]])

    def test_retryable_infrastructure_does_not_starve_untried(self):
        state = sweep.reconcile(self.plan, self.policy, self.state, [self.receipt("BLOCKED")])
        plan, _ = sweep.plan_batch(SHA, self.catalog, self.policy, state, 100)
        self.assertEqual(plan["tests"][0]["id"], "master:second")

    def test_prelaunch_failure_is_retryable_not_test_failure(self):
        with tempfile.TemporaryDirectory() as d:
            self.attempt(d)
            Path(d, "journal.jsonl").write_text(
                json.dumps({"name": "first", "commit": SHA, "status": "FAIL", "attempted_tests": False}) + "\n"
            )
            result = sweep.collect(self.plan, self.policy, "host", d)
            self.assertEqual(result["results"][0]["status"], "BLOCKED")
            self.assertFalse(result["results"][0]["attempted_tests"])

    def test_stable_target_capability_is_exclusion_not_coverage(self):
        receipt = self.receipt("BLOCKED")
        receipt["results"][0].update(
            retryable=False, blocker_kind="target-capability", reason="suite absent in immutable target"
        )
        state = sweep.reconcile(self.plan, self.policy, self.state, [receipt])
        self.assertEqual(state["records"], {})
        self.assertIn("master:first", state["excluded"])
        self.assertNotIn(
            "master:first", [e["id"] for e in sweep.plan_batch(SHA, self.catalog, self.policy, state)[0]["tests"]]
        )

    def test_large_leaf_tracebacks_stay_out_of_permanent_ledger(self):
        receipt = self.receipt()
        receipt["results"][0].update(
            cases=[{"id": "leaf", "status": "FAIL", "reason": "x" * 100000}], coverage_complete=False
        )
        state = sweep.reconcile(self.plan, self.policy, self.state, [receipt])
        self.assertLess(len(json.dumps(state)), 2000)
        self.assertEqual(state["records"]["master:first"]["case_counts"]["FAIL"], 1)
        self.assertEqual(state["records"]["master:first"]["status"], "FAIL")
        self.assertFalse(state["records"]["master:first"]["coverage_complete"])

    def test_success_receipt_is_not_labelled_infrastructure(self):
        with tempfile.TemporaryDirectory() as d:
            self.attempt(d)
            Path(d, "journal.jsonl").write_text(
                json.dumps({"name": "first", "status": "PASS", "commit": SHA, "attempted_tests": True}) + "\n"
            )
            with patch.dict(
                os.environ,
                {"GITHUB_SERVER_URL": "https://github.com", "GITHUB_REPOSITORY": "o/r", "GITHUB_RUN_ID": "1"},
            ):
                self.attempt(d)
                result = sweep.collect(self.plan, self.policy, "host", d)
            row = result["results"][0]
            self.assertEqual(row["status"], "PASS")
            self.assertFalse(row["retryable"])
            self.assertEqual(row["blocker_kind"], "")

    def test_timeout_cleanup_is_bounded(self):
        with tempfile.TemporaryDirectory() as d, open(os.devnull, "w") as log:
            with self.assertRaises(subprocess.TimeoutExpired):
                sweep.run_command([sys.executable, "-c", "import time; time.sleep(60)"], d, log, timeout=0.1)

    def test_host_metadata_cli_bootstraps_before_private_inputs(self):
        workflow = (Path(__file__).parents[2] / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        host = workflow[workflow.index("  host:") : workflow.index("  android:")]
        prerequisites = host.split("      - name: Install host prerequisites", 1)[1].split("      - name:", 1)[0]
        self.assertIn("timeout-minutes: 15", prerequisites)
        self.assertIn("    timeout-minutes: 100", host.split("    steps:", 1)[0])
        execution = host.split("      - name: Execute admitted host owners", 1)[1].split("      - name:", 1)[0]
        self.assertIn("timeout-minutes: 65", execution)
        setup = host.split("      - name: Prepare asset-free metadata CLI", 1)[1].split("      - name:", 1)[0]
        self.assertIn("VALIDATE_METADATA_CLI: ${{ github.event_name == 'pull_request' }}", setup)
        self.assertIn('"$VALIDATE_METADATA_CLI" != true', setup)
        self.assertIn("e.get('fixture') == 'retail-corpus'", setup)
        self.assertIn("timeout-minutes: 15", setup)
        self.assertLess(setup.index("get_jdk.sh"), setup.index("export JAVA_HOME="))
        self.assertLess(setup.index("export JAVA_HOME="), setup.index("get_gradle_wrapper.sh"))
        self.assertLess(setup.index("get_gradle_wrapper.sh"), setup.index("bash target/android/gradlew"))
        self.assertIn("-PhostJvmOnly=true :mission-metadata-cli:installDist --console=plain", setup)
        self.assertIn(
            "test -x target/android/mission-metadata-cli/build/install/mission-metadata-cli/bin/mission-metadata-cli",
            setup,
        )
        self.assertLess(host.index("> target/dependency_base.txt"), host.index("Prepare asset-free metadata CLI"))
        self.assertLess(
            host.index("Prepare asset-free metadata CLI"), host.index("Verify trusted retail checkout boundary")
        )
        self.assertLess(
            host.index("Prepare asset-free metadata CLI"), host.index("Check out immutable private retail inputs")
        )
        self.assertNotIn("secrets.", setup)
        self.assertIn("steps.metadata_cli_prerequisites.outcome == 'failure'", host)

    def test_source_hooks_do_not_use_wildcard_or_random_target(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "android/run_all_tests.ps1").read_text()
        self.assertIn("$name -cnotin $allTests.Name", source)
        self.assertIn("Write-SuiteEvidenceJournal -Observation $observation", source)
        self.assertIn("if (-not $SelectedTestsPath -and", source)
        workflow = (root / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        self.assertIn("cancel-in-progress: false", workflow)
        self.assertIn("persist-credentials: false", workflow)
        self.assertIn("Recover completed receipts", workflow)
        self.assertEqual(workflow.count("> target/dependency_base.txt"), 2)
        self.assertLess(
            workflow.index("> target/dependency_base.txt"), workflow.index("Export authoritative master catalog")
        )
        host = workflow[workflow.index("  host:") : workflow.index("  android:")]
        self.assertLess(host.index("> target/dependency_base.txt"), host.index("Execute admitted host owners"))


if __name__ == "__main__":
    unittest.main()

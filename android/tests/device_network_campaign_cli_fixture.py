"""Host-only campaign CLI and real orchestration with controlled process boundaries."""

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import Mock, patch


ROOT = Path(__file__).resolve().parents[2]
WORKSPACE = Path(sys.argv[1]).resolve()
RUNNER = Path(__file__).with_name("run_device_network_campaign.py")
spec = importlib.util.spec_from_file_location("network_campaign", RUNNER)
campaign = importlib.util.module_from_spec(spec)
spec.loader.exec_module(campaign)


class CampaignCliTest(unittest.TestCase):
    def test_object_allocator_requires_consistent_live_and_owned_counts(self):
        mp = {
            "active_object_count": 3,
            "allocated_object_count": 3,
            "unowned_object_count": 2,
            "object_owner_counts": [1, 0],
        }
        campaign.verify_object_allocator({"multiplayer": mp})
        for field, value in (
            ("allocated_object_count", 4),
            ("allocated_object_count", 2),
            ("active_object_count", 0),
            ("unowned_object_count", 3),
            ("object_owner_counts", [-1, 2]),
            ("object_owner_counts", []),
            ("active_object_count", None),
        ):
            with self.assertRaises(RuntimeError):
                campaign.verify_object_allocator({"multiplayer": {**mp, field: value}})
        for field in mp:
            with self.assertRaises(RuntimeError):
                campaign.verify_object_allocator({"multiplayer": {k: v for k, v in mp.items() if k != field}})
        with self.assertRaises(RuntimeError):
            campaign.verify_object_allocator(None)

    def test_allocation_fixture_requires_host_owned_transfer_object(self):
        campaign.verify_allocation_owner(
            {"multiplayer": {"my_player_num": 0, "synchronized_object_owner_counts": [1, 0]}}
        )
        for invalid in (
            None,
            {},
            {"my_player_num": 0},
            {"my_player_num": 0, "synchronized_object_owner_counts": [0, 1]},
            {"my_player_num": -1, "synchronized_object_owner_counts": [1, 1]},
        ):
            with self.assertRaises(RuntimeError):
                campaign.verify_allocation_owner({"multiplayer": invalid} if invalid else None)

    def test_allocation_recovery_requires_injection_and_fresh_completion(self):
        state = {"join_wait": {"retries": 1, "active": False, "objects_complete": True}}
        log = "test_allocation_failed\nobjects_allocation_failed sender=7"
        campaign.verify_allocation_recovery(state, log)
        for invalid_log in ("", "test_allocation_failed", "objects_allocation_failed", log + log):
            with self.assertRaises(RuntimeError):
                campaign.verify_allocation_recovery(state, invalid_log)
        for key, value in (("retries", 0), ("retries", 2), ("active", True), ("objects_complete", False)):
            invalid = {"join_wait": {**state["join_wait"], key: value}}
            with self.assertRaises(RuntimeError):
                campaign.verify_allocation_recovery(invalid, log)
        with self.assertRaises(RuntimeError):
            campaign.verify_allocation_recovery(None, log)
        self.assertEqual(len([case for case in campaign.cases() if case["kind"] == "join-allocation"]), 3)

    def test_exhaustion_requires_bounded_terminal_failure(self):
        def sample(retries=3, active=False, complete=False, status=None, message="after three retries"):
            return {
                "peers": [
                    None,
                    {
                        "join_wait": {"retries": retries, "active": active, "objects_complete": complete},
                        "network_status": status,
                        "menu_subtitle": message,
                    },
                ]
            }

        campaign.verify_join_exhaustion([sample()])
        for invalid in (
            sample(retries=2),
            sample(retries=4),
            sample(active=True),
            sample(complete=True),
            sample(status=1),
            sample(message=""),
        ):
            with self.assertRaises(RuntimeError):
                campaign.verify_join_exhaustion([invalid])
        with self.assertRaises(RuntimeError):
            campaign.verify_join_exhaustion([sample(status=1), sample()])
        with self.assertRaises(RuntimeError):
            campaign.verify_join_exhaustion([])

    def test_recovery_evidence_requires_actual_path(self):
        def sample(**wait):
            return {"peers": [None, {"join_wait": wait}]}

        recovered = sample(retries=1, object_packets=8, early_syncs=2)
        logs = "test_drop_quit attempt=1\ntransfer_replace old=1 new=2"
        campaign.verify_join_recovery_evidence("second_and_quit", [recovered], logs)
        for missing in ("", "test_drop_quit attempt=1", "transfer_replace old=1 new=2"):
            with self.assertRaises(RuntimeError):
                campaign.verify_join_recovery_evidence("second_and_quit", [recovered], missing)
        with self.assertRaises(RuntimeError):
            campaign.verify_join_recovery_evidence("second_and_quit", [sample(retries=0)], logs)
        campaign.verify_join_recovery_evidence(
            "all_objects", [sample(retries=0, object_packets=0, early_syncs=2), recovered], ""
        )
        with self.assertRaises(RuntimeError):
            campaign.verify_join_recovery_evidence("all_objects", [recovered], "")

    def setUp(self):
        self.directory = WORKSPACE / self._testMethodName
        self.directory.mkdir()
        self.assertTrue(self.directory.resolve().is_relative_to(WORKSPACE))

    def cli(self, *args):
        return subprocess.run(
            [sys.executable, str(RUNNER), *map(str, args)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=20,
            cwd=ROOT,
        )

    def test_invalid_repeats_fail_without_creating_output(self):
        for repeat in (0, -1, 101):
            with self.subTest(repeat=repeat):
                output = self.directory / str(repeat)
                result = self.cli(
                    "--repeat",
                    repeat,
                    "--host",
                    "fixture-host",
                    "--client",
                    "fixture-client",
                    "--output",
                    output,
                    "--case",
                    "d1-baseline",
                    "--adb",
                    self.directory / "adb-must-not-exist",
                )
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn("--repeat must be between 1 and 100", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertFalse(output.exists())

    def test_listing_remains_read_only_and_ignores_repeat(self):
        baseline = self.cli("--list", "--case", "*baseline")
        self.assertEqual(baseline.returncode, 0, baseline.stderr)
        self.assertEqual(
            [row["id"] for row in json.loads(baseline.stdout)],
            ["d1-baseline", "d2-baseline", "d1d2-baseline"],
        )
        for repeat in (0, -1, 1, 100, 101):
            with self.subTest(repeat=repeat):
                output = self.directory / str(repeat)
                result = self.cli("--list", "--case", "*baseline", "--repeat", repeat, "--output", output)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, baseline.stdout)
                self.assertFalse(output.exists())
        selected = self.cli("--list", "--case", "d1-baseline")
        self.assertEqual([row["id"] for row in json.loads(selected.stdout)], ["d1-baseline"])

    def test_bounds_reach_existing_required_arguments_diagnostic(self):
        for repeat in (1, 100):
            with self.subTest(repeat=repeat):
                result = self.cli("--repeat", repeat)
                self.assertEqual(result.returncode, 2)
                self.assertIn("distinct --host and --client plus --output are required", result.stderr)
                self.assertNotIn("--repeat must", result.stderr)

    def test_join_loss_catalog_has_control_and_twenty_two_faults_per_engine(self):
        result = self.cli("--list", "--case", "*-join-loss-*")
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = json.loads(result.stdout)
        self.assertEqual(len(rows), 69)
        for content in ("d1", "d2", "d1d2"):
            selected = [row for row in rows if row["id"].startswith(content + "-")]
            self.assertEqual(
                [row["loss"] for row in selected],
                [
                    "control",
                    "first",
                    "second",
                    "tail",
                    "end",
                    "sync",
                    "duplicate_first",
                    "duplicate_second",
                    "duplicate_tail",
                    "duplicate_end",
                    "duplicate_sync",
                    "reorder_second",
                    "reorder_tail",
                    "reorder_end",
                    "stale_attempt_second",
                    "stale_visit_second",
                    "wrong_session_second",
                    "delay_second_to_end",
                    "delay_second_to_sync",
                    "second_and_quit",
                    "all_objects",
                    "stale_dump_retry",
                    "end_persistent",
                ],
            )
            self.assertTrue(all(row["kind"] == "join-loss" for row in selected))

    def test_exit_waits_for_launcher_teardown_after_process_exit(self):
        device = Mock()
        device.shell.return_value = ""
        device.snapshot.side_effect = [
            {"game_running": True, "has_returnable_game_activity": True},
            {"game_running": False, "has_returnable_game_activity": True},
            {"game_running": False, "has_returnable_game_activity": False},
        ]
        with patch.object(campaign.time, "sleep"):
            campaign.exit_to_launcher(device)
        self.assertEqual(device.snapshot.call_count, 3)
        device.automate.assert_called_once_with([{"action": "enter_launcher"}], wait=False)

    def test_exit_rejects_a_persistently_returnable_activity(self):
        device = Mock()
        device.shell.return_value = ""
        device.snapshot.return_value = {"game_running": False, "has_returnable_game_activity": True}
        with (
            patch.object(campaign.time, "sleep"),
            patch.object(campaign.time, "monotonic", side_effect=range(100)),
            self.assertRaisesRegex(TimeoutError, "launcher clears exited game"),
        ):
            campaign.exit_to_launcher(device)

    def test_invalid_repeats_never_retain_artifacts_or_construct_devices(self):
        for repeat in (0, -1, 101):
            with self.subTest(repeat=repeat):
                error = io.StringIO()
                output = self.directory / str(repeat)
                with (
                    patch.object(sys, "argv", ["campaign", "--repeat", str(repeat), "--output", str(output)]),
                    patch.object(campaign, "Device") as device,
                    patch.object(campaign.subprocess, "run") as retain,
                    patch.object(campaign.subprocess, "Popen") as process,
                    contextlib.redirect_stderr(error),
                    self.assertRaises(SystemExit) as caught,
                ):
                    campaign.main()
                self.assertEqual(caught.exception.code, 2)
                self.assertIn("--repeat must be between 1 and 100", error.getvalue())
                device.assert_not_called()
                retain.assert_not_called()
                process.assert_not_called()
                self.assertFalse(output.exists())

    def orchestrate(self, repeat, codes=(), extra=()):
        # Keep actual main/run_suite, file publication and case definitions.
        # Replace only external devices, artifact-retention process and child processes.
        sandbox = self.directory / f"repeat-{repeat}-{'stop' if extra else 'normal'}"
        output = sandbox / "android/temp/results"
        events = []
        launches = []
        pending_codes = iter(codes)

        class Device:
            def __init__(self, adb, serial, directory, background_lobby):
                self.serial = serial
                self.background_lobby = background_lobby
                events.append(("create", serial))

            def call(self, *args):
                events.append(("call", self.serial, *args))
                return "device" if args == ("get-state",) else ""

            def shell(self, *args):
                events.append(("shell", self.serial, *args))
                return "fixture"

            def wake(self):
                events.append(("wake", self.serial))

            def collect(self):
                events.append(("collect", self.serial))

        class Process:
            def __init__(self, command, **kwargs):
                self.returncode = None
                if command[0] == "pwsh":
                    launches.append(command)
                    self.returncode = next(pending_codes, 0)
                    kwargs["stdout"].write("controlled LAN child boundary\n")

            def poll(self):
                return self.returncode

            def terminate(self):
                self.returncode = 0

            def wait(self, timeout):
                return self.returncode

        argv = [
            "campaign",
            "--host",
            "fixture-host",
            "--client",
            "fixture-client",
            "--output",
            str(output),
            "--case",
            "d1-baseline",
            "--repeat",
            str(repeat),
            "--adb",
            "controlled-adb",
            *extra,
        ]
        with (
            patch.object(sys, "argv", argv),
            patch.object(campaign, "ROOT", sandbox),
            patch.object(campaign, "Device", Device),
            patch.object(campaign.subprocess, "Popen", Process),
            patch.object(campaign.subprocess, "run") as retention,
            contextlib.redirect_stdout(io.StringIO()),
        ):
            status = campaign.main()
        retention.assert_called_once()
        self.assertIn("retain-recent-artifacts.ps1", retention.call_args.args[0][3])
        manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
        results = json.loads((output / "results.json").read_text(encoding="utf-8"))
        self.assertEqual([row["id"] for row in manifest["cases"]], ["d1-baseline"])
        self.assertEqual(len(launches), len(results))
        self.assertEqual(sum(event[0] == "create" for event in events), 2 * len(results))
        self.assertEqual(sum(event[0] == "collect" for event in events), 2 * len(results))
        for index, (result, command) in enumerate(zip(results, launches), 1):
            case_dir = output / f"{index:02d}-d1-baseline"
            self.assertEqual(result["iteration"], index)
            self.assertEqual(json.loads((case_dir / "result.json").read_text(encoding="utf-8")), result)
            self.assertEqual(json.loads((case_dir / "command.json").read_text(encoding="utf-8")), command)
            self.assertEqual(command[3], str(sandbox / "android/tests/test_lan.ps1"))
            self.assertEqual(command[-2:], ["-Game", "d1"])
            self.assertIn("-SkipBuild", command)
        return status, results, launches, manifest

    def test_actual_suite_dispatch_runs_exact_lower_and_upper_repeats(self):
        for repeat in (1, 100):
            with self.subTest(repeat=repeat):
                status, results, _, _ = self.orchestrate(repeat)
                self.assertEqual(status, 0)
                self.assertEqual(len(results), repeat)
                self.assertTrue(all(row["status"] == "PASS" for row in results))

    def test_reversed_devices_reach_actual_suite_command(self):
        status, results, commands, manifest = self.orchestrate(2, extra=("--reverse",))
        self.assertEqual(status, 0)
        self.assertEqual(len(results), 2)
        self.assertEqual(manifest["serials"], ["fixture-client", "fixture-host"])
        for command in commands:
            self.assertEqual(command[command.index("-HostDevice") + 1], "fixture-client")
            self.assertEqual(command[command.index("-JoinDevice") + 1], "fixture-host")

    def test_child_failure_is_retained_and_propagated_after_remaining_repeats(self):
        status, results, _, _ = self.orchestrate(3, codes=(7, 0, 0))
        self.assertEqual(status, 1)
        self.assertEqual([row["status"] for row in results], ["FAIL", "PASS", "PASS"])
        self.assertIn("LAN suite exited 7", results[0]["error"])

    def test_stop_on_failure_does_not_report_empty_success(self):
        status, results, _, _ = self.orchestrate(3, codes=(7,), extra=("--stop-on-failure",))
        self.assertEqual(status, 1)
        self.assertEqual([row["status"] for row in results], ["FAIL"])


if __name__ == "__main__":
    if not WORKSPACE.is_relative_to((ROOT / "android/temp").resolve()):
        raise SystemExit("Fixture workspace must be inside android/temp")
    unittest.main(argv=[sys.argv[0]], verbosity=2)

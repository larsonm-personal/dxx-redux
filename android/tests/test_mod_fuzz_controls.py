"""Offline failure-path checks for the device campaign controller"""

import importlib.util
import itertools
from pathlib import Path
import tempfile
import subprocess
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("mod_fuzz", Path(__file__).resolve().parents[1] / "helpers/mod_fuzz.py")
fuzz = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fuzz)


class CampaignControls(unittest.TestCase):
    def test_prefix_runs_in_one_script_without_between_mission_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            config = dict(
                serial="device",
                package="test",
                fixtures={
                    "vertigo": dict(game="d2", set="vertigo"),
                    "d2": dict(game="d2", set="counterstrike"),
                },
            )
            campaign = fuzz.Campaign(config, directory, "adb")
            case = dict(
                id="prefix",
                mode="d2",
                mods=[],
                level=11,
                transition="toggle",
                phase="repro",
                timeout_seconds=360,
                prefix_cases=[dict(mode="vertigo")],
            )

            def script(config, recipe, manifest):
                return [{"_info": {"_deps": [recipe["mode"]], "games": ["d2"]}}, {"action": recipe["mode"]}]

            with (
                patch.object(campaign, "manifest", return_value={}),
                patch.object(fuzz, "script_for", side_effect=script),
                patch.object(fuzz.subprocess, "Popen") as child,
                patch.object(campaign, "capture"),
                patch.object(campaign, "adb"),
            ):
                child.return_value.wait.return_value = 1
                campaign.run_case(case, fuzz.time.monotonic() + 500)
            steps = fuzz.read_json(Path(directory) / "prefix" / "steps.jsonc")
            self.assertEqual(
                steps,
                [
                    {"_info": {"_deps": ["vertigo", "d2"], "games": ["d2"]}},
                    {"action": "vertigo"},
                    {"action": "d2"},
                ],
            )
            self.assertEqual(child.call_count, 1)

    def test_failure_capture_includes_secret_companions_and_lettered_slots(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            campaign = fuzz.Campaign(dict(serial="device", package="test"), root, "adb")
            saves = [
                "files/d2/Players/player.sg8",
                "files/d2/Players/8secret.sgc",
                "files/d2/Players/secret.sgb",
                "files/d2/Players/player.sga",
            ]

            def adb(*args, **kwargs):
                if "find" in args:
                    return "\n".join(saves + ["files/imported/descent2.hog"])
                return b"archive" if kwargs.get("binary") else ""

            with patch.object(campaign, "private", return_value="{}"), patch.object(campaign, "adb", side_effect=adb):
                campaign.capture(root, True)
            self.assertEqual(fuzz.read_json(root / "private-artifact-paths.json"), saves)
            self.assertEqual(fuzz.read_json(root / "capture-errors.json"), [])

    def test_removing_last_mod_retains_save_restore_transitions(self):
        config = dict(
            packs=[],
            fixtures={"d2": dict(game="d2", set="fixture", deps=[], mission="Counterstrike")},
            template=[dict(action="enter_game"), dict(action="wait_for", field="in_game", value="true")],
        )
        case = dict(mode="d2", mods=[], diagnostic=False, level=5, transition="toggle", play_seconds=12)
        steps = fuzz.script_for(config, case, {})
        restores = [step for step in steps if step.get("text") == "Load Last Save"]
        self.assertEqual(len(restores), 2)
        self.assertTrue(all(step["launches_game"] for step in restores))
        self.assertFalse(any(step.get("command") == "set_mod_enabled" for step in steps))
        case["texture_diagnostics"] = True
        detailed = fuzz.script_for(config, case, {})
        for script, enabled in ((steps, False), (detailed, True)):
            pref = next(step["args"] for step in script if step.get("args", {}).get("key") == "dlog_texture_enabled")
            self.assertEqual(pref["value"], enabled)

    def test_shared_list_preserves_disabled_variants_and_game_specific_owners(self):
        names = [
            "d1-hires-256-textures-ktx2.dxa",
            "d2-hires-256-textures-ktx2.dxa",
            "d1-hires-sounds.dxa",
            "uud2sp-sounds.dxa",
            "d1xr-mac-demo-sounds.dxa",
            "d1-hires-128-textures-ktx2.dxa",
        ]
        config = {"packs": [dict(filename=n, recommended="128" not in n) for n in names]}
        cases = list(fuzz.shared_schedule(config, 42))
        self.assertEqual(len(cases), 16)
        for case in cases:
            self.assertEqual(fuzz.installed_candidates(config, case), names)
            self.assertNotIn("d1xr-mac-demo-sounds.dxa", case["mods"])
            self.assertNotIn("d1-hires-128-textures-ktx2.dxa", case["mods"])
            expected_game = "d1" if case["mode"] in ("d1", "d1-in-d2") else "d2"
            effective = fuzz.effective_case(case)
            self.assertEqual(effective["mods"], fuzz.compatible(expected_game, case["mods"]))
            if case["transition"] == "menus":
                self.assertEqual(case["mods"], [])
            else:
                assertions = fuzz.asset_assertions(case)
                played = [a["expect"].get("mod_usage.sounds", {}).get("contains_object") for a in assertions]
                self.assertTrue(any(a and a["played"] for a in played))

    def test_exhausted_resume_preserves_prior_failure_and_restores_power(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = dict(serial="device", package="com.dxxredux.app.nsdtest", fixtures={"d1": {}})
            fuzz.write_json(root / "inventory.json", config)
            fuzz.append_record(
                root / "journal.jsonl",
                dict(event="completed", id="failed", outcome="assertion_failure", elapsed_seconds=5),
            )
            arguments = [
                "mod_fuzz",
                "--inventory",
                str(root / "inventory.json"),
                "--adb",
                "fake",
                "--resume",
                "--hours",
                "0",
            ]
            with (
                patch.object(fuzz.sys, "argv", arguments),
                patch.object(fuzz, "levels_for", return_value=["level01"]),
                patch.object(fuzz, "schedule", return_value=iter([])),
                patch.object(fuzz.Campaign, "adb", return_value="0") as adb,
            ):
                self.assertEqual(fuzz.main(), 1)
            summary = fuzz.read_json(root / "summary.json")
            self.assertEqual(summary["outcomes"], {"assertion_failure": 1})
            self.assertEqual(
                adb.call_args.args, ("shell", "settings", "put", "global", "stay_on_while_plugged_in", "0")
            )

    def test_timeout_is_clamped_and_evidence_precedes_recovery(self):
        with tempfile.TemporaryDirectory() as directory:
            config = dict(serial="device", package="test", fixtures={"d1": {"game": "d1"}})
            campaign = fuzz.Campaign(config, directory, "adb")
            calls = []
            case = dict(
                id="deadline", mode="d1", timeout_seconds=240, phase="baseline", level=1, mods=[], transition="menus"
            )

            class Child:
                pid = 123
                count = 0

                def wait(self, timeout):
                    self.count += 1
                    if self.count == 1:
                        calls.append(("wait", timeout))
                        raise subprocess.TimeoutExpired("fake", timeout)
                    return 1

                def kill(self):
                    calls.append(("kill",))

            with (
                patch.object(campaign, "manifest", return_value={}),
                patch.object(fuzz, "script_for", return_value=[]),
                patch.object(fuzz.subprocess, "Popen", return_value=Child()),
                patch.object(fuzz.subprocess, "run"),
                patch.object(campaign, "capture", side_effect=lambda *args: calls.append(("capture", args[1]))),
                patch.object(campaign, "adb", side_effect=lambda *args: calls.append(("adb", args))),
            ):
                result = campaign.run_case(case, fuzz.time.monotonic() + 2)
            self.assertEqual(result["outcome"], "timeout")
            self.assertLessEqual(calls[0][1], 2)
            self.assertLess(
                next(i for i, c in enumerate(calls) if c[0] == "capture"),
                next(i for i, c in enumerate(calls) if c[0] == "adb"),
            )
            self.assertEqual([r["event"] for r in fuzz.records(campaign.journal)], ["planned", "started", "completed"])

    def test_partial_journal_does_not_complete_interrupted_case(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "journal.jsonl"
            fuzz.append_record(path, {"event": "planned", "id": "a"})
            fuzz.append_record(path, {"event": "started", "id": "a"})
            with path.open("a") as stream:
                stream.write('{"event":"completed"')
            self.assertFalse(any(r["event"] == "completed" for r in fuzz.records(path)))

    def test_outcomes_keep_missing_data_and_timeouts_out_of_passes(self):
        self.assertEqual(fuzz.classify(0, "RESULT: SKIP"), "missing_prerequisite")
        self.assertEqual(fuzz.classify(1, "", True), "timeout")
        self.assertEqual(fuzz.classify(1, "Fatal signal 11"), "crash")
        self.assertEqual(fuzz.classify(1, "ANR in com.dxxredux.app.nsdtest"), "anr")
        self.assertEqual(fuzz.classify(1, "device offline"), "infrastructure_failure")

    def test_shrinking_keeps_trigger_and_bounds_attempts(self):
        attempts = []

        def reproduce(case):
            attempts.append(case)
            return "bad" in case["mods"]

        case = {"mods": ["a", "bad", "b", "c"], "transition": "toggle"}
        result = fuzz.minimize(case, reproduce, limit=3)
        self.assertEqual(result["mods"], ["bad", "c"])
        self.assertEqual(len(attempts), 3)
        self.assertEqual(case["mods"], ["a", "bad", "b", "c"])

    def test_failure_signature_ignores_android_service_noise_and_duplicates(self):
        cause = "controller: High-resolution pack was not rendered: d1-hires-128-textures-ktx2.dxa"
        noisy = cause + "\nFAIL), isBlocked=true, 0ms\nFAIL), isBlocked=true, 0ms\n" + cause
        self.assertEqual(
            fuzz.failure_signature("assertion_failure", cause), fuzz.failure_signature("assertion_failure", noisy)
        )
        self.assertNotEqual(
            fuzz.failure_signature("assertion_failure", cause),
            fuzz.failure_signature("assertion_failure", "controller: No playable final game state"),
        )

    def test_native_failure_signature_keeps_distinct_relative_fault_locations(self):
        first = "#00 pc 0000000000123456 libd1x-redux.so\nAbort message: invalid pointer 0x12345678"
        replay = "#00 pc 0000000000123456 libd1x-redux.so\nAbort message: invalid pointer 0x87654321"
        different = replay.replace("0000000000123456", "0000000000234567")
        self.assertEqual(fuzz.failure_signature("crash", first), fuzz.failure_signature("crash", replay))
        self.assertNotEqual(fuzz.failure_signature("crash", first), fuzz.failure_signature("crash", different))

    def test_reproducibility_and_valid_resolution_alternatives(self):
        modes = ("d1", "d2", "d1-in-d2", "vertigo", "mac-d1", "demo-d1", "demo-d2")
        names = [
            "uud1tp-textures.dxa",
            "uud2tp-textures.dxa",
            "uud2sp-sounds.dxa",
            "d1xr-hires.dxa",
            "d1xr-mac-demo-sounds.dxa",
        ]
        names += [f"{game}-hires-{size}-textures-ktx2.dxa" for game in ("d1", "d2") for size in (128, 256, 512)]
        config = dict(
            packs=[dict(filename=n) for n in names],
            fixtures={m: {} for m in modes},
            levels={m: ["a", "b"] for m in modes},
        )
        first = list(itertools.islice(fuzz.schedule(config, 17), 200))
        second = list(itertools.islice(fuzz.schedule(config, 17), 200))
        self.assertEqual(first, second)
        self.assertEqual(len({c["id"] for c in first}), len(first))
        for case in first:
            self.assertLessEqual(sum("textures-ktx2" in name for name in case["mods"]), 1)
            if case["mode"] != "mac-d1":
                self.assertNotIn("d1xr-mac-demo-sounds.dxa", case["mods"])

    def test_exec_out_missing_file_is_not_valid_json(self):
        campaign = fuzz.Campaign(dict(serial="device", package="test"), ".", "adb")
        with patch.object(campaign, "adb", return_value="cat: missing: No such file or directory"):
            with self.assertRaisesRegex(RuntimeError, "No such file"):
                campaign.private("missing")


if __name__ == "__main__":
    unittest.main()

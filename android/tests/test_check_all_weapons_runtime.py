"""All-weapons evidence gates, immutable controls and native build identity."""

import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import check_all_weapons_runtime as runtime
from d1_replay_parity import EvidenceError


def command(name, grant=None):
    event = {"kind": "direct_command", "command": name}
    if grant:
        event["grant"] = grant
    return {"k": "command", "ft": 8192, "event": event}


def live_fixture(case):
    name, game, profile, grant = case
    count = 5 if game == "d1" else 10
    mission = "descent" if name == "imported-d1" else runtime.compact.PUBLIC_MISSIONS[game]
    header = {
        "type": "header",
        "version": 5,
        "game": game,
        "mission": mission,
        "start_mode": "new_level",
        "level": 1,
        "frame_count": 40,
        "rng_mode": "lcg_state",
        "git_version": "a" * 9,
        "difficulty": 0,
    }
    initial = {
        "primary_flags": 1,
        "secondary_flags": 1,
        "primary_ammo": [0] * count,
        "secondary_ammo": [5] + [0] * (count - 1),
        "energy": 100 * 65536,
        "laser_level": 0,
        "flags": 0,
        "score": 0,
        "cheats_enabled": 0,
        "primary_selected": 0,
        "secondary_selected": 0,
    }
    granted = {**copy.deepcopy(initial), **runtime.expected_grant(game, grant), "flags": 1024}
    between = copy.deepcopy(granted)
    between["secondary_ammo"][0] -= 1
    spent = copy.deepcopy(granted)
    spent["energy"] -= 3 * 65536
    states = [granted, between, granted, spent, granted, granted]
    observations = [
        {"phase": phase, "frame": frame, "inventory": copy.deepcopy(state)}
        for phase, frame, state in zip(runtime.PHASES, [2, 2, 2, 34, 34, 40], states)
    ]
    shots = [{"frame": 4, "object": 71, "signature": 101, "weapon_id": 20 if profile == "d1" else 12, "selected": 2}]
    if profile == "d2":
        shots.append({"frame": 21, "object": 73, "signature": 111, "weapon_id": 32, "selected": 6})
    report = {
        "game": game,
        "mission": mission,
        "scenario": "all-weapons-bigred" if grant == "d1_bigred" else "all-weapons",
        "profile": profile,
        "grant": runtime.GRANT_CODES[grant],
        "native_code": "bigred" if grant == "d1_bigred" else "scourge" if game == "d1" else "honestbob",
        "frames": 40,
        "post_capture_policy_checks": "pass",
        "retail_assets_playback": False,
        "android_device_run": False,
        "initial": initial,
        "terminal": granted,
        "observations": observations,
        "shots": shots,
    }
    rows = [header]
    for frame in range(41):
        row = {
            "type": "input_tail" if frame == 40 else "frame",
            "f": frame,
            "input": {},
            "rng": {"s": 1, "c": 0},
            "dispatch": {"pre_rng": "=", "ops": []},
        }
        if frame != 40:
            row["ft"] = 8192
        if frame in (2, 34, 40):
            row["dispatch"]["ops"] = [command("grant_all_weapons", grant)]
        if frame == 2:
            row["dispatch"]["ops"] += [
                {"k": "sample", "ft": 8192, "s": {}, "p": {"f2": 1}},
                command("grant_all_weapons", grant),
            ]
            if game == "d1":
                row["dispatch"]["ops"].insert(0, command("enable_cheats"))
        rows.append(row)
    player = {key: granted[key] for key in ("energy", "laser_level", "flags", "score")}
    player["energy"] //= 65536
    for key in ("primary_ammo", "secondary_ammo"):
        player[key] = granted[key] + [0] * (16 - count)
    rows.append(
        {
            "type": "result",
            "result": {
                "game": game,
                "mission": mission,
                "level": 1,
                "difficulty": 0,
                "frame_count": 40,
                "player0": player,
            },
        }
    )
    return rows, report


class RuntimeEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.revision = "a" * 40
        self.stamp = "a" * 9

    def write_capture(self, rows, report):
        (self.root / "capture.dximdemo").write_bytes(b"".join(runtime.compact.canonical(row) + b"\n" for row in rows))
        (self.root / "capture.json").write_text(json.dumps(report))
        (self.root / "capture.dximdemo.rngtrace.jsonl").write_text("{}\n")

    def test_all_four_live_capture_profiles(self):
        for case in runtime.CASES:
            with self.subTest(case=case):
                rows, report = live_fixture(case)
                self.write_capture(rows, report)
                _, evidence = runtime.verify_capture(self.root, case, self.revision, self.stamp)
                self.assertEqual(evidence["ordered_grant_frames"], [2, 2, 34, 40])
                self.assertEqual(bool(evidence["gauss"]), case[2] == "d2")

    def test_report_rejects_bad_inventory_identity_shots_and_scope(self):
        case = runtime.CASES[0]
        changes = [
            lambda rows, report: report["initial"].update(primary_flags=7),
            lambda rows, report: report["observations"][0]["inventory"].update(primary_flags=7),
            lambda rows, report: report["observations"][1]["inventory"]["secondary_ammo"].__setitem__(0, 20),
            lambda rows, report: report["shots"][0].update(weapon_id=12),
            lambda rows, report: report["shots"].append(copy.deepcopy(report["shots"][0])),
            lambda rows, report: report.update(retail_assets_playback=True),
            lambda rows, report: report.update(android_device_run=True),
            lambda rows, report: rows[0].update(start_mode="save_checkpoint"),
            lambda rows, report: rows[0].update(git_version="a" * 9 + "-dirty"),
            lambda rows, report: rows[-2]["dispatch"]["ops"].clear(),
            lambda rows, report: rows[3]["dispatch"]["ops"].__delitem__(2),
        ]
        for index, change in enumerate(changes):
            with self.subTest(index=index):
                rows, report = live_fixture(case)
                change(rows, report)
                self.write_capture(rows, report)
                with self.assertRaises(EvidenceError):
                    runtime.verify_capture(self.root, case, self.revision, self.stamp)

    def test_exact_native_ammo_and_masks(self):
        scourge = runtime.expected_grant("d1", "d1_scourge")
        bigred = runtime.expected_grant("d1", "d1_bigred")
        native_d2 = runtime.expected_grant("d2", "d2_honestbob")
        imported = runtime.expected_grant("d2", "d1_scourge")
        self.assertEqual((scourge["primary_flags"], scourge["secondary_flags"]), (231, 231))
        self.assertEqual(bigred["primary_flags"], 255)
        self.assertEqual(bigred["secondary_ammo"], [20, 10, 10, 5, 5])
        self.assertEqual((native_d2["primary_flags"], native_d2["secondary_flags"]), (64719, 64751))
        self.assertEqual(native_d2["secondary_ammo"], [20, 10, 10, 5, 0, 20, 20, 15, 0, 0])
        self.assertEqual(scourge["primary_ammo"][1], 784)
        self.assertEqual(imported["primary_ammo"], [0, 784, 0, 0, 0, 0, 0, 0, 0, 0])
        for grant in ("d1_scourge", "d1_bigred"):
            self.assertEqual(runtime.expected_grant("d2", grant)["primary_ammo"], [0, 784, 0, 0, 0, 0, 0, 0, 0, 0])

    def test_mutations_keep_original_result_rng_and_other_inputs(self):
        for case in runtime.CASES[:3]:
            rows, _ = live_fixture(case)
            before = copy.deepcopy(rows)
            mutations = ["missing-final-grant", "wrong-profile"] + (["wrong-variant"] if case[1] == "d1" else [])
            for mutation in mutations:
                with self.subTest(case=case, mutation=mutation):
                    changed = runtime.mutate(rows, mutation)
                    self.assertEqual(rows, before)
                    self.assertEqual(changed[0], rows[0])
                    self.assertEqual(changed[-1], rows[-1])
                    for a, b in zip(rows[1:-1], changed[1:-1]):
                        self.assertEqual(
                            {k: v for k, v in a.items() if k != "dispatch"},
                            {k: v for k, v in b.items() if k != "dispatch"},
                        )
                    if mutation == "missing-final-grant":
                        self.assertEqual(changed[:-2], rows[:-2])
                        self.assertEqual(len(runtime.grant_operations(changed)), 3)
                        self.assertEqual(changed[-2]["dispatch"]["ops"], [])
                    else:
                        self.assertEqual(len(runtime.grant_operations(changed)), 4)
                        self.assertTrue(
                            all(op["event"]["grant"] != case[3] for _, op in runtime.grant_operations(changed))
                        )

    def test_source_rejects_dirty_or_wrong_head_before_reading_recipes(self):
        for answers, message in (([b"a" * 40, b" M test\n"], "dirty"), ([b"b" * 40], "HEAD")):
            with mock.patch.object(runtime, "git", side_effect=answers):
                with self.assertRaisesRegex(EvidenceError, message):
                    runtime.source_identity(self.root, self.revision)
        with self.assertRaisesRegex(EvidenceError, "40-character"):
            runtime.source_identity(self.root, "HEAD")

    def test_source_hashes_exact_recipe_bytes(self):
        source = {}
        for name in runtime.RECIPES:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            source[name] = (name + "\n").encode()
            path.write_bytes(source[name])

        def git(_repo, *arguments):
            if arguments[0] == "rev-parse":
                return self.revision.encode()
            if arguments[0] == "status":
                return b""
            return source[arguments[1].split(":", 1)[1]]

        with mock.patch.object(runtime, "git", side_effect=git):
            hashes = runtime.source_identity(self.root, self.revision)
            self.assertEqual(set(hashes), set(runtime.RECIPES))
            (self.root / runtime.FIXTURE).write_text("modified")
            with self.assertRaisesRegex(EvidenceError, "recipe differs"):
                runtime.source_identity(self.root, self.revision)

    def write_build(self, game):
        build = self.root / ("build-" + game)
        (build / "main").mkdir(parents=True)
        (build / "CMakeCache.txt").write_text(
            f"CMAKE_HOME_DIRECTORY:INTERNAL={self.root / game}\nCMAKE_GENERATOR:INTERNAL=Ninja\n"
        )
        rows, exes = [], {}
        for role, target in (
            ("capture", "test_upstream_compat"),
            ("graphics", "dxx-redux-" + game),
            ("headless", "dxx-redux-" + game + "-headless"),
        ):
            names = [runtime.RECORDER, runtime.CHEAT, game + "/main/gamecntl.c", game + "/main/input_demo_hooks.c"]
            if role == "capture":
                names.append("android/tests/test_upstream_compat.cpp")
            args = ["c++", '-DDXX_INPUT_DEMO_GIT_VERSION="' + self.stamp + '"']
            if game == "d2":
                args.append("-DDXX_BUILD_DESCENT_II")
            if role != "graphics":
                args.append("-DDXX_HEADLESS_CONSOLE=1")
            for name in names:
                rows.append(
                    {
                        "directory": str(build),
                        "file": str(self.root / name),
                        "arguments": args,
                        "output": f"main/CMakeFiles/{target}.dir/{Path(name).name}.o",
                    }
                )
            exes[role] = build / "main" / (game + "x-redux" if role == "graphics" else target)
            exes[role].write_bytes(b"fake\0" + self.stamp.encode() + b"\0")
            exes[role].chmod(0o755)
        (build / "compile_commands.json").write_text(json.dumps(rows))
        return build, exes, rows

    def test_build_gate_checks_every_target_and_native_profile(self):
        for game in ("d1", "d2"):
            build, exes, rows = self.write_build(game)
            result = runtime.build_identity(self.root, build, game, self.revision, exes)
            self.assertEqual(result["stamp"], self.stamp)
            self.assertEqual(set(result["executables"]), {"capture", "graphics", "headless"})
            rows.pop()
            (build / "compile_commands.json").write_text(json.dumps(rows))
            with self.assertRaisesRegex(EvidenceError, "Missing or inconsistent"):
                runtime.build_identity(self.root, build, game, self.revision, exes)

    def test_build_rejects_stale_binary_even_with_clean_compile_metadata(self):
        build, exes, _ = self.write_build("d1")
        exes["headless"].write_bytes(b"old revision\0")
        with self.assertRaisesRegex(EvidenceError, "Executable lacks"):
            runtime.build_identity(self.root, build, "d1", self.revision, exes)

    def test_build_rejects_unrelated_binary_path_with_matching_stamp(self):
        build, exes, _ = self.write_build("d1")
        unrelated = self.root / "unrelated-d1x-redux"
        unrelated.write_bytes(exes["graphics"].read_bytes())
        unrelated.chmod(0o755)
        exes["graphics"] = unrelated
        with self.assertRaisesRegex(EvidenceError, "original target"):
            runtime.build_identity(self.root, build, "d1", self.revision, exes)

    def test_wrong_profile_requires_native_rejection_not_generic_failure(self):
        failure_dir = self.root / "result.json.failure_1"
        failure_dir.mkdir()
        (failure_dir / "failure.json").write_text(json.dumps({"forced_stop": False, "native_exit_code": 1}))
        (failure_dir / "stderr.txt").write_text("unsupported all-weapons grant for fixture game")
        result = runtime.verify_negative(self.root, None, None, "wrong-profile", "d1", 1)
        self.assertEqual(result["status"], "pass")
        (failure_dir / "stderr.txt").write_text("Missing assets while loading all-weapons capture: grant_all_weapons")
        with self.assertRaisesRegex(EvidenceError, "precise native grant rejection"):
            runtime.verify_negative(self.root, None, None, "wrong-profile", "d1", 1)

    def test_tail_negative_uses_original_oracle_and_preserves_all_frames(self):
        failure_dir = self.root / "result.json.failure_1"
        failure_dir.mkdir()
        (failure_dir / "failure.json").write_text(json.dumps({"forced_stop": False, "native_exit_code": 1}))
        rows, _ = live_fixture(runtime.CASES[0])
        baseline = rows[-1]["result"]
        actual = copy.deepcopy(baseline)
        actual["player0"]["energy"] -= 1
        (self.root / "state.jsonl.gz").touch()
        (self.root / "rng.jsonl").touch()
        (self.root / "result.json").write_text(json.dumps(actual))
        demo, oracle = self.root / "original.dximdemo", self.root / "original.oracle.json"
        demo.write_text("original untouched demo")
        oracle.write_text("original untouched oracle")
        hashes = [str(index) for index in range(40)]
        with (
            mock.patch.object(
                runtime.compact,
                "validate",
                return_value=(rows[0], {"frame_sha256": hashes, "terminal_sha256": runtime.compact.sha(baseline)}),
            ),
            mock.patch.object(runtime.compact, "_frame_hashes", return_value=hashes),
            mock.patch.object(runtime.parity, "records", return_value=iter(rows)),
            mock.patch.object(
                runtime.compact, "verify", side_effect=EvidenceError("Replay terminal result differs from live capture")
            ) as verify,
        ):
            result = runtime.verify_negative(self.root, demo, oracle, "missing-final-grant", "d1", 1)
            self.assertEqual(result["status"], "pass")
            self.assertIsNone(result["first_different_frame"])
            self.assertEqual(verify.call_args.args[1:3], (demo, oracle))
        self.assertEqual(demo.read_text(), "original untouched demo")
        self.assertEqual(oracle.read_text(), "original untouched oracle")


if __name__ == "__main__":
    unittest.main()

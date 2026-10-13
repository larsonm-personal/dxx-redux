"""All-weapons evidence gates, immutable controls and native build identity."""

import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest import mock

import check_all_weapons_runtime as runtime
import check_ordered_input_runtime as ordered_runtime
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

    def test_replay_console_output_default_and_explicit_diagnostics(self):
        args = argparse.Namespace(
            repo=self.root,
            pwsh="pwsh",
            headless_d1=self.root / "dxx-redux-d1-headless",
            graphics_d1=self.root / "d1x-redux",
            data_d1=self.root / "data",
            timeout=12,
            minimum_free_gb=1,
        )
        for mode in ("headless", "graphics"):
            for requested, expected in (({}, "1"), ({"headless_console_output": 2}, "2")):
                with self.subTest(mode=mode, expected=expected):
                    destination = self.root / (mode + "-" + expected)
                    with mock.patch.object(
                        ordered_runtime.subprocess, "run", return_value=mock.Mock(returncode=0)
                    ) as run:
                        self.assertEqual(
                            runtime.replay(args, "d1", mode, self.root / "capture.dximdemo", destination, **requested),
                            0,
                        )
                    command_line = run.call_args.args[0]
                    self.assertEqual(command_line.count("-HeadlessConsoleOutput"), 1)
                    self.assertEqual(command_line[command_line.index("-HeadlessConsoleOutput") + 1], expected)
                    self.assertEqual(
                        command_line[command_line.index("-Runner") + 1],
                        "headless-console" if mode == "headless" else "visual",
                    )
                    self.assertEqual(run.call_args.kwargs["timeout"], args.timeout + 90)
                    self.assertEqual(run.call_args.kwargs["cwd"], self.root)

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

    def test_wrong_profile_reads_native_runner_log_without_archived_text(self):
        failure_dir = self.root / "result.json.failure_1"
        failure_dir.mkdir()
        (failure_dir / "failure.json").write_text(json.dumps({"forced_stop": False, "native_exit_code": 1}))
        for game, reason in (
            ("d1", "unsupported all-weapons grant for fixture game"),
            ("d2", "all-weapons grant differs from the active gameplay profile"),
        ):
            with self.subTest(game=game):
                log = self.root / "runner.log"
                log.write_text("HEADLESS-RUN FAIL load demo line 4: " + reason + "\n")
                result = runtime.verify_negative(self.root, None, None, "wrong-profile", game, 1)
                self.assertEqual(result["status"], "pass")
                self.assertEqual(list(failure_dir.glob("*.txt")), [])
                log.write_text("Missing assets while loading all-weapons capture: grant_all_weapons")
                with self.assertRaisesRegex(EvidenceError, "precise native grant rejection"):
                    runtime.verify_negative(self.root, None, None, "wrong-profile", game, 1)

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


CORPUS = Path(__file__).parent / "fixtures/public_all_weapons"
CAPTURE_REVISION = "7145f682f4dd946ce0a8019d087eba6523794c16"
CAPTURE_TREE = "913dcf6dea58cc505b95e5faafc385301621d342"
QUALIFICATION_SHA256 = "51329d86ac132c0373799737825b4b165e4dd6c8fcbc56a5b7eb76e311c80280"


def validate_stored_corpus(root):
    """Fail closed on the exact bounded, immutable all-weapons admission."""

    def read(name):
        path = root / name
        runtime.require(
            path.is_file() and not path.is_symlink() and path.resolve().is_relative_to(root.resolve()),
            "Missing or unsafe corpus evidence",
        )
        runtime.require(path.stat().st_size <= runtime.MAX_REPORT_BYTES, "Corpus evidence exceeds its byte budget")
        return runtime.compact._json(path.read_bytes())

    manifest = read("manifest.json")
    runtime.require(manifest["schema"] == "public-all-weapons-v1", "Wrong corpus schema")
    runtime.require(
        (manifest["capture_revision"], manifest["capture_tree"]) == (CAPTURE_REVISION, CAPTURE_TREE),
        "Wrong immutable capture source",
    )
    runtime.require(
        manifest["max_input_and_oracle_bytes"] == 100000 and manifest["max_evidence_bytes"] == runtime.MAX_REPORT_BYTES,
        "Wrong evidence budgets",
    )
    runtime.require(manifest["native_backends"] == ["headless", "graphics"], "Missing native backend")
    runtime.require(
        manifest["retail_assets_playback"] is False and manifest["android_device_run"] is False,
        "Unsupported qualification scope",
    )
    runtime.require(
        manifest["asset_sha256"] == {game: runtime.compact.pinned_assets(game) for game in ("d1", "d2")},
        "Wrong pinned public assets",
    )
    names = {case[0] for case in runtime.CASES}
    native = names - {"imported-d1"}
    expected_files = (
        {"qualification.json"}
        | {f"captures/{name}.json" for name in names}
        | {name + suffix for name in native for suffix in (".dximdemo", ".oracle.json")}
    )
    actual_files = {path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file()}
    runtime.require(
        actual_files == expected_files | {"manifest.json", "README.md"} and set(manifest["files"]) == expected_files,
        "Missing or unexpected corpus evidence",
    )
    runtime.require(
        runtime.digest(root / "qualification.json") == QUALIFICATION_SHA256,
        "Reviewed qualification evidence changed",
    )
    for name, expected in manifest["files"].items():
        path = root / name
        read(name) if not name.endswith(".dximdemo") else None
        runtime.require(
            not path.is_symlink() and path.stat().st_size <= runtime.MAX_REPORT_BYTES,
            "Unsafe or oversized corpus evidence",
        )
        runtime.require(
            expected == {"bytes": path.stat().st_size, "sha256": runtime.digest(path)}, "Stored evidence hash differs"
        )
    runtime.require(manifest["qualification_report"] == "qualification.json", "Wrong qualification evidence path")
    report = read(manifest["qualification_report"])
    runtime.require(
        report["status"] == "pass" and report["complete"] is True and report["source_revision"] == CAPTURE_REVISION,
        "Incomplete or wrong-source qualification",
    )
    runtime.require(
        report["retail_assets_playback"] is False
        and report["android_device_run"] is False
        and report["published"] is False,
        "Original qualification scope was changed",
    )
    runtime.require(report["reviewed_scope_statement"] == runtime.SCOPE, "Qualification scope statement changed")
    for value in report["source_recipe_sha256"].values():
        runtime.require(runtime.compact._digest(value), "Invalid source recipe digest")
    runtime.require(set(report["builds"]) == {"d1", "d2"}, "Missing native build provenance")
    for game, build in report["builds"].items():
        runtime.require(
            build["stamp"] == CAPTURE_REVISION[:9] and runtime.compact._digest(build["compile_commands_sha256"]),
            "Wrong native compile provenance",
        )
        runtime.require(
            set(build["executables"]) == {"capture", "graphics", "headless"}, "Missing native executable identity"
        )
        for exe in build["executables"].values():
            runtime.require(runtime.compact._digest(exe["sha256"]), "Invalid executable digest")
    runtime.require(len(manifest["cases"]) == len(report["cases"]) == 4, "Incomplete capture inventory")
    declarations = {case["id"]: case for case in manifest["cases"]}
    captures = {case["case"]: case for case in report["cases"]}
    runtime.require(set(declarations) == set(captures) == names, "Missing or duplicate live scenario")
    oracles = {}
    for name, game, profile, grant in runtime.CASES:
        declaration, entry = declarations[name], captures[name]
        runtime.require(
            (declaration["game"], declaration["profile"], declaration["grant"]) == (game, profile, grant),
            "Wrong scenario profile",
        )
        runtime.require(
            declaration["capture"] == f"captures/{name}.json" and entry["capture"] == f"{name}-capture",
            "Wrong original capture path",
        )
        capture = read(declaration["capture"])
        runtime.require(
            manifest["files"][declaration["capture"]] == report["artifacts"][f"{name}-capture/capture.json"],
            "Live observation bytes changed",
        )
        runtime.require(
            (capture["game"], capture["profile"], capture["grant"], capture["frames"])
            == (game, profile, runtime.GRANT_CODES[grant], 40),
            "Wrong live report identity",
        )
        runtime.require(
            capture["retail_assets_playback"] is False
            and capture["android_device_run"] is False
            and capture["post_capture_policy_checks"] == "pass",
            "Unsupported live report scope",
        )
        evidence = entry["evidence"]
        for key in ("initial", "observations", "terminal"):
            runtime.require(evidence[key] == capture[key], "Qualification replaced a live inventory observation")
        runtime.require(
            evidence["all_projectiles"] == capture["shots"] and evidence["ordered_grant_frames"] == [2, 2, 34, 40],
            "Qualification replaced live ordered/projectile evidence",
        )
        phases = capture["observations"]
        runtime.require(
            [item["phase"] for item in phases] == list(runtime.PHASES)
            and [item["frame"] for item in phases] == [2, 2, 2, 34, 34, 40],
            "Missing live inventory phase",
        )
        for state in [capture["initial"], capture["terminal"], *(row["inventory"] for row in phases)]:
            runtime.inventory(state, game)
        runtime.require(
            capture["initial"]["primary_flags"] == capture["initial"]["secondary_flags"] == 1
            and capture["initial"]["cheats_enabled"] == 0,
            "Capture did not begin with ordinary inventory",
        )
        expected = runtime.expected_grant(game, grant)
        for row in phases:
            if row["phase"] in ("first_grant", "repeated_grant", "later_grant", "tail_grant"):
                runtime.require(
                    all(row["inventory"][key] == value for key, value in expected.items()),
                    "Grant changed faithful native inventory",
                )
        spread = [
            shot
            for shot in capture["shots"]
            if shot["weapon_id"] == (20 if profile == "d1" else 12) and shot["selected"] == 2
        ]
        gauss = [shot for shot in capture["shots"] if shot["weapon_id"] == 32 and shot["selected"] == 6]
        runtime.require(
            spread == evidence["spreadfire"]
            and spread
            and gauss == evidence["gauss"]
            and bool(gauss) == (profile == "d2"),
            "Missing native projectile signatures",
        )
        runtime.require(
            len({shot["signature"] for shot in capture["shots"]}) == len(capture["shots"]),
            "Duplicate projectile signature",
        )
        if name == "imported-d1":
            runtime.require(
                declaration["scope"] == entry["scope"] == "live-diagnostic-only"
                and not ({"demo", "oracle"} & set(declaration)),
                "Imported D1 was incorrectly admitted to compact replay",
            )
            continue
        runtime.require(
            declaration["scope"] == "native-compact-replay" and entry["scope"] == "native-compact-candidate",
            "Wrong native admission scope",
        )
        for key, suffix in (("demo", ".dximdemo"), ("oracle", ".oracle.json")):
            filename = name + suffix
            runtime.require(
                declaration[key] == entry[key] == filename
                and manifest["files"][filename] == report["artifacts"][filename],
                "Compact companion changed after live qualification",
            )
            runtime.require((root / filename).stat().st_size <= 100000, "Oversized compact companion")
        header, oracle = runtime.compact.validate(root / declaration["demo"], root / declaration["oracle"])
        oracles[name] = oracle
        runtime.require(
            (
                header["game"],
                header["mission"],
                header["level"],
                header["frame_count"],
                header["start_mode"],
                header["version"],
                header["git_version"],
            )
            == (game, runtime.compact.PUBLIC_MISSIONS[game], 1, 40, "new_level", 5, CAPTURE_REVISION[:9]),
            "Wrong compact new-level identity",
        )
        runtime.require(
            oracle["provenance"]
            == {
                "source_revision": CAPTURE_REVISION,
                "capture_executable_sha256": report["builds"][game]["executables"]["capture"]["sha256"],
                "capture_source_sha256": report["source_recipe_sha256"][runtime.FIXTURE],
                "asset_sha256": runtime.compact.pinned_assets(game),
            },
            "Compact oracle changed live source provenance",
        )
        runtime.require(
            oracle["full_recording_sha256"] == report["artifacts"][f"{name}-capture/capture.dximdemo"]["sha256"],
            "Oracle changed its original recording identity",
        )
    expected_runs = {
        (name, backend, control)
        for name in native
        for backend in ("headless", "graphics")
        for control in (
            ["original", "wrong-profile", "missing-final-grant"] + (["wrong-variant"] if name.startswith("d1-") else [])
        )
    }
    runtime.require(
        len(report["runs"]) == 22
        and {(run["case"], run["backend"], run["control"]) for run in report["runs"]} == expected_runs,
        "Incomplete or duplicate replay-control matrix",
    )
    for run in report["runs"]:
        name, mode, control = (run[key] for key in ("case", "backend", "control"))
        runtime.require(run["status"] == "pass", "Failed admitted replay control")
        if control == "original":
            runtime.require(
                run["frames_compared"] == 40
                and run["simulation_events_compared"] == oracles[name]["simulation_rng_events"],
                "Incomplete frame or RNG comparison",
            )
            continue
        runtime.require(
            run["oracle_resealed"] is False
            and run["oracle_sha256"] == manifest["files"][name + ".oracle.json"]["sha256"],
            "Negative control resealed its live oracle",
        )
        rows = list(runtime.parity.records(root / (name + ".dximdemo")))
        changed = b"".join(runtime.compact.canonical(row) + b"\n" for row in runtime.mutate(rows, control))
        runtime.require(
            hashlib.sha256(changed).hexdigest()
            == run["negative_input_sha256"]
            == report["artifacts"][f"{name}-{mode}-{control}.dximdemo"]["sha256"],
            "Negative differs from the declared input-only mutation",
        )
        runtime.require(type(run["native_exit_code"]) is int, "Missing natural native exit evidence")
        if control == "wrong-profile":
            runtime.require(run["evidence"] == "native grant/profile rejection", "Missing native profile rejection")
        else:
            reason = (
                "Replay terminal result differs from live capture"
                if control == "missing-final-grant"
                else "Replay frame state, RNG or diagnostics differ from live capture"
            )
            runtime.require(
                run["original_oracle_rejection"] == reason, "Negative did not fail the original live oracle"
            )
            runtime.require(
                run["first_different_frame"] == (None if control == "missing-final-grant" else 2),
                "Wrong semantic negative boundary",
            )
    return manifest, report


class StoredAllWeaponsCorpusTests(unittest.TestCase):
    def copy_corpus(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        destination = Path(temporary.name) / "corpus"
        shutil.copytree(CORPUS, destination)
        return destination

    def test_complete_immutable_stored_corpus(self):
        manifest, report = validate_stored_corpus(CORPUS)
        self.assertEqual(len(manifest["files"]), 11)
        self.assertEqual(len(report["runs"]), 22)

    def test_missing_and_corrupt_stored_evidence_fail_closed(self):
        for filename, remove in (
            ("d1-scourge.dximdemo", True),
            ("captures/imported-d1.json", True),
            ("qualification.json", True),
            ("d2-honestbob.oracle.json", False),
            ("captures/d1-bigred.json", False),
        ):
            with self.subTest(filename=filename, remove=remove):
                root = self.copy_corpus()
                path = root / filename
                path.unlink() if remove else path.write_bytes(path.read_bytes() + b" ")
                with self.assertRaises((EvidenceError, OSError)):
                    validate_stored_corpus(root)

    def test_rehashed_report_mutations_cannot_reseal_reviewed_evidence(self):
        for mutation in ("missing-run", "resealed-oracle", "native-exit", "source-recipe"):
            with self.subTest(mutation=mutation):
                root = self.copy_corpus()
                path = root / "qualification.json"
                report = runtime.compact._json(path.read_bytes())
                if mutation == "missing-run":
                    report["runs"].pop()
                elif mutation == "resealed-oracle":
                    next(run for run in report["runs"] if run["control"] != "original")["oracle_resealed"] = True
                elif mutation == "native-exit":
                    run = next(run for run in report["runs"] if run["control"] != "original")
                    run["native_exit_code"] += 1
                else:
                    report["source_recipe_sha256"]["android/tests/check_ordered_input_runtime.py"] = "0" * 64
                path.write_text(json.dumps(report))
                manifest = runtime.compact._json((root / "manifest.json").read_bytes())
                manifest["files"]["qualification.json"] = {"bytes": path.stat().st_size, "sha256": runtime.digest(path)}
                (root / "manifest.json").write_text(json.dumps(manifest))
                with self.assertRaisesRegex(EvidenceError, "Reviewed qualification evidence changed"):
                    validate_stored_corpus(root)


if __name__ == "__main__":
    unittest.main()

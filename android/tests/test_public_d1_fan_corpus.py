"""Offline admission of native-only fan collision fault/control evidence.

These bounded evidence checks never admit the aborted imported replay or claim
full raw parity. No engine, asset installation or retained scratch is required
"""

import copy
import gzip
import hashlib
import re
import unittest
from pathlib import Path

import check_public_effect_phase as phase
import check_public_fan_transparency as fan
import compact_public_replay as compact
import d1_replay_parity as parity

ROOT = Path(__file__).parent / "fixtures/public_d1_fan_probes"
REVISION = "03f9c85239dbbaeae668693bb82186452179bbd8"
TREE = "9dabc259cdd19c143dc00e082101a75688acc6d6"
CASES = {
    "d1-fan-transparency": ("positive", "known-imported-fan-transparency-fault", 56604, 66992),
    "d1-fan-no-fire": ("no-fire", "causal-no-fire-control", 56570, 62181),
    "d1-fan-delay": ("delay", "causal-delayed-flare-control", 56604, 66784),
}
BACKENDS = ("console", "headed")
MAX_EVIDENCE_BYTES = 8 * 1024 * 1024


class PublicD1FanCorpusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = phase.load_json(ROOT / "manifest.json")
        cls.cases = {case["capture_id"]: case for case in cls.manifest["cases"]}

    def raw(self, name):
        path = ROOT / name
        self.assertFalse(path.is_symlink(), name)
        self.assertTrue(path.resolve().is_relative_to(ROOT.resolve()), name)
        opener = gzip.open if path.suffix == ".gz" else open
        with opener(path, "rb") as stream:
            raw = stream.read(MAX_EVIDENCE_BYTES + 1)
        self.assertLessEqual(len(raw), MAX_EVIDENCE_BYTES, name)
        return raw

    def read(self, name):
        return compact._json(self.raw(name))

    def detail(self, name):
        return self.read(self.cases[name]["detail_oracle"])

    def test_exact_cases_remain_bounded_diagnostics(self):
        manifest = self.manifest
        self.assertEqual(manifest["schema"], "public-d1-fan-corpus-v1")
        self.assertEqual(manifest["max_input_and_oracle_bytes"], 100000)
        self.assertEqual((manifest["capture_revision"], manifest["capture_tree"]), (REVISION, TREE))
        self.assertEqual(manifest["checker_revision"], REVISION)
        self.assertEqual(manifest["asset_sha256"], compact.pinned_assets("d1"))
        self.assertIs(manifest["public_replay_admission"], False)
        self.assertEqual(
            manifest["known_fault"],
            {
                "name": "animation startup phase causes different fan transparency collision",
                "effect_id": 3,
                "segment": 135,
                "side": 4,
                "wall": 5,
                "child_segment": 142,
                "base_texture": 83,
                "overlay_texture": 329,
                "flare_slot": 77,
                "flare_signature": 179,
                "flare_radius": 26709,
                "request_frame": 299,
                "causal_sim_frame": 300,
                "first_physical_boundary": 301,
                "imported_abort_before_input": 301,
                "native_position": [8216206, -2905300, 30439592],
                "imported_position": [8208474, -3156161, 30042682],
                "native_velocity": [129480, 4201080, 6646920],
                "imported_velocity": [0, 0, 0],
                "native_bitmap": 823,
                "imported_bitmap": 824,
                "exact_impact_uv": [-17056, 35286],
                "exact_impact_texel": [47, 34],
                "native_pixel": 255,
                "imported_pixel": 59,
                "summary_rng_prefix_frames_equal": 301,
                "imported_extra_rng_calls": [345, 346],
                "imported_extra_rng_state": [2301841099, 2296157864],
                "raw_identity_and_sim_context": "failed and retained",
                "not_a_full_parity_pass": True,
            },
        )
        self.assertEqual(len(manifest["cases"]), 3)
        self.assertEqual({case["id"] for case in manifest["cases"]}, set(CASES))
        self.assertEqual(set(self.cases), {"positive", "no-fire", "delay"})
        for case in manifest["cases"]:
            with self.subTest(case=case["id"]):
                capture, role, input_bytes, detail_bytes = CASES[case["id"]]
                self.assertEqual((case["capture_id"], case["role"]), (capture, role))
                self.assertEqual(tuple(case[k] for k in ("frames", "level", "difficulty")), (324, 1, 0))
                self.assertEqual(case["full_imported_parity"], "unqualified")
                self.assertEqual(case["native_backends"], ["headless-console", "headed"])
                self.assertEqual(case["imported_backend"], "headed")
                self.assertEqual(
                    case["expected_imported_outcome"],
                    "strict-carried-rng-abort-before-input301" if capture == "positive" else "complete324",
                )
                for key in ("input", "summary_oracle", "detail_oracle"):
                    self.assertIn(case[key], manifest["files"])
                    self.assertLessEqual((ROOT / case[key]).stat().st_size, 100000)
                self.assertEqual((ROOT / case["input"]).stat().st_size, input_bytes)
                self.assertEqual((ROOT / case["detail_oracle"]).stat().st_size, detail_bytes)

    def test_inventory_hashes_include_uncompressed_evidence(self):
        inventory = self.manifest["files"]
        actual = {p.relative_to(ROOT).as_posix() for p in ROOT.rglob("*") if p.is_file()}
        self.assertEqual(actual - {"manifest.json", "README.md"}, set(inventory))
        for name, expected in inventory.items():
            with self.subTest(file=name):
                path = ROOT / name
                raw = self.raw(name)
                self.assertEqual(path.stat().st_size, expected["bytes"])
                self.assertEqual(parity.digest(path), expected["sha256"])
                if path.suffix == ".gz":
                    self.assertEqual(len(raw), expected["uncompressed_bytes"])
                    self.assertEqual(hashlib.sha256(raw).hexdigest(), expected["uncompressed_sha256"])

    def test_compact_inputs_bind_immutable_native_capture_and_build(self):
        build_name = "provenance/native-build.json"
        build = self.read(build_name)
        self.assertEqual((build["revision"], build["tree"], build["recorder_stamp"]), (REVISION, TREE, REVISION[:9]))
        executable = build["targets"]["temp/native-d1-build/main/test_upstream_compat"]["sha256"]
        for name, case in self.cases.items():
            with self.subTest(case=name):
                header, summary = compact.validate(ROOT / case["input"], ROOT / case["summary_oracle"])
                self.assertEqual(
                    tuple(header[k] for k in ("version", "game", "mission", "start_mode", "rng_mode")),
                    (5, "d1", "d1", "new_level", "lcg_state"),
                )
                self.assertEqual(header["git_version"], REVISION[:9])
                self.assertEqual(tuple(header[k] for k in ("frame_count", "level", "difficulty")), (324, 1, 0))
                self.assertEqual(header["view_projection"]["missile_view_enabled"], 0)
                prefix = "provenance/" + name
                source = self.read(prefix + ".json")
                helper_name = prefix + ".capture-manifest.json"
                helper = self.read(helper_name)
                self.assertEqual((source["engine_source_revision"], source["source_tree"]), (REVISION, TREE))
                self.assertIs(source["engine_inputs_unchanged"], True)
                self.assertIs(source["capture_files_verified"], True)
                self.assertEqual(source["actual_recorder_stamp"], REVISION[:9])
                self.assertEqual(source["executable_sha256"], executable)
                self.assertEqual(source["capture_manifest_sha256"], parity.digest(ROOT / helper_name))
                self.assertEqual(source["build_provenance_sha256"], parity.digest(ROOT / build_name))
                self.assertEqual(source["source_recipe_sha256"], helper["source_recipe_sha256"])
                self.assertEqual(helper["source_recipe_sha256"]["android/tests/public_fan_fault_fixture.hpp"], "40eb1f6fed8757d1eeaea4bc85967227afc504291f44c805ccfd087b21772c2b")
                self.assertEqual(source["asset_sha256"], compact.pinned_assets("d1"))
                self.assertEqual(
                    (helper["status"], helper["stage"], helper["capture_invoked"]), ("candidate", "complete", True)
                )
                self.assertEqual(helper["source_revision"], REVISION)
                self.assertEqual(helper["capture_executable_sha256"], executable)
                self.assertEqual(helper["recorder_git_version"], REVISION[:9])
                self.assertEqual(helper["recording_header"], header)
                self.assertEqual(helper["scenario"], "fan-transparency")
                self.assertIs(helper["pixel_observation"], True)
                self.assertIs(helper["screenshots"], True)
                self.assertEqual((helper["no_flare"], helper["flare_delay"]), (name == "no-fire", int(name == "delay")))
                self.assertEqual(
                    summary["provenance"],
                    {
                        "source_revision": REVISION,
                        "capture_executable_sha256": executable,
                        "capture_source_sha256": helper["source_recipe_sha256"][
                            "android/tests/public_fan_fault_fixture.hpp"
                        ],
                        "asset_sha256": compact.pinned_assets("d1"),
                    },
                )
                for packed, captured in (
                    (case["input"], "candidate.dximdemo"),
                    (case["summary_oracle"], "candidate.oracle.json"),
                ):
                    self.assertEqual(self.manifest["files"][packed], helper["files"][captured])
                self.assertEqual(summary["full_recording_sha256"], helper["files"]["raw/capture.dximdemo"]["sha256"])

    def test_detail_oracles_are_native_only_and_match_summary_and_input(self):
        for name, case in self.cases.items():
            with self.subTest(case=name):
                oracle = self.detail(name)
                summary = self.read(case["summary_oracle"])
                demo = fan.validate_companion(oracle, ROOT / case["input"])
                capture = fan.validate_oracle(oracle)
                live_name = f"evidence/{name}.capture.json.gz"
                live = fan.validate_capture(self.read(live_name))
                helper = self.read(f"provenance/{name}.capture-manifest.json")
                provenance = self.read(f"provenance/{name}.json")
                self.assertEqual(oracle["header"], demo["header"])
                self.assertEqual(oracle["provenance"], {k: provenance[k] for k in oracle["provenance"]})
                self.assertEqual(capture["capture_role"], "native_d1_reference")
                self.assertIs(capture["public_replay_admission"], False)
                self.assertEqual((capture["no_fire"], capture["fire_delay"]), (name == "no-fire", int(name == "delay")))
                self.assertEqual(capture["armed_frame"], 299)
                self.assertEqual(oracle["metadata"], {k: live[k] for k in fan.META})
                self.assertEqual(
                    oracle["observations"],
                    [
                        [
                            row["fan_frame"],
                            row["fan_timer"],
                            row["fan_bitmap"],
                            row.get("flare"),
                            row.get("pixel_probe"),
                            row.get("collision_pixel_probe"),
                        ]
                        for row in live["observations"]
                    ],
                )
                self.assertEqual(hashlib.sha256(self.raw(live_name)).hexdigest(), oracle["source"]["capture_sha256"])
                self.assertEqual(oracle["source"]["compact_sha256"], summary["compact_sha256"])
                self.assertEqual(
                    oracle["source"]["capture_provenance_sha256"], parity.digest(ROOT / f"provenance/{name}.json")
                )
                for field, captured in (
                    ("capture_sha256", "raw/capture.json"),
                    ("full_recording_sha256", "raw/capture.dximdemo"),
                    ("capture_rng_sha256", "raw/capture.dximdemo.rngtrace.jsonl"),
                ):
                    self.assertEqual(oracle["source"][field], helper["files"][captured]["sha256"])
                checks = oracle["recording_checks"]
                self.assertEqual(checks["full_frame_sha256"], summary["frame_sha256"])
                self.assertEqual(checks["simulation_rng"]["sha256"], summary["simulation_rng_sha256"])
                self.assertEqual(checks["simulation_rng"]["events"], summary["simulation_rng_events"])
                self.assertEqual(checks["simulation_rng"]["raw_sha256"], oracle["source"]["capture_rng_sha256"])
                self.assertEqual(checks["simulation_rng"]["carried_state"], demo["batches"][-1]["post_rng"])
                self.assertEqual(compact.sha(oracle["terminal"]), summary["terminal_sha256"])

    def test_both_causal_controls_change_only_the_flare_pulse(self):
        names = ("positive", "no-fire", "delay")
        oracles = [self.detail(name) for name in names]
        paths = [ROOT / self.cases[name]["input"] for name in names]
        self.assertEqual(
            fan.causal_gate(*[v for pair in zip(oracles, paths) for v in pair]),
            self.read("evidence/causal-inputs.json"),
        )
        demos = [fan.validate_companion(oracle, path) for oracle, path in zip(oracles, paths)]
        self.assertEqual(demos[0]["header"], demos[1]["header"])
        self.assertEqual(demos[0]["header"], demos[2]["header"])
        controls = [
            [
                {k: copy.deepcopy(v) for k, v in batch.items() if k not in ("pre_rng", "post_rng")}
                for batch in demo["batches"]
            ]
            for demo in demos
        ]
        omission = {"path": "$.batches[299].ops[0].p.fl", "expected": 1, "actual": 0}
        delayed = {"path": "$.batches[300].ops[0].p.fl", "expected": 0, "actual": 1}
        self.assertEqual(list(parity.field_differences(controls[0], controls[1], "$.batches")), [omission])
        self.assertEqual(list(parity.field_differences(controls[0], controls[2], "$.batches")), [omission, delayed])
        for changed in oracles[1:]:
            with self.assertRaises(parity.EvidenceError):
                fan.validate_companion(changed, paths[0])

    def test_observation_controls_are_byte_neutral(self):
        positive = self.read("provenance/positive.capture-manifest.json")
        disabled = self.read("provenance/positive-observation-off.capture-manifest.json")
        self.assertIs(disabled["pixel_observation"], False)
        self.assertIs(disabled["screenshots"], False)
        self.assertEqual(disabled["source_revision"], REVISION)
        self.assertEqual(disabled["recording_header"], positive["recording_header"])
        self.assertEqual(disabled["source_recipe_sha256"], positive["source_recipe_sha256"])
        self.assertEqual(disabled["capture_executable_sha256"], positive["capture_executable_sha256"])
        for name in (
            "raw/capture.dximdemo",
            "raw/capture.dximdemo.rngtrace.jsonl",
            "candidate.dximdemo",
            "candidate.oracle.json",
        ):
            self.assertEqual(disabled["files"][name], positive["files"][name])

    def test_native_pixel_and_collision_controls_remain_distinct(self):
        positive, no_fire, delayed = [
            fan.validate_oracle(self.detail(name)) for name in ("positive", "no-fire", "delay")
        ]
        self.assertTrue(all(row.get("flare") is None for row in no_fire["observations"]))
        passed = positive["observations"][300]
        self.assertEqual((passed["fan_frame"], passed["flare"]["segment"]), (0, 142))
        self.assertEqual(passed["flare"]["position"], [8216206, -2905300, 30439592])
        self.assertEqual(passed["flare"]["velocity"], [129480, 4201080, 6646920])
        stopped = delayed["observations"][301]
        self.assertEqual((stopped["fan_frame"], stopped["flare"]["segment"]), (1, 135))
        self.assertEqual(stopped["flare"]["velocity"], [0, 0, 0])
        self.assertNotEqual(
            positive["observations"][299]["flare"]["position"], delayed["observations"][300]["flare"]["position"]
        )
        for frame, pixel in ((300, 255), (301, 59)):
            observed = positive["observations"][frame]["collision_pixel_probe"]
            self.assertEqual(observed["point"], [8208474, -3156161, 30042682])
            self.assertEqual((observed["x"], observed["y"], observed["pixel"]), (47, 34, pixel))
            self.assertEqual(observed["native_crossable"], int(pixel == 255))

    def test_replay_manifest_binds_both_native_backends_and_matched_import(self):
        build = self.read("provenance/native-build.json")
        replay = self.read("provenance/replay-manifest.json")
        profile = self.read("provenance/comparison-profile.json")
        self.assertEqual(replay["source_revision"], REVISION)
        self.assertEqual(replay["build_provenance_sha256"], parity.digest(ROOT / "provenance/native-build.json"))
        self.assertEqual(profile["source_revision"], REVISION)
        self.assertEqual(
            profile["executable_sha256"], build["targets"]["temp/native-d2-build/main/test_upstream_compat"]["sha256"]
        )
        writer = profile["writer_report"]
        self.assertEqual(writer["profile"], "d1match")
        self.assertIs(writer["native_readback_verified"], True)
        self.assertIs(writer["text_profile_unchanged"], True)
        self.assertEqual(writer["binary_changes"], [{"comparison": 0, "default": 1, "offset": 15}])
        self.assertEqual((writer["default_missile_view"], writer["comparison_missile_view"]), (1, 0))
        self.assertEqual(self.read("profile/comparison-profile.json"), writer)
        self.assertEqual(self.manifest["comparison_profile"]["pilot"], "d1match")
        self.assertIs(self.manifest["comparison_profile"]["production_default_unchanged"], True)
        for name in ("comparison-profile.json", "default.plr", "d1match.plr", "default.plx", "d1match.plx"):
            self.assertEqual(self.manifest["files"]["profile/" + name], profile["files"][name])
        default, matched = self.raw("profile/default.plr"), self.raw("profile/d1match.plr")
        self.assertEqual(len(default), len(matched))
        self.assertEqual(
            [
                {"offset": i, "default": left, "comparison": right}
                for i, (left, right) in enumerate(zip(default, matched))
                if left != right
            ],
            writer["binary_changes"],
        )
        self.assertEqual(self.raw("profile/default.plx"), self.raw("profile/d1match.plx"))
        self.assertEqual(len(replay["runs"]), 9)
        self.assertEqual(
            {(run["case"], run["backend"]) for run in replay["runs"]},
            {(name, backend) for name in self.cases for backend in (*BACKENDS, "imported")},
        )
        for run in replay["runs"]:
            with self.subTest(case=run["case"], backend=run["backend"]):
                imported = run["backend"] == "imported"
                expected_exit = int(imported and run["case"] == "positive")
                self.assertEqual((run["returncode"], run["expected_returncode"]), (expected_exit, expected_exit))
                self.assertEqual(run["executable_sha256"], build["targets"][run["executable"]]["sha256"])
                command = run["command"]
                self.assertIn("-RequireCleanExit", command)
                self.assertEqual(command[command.index("-Game") + 1], "d2" if imported else "d1")
                self.assertEqual(
                    command[command.index("-Runner") + 1],
                    "headless-console" if run["backend"] == "console" else "visual",
                )
                self.assertEqual(
                    command[command.index("-DemoPath") + 1], f"temp/final-fan-captures/{run['case']}/candidate.dximdemo"
                )
                if imported:
                    self.assertIn("-D1InD2", command)
                    self.assertEqual(command[command.index("-Pilot") + 1], "d1match")
        self.assertNotIn("positive-imported/result.json", replay["files"])
        for name in self.cases:
            for file in ("state.jsonl.gz", "rng.jsonl", "result.json"):
                self.assertEqual(replay["files"][f"{name}-console/{file}"], replay["files"][f"{name}-headed/{file}"])

    def test_all_pair_reports_retain_fault_control_and_full_raw_failure(self):
        replay = self.read("provenance/replay-manifest.json")
        summary = self.read("evidence/final-pair-summary.json")
        self.assertEqual(len(summary["pairs"]), 6)
        self.assertEqual(
            {(p["case"], p["native_backend"]) for p in summary["pairs"]},
            {(name, backend) for name in self.cases for backend in BACKENDS},
        )
        for item in summary["pairs"]:
            name, backend = item["case"], item["native_backend"]
            with self.subTest(case=name, backend=backend):
                oracle = self.detail(name)
                report_name = f"evidence/{name}.{backend}.pair.json.gz"
                report = self.read(report_name)
                count = 301 if name == "positive" else 324
                self.assertEqual(item["report"], f"{name}.{backend}.pair.json")
                self.assertEqual(item["sha256"], hashlib.sha256(self.raw(report_name)).hexdigest())
                self.assertEqual(report["schema"], fan.SCHEMA + "-report")
                self.assertIs(report["public_replay_admission"], False)
                self.assertEqual(
                    report["status"], "known_fault_reproduced" if name == "positive" else "control_reproduced"
                )
                self.assertEqual(report["oracle_sha256"], fan.sha(oracle))
                self.assertEqual(report["source"], oracle["source"])
                self.assertEqual(report["provenance"], oracle["provenance"])
                self.assertEqual(report["metadata"], {**oracle["metadata"], "observations": None})
                self.assertEqual(report["native_live"], "pass")
                self.assertEqual(report["gameplay_rng_prefix"], {"status": "pass", "frames": count})
                self.assertEqual(
                    report["imported_completion"],
                    {"phase": "aborted" if name == "positive" else "terminal", "frames": count, "expected_frames": 324},
                )
                self.assertEqual(report["imported_exit_code"], int(name == "positive"))
                rng = report["simulation_rng_value_prefix"]
                self.assertEqual(
                    (rng["status"], rng["events"]), ("pass", {"positive": 345, "no-fire": 344, "delay": 346}[name])
                )
                self.assertIn("raw SIM RNG equality remains separately reported", rng["permitted_reference_difference"])
                self.assertEqual(
                    report["trace_sha256"],
                    {
                        "native": replay["files"][f"{name}-{backend}/state.jsonl.gz"]["sha256"],
                        "imported": replay["files"][f"{name}-imported/state.jsonl.gz"]["sha256"],
                    },
                )
                self.assertEqual(report["run_log_sha256"], replay["files"][f"{name}-imported/run.log"]["sha256"])
                for field in (
                    "status",
                    "native_live",
                    "gameplay_rng_prefix",
                    "simulation_rng_value_prefix",
                    "imported_completion",
                ):
                    self.assertEqual(item[field], report[field])
                raw = report["raw_comparison"]
                self.assertEqual((raw["status"], item["raw_status"]), ("fail", "fail"))
                for field, expected_count in zip(
                    ("objects_and_fan", "full_diagnostics", "simulation_rng"),
                    {"positive": (916, 301, 37), "no-fire": (974, 324, 35), "delay": (999, 324, 35)}[name],
                ):
                    difference = raw[field]
                    self.assertEqual((difference["status"], difference["mismatch_count"]), ("fail", expected_count))
                    self.assertEqual(len(difference["differences"]), expected_count)
                    self.assertEqual(difference["first_mismatch"], difference["differences"][0])
                if name != "positive":
                    self.assertNotIn("collision_probe", report)

    def test_positive_fault_has_exact_physical_and_rng_fingerprint(self):
        report = self.read("evidence/positive.console.pair.json.gz")
        replay = self.read("provenance/replay-manifest.json")
        log = self.raw("evidence/positive-imported.run.log.gz")
        self.assertEqual(hashlib.sha256(log).hexdigest(), report["run_log_sha256"])
        self.assertEqual(
            re.findall(r"Input demo replay stopped: ([^\r\n]+)", log.decode("utf-8")),
            ["carried SIM RNG differs before input at frame 301"],
        )
        self.assertIn("imported aborted301 is before input301", report["alignment"])
        self.assertIn("clocks are not equated", report["alignment"])
        self.assertIn("hits pixel46,34, unlike positive47,34", report["scope"])
        self.assertEqual(
            report["collision_probe"]["sha256"], replay["files"]["positive-imported/collision-probe.log"]["sha256"]
        )
        probes = report["collision_probe"]["target_rows"]
        self.assertEqual(fan.probe_evidence(ROOT / "evidence/collision-probe-excerpt.txt")["target_rows"], probes)
        self.assertEqual(set(probes), {"trans_wall_pixel", "physics_fvi_fate", "awareness_rng_gate"})
        for row in probes.values():
            self.assertEqual(
                tuple(row[k] for k in ("frame", "gt", "obj", "sig", "id")), ("300", "1232896", "77", "179", "9")
            )
        pixel = probes["trans_wall_pixel"]
        self.assertEqual(
            tuple(pixel[k] for k in ("seg", "side", "u", "v", "bmx", "bmy", "direct", "gpixel", "transparent", "p")),
            ("135", "4", "-17056", "35286", "47", "34", "59", "59", "0", "(8208474,-3156161,30042682)"),
        )
        fate = probes["physics_fvi_fate"]
        self.assertEqual(
            tuple(fate[k] for k in ("fate", "child", "size", "parent", "parent_sig", "target", "vel")),
            ("wall", "142", "26709", "0", "0", "(8216206,-2905300,30439592)", "(129480,4201080,6646920)"),
        )
        rng = probes["awareness_rng_gate"]
        self.assertEqual(
            tuple(rng[k] for k in ("source", "source_obj", "aux_obj", "calls", "state", "added")),
            ("collide_weapon_wall", "77", "135", "345->346", "2301841099->2296157864", "1"),
        )
        actual = [d for d in report["raw_comparison"]["objects_and_fan"]["differences"] if d["trace_frame"] == 301]
        expected = [
            ("$.flare.next", -1, 75),
            *[(f"$.flare.physics.velocity[{i}]", value, 0) for i, value in enumerate((129480, 4201080, 6646920))],
            *[
                (f"$.flare.pos[{i}]", left, right)
                for i, (left, right) in enumerate(zip((8216206, -2905300, 30439592), (8208474, -3156161, 30042682)))
            ],
            ("$.flare.segnum", 142, 135),
            ("$.flare.weapon.parent_signature", 1, 0),
            ("$.player.signature", 1, 0),
            ("$.fan_effect.frame_count", 0, 1),
            ("$.fan_effect.wall_bitmap", 823, 824),
        ]
        self.assertEqual(
            actual,
            [{"trace_frame": 301, "path": path, "expected": left, "actual": right} for path, left, right in expected],
        )

    def test_actual_checker_rejections_bind_unchanged_final_evidence(self):
        report = self.read("evidence/checker-rejections.json")
        replay = self.read("provenance/replay-manifest.json")
        self.assertEqual(report["schema"], "public-d1-fan-checker-rejections-v1")
        self.assertEqual(report["status"], "all_expected_rejections")
        self.assertEqual(report["source_revision"], REVISION)
        self.assertEqual(report["matched_positive"], "known_fault_reproduced")
        self.assertIs(report["raw_evidence_unchanged"], True)
        self.assertIs(report["controls_are_separate_live_captures"], True)
        expected = {
            "wrong-phase": "Wrong imported collision phase",
            "wrong-magnitude": "Wrong imported collision magnitude/trajectory",
            "absent-physical-fault": "Wrong imported collision magnitude/trajectory",
            "no-fire-is-not-positive-fault": "Unexpected imported process exit",
            "delay-is-not-positive-fault": "Unexpected imported process exit",
        }
        self.assertEqual(len(report["checks"]), len(expected))
        self.assertEqual({check["id"] for check in report["checks"]}, set(expected))
        for check in report["checks"]:
            self.assertEqual((check["status"], check["error"]), ("rejected", expected[check["id"]]))
            if check["id"].endswith("is-not-positive-fault"):
                self.assertEqual(
                    check["scope"], "Actual complete unmodified control execution supplied to positive fault gate"
                )
                self.assertIn("preserve its actual exit0", check["intervention"])
            else:
                self.assertEqual(
                    check["scope"],
                    "Synthetic in-memory checker mutation of the verified final decoded imported trace; raw evidence and oracle unchanged",
                )
        sources = {
            "temp/final-fan-analysis/positive.detail.oracle.json": self.manifest["files"][
                self.cases["positive"]["detail_oracle"]
            ],
            "temp/final-fan-captures/positive/candidate.dximdemo": self.manifest["files"][
                self.cases["positive"]["input"]
            ],
        }
        sources.update(
            {
                f"temp/final-fan-replays/{name}-{backend}/{file}": replay["files"][f"{name}-{backend}/{file}"]
                for name in self.cases
                for backend in ("console", "imported")
                for file in ("state.jsonl.gz", "rng.jsonl", "run.log")
            }
        )
        self.assertEqual(report["inputs"], sources)

    def test_independent_audits_retain_complete_passing_checks(self):
        capture = self.read("evidence/independent-capture-audit.json")
        detail = self.read("evidence/independent-detail-audit.json")
        self.assertEqual((capture["revision"], capture["tree"]), (REVISION, TREE))
        for audit, minimum in ((capture, 90), (detail, 39)):
            self.assertEqual(audit["status"], "pass")
            self.assertGreaterEqual(len(audit["checks"]), minimum)
            self.assertEqual(len({c["check"] for c in audit["checks"]}), len(audit["checks"]))
            self.assertTrue(all(c["pass"] is True for c in audit["checks"]))
        capture_checks = {c["check"] for c in capture["checks"]}
        self.assertTrue({"published source tree and parent", "remote retained capture ref", "published source identical to reviewed source tree", "d1 actual fixture/recorder/native effects compile labels", "d2 actual fixture/recorder/native effects compile labels", "actual D1 shareware asset hashes pinned", "native matched pilot only documented camera byte differs", "matched pilot source and writer identity", "copied matched pilot bytes unchanged", "nine replay process outcomes complete", "replay build provenance exact"} <= capture_checks)
        checks = {c["check"]: c for c in detail["checks"]}
        for name in self.cases:
            for suffix in ("actual source recipe hashes", "actual raw compact manifest headers identical", "compact contains no telemetry/state/save/checkpoint or opaque payload", "independent native-only oracle reconstruction", "native headed/console complete raw equality"):
                self.assertIn(f"{name} {suffix}", capture_checks)
            for suffix in ("detail source/provenance bridge hashes", "detail exact native observations and terminal", "independent detail frame and carried RNG fingerprints", "independent detail decoded control hash", "independent native replay matches entire live full+gameplay frames", "native replay whole RNG matches native capture", "report source integrity and raw failure retained"):
                self.assertIn(f"{name} {suffix}", checks)
            completed = checks[f"{name} imported complete expected prefix and boundaries"]["details"]
            count, phase_name = (301, "aborted") if name == "positive" else (324, "terminal")
            for kind in ("frame_state", "object_state", "world_state"):
                self.assertEqual(completed["counts"][kind], count)
            self.assertEqual(
                completed["boundaries"],
                [
                    [kind, phase, frame]
                    for phase, frame in (("restored", 0), (phase_name, count))
                    for kind in ("object_boundary", "world_boundary")
                ],
            )
            rng = checks[f"{name} imported RNG value prefix independently equal"]["details"]
            self.assertIs(rng["raw_exact"], False)
            self.assertEqual(rng["events"], {"positive": 345, "no-fire": 344, "delay": 346}[name])
        event = checks["positive imported extra wall collision RNG exact carried event"]["details"]
        self.assertEqual(
            event,
            {
                "type": "rand",
                "frame": 300,
                "gt": 1232896,
                "call_count": 346,
                "ctx_obj": 77,
                "ctx_sig": 179,
                "ctx_id": 9,
                "state_before": 2301841099,
                "state_after": 2296157864,
                "result": 2268,
            },
        )


if __name__ == "__main__":
    unittest.main()

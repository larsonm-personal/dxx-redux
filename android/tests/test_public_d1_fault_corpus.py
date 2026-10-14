"""Offline admission of immutable, ordinary-start D1 known-fault evidence.

Passing these data checks is not an imported replay or full-parity pass
"""

import copy
import gzip
import hashlib
import unittest
from pathlib import Path

import check_public_effect_phase as phase
import check_public_mine_blast as mine
import compact_public_replay as compact
import d1_replay_parity as parity
import ordered_public_replay as ordered

ROOT = Path(__file__).parent / "fixtures/public_d1_fault_probes"
REVISION = "8b19ab9ff29c5f54d815cc735b37c8e1ddf3c3fb"
TREE = "5d9d3efb3b07d54fec189eaef6ec8dfaa7355fb8"
CASES = {
    "d1-monitor-phase": (181, 1, 2, "known-imported-animation-startup-fault"),
    "d1-self-splash": (139, 1, 2, "passing-bounded-gameplay-coverage"),
    "d1-self-splash-no-fire": (139, 1, 2, "causal-no-fire-control"),
    "d1-mine-blast": (424, 3, 0, "known-imported-mine-force-fault"),
    "d1-mine-no-concussion": (424, 3, 0, "causal-no-concussion-control"),
}


class PublicD1FaultCorpusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = phase.load_json(ROOT / "manifest.json")

    def read(self, name):
        path = ROOT / name
        opener = gzip.open if path.suffix == ".gz" else open
        with opener(path, "rb") as stream:
            raw = stream.read(1000001)
        self.assertLessEqual(len(raw), 1000000, name)
        return compact._json(raw)

    def test_exact_cases_remain_bounded_diagnostics(self):
        manifest = self.manifest
        self.assertEqual(manifest["schema"], "public-d1-fault-corpus-v1")
        self.assertEqual(manifest["max_input_and_oracle_bytes"], 100000)
        self.assertEqual(manifest["capture_revision"], REVISION)
        self.assertEqual(manifest["capture_tree"], TREE)
        self.assertEqual(manifest["asset_sha256"], compact.pinned_assets("d1"))
        self.assertEqual(len(manifest["cases"]), 5)
        self.assertEqual({case["id"] for case in manifest["cases"]}, set(CASES))
        for case in manifest["cases"]:
            with self.subTest(case=case["id"]):
                self.assertEqual(
                    tuple(case[key] for key in ("frames", "level", "difficulty", "role")), CASES[case["id"]]
                )
                self.assertEqual(case["full_imported_parity"], "unqualified")
                self.assertEqual(case["native_backends"], ["headless-console", "headed"])
                self.assertEqual(case["imported_backend"], "headed")
                for key in ("input", "summary_oracle", "detail_oracle"):
                    if key in case:
                        self.assertIn(case[key], manifest["files"])
                        self.assertLessEqual((ROOT / case[key]).stat().st_size, 100000)
        for fault in manifest["known_faults"].values():
            self.assertIs(fault["not_a_full_parity_pass"], True)
        self.assertEqual(manifest["known_faults"]["animation_startup_phase"]["collision_consequences"], "unproven")

    def test_inventory_hashes_include_uncompressed_evidence(self):
        inventory = self.manifest["files"]
        actual = {path.relative_to(ROOT).as_posix() for path in ROOT.rglob("*") if path.is_file()}
        self.assertEqual(actual - {"manifest.json", "README.md"}, set(inventory))
        for name, expected in inventory.items():
            with self.subTest(file=name):
                path = ROOT / name
                self.assertFalse(path.is_symlink())
                self.assertEqual(path.stat().st_size, expected["bytes"])
                self.assertEqual(parity.digest(path), expected["sha256"])
                if path.suffix == ".gz":
                    with gzip.open(path, "rb") as stream:
                        raw = stream.read(1000001)
                    self.assertLessEqual(len(raw), 1000000)
                    self.assertEqual(len(raw), expected["uncompressed_bytes"])
                    self.assertEqual(hashlib.sha256(raw).hexdigest(), expected["uncompressed_sha256"])

    def test_compact_inputs_bind_immutable_native_live_provenance(self):
        verified = self.read("evidence/final-live-validation.json")
        build = self.read("provenance/native-build.json")
        self.assertEqual(
            (verified["status"], verified["source_revision"], verified["source_tree"]), ("pass", REVISION, TREE)
        )
        self.assertEqual((build["revision"], build["tree"]), (REVISION, TREE))
        executable = build["targets"]["temp/native-d1-build/main/test_upstream_compat"]["sha256"]
        for case in self.manifest["cases"]:
            with self.subTest(case=case["id"]):
                header, oracle = compact.validate(ROOT / case["input"], ROOT / case["summary_oracle"])
                self.assertEqual(
                    (header["version"], header["game"], header["mission"], header["start_mode"]),
                    (5, "d1", "d1", "new_level"),
                )
                self.assertEqual(header["git_version"], REVISION[:9])
                self.assertEqual((header["frame_count"], header["level"], header["difficulty"]), CASES[case["id"]][:3])
                prefix = "provenance/" + case["capture_id"]
                source = self.read(prefix + ".json")
                helper_name = prefix + ".capture-manifest.json"
                helper = self.read(helper_name)
                self.assertEqual(source["engine_source_revision"], REVISION)
                self.assertEqual(source["source_tree"], TREE)
                self.assertIs(source["engine_inputs_unchanged"], True)
                self.assertEqual(source["executable_sha256"], executable)
                self.assertEqual(source["helper_manifest_sha256"], parity.digest(ROOT / helper_name))
                self.assertEqual(source["source_recipe_sha256"], helper["source_recipe_sha256"])
                self.assertEqual(
                    (helper["status"], helper["stage"], helper["capture_invoked"]), ("candidate", "complete", True)
                )
                self.assertEqual(helper["source_revision"], REVISION)
                self.assertEqual(helper["recording_header"], header)
                fixture = (
                    "public_effect_phase_fixture.hpp"
                    if case["id"] == "d1-monitor-phase"
                    else "public_missile_fault_fixture.hpp"
                )
                self.assertEqual(
                    oracle["provenance"],
                    {
                        "source_revision": REVISION,
                        "capture_executable_sha256": executable,
                        "capture_source_sha256": helper["source_recipe_sha256"]["android/tests/" + fixture],
                        "asset_sha256": compact.pinned_assets("d1"),
                    },
                )
                for packed, captured in (
                    (case["input"], "candidate.dximdemo"),
                    (case["summary_oracle"], "candidate.oracle.json"),
                ):
                    self.assertEqual(self.manifest["files"][packed], helper["files"][captured])
                self.assertEqual(oracle["full_recording_sha256"], helper["files"]["raw/capture.dximdemo"]["sha256"])
                live = verified["captures"][case["capture_id"]]
                self.assertEqual((live["status"], live["final_immutable_hash_recheck"]), ("pass", "pass"))
                self.assertEqual(live["compact_sha256"], oracle["compact_sha256"])
                self.assertEqual(live["live_recording_sha256"], oracle["full_recording_sha256"])
                self.assertEqual(live["live_frame_hash_list_sha256"], compact.sha(oracle["frame_sha256"]))

    def test_detail_oracles_are_native_only_and_bound_to_companions(self):
        for case in self.manifest["cases"]:
            if "detail_oracle" not in case:
                continue
            with self.subTest(case=case["id"]):
                oracle = self.read(case["detail_oracle"])
                prefix = "provenance/" + case["capture_id"]
                helper = self.read(prefix + ".capture-manifest.json")
                self.assertEqual(oracle["source"]["capture_sha256"], helper["files"]["raw/capture.json"]["sha256"])
                if case["id"] == "d1-monitor-phase":
                    capture = phase.unpack_oracle(oracle)
                    self.assertEqual(capture["capture_role"], "native_d1_reference")
                    self.assertIs(capture["public_replay_admission"], False)
                    self.assertEqual(oracle["source"]["demo_sha256"], helper["files"]["raw/capture.dximdemo"]["sha256"])
                    controls = phase.read_controls(ROOT / case["input"], capture)
                    self.assertEqual(controls["header"], oracle["controls"]["header"])
                    result = phase.compare(oracle, capture, controls)
                    self.assertEqual(result["status"], "pass")
                    self.assertEqual(result["controls"]["status"], "pass")
                    self.assertEqual(result["raw_control_encoding"]["status"], "fail")
                    changed = copy.deepcopy(controls)
                    changed["rows"][0][2]["s"]["f"] += 1
                    self.assertEqual(phase.compare(oracle, capture, changed)["controls"]["status"], "fail")
                else:
                    self.assertEqual(mine.validate_companion(oracle, ROOT / case["input"])["game"], "d1")
                    self.assertEqual(oracle["provenance"]["engine_source_revision"], REVISION)
                    self.assertEqual(
                        oracle["source"]["capture_provenance_sha256"], parity.digest(ROOT / (prefix + ".json"))
                    )
                    self.assertEqual(
                        oracle["source"]["full_recording_sha256"], helper["files"]["raw/capture.dximdemo"]["sha256"]
                    )
                    self.assertEqual(
                        oracle["source"]["capture_rng_sha256"],
                        helper["files"]["raw/capture.dximdemo.rngtrace.jsonl"]["sha256"],
                    )

    def test_causal_pairs_withhold_only_declared_secondary_fire(self):
        for positive, negative, shot in (
            ("d1-mine-blast", "d1-mine-no-concussion", 407),
            ("d1-self-splash", "d1-self-splash-no-fire", 48),
        ):
            with self.subTest(positive=positive):
                rows = [compact._records(ROOT / (name + ".dximdemo"), full=False)[1] for name in (positive, negative)]
                self.assertEqual(rows[0][0], rows[1][0])
                batches = [ordered.validate(r[0], r[1:-2], r[-2]) for r in rows]
                self.assertEqual(batches[0][:shot], batches[1][:shot])
                controls = [
                    [{k: v for k, v in batch.items() if k not in ("pre_rng", "post_rng")} for batch in run]
                    for run in batches
                ]
                differences = list(parity.field_differences(*controls, "$.batches"))
                self.assertEqual(
                    differences,
                    [
                        {"path": f"$.batches[{shot}].{field}.s.f2s", "expected": 1, "actual": 0}
                        for field in ("ops[0]", "residual")
                    ],
                )
        report = self.read("evidence/self-splash.native-causal.json")
        self.assertEqual(
            (report["status"], report["shot_frame_positive"], report["shot_frame_negative"]), ("pass", 48, -1)
        )
        self.assertEqual(report["control_differences"], differences)
        self.assertEqual(report["negative_damage"], [])
        self.assertEqual(report["actual_damage_frame"], 51)

    def test_mine_exact_causal_gate_preserves_raw_failures(self):
        positive = self.read("d1-mine-blast.detail.oracle.json")
        negative = self.read("d1-mine-no-concussion.detail.oracle.json")
        for backend in ("console", "headed"):
            with self.subTest(backend=backend):
                reports = [
                    self.read(f"evidence/{name}.{backend}.pair.json") for name in ("mine-blast", "mine-no-concussion")
                ]
                gate = mine.causal_gate(
                    positive,
                    ROOT / "d1-mine-blast.dximdemo",
                    reports[0],
                    negative,
                    ROOT / "d1-mine-no-concussion.dximdemo",
                    reports[1],
                )
                self.assertEqual(gate, self.read(f"evidence/mine-causal.{backend}.json"))
                self.assertEqual(gate["status"], "known_fault_reproduced")
                self.assertEqual(gate["failures"], [])
                for report in reports:
                    self.assertEqual(report["status"], "fail")
                    self.assertEqual(report["raw_simulation_rng"]["status"], "fail")
                    self.assertEqual(report["imported"]["live_identity"]["status"], "fail")

    def test_monitor_reports_retain_fault_and_reject_negative_controls(self):
        source = self.read("d1-monitor-phase.detail.oracle.json")["source"]
        report = self.read("evidence/monitor.imported-live.json.gz")
        self.assertEqual(report["native_source"], source)
        self.assertEqual(report["status"], "fail")
        self.assertEqual(report["phase"]["mismatch_count"], 1812)
        self.assertEqual(len(report["phase"]["mismatches"]), 1812)
        self.assertEqual(report["known_imported_fault"]["status"], "matched")
        self.assertEqual(report["known_imported_fault"]["comparison_status"], "fail")
        trace = self.read("evidence/monitor.imported.trace.json.gz")
        self.assertEqual((trace["status"], trace["phase_scope"]["mismatch_count"]), ("fail", 728))
        for name, section, count in (("live", "phase", 5), ("console", "phase_scope", 2)):
            wrong = self.read(f"evidence/monitor.wrong-phase.{name}.json")
            self.assertEqual(wrong["native_source"], source)
            self.assertEqual(
                (wrong["status"], wrong[section]["status"], wrong[section]["mismatch_count"]), ("fail", "fail", count)
            )
            self.assertEqual(len(wrong[section]["mismatches"]), count)
            self.assertEqual(
                wrong["first_mismatch"]["path"],
                "$.observations[170].effect24.frame_count" if name == "live" else "$.effects[24].frame_count",
            )
        for name, comparison in (("absent-fault", "pass"), ("wrong-magnitude", "fail")):
            control = self.read(f"evidence/monitor.{name}.control.json.gz")
            self.assertEqual((control["status"], control["comparison_status"]), ("not_matched", comparison))
            self.assertTrue(control["mismatches"])

    def test_native_replay_reports_cover_each_case_and_backend(self):
        reports = self.read("evidence/native-live-parity.json")
        expected = {
            case["capture_id"] + "-" + backend for case in self.manifest["cases"] for backend in ("console", "headed")
        }
        self.assertEqual(set(reports), expected)
        for case in self.manifest["cases"]:
            oracle = self.read(case["summary_oracle"])
            for backend in ("console", "headed"):
                self.assertEqual(
                    reports[case["capture_id"] + "-" + backend],
                    {
                        "status": "pass",
                        "frames_compared": case["frames"],
                        "simulation_events_compared": oracle["simulation_rng_events"],
                    },
                )

    def test_self_splash_gameplay_scope_does_not_promote_raw_parity(self):
        reports = self.read("evidence/self-splash.imported-parity.json.gz")
        self.assertEqual(set(reports), {"self-splash", "self-splash-no-fire"})
        for name, report in reports.items():
            with self.subTest(case=name):
                self.assertEqual(report["status"], "fail")
                self.assertEqual(
                    report["declared_gameplay_scope"],
                    {
                        "status": "pass",
                        "fields": ["f", "ft", "state", "rng"],
                        "frames_compared": 139,
                        "raw_diagnostics_status": "fail",
                        "not_full_parity": True,
                    },
                )
                checks = report["checks"]
                self.assertEqual(checks["terminal_result"]["status"], "pass")
                self.assertEqual(set(checks["frames"]["first_differences"]), {"diag"})
                self.assertEqual(checks["simulation_rng"]["status"], "pass")
                self.assertEqual(
                    checks["simulation_rng"]["simulation_events_compared"],
                    self.read("d1-" + name + ".summary.oracle.json")["simulation_rng_events"],
                )
                for section in ("object_states", "world_states"):
                    self.assertEqual(checks[section]["status"], "fail")
        wrong = self.read("evidence/self-splash.wrong-oracle.json")
        self.assertEqual(set(wrong), {"console", "headed"})
        for report in wrong.values():
            self.assertEqual(report["status"], "rejected")
            self.assertEqual(report["error"], "Replay frame state, RNG or diagnostics differ from live capture")


if __name__ == "__main__":
    unittest.main()

"""Raw monitor-phase oracle rejects shifted expectations and malformed evidence."""

import copy
import gzip
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import check_public_effect_phase as phase


def sample_capture():
    def effect(index, texture, base, frame_time, count):
        return {
            "effect": index,
            "frame_count": 0,
            "time_left": frame_time,
            "frame_time": frame_time,
            "num_frames": count,
            "flags": 0,
            "changing_wall_texture": texture,
            "selected_bitmap": base,
            "selected_bitmap_name": f"monitor{index}#0",
            "frame_bitmap": base,
            "frame_bitmap_name": f"monitor{index}#0",
        }

    initial = {
        "game_time": 0,
        "segment": 0,
        "position": [0, 0, 0],
        "velocity": [0, 0, 0],
        "orientation": {"right": [65536, 0, 0], "up": [0, 65536, 0], "forward": [0, 0, 65536]},
        "rotational_velocity": [0, 0, 0],
        "shields": 6553600,
        "effect24": effect(24, 349, 881, 4369, 9),
        "effect26": effect(26, 352, 891, 10724, 11),
    }
    rows = []
    for frame in range(32):
        row = copy.deepcopy(initial)
        row.update(
            game_time=(frame + 1) * 2184,
            segment=11 if frame else 0,
            f=frame,
            frame_time=2184,
            aim_alignment=65536,
            stable_view=frame >= 2,
            stable_frames=max(0, frame - 1),
            waypoint=7,
        )
        for index, base in ((24, 881), (26, 891)):
            effect = row[f"effect{index}"]
            selected = frame % effect["num_frames"]
            effect.update(
                frame_count=selected,
                selected_bitmap=base + selected,
                frame_bitmap=base + selected,
                selected_bitmap_name=f"monitor{index}#{selected}",
                frame_bitmap_name=f"monitor{index}#{selected}",
            )
        rows.append(row)
    return {
        "game": "d1",
        "mission": "d1",
        "level": 1,
        "scenario": "effect-phase",
        "capture_role": "native_d1_reference",
        "public_replay_admission": False,
        "frame_limit": 240,
        "frames": len(rows),
        "monitor": {"asset": "misc066.abm", "effect": 24, "segment": 11, "side": 0, "texture": 349},
        "adjacent_monitor": {"effect": 26, "segment": 12, "side": 3, "texture": 352},
        "hold": [0, -1310720, 7864320],
        "aim": [2211840, -1310720, 7864320],
        "initial": initial,
        "observations": rows,
        "arrival_frame": 1,
        "first_stable_frame": 2,
        "stable_start_frame": 2,
        "stable_frames": 30,
        "screenshot_frames": [],
        "screenshot_events": [],
        "screenshots_uncropped": True,
        "observation_phase": "post_GameProcessFrame_pre_render",
    }


def sample_controls(capture):
    return {
        "header": {
            "type": "header",
            "version": 5,
            "game": capture["game"],
            "mission": capture["mission"],
            "level": 1,
            "difficulty": 2,
            "start_mode": "new_level",
            "frame_count": capture["frames"],
        },
        "rows": [
            [frame, 2184, {"s": {"f": 2184}}, [{"k": "sample", "ft": 2184, "s": {"f": 2184}, "p": {}}]]
            for frame in range(capture["frames"])
        ]
        + [[capture["frames"], None, {}, []]],
    }


class EffectPhaseTests(unittest.TestCase):
    def setUp(self):
        self.capture = sample_capture()
        self.controls = sample_controls(self.capture)
        self.oracle = phase.make_oracle(self.capture, self.controls, "a" * 64, "b" * 64)

    def compare(self):
        return phase.compare(self.oracle, self.capture, self.controls)

    def test_lossless_native_oracle(self):
        self.assertEqual(phase.unpack_oracle(self.oracle), self.capture)
        self.assertLessEqual(len(phase.encoded(self.oracle)), 100000)
        self.assertEqual(self.compare()["status"], "pass")

    def test_wrong_expected_phase_rejects_native_observation(self):
        # This changes the expected selected/frame phase, not the live capture
        wrong = self.oracle["observations"][0][1]
        for name in ("frame_count", "selected_bitmap", "frame_bitmap"):
            wrong[phase.PHASE_FIELDS.index(name)] += 1
        for name in ("selected_bitmap_name", "frame_bitmap_name"):
            wrong[phase.PHASE_FIELDS.index(name)] = "monitor24#1"
        result = self.compare()
        self.assertEqual(result["status"], "fail")
        self.assertEqual(result["phase"]["mismatch_count"], 5)
        self.assertEqual(result["phase"]["first_mismatch"]["path"], "$.observations[0].effect24.frame_count")
        self.assertEqual(result["camera"]["status"], "pass")
        self.assertEqual(result["controls"]["status"], "pass")

    def test_every_raw_effect_field_is_compared(self):
        for name in phase.PHASE_FIELDS:
            with self.subTest(name=name):
                self.setUp()
                value = self.capture["observations"][0]["effect26"][name]
                self.capture["observations"][0]["effect26"][name] = (
                    value + "wrong" if isinstance(value, str) else value + 1
                )
                result = self.compare()
                self.assertEqual(result["phase"]["mismatch_count"], 1)
                self.assertEqual(result["first_mismatch"]["field"], name)

    def test_known_imported_fault_remains_failure(self):
        self.capture.update(game="d2", mission="descent", capture_role="imported_d1_diagnostic")
        self.controls["header"].update(game="d2", mission="descent")
        for effect in (24, 26):
            self.capture["initial"][f"effect{effect}"]["time_left"] = 0
            for row in self.capture["observations"]:
                row[f"effect{effect}"]["frame_count"] = (row[f"effect{effect}"]["frame_count"] + 1) % row[
                    f"effect{effect}"
                ]["num_frames"]
        result = self.compare()
        self.assertEqual(result["status"], "fail")
        self.assertEqual(result["phase"]["mismatch_count"], 66)
        self.assertEqual(result["first_mismatch"]["expected"], 4369)
        self.assertEqual(result["first_mismatch"]["actual"], 0)
        self.assertEqual(result["camera"]["status"], "pass")
        self.assertEqual(result["controls"]["status"], "pass")

    def test_imported_capture_cannot_create_oracle(self):
        self.capture.update(game="d2", mission="descent", capture_role="imported_d1_diagnostic")
        with self.assertRaisesRegex(phase.parity.EvidenceError, "native D1"):
            phase.make_oracle(self.capture, self.controls, "a" * 64, "b" * 64)

    def test_camera_and_controls_are_independent_failures(self):
        self.capture["observations"][3]["orientation"]["right"][0] += 1
        self.controls["rows"][2][2]["s"]["f"] -= 1
        result = self.compare()
        self.assertEqual(result["phase"]["status"], "pass")
        self.assertEqual(result["camera"]["status"], "fail")
        self.assertEqual(result["controls"]["status"], "fail")
        self.assertEqual(result["status"], "fail")

    def test_malformed_capture_rejected(self):
        for mutation in (
            "missing_effect",
            "missing_field",
            "missing_frame",
            "reorder",
            "bool",
            "name",
            "time",
            "identity",
            "phase",
            "extra_phase",
        ):
            with self.subTest(mutation=mutation):
                self.setUp()
                row = self.capture["observations"][0]
                if mutation == "missing_effect":
                    del row["effect24"]
                elif mutation == "missing_field":
                    del row["effect24"]["selected_bitmap"]
                elif mutation == "missing_frame":
                    self.capture["observations"].pop()
                elif mutation == "reorder":
                    self.capture["observations"][:2] = self.capture["observations"][1::-1]
                elif mutation == "bool":
                    row["effect24"]["frame_count"] = False
                elif mutation == "name":
                    row["effect24"]["frame_bitmap_name"] = None
                elif mutation == "time":
                    row["game_time"] += 1
                elif mutation == "identity":
                    self.capture["mission"] = "descent"
                elif mutation == "phase":
                    self.capture["observation_phase"] = "pre_frame"
                else:
                    row["effect24"]["ignored"] = 99
                with self.assertRaises(phase.parity.EvidenceError):
                    self.compare()

    def test_malformed_oracle_rejected(self):
        for mutation in (
            "fields",
            "row",
            "frame_order",
            "source",
            "control_order",
            "missing_controls",
            "control_identity",
            "control_type",
        ):
            with self.subTest(mutation=mutation):
                self.setUp()
                if mutation == "fields":
                    self.oracle["phase_fields"].pop()
                elif mutation == "row":
                    self.oracle["observations"][0][1].pop()
                elif mutation == "frame_order":
                    self.oracle["observations"][:2] = self.oracle["observations"][1::-1]
                elif mutation == "source":
                    self.oracle["source"]["capture_sha256"] = "unverified"
                elif mutation == "control_order":
                    self.oracle["controls"]["rows"][:2] = self.oracle["controls"]["rows"][1::-1]
                elif mutation == "missing_controls":
                    self.oracle["controls"]["rows"].pop()
                elif mutation == "control_identity":
                    self.oracle["controls"]["header"]["game"] = "d2"
                else:
                    self.oracle["controls"]["rows"][0][2]["s"]["f"] = True
                with self.assertRaises(phase.parity.EvidenceError):
                    self.compare()

    def test_json_missing_duplicate_malformed_and_budget(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.json"
            for text in ('{"x":1,"x":2}', "{", "[NaN]"):
                path.write_text(text, encoding="utf-8")
                with self.assertRaises(phase.parity.EvidenceError):
                    phase.load_json(path)
            path.write_text(" " * 100001, encoding="utf-8")
            with self.assertRaisesRegex(phase.parity.EvidenceError, "budget"):
                phase.load_json(path, phase.MAX_ORACLE_BYTES)

    def test_actual_retained_native_wrong_phase_and_imported_failure(self):
        root = Path(__file__).resolve().parents[2]
        native = root / "temp/explore-effect-native"
        imported = root / "temp/explore-effect-imported"
        if not all(
            (p / "capture.json").is_file() and (p / "capture.dximdemo.gz").is_file() for p in (native, imported)
        ):
            self.skipTest("Retained local captures unavailable; synthetic contract cases still run")
        capture = phase.validate_capture(phase.load_json(native / "capture.json"), native=True)
        controls = phase.read_controls(native / "capture.dximdemo.gz", capture)
        oracle = phase.make_oracle(
            capture,
            controls,
            phase.parity.digest(native / "capture.json"),
            phase.parity.digest(native / "capture.dximdemo.gz"),
        )
        self.assertEqual(phase.compare(oracle, capture, controls)["status"], "pass")
        wrong = copy.deepcopy(oracle)
        for field in ("frame_count", "selected_bitmap", "frame_bitmap"):
            wrong["observations"][170][1][phase.PHASE_FIELDS.index(field)] += 1
        negative = phase.compare(wrong, capture, controls)
        self.assertEqual(negative["status"], "fail")
        self.assertEqual(negative["phase"]["mismatch_count"], 3)
        self.assertEqual(negative["first_mismatch"]["f"], 170)
        other = phase.load_json(imported / "capture.json")
        actual = phase.compare(oracle, other, phase.read_controls(imported / "capture.dximdemo.gz", other))
        self.assertEqual(actual["status"], "fail")
        self.assertEqual(actual["phase"]["mismatch_count"], 1812)
        self.assertEqual(actual["camera"]["status"], "pass")
        self.assertEqual(actual["controls"]["status"], "pass")

    def test_known_fault_rejects_absence_wrong_magnitude_and_other_changes(self):
        self.capture.update(game="d2", mission="descent", capture_role="imported_d1_diagnostic")
        self.controls["header"].update(game="d2", mission="descent")
        self.assertEqual(phase.known_imported_fault(self.oracle, self.capture, self.controls)["status"], "not_matched")
        native = phase.unpack_oracle(self.oracle)
        for effect in (24, 26):
            key = f"effect{effect}"
            self.capture["initial"][key]["time_left"] = 0
            for row in self.capture["observations"]:
                wanted = (row[key]["frame_count"] + 1) % row[key]["num_frames"]
                source = next(r[key] for r in native["observations"] if r[key]["frame_count"] == wanted)
                for field in (
                    "frame_count",
                    "selected_bitmap",
                    "selected_bitmap_name",
                    "frame_bitmap",
                    "frame_bitmap_name",
                ):
                    row[key][field] = source[field]
        self.assertEqual(phase.known_imported_fault(self.oracle, self.capture, self.controls)["status"], "matched")
        good = copy.deepcopy(self.capture)
        for mutation in ("magnitude", "startup_timer", "later_timer", "name", "flags", "camera"):
            with self.subTest(mutation=mutation):
                self.capture = copy.deepcopy(good)
                if mutation == "magnitude":
                    self.capture["observations"][3]["effect24"]["frame_count"] += 1
                elif mutation == "startup_timer":
                    self.capture["initial"]["effect26"]["time_left"] = 1
                elif mutation == "later_timer":
                    self.capture["observations"][3]["effect26"]["time_left"] += 1
                elif mutation == "name":
                    self.capture["observations"][3]["effect26"]["selected_bitmap_name"] = "wrong"
                elif mutation == "flags":
                    self.capture["observations"][3]["effect26"]["flags"] = 1
                else:
                    self.capture["observations"][3]["position"][0] += 1
                result = phase.known_imported_fault(self.oracle, self.capture, self.controls)
                self.assertEqual(result["status"], "not_matched")
                self.assertEqual(result["comparison_status"], "fail")

    def trace_records(self):
        header = self.controls["header"]
        meta = {
            "type": "meta",
            "game": "d1",
            "mission": "d1",
            "difficulty": 2,
            "level": 1,
            "difficulty": 2,
            "start_mode": "new_level",
            "frame_count": self.capture["frames"],
        }
        header["difficulty"] = 2
        rows = [meta]

        def world(kind, frame, expected, time, boundary=None):
            effects = [{} for _ in range(27)]
            for effect in (24, 26):
                effects[effect] = {raw: expected[f"effect{effect}"][name] for name, raw in phase.TRACE_FIELDS.items()}
            state = {key: {} for key in phase.parity.WORLD_GROUPS}
            state.update(effects=effects, globals={"GameTime64": time})
            row = {
                "type": kind,
                "version": 10,
                "f": frame,
                "reset": kind == "world_boundary" or frame == 0,
                "state": state,
            }
            if boundary:
                row["phase"] = boundary
            return row

        rows.append(world("world_boundary", 0, self.capture["initial"], 0, "restored"))
        rows.append({"type": "object_boundary", "version": 2, "f": 0, "reset": True, "phase": "restored"})
        for frame, observation in enumerate(self.capture["observations"]):
            expected = self.capture["initial"] if frame == 0 else self.capture["observations"][frame - 1]
            rows.append(world("world_state", frame, expected, observation["game_time"]))
        rows.append(
            world(
                "world_boundary",
                self.capture["frames"],
                self.capture["observations"][-1],
                self.capture["observations"][-1]["game_time"],
                "terminal",
            )
        )
        rows.append(
            {"type": "object_boundary", "version": 2, "f": self.capture["frames"], "reset": True, "phase": "terminal"}
        )
        return rows

    def check_trace(self, rows):
        # Keep strict raw parsing and the real metadata/delta/order reader;
        # only unrelated full world/object schemas use minimal test fixtures
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "state.jsonl"
            path.write_bytes(b"".join(phase.encoded(row) for row in rows))
            with (
                mock.patch.object(phase.parity, "validate_world_state"),
                mock.patch.object(phase.parity, "validate_object_storage"),
            ):
                return phase.compare_trace(self.oracle, path, self.controls, "d1")

    def test_trace_alignment_and_wrong_phase_negative(self):
        rows = self.trace_records()
        result = self.check_trace(rows)
        self.assertEqual(result["status"], "pass")
        self.assertEqual(result["phase_scope"]["status"], "pass")
        self.assertIn("frame_bitmap_name", result["unobserved_phase_fields"])
        self.assertEqual(result["camera"]["status"], "unobserved")
        self.oracle["observations"][5][1][phase.PHASE_FIELDS.index("selected_bitmap")] += 1
        result = self.check_trace(rows)
        self.assertEqual(result["status"], "fail")
        self.assertEqual(result["first_mismatch"]["trace_frame"], 6)
        self.assertEqual(result["first_mismatch"]["capture_frame"], 5)
        self.assertEqual(result["first_mismatch"]["field"], "selected_bitmap")

    def test_trace_missing_malformed_and_reordered_evidence(self):
        for mutation in (
            "missing_frame",
            "missing_terminal",
            "reorder",
            "missing_timer",
            "boolean",
            "metadata",
            "missing_initial",
        ):
            with self.subTest(mutation=mutation):
                self.setUp()
                rows = self.trace_records()
                if mutation == "missing_frame":
                    del rows[4]
                elif mutation == "missing_terminal":
                    del rows[-2:]
                elif mutation == "reorder":
                    rows[4:6] = rows[5:3:-1]
                elif mutation == "missing_timer":
                    del rows[4]["state"]["effects"][24]["time_left"]
                elif mutation == "boolean":
                    rows[4]["state"]["effects"][24]["flags"] = False
                elif mutation == "metadata":
                    rows[0]["mission"] = "unknown"
                else:
                    del rows[1]
                with self.assertRaises(phase.parity.EvidenceError):
                    self.check_trace(rows)

    def demo_records(self):
        result = {
            "version": 2,
            "game": self.capture["game"],
            "mission": self.capture["mission"],
            "level": 1,
            "difficulty": 2,
            "frame_count": self.capture["frames"],
            "game_time64": self.capture["observations"][-1]["game_time"],
            "player0": {
                "energy": 100,
                "shields": 100,
                "score": 0,
                "lives": 3,
                "laser_level": 0,
                "primary_weapon": 0,
                "secondary_weapon": 0,
                "flags": 0,
                "hostages": 0,
                "primary_ammo": [0] * 16,
                "secondary_ammo": [0] * 16,
            },
            "position": {"segment": 11, "x": 0, "y": 0, "z": 0, "forward_x": 0, "forward_y": 0, "forward_z": 65536},
            "level_summary": {
                "robots_alive": 39,
                "robots_killed": 0,
                "hostages_remaining": 2,
                "powerups_remaining": 24,
                "control_center_destroyed": False,
                "endlevel_completed": False,
            },
        }
        rows = [copy.deepcopy(self.controls["header"])]
        for index, timestep, residual, operations in self.controls["rows"]:
            row = {
                "type": "frame" if timestep is not None else "input_tail",
                "f": index,
                "input": residual,
                "rng": {"s": 0, "c": 0},
                "dispatch": {"pre_rng": {"s": 0, "c": 0}, "ops": operations},
            }
            if timestep is not None:
                row["ft"] = timestep
            rows.append(row)
        rows.append({"type": "result", "result": result})
        return rows

    def test_companion_terminal_is_complete_typed_and_bound_to_capture(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.dximdemo"
            for mutation in (
                None,
                "missing_result",
                "missing_player",
                "empty_player",
                "ammo_shape",
                "ammo_bool",
                "identity",
                "level",
                "difficulty",
                "clock",
                "pose",
                "boolean",
                "extra_field",
            ):
                with self.subTest(mutation=mutation):
                    rows = self.demo_records()
                    result = rows[-1]["result"]
                    if mutation == "missing_result":
                        del rows[-1]["result"]
                    elif mutation == "missing_player":
                        del result["player0"]
                    elif mutation == "empty_player":
                        result["player0"] = {}
                    elif mutation == "ammo_shape":
                        result["player0"]["primary_ammo"].pop()
                    elif mutation == "ammo_bool":
                        result["player0"]["secondary_ammo"][0] = False
                    elif mutation == "identity":
                        result["mission"] = "descent"
                    elif mutation == "level":
                        result["level"] = 2
                    elif mutation == "difficulty":
                        result["difficulty"] = 3
                    elif mutation == "clock":
                        result["game_time64"] += 1
                    elif mutation == "pose":
                        result["position"]["x"] += 1
                    elif mutation == "boolean":
                        result["level_summary"]["control_center_destroyed"] = 0
                    elif mutation == "extra_field":
                        result["unknown"] = 1
                    path.write_bytes(b"".join(phase.encoded(row) for row in rows))
                    if mutation is None:
                        self.assertEqual(phase.read_controls(path, self.capture), self.controls)
                    else:
                        with self.assertRaises(phase.parity.EvidenceError):
                            phase.read_controls(path, self.capture)
            self.capture.update(game="d2", mission="descent", capture_role="imported_d1_diagnostic")
            self.controls["header"].update(game="d2", mission="descent")
            path.write_bytes(b"".join(phase.encoded(row) for row in self.demo_records()))
            self.assertEqual(phase.read_controls(path, self.capture), self.controls)

    def test_strict_companion_and_trace_reject_duplicate_and_nonfinite_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "evidence.jsonl"
            for kind in ("companion", "trace"):
                rows = self.demo_records() if kind == "companion" else self.trace_records()
                original = b"".join(phase.encoded(row) for row in rows)
                for mutation in ("duplicate", "NaN", "Infinity", "overflow"):
                    with self.subTest(kind=kind, mutation=mutation):
                        if mutation == "duplicate":
                            # A duplicate identity key is malformed even when equal
                            changed = original.replace(b'"game":"d1"', b'"game":"d1","game":"d1"', 1)
                        else:
                            value = {"NaN": "NaN", "Infinity": "Infinity", "overflow": "1e999"}[mutation]
                            changed = original.replace(b"{", ('{"unused":' + value + ",").encode(), 1)
                        path.write_bytes(changed)
                        with self.assertRaises(phase.parity.EvidenceError):
                            if kind == "companion":
                                phase.read_controls(path, self.capture)
                            else:
                                phase.compare_trace(self.oracle, path, self.controls, "d1")
            original = b"".join(phase.encoded(row) for row in self.demo_records())
            path.write_bytes(original.replace(b'"s":{"f":2184}', b'"s":{"f":1,"f":2184}', 1))
            with self.assertRaisesRegex(phase.parity.EvidenceError, "Duplicate JSON key"):
                phase.read_controls(path, self.capture)

    def test_truncated_gzip_returns_invalid_not_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            bad = directory / "bad.jsonl.gz"
            bad.write_bytes(gzip.compress(b"{}\n")[:-4])
            oracle = directory / "oracle.json"
            capture = directory / "capture.json"
            report = directory / "report.json"
            oracle.write_bytes(phase.encoded(self.oracle))
            capture.write_bytes(phase.encoded(self.capture))
            with mock.patch("builtins.print"):
                status = phase.main(
                    [
                        "compare",
                        "--oracle",
                        str(oracle),
                        "--capture",
                        str(capture),
                        "--demo",
                        str(bad),
                        "--report",
                        str(report),
                    ]
                )
            self.assertEqual(status, 2)
            self.assertEqual(json.loads(report.read_text())["status"], "invalid")
            with self.assertRaisesRegex(phase.parity.EvidenceError, "invalid JSONL"):
                phase.compare_trace(self.oracle, bad, self.controls, "d1")

    def test_cli_mismatch_exit_is_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            oracle_path = directory / "oracle.json"
            capture_path = directory / "capture.json"
            report_path = directory / "report.json"
            self.capture["initial"]["effect24"]["time_left"] = 0
            oracle_path.write_bytes(phase.encoded(self.oracle))
            capture_path.write_bytes(phase.encoded(self.capture))
            with (
                mock.patch.object(phase, "read_controls", return_value=self.controls),
                mock.patch.object(phase.parity, "digest", return_value="a" * 64),
                mock.patch("builtins.print"),
            ):
                status = phase.main(
                    [
                        "compare",
                        "--oracle",
                        str(oracle_path),
                        "--capture",
                        str(capture_path),
                        "--demo",
                        "unused",
                        "--report",
                        str(report_path),
                    ]
                )
            self.assertEqual(status, 1)
            self.assertEqual(json.loads(report_path.read_text())["status"], "fail")


if __name__ == "__main__":
    unittest.main()

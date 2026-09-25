"""Fail-closed tests for the paired replay evidence comparator."""

import copy
import argparse
import contextlib
import io
import json
import re
import tempfile
import unittest
from unittest import mock
from pathlib import Path

import d1_replay_parity as parity


def schema_example(schema):
    if schema is int:
        return 0
    if isinstance(schema, dict):
        return {key: schema_example(child) for key, child in schema.items() if key != "*"}
    if isinstance(schema, tuple):
        return [schema_example(child) for child in schema]
    return []


class ParityTests(unittest.TestCase):
    def test_untracked_sources_are_pinned_and_ignore_build_products(self):
        repo = self.root / "repo"
        repo.mkdir()
        parity.subprocess.run(["git", "init", "-q", str(repo)], check=True)
        (repo / ".gitignore").write_text("build/\n")
        (repo / "tracked.c").write_bytes(b"tracked by the patch")
        parity.subprocess.run(["git", "add", ".gitignore", "tracked.c"], cwd=repo, check=True)
        (repo / "new source.c").write_bytes(b"new observer\x00\xff")
        (repo / "build").mkdir()
        (repo / "build" / "game.exe").write_bytes(b"separately pinned binary")
        manifest = parity.snapshot_untracked_sources(repo, self.root / "snapshot", 0)
        self.assertEqual([entry["source"] for entry in manifest], ["new source.c"])
        (repo / "new source.c").write_bytes(b"later edit")
        self.assertEqual(Path(manifest[0]["path"]).read_bytes(), b"new observer\x00\xff")
        self.assertEqual(parity.digest(manifest[0]["path"]), manifest[0]["sha256"])

    def test_untracked_source_change_during_snapshot_is_rejected(self):
        repo = self.root / "repo"
        repo.mkdir()
        parity.subprocess.run(["git", "init", "-q", str(repo)], check=True)
        (repo / "new.c").write_bytes(b"before")
        original_copy = parity.shutil.copy2

        def change_after_copy(source, target):
            original_copy(source, target)
            Path(source).write_bytes(b"changed during staging")

        with mock.patch.object(parity.shutil, "copy2", side_effect=change_after_copy):
            with self.assertRaises(parity.EvidenceError):
                parity.snapshot_untracked_sources(repo, self.root / "snapshot", 0)

    def test_executable_snapshot_survives_source_rebuild(self):
        source = self.root / "build"
        source.mkdir()
        (source / "game.exe").write_bytes(b"original executable")
        (source / "runtime.dll").write_bytes(b"original dependency")
        (source / "unrelated.txt").write_bytes(b"excluded")
        staged = parity.snapshot_executable(source / "game.exe", self.root / "pinned")
        (source / "game.exe").write_bytes(b"rebuilt executable")
        (source / "runtime.dll").write_bytes(b"rebuilt dependency")
        self.assertEqual(Path(staged["path"]).read_bytes(), b"original executable")
        self.assertEqual(len(staged["package"]), 2)
        self.assertTrue(all(parity.digest(item["path"]) == item["sha256"] for item in staged["package"]))
        self.assertFalse((self.root / "pinned" / "unrelated.txt").exists())

    def test_executable_change_while_staging_is_rejected(self):
        source = self.root / "game.exe"
        source.write_bytes(b"original")
        copy = parity.shutil.copy2

        def changing_copy(src, dest):
            copy(src, dest)
            source.write_bytes(b"rebuilt")

        with mock.patch.object(parity.shutil, "copy2", side_effect=changing_copy):
            with self.assertRaisesRegex(parity.EvidenceError, "changed while staging"):
                parity.snapshot_executable(source, self.root / "pinned")

    def test_disk_reserve_rejects_capture_before_creating_output(self):
        args = argparse.Namespace(output=self.root / "capture", minimum_free_gb=4)
        with mock.patch.object(parity.shutil, "disk_usage", return_value=mock.Mock(free=1024)):
            with self.assertRaisesRegex(parity.EvidenceError, "Insufficient output disk space"):
                parity.run(args)
        self.assertFalse(args.output.exists())

    def test_trace_compression_verified_and_failure_keeps_source(self):
        source = self.root / "rng.jsonl"
        original = b'{"value":123}\n' * 100
        source.write_bytes(original)
        compressed = parity.compress_trace(source)
        with parity.gzip.open(compressed, "rb") as stream:
            self.assertEqual(stream.read(), original)
        self.assertFalse(source.exists())
        source.write_bytes(original)
        compressed.unlink()
        with mock.patch.object(parity.shutil, "copyfileobj", side_effect=OSError("disk full")):
            with self.assertRaises(OSError):
                parity.compress_trace(source)
        self.assertEqual(source.read_bytes(), original)
        self.assertFalse(compressed.exists())

    def setUp(self):
        scratch = Path(__file__).resolve().parents[2] / "temp"
        scratch.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="d1-parity-unit-", dir=scratch)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.header = {"game": "d1", "mission": "d1", "level": 14, "difficulty": 0,
                       "frame_count": 2, "start_mode": "save_checkpoint"}
        self.result = {**self.header, "version": 2, "player0": {"shields": 30},
                       "position": {"x": 10}, "level_summary": {"endlevel_completed": True}}

    def trace(self, name, engine="d1"):
        meta = {"type": "meta", "diag_version": parity.FRAME_DIAGNOSTIC_VERSION, **self.header}
        # Both binaries emit input_demo_replay_game/mission in trace metadata
        rows = [meta] + [{"type": "frame_state", "f": i, "ft": 100,
                          "state": {"x": i}, "rng": {"s": 7}, "diag": {**{name: [0] * length if length else 0
                                      for name, length in parity.FRAME_DIAGNOSTIC_FIELDS.items()}, "objects": i}}
                         for i in range(2)]
        return self.write(name, rows)

    def write(self, name, rows):
        path = self.root / name
        path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")
        return path

    def mutate(self, path, operation):
        rows = list(parity.records(path))
        operation(rows)
        self.write(path.name, rows)

    def test_explicit_identity_mapping_preserves_all_gameplay(self):
        imported = copy.deepcopy(self.result)
        imported.update(game="d2", mission="descent")
        self.assertEqual(parity.canonical_result(imported, self.header, "d2"), self.result)
        self.assertEqual(imported["game"], "d2")
        imported["mission"] = "counterstrike"
        with self.assertRaises(parity.EvidenceError):
            parity.canonical_result(imported, self.header, "d2")

    def test_full_terminal_comparison_never_substitutes_player_values(self):
        demo = {"header": self.header, "result": self.result}
        actual = copy.deepcopy(self.result)
        actual["player0"]["shields"] = 29
        path = self.root / "actual.json"
        path.write_text(json.dumps(actual))
        report = parity.compare_pair(demo, {"state": self.root / "missing", "rng": self.root / "missing"},
                                     {"result": path, "state": self.root / "missing", "rng": self.root / "missing"},
                                     "d1", recorded=True)
        self.assertEqual(report["checks"]["terminal_result"]["status"], "fail")
        self.assertEqual(report["checks"]["frames"]["status"], "incomplete")
        self.assertEqual(self.result["player0"]["shields"], 30)

    def test_first_frame_mismatch_and_full_lengths(self):
        a, b = self.trace("a"), self.trace("b", "d2")
        self.assertEqual(parity.compare_frames(a, b, self.header, "d2")["status"], "pass")
        self.mutate(b, lambda rows: rows[1]["state"].update(x=7))
        report = parity.compare_frames(a, b, self.header, "d2")
        self.assertEqual(report["frames_compared"], 2)
        self.assertEqual(report["first_differences"]["state"]["frame"], 0)
        self.mutate(b, lambda rows: rows.pop())
        with self.assertRaises(parity.EvidenceError):
            parity.compare_frames(a, b, self.header, "d2")

    def test_wrong_identity_missing_fields_and_extra_frames_fail(self):
        a = self.trace("a")
        for mutation in (lambda rows: rows[0].update(mission="wrong"),
                         lambda rows: rows[1].pop("diag"),
                         lambda rows: rows.append({**rows[-1], "f": 2})):
            b = self.trace("b")
            self.mutate(b, mutation)
            with self.assertRaises(parity.EvidenceError):
                parity.compare_frames(a, b, self.header, "d1")

    def test_all_diagnostics_remain_visible(self):
        a, b = self.trace("a"), self.trace("b")
        self.mutate(b, lambda rows: rows[1]["diag"].update(new_unmapped_field=42))
        report = parity.compare_frames(a, b, self.header, "d1")
        self.assertEqual(report["status"], "fail")
        self.assertIn("diag", report["first_differences"])

    def test_diagnostic_contract_covers_the_complete_producer(self):
        shared = Path(__file__).resolve().parents[1] / "app/src/main/cpp/shared"
        source = (shared / "input_demo_state_trace.cpp").read_text()
        emitted = re.findall(r'root.emplace_back\("(\w+)", diag\.(\w+)\);', source)
        self.assertEqual(len(emitted), 360)
        self.assertTrue(all(key == member for key, member in emitted))
        self.assertEqual(set(parity.FRAME_DIAGNOSTIC_FIELDS), {key for key, _ in emitted})
        header = (shared / "input_demo_state_trace.h").read_text()
        body = header.split("typedef struct input_demo_state_trace_diag {")[1].split("} input_demo_state_trace_diag;")[0]
        declared = re.findall(r'\b(?:u?int(?:32|64)_t) (\w+)(?:\[(\w+)\])?;', body)
        self.assertEqual({key for key, _ in declared}, set(parity.FRAME_DIAGNOSTIC_FIELDS))

    def test_missing_diagnostic_from_both_captures_is_incomplete(self):
        a, b = self.trace("a"), self.trace("b")
        for path in (a, b):
            self.mutate(path, lambda rows: rows[1]["diag"].pop("weapon_fusion_charge"))
        with self.assertRaisesRegex(parity.EvidenceError, "Missing diagnostic field"):
            parity.compare_frames(a, b, self.header, "d1")

    def test_every_diagnostic_is_required_and_typed(self):
        complete = {name: [0] * length if length else 0
                    for name, length in parity.FRAME_DIAGNOSTIC_FIELDS.items()}
        parity.validate_frame_diagnostics(complete, 0)
        for name, length in parity.FRAME_DIAGNOSTIC_FIELDS.items():
            with self.subTest(field=name):
                missing = dict(complete)
                del missing[name]
                with self.assertRaises(parity.EvidenceError):
                    parity.validate_frame_diagnostics(missing, 0)
                invalid_values = (None, True, "0", [0] * (length - 1), [0] * (length + 1)) if length else (None, True, "0", 0.0, [])
                if length:
                    invalid_values += ([False] * length,)
                for value in invalid_values:
                    with self.assertRaises(parity.EvidenceError):
                        parity.validate_frame_diagnostics({**complete, name: value}, 0)

    def test_fresh_diagnostics_require_the_declared_version(self):
        a = self.trace("a")
        for version in (None, False, "1", 0, 2):
            b = self.trace("b")
            self.mutate(b, lambda rows: rows[0].update(diag_version=version))
            with self.assertRaisesRegex(parity.EvidenceError, "diagnostic schema"):
                parity.compare_frames(a, b, self.header, "d1")

    def test_historical_recording_diagnostics_remain_independent(self):
        a, b = self.trace("a"), self.trace("b")
        self.mutate(a, lambda rows: rows[0].update(type="header", diag_version=None))
        self.mutate(a, lambda rows: rows[1]["diag"].pop("weapon_fusion_charge"))
        report = parity.compare_frames(a, b, self.header, "d1", recorded=True)
        self.assertEqual(report["status"], "fail")
        self.assertEqual(report["diagnostics"]["weapon_fusion_charge"]["first_difference"]["expected"], "<missing>")

    def test_early_diagnostic_difference_does_not_mask_later_fields(self):
        a, b = self.trace("a"), self.trace("b")
        for path in (a, b):
            self.mutate(path, lambda rows: [row["diag"].update(angle=[1, 2, 3]) for row in rows[1:]])
        self.mutate(b, lambda rows: rows[1]["diag"].update(objects=42))
        self.mutate(b, lambda rows: rows[2]["diag"].update(angle=[1, 2, 4], extra=0))
        report = parity.compare_frames(a, b, self.header, "d2")
        self.assertEqual(report["first_differences"]["diag"]["frame"], 0)
        angle = report["diagnostics"]["angle"]
        self.assertEqual(angle["frames_compared"], 2)
        self.assertEqual(angle["frames_different"], 1)
        self.assertEqual(angle["first_difference"]["frame"], 1)
        self.assertEqual(angle["first_difference"]["path"], "$.diag.angle[2]")
        self.assertEqual(report["diagnostics"]["extra"]["first_difference"]["expected"], "<missing>")
        self.assertEqual(report["status"], "fail")

    def test_null_diagnostics_are_not_evidence(self):
        a, b = self.trace("a"), self.trace("b")
        for path in (a, b):
            self.mutate(path, lambda rows: rows[1].update(diag=None))
        with self.assertRaises(parity.EvidenceError):
            parity.compare_frames(a, b, self.header, "d1")

    def rng(self, name):
        return self.write(name, [{"type": "meta", "events": 2, "truncated": False},
                                 {"type": "rand", "seq": 0, "frame": 0, "gt": 100, "call_count": 1,
                                  "state_before": 7, "state_after": 8, "result": 1, "file": "native.c"},
                                 {"type": "rand", "seq": 1, "frame": 1, "gt": 200, "call_count": 2,
                                  "state_before": 8, "state_after": 9, "result": 2}])

    def object_trace(self, name):
        rows = [{"type": "meta", **self.header}]
        for i in range(2):
            row = self.object_storage()
            row.update(type="object_state", version=2, f=i, reset=i == 0)
            shot = schema_example(parity.OBJECT_SCHEMA)
            shot.update(type=5, id=11, signature=i + 7, control_type=9, movement_type=1, render_type=3,
                        physics=schema_example(parity.PHYSICS_SCHEMA),
                        weapon=dict.fromkeys("parent_type parent_num parent_signature creation_time last_hitobj track_goal multiplier creation_framecount".split(), 0))
            shot["weapon"]["hitobj_list"] = [0, i] + [0] * 998
            row["slots"]["3"] = shot
            row["allocator"].update(num_objects=1, highest_object_index=3)
            rows.append(row)
        return self.write(name, rows)

    def object_storage(self):
        row = schema_example(parity.OBJECT_STORAGE_SCHEMA)
        row["capacity"] = 1000
        row["allocator"]["free_obj_list"] = list(range(1000))
        return row

    def robot_object(self, control=1):
        obj = schema_example(parity.OBJECT_SCHEMA)
        obj.update(type=2, control_type=control)
        obj["ai"] = dict.fromkeys("behavior hide_segment hide_index path_length cur_path_index danger_laser_signature danger_laser_num".split(), 0)
        obj["ai"].update(flags=[0] * 11, follow_path_start_seg=168, follow_path_end_seg=-1)
        local = schema_example(parity.AI_LOCAL_SCHEMA)
        local.update(last_see_time=-123, last_attack_time=456, wait_time=789)
        for key in ("goal_angles", "delta_angles", "goal_state", "achieved_state"):
            local[key] = [schema_example(parity.AI_LOCAL_SCHEMA[key][0]) for _ in range(10)]
        obj["ai_local"] = local
        return obj

    @staticmethod
    def import_ai_storage(value, fields, **extras):
        value["d1_saved"] = {field: value.pop(field) for field in fields}
        value.update(extras)

    def import_robot(self, obj):
        self.import_ai_storage(obj["ai"], parity.AI_SAVED_STATIC_FIELDS, dying_sound_playing=0, dying_start_time=0)
        self.import_ai_storage(obj["ai_local"], parity.AI_SAVED_LOCAL_FIELDS, next_action_time=0, next_fire2=0)

    def test_saved_ai_mapping_compares_values_and_preserves_raw_and_unknown_state(self):
        for control in (1, 11):
            a = self.object_trace("ai-native")
            def robots(rows):
                for row in rows[1:]:
                    row["slots"]["3"] = self.robot_object(control)
            self.mutate(a, robots)
            b = self.write("ai-imported", list(parity.records(a)))
            self.mutate(b, lambda rows: [self.import_robot(row["slots"]["3"]) for row in rows[1:]])
            before = b.read_bytes()
            report = parity.compare_objects(a, b, self.header, "d2")
            self.assertEqual(report["first_field_differences"], {})
            self.assertEqual(report["status"], "pass")
            self.assertEqual(b.read_bytes(), before)
            for group, fields in (("ai", parity.AI_SAVED_STATIC_FIELDS), ("ai_local", parity.AI_SAVED_LOCAL_FIELDS)):
                for field in fields:
                    damaged = self.write("ai-damaged", list(parity.records(b)))
                    def change(rows):
                        rows[2]["slots"]["3"][group]["d1_saved"][field] += 1
                    self.mutate(damaged, change)
                    differences = parity.compare_objects(a, damaged, self.header, "d2")["first_field_differences"]
                    self.assertEqual(differences[f"$.object.{group}.{field}"]["frame"], 1)
            self.mutate(b, lambda rows: rows[2]["slots"]["3"]["ai"]["d1_saved"].update(unknown=27))
            differences = parity.compare_objects(a, b, self.header, "d2")["first_field_differences"]
            self.assertEqual(differences["$.object.ai.d1_saved"]["actual"], {"unknown": 27})
            raw = list(parity.object_states(b, self.header, "d2"))[-1]["objects"]["3"]
            frozen = copy.deepcopy(raw)
            parity.canonical_d1_object(raw)
            self.assertEqual(raw, frozen)

    def test_ai_mapping_requires_content_and_storage_identity(self):
        a = self.object_trace("identity-native")
        for header, engine in (({**self.header, "game": "d2"}, "d2"), (self.header, "other")):
            for operation in (parity.compare_objects, parity.compare_world):
                with self.assertRaises(parity.EvidenceError):
                    operation(a, a, header, engine)
        native = self.robot_object()
        imported = copy.deepcopy(native)
        self.import_robot(imported)
        for obj, engine in ((native, "d2"), (imported, "d1")):
            with self.assertRaises(parity.EvidenceError):
                parity.validate_object(obj, engine, "$.object")
        imported["ai"]["follow_path_start_seg"] = 0
        with self.assertRaises(parity.EvidenceError):
            parity.validate_object(imported, "d2", "$.object")
        native["ai_local"]["d1_saved"] = {"unexpected": 1}
        with self.assertRaises(parity.EvidenceError):
            parity.validate_object(native, "d1", "$.object")

    def test_neutral_object_fields_and_capacity_cannot_hide_d2_behavior(self):
        robot = self.robot_object()
        self.import_robot(robot)
        for group, field in (("ai", "dying_sound_playing"), ("ai", "dying_start_time"),
                             ("ai_local", "next_action_time"), ("ai_local", "next_fire2")):
            changed = copy.deepcopy(robot)
            changed[group][field] = 1
            with self.assertRaises(parity.EvidenceError):
                parity.canonical_d1_object(changed)
        powerup = schema_example(parity.OBJECT_SCHEMA)
        powerup.update(type=7, control_type=13, powerup={"count": 17, "flags": 0, "creation_time": 123456})
        parity.validate_object(powerup, "d2", "$.object")
        self.assertEqual(parity.canonical_d1_object(powerup)["powerup"], {"count": 17})
        self.assertEqual(powerup["powerup"]["creation_time"], 123456)
        powerup["powerup"]["flags"] = 1
        with self.assertRaises(parity.EvidenceError):
            parity.canonical_d1_object(powerup)
        reactor = schema_example(parity.OBJECT_SCHEMA)
        reactor.update(type=9, control_type=16, reactor_gun_pos=[[1, 2, 3]] * 4 + [[0, 0, 0]] * 4,
                       reactor_gun_dir=[[4, 5, 6]] * 4 + [[0, 0, 0]] * 4)
        parity.validate_object(reactor, "d2", "$.object")
        self.assertEqual(parity.canonical_d1_object(reactor)["reactor_gun_pos"], [[1, 2, 3]] * 4)
        for key in ("reactor_gun_pos", "reactor_gun_dir"):
            changed = copy.deepcopy(reactor)
            changed[key][-1][0] = 1
            with self.assertRaises(parity.EvidenceError):
                parity.canonical_d1_object(changed)

    def test_object_deltas_preserve_every_field_and_do_not_mask_later_changes(self):
        a, b = self.object_trace("a"), self.object_trace("b")
        self.assertEqual(parity.compare_objects(a, b, self.header)["status"], "pass")
        self.mutate(b, lambda rows: rows[1]["slots"]["3"].update(extra_engine_field=0))
        self.mutate(b, lambda rows: rows[2]["slots"]["3"]["weapon"]["hitobj_list"].__setitem__(1, 2))
        diff = parity.compare_objects(a, b, self.header)["first_field_differences"]
        self.assertEqual(diff["$.object.extra_engine_field"]["frame"], 0)
        self.assertEqual(diff["$.object.weapon.hitobj_list[1]"]["frame"], 1)
        self.assertEqual(diff["$.object.weapon.hitobj_list[1]"]["slot"], 3)
        def retire(rows):
            rows[2]["slots"]["3"] = None
            rows[2]["allocator"].update(num_objects=0, highest_object_index=-1)
        self.mutate(b, retire)
        self.assertIn("$.object", parity.compare_objects(a, b, self.header)["first_field_differences"])

    def test_object_evidence_requires_complete_ordered_deltas(self):
        for mutation in (lambda rows: rows.pop(), lambda rows: rows[1].update(reset=False),
                         lambda rows: rows[2].update(f=3), lambda rows: rows[2].update(capacity=11),
                         lambda rows: rows[1]["slots"].update({"10": {}}),
                         lambda rows: rows[1]["slots"].update({"2": None}),
                         lambda rows: rows[2].pop("rng")):
            path = self.object_trace("bad")
            self.mutate(path, mutation)
            with self.assertRaises(parity.EvidenceError):
                list(parity.object_states(path, self.header))

    def test_object_arrays_retain_numeric_types(self):
        diff = list(parity.field_differences({"v": [1]}, {"v": [True]}))
        self.assertEqual(diff[0]["path"], "$.v[0]")

    def test_matching_incomplete_objects_are_not_parity_evidence(self):
        a, b = self.object_trace("a"), self.object_trace("b")
        for path in (a, b):
            self.mutate(path, lambda rows: rows[1]["slots"]["3"].pop("orient"))
        with self.assertRaises(parity.EvidenceError):
            parity.compare_objects(a, b, self.header)

    def test_missing_live_slots_cannot_match_an_allocator_with_objects(self):
        for mutation in (lambda row: row["slots"].clear(),
                         lambda row: row["allocator"].update(num_objects=2),
                         lambda row: row["allocator"].update(highest_object_index=2)):
            path = self.object_trace("bad")
            self.mutate(path, lambda rows: mutation(rows[1]))
            with self.assertRaises(parity.EvidenceError):
                list(parity.object_states(path, self.header))
            row = list(parity.records(self.object_trace("boundary")))[1]
            row.update(type="object_boundary", phase="restored")
            mutation(row)
            with self.assertRaises(parity.EvidenceError):
                parity.validate_object_storage(row, "d1")

    def test_object_fields_and_allocator_are_required_at_every_boundary(self):
        mutations = (
            lambda row: row["slots"]["3"].pop("physics"),
            lambda row: row["slots"]["3"]["physics"]["velocity"].pop(),
            lambda row: row["slots"]["3"]["physics"].update(mass=True),
            lambda row: row["slots"]["3"]["weapon"].pop("creation_framecount"),
            lambda row: row["slots"]["3"]["weapon"]["hitobj_list"].pop(),
            lambda row: row["slots"]["3"]["orient"][0].__setitem__(0, 0.0),
            lambda row: row["slots"]["3"].update(movement_type=2),
            lambda row: row["slots"]["3"].update(control_type=3),
            lambda row: row["slots"]["3"].update(render_type=8),
            lambda row: row["slots"]["3"].update(type=255),
            lambda row: row["allocator"]["free_obj_list"].pop(),
            lambda row: row["clock"].pop("d_tick_count"),
            lambda row: row["rng"].pop(),
            lambda row: row["rng"][0].update(calls=False),
        )
        valid = list(parity.records(self.object_trace("valid")))[1]
        for mutation in mutations:
            for boundary in (None, "restored", "terminal"):
                with self.subTest(mutation=mutation, boundary=boundary):
                    if boundary is None:
                        path = self.object_trace("bad-object")
                        self.mutate(path, lambda rows: mutation(rows[2]))
                        operation = lambda: list(parity.object_states(path, self.header))
                    else:
                        path = self.world_trace("bad-boundary")
                        def corrupt(rows):
                            row = rows[1 if boundary == "restored" else -2]
                            row.update({key: copy.deepcopy(valid[key]) for key in parity.OBJECT_STORAGE_SCHEMA})
                            mutation(row)
                        self.mutate(path, corrupt)
                        operation = lambda: list(parity.world_states(path, self.header))
                    with self.assertRaises(parity.EvidenceError):
                        operation()

    def test_object_union_variants_require_their_full_storage(self):
        for engine in ("d1", "d2"):
            variants = [
                ({"movement_type": 3}, {"spin_rate": [1, 2, 3]}),
                ({"control_type": 2}, {"explosion": dict.fromkeys("spawn_time delete_time delete_objnum attach_parent prev_attach next_attach".split(), 0)}),
                ({"control_type": 14}, {"light_intensity": 123}),
                ({"control_type": 13}, {"powerup": {"count": 1, **({"creation_time": 0, "flags": 0} if engine == "d2" else {})}}),
                ({"control_type": 16}, {"reactor_gun_pos": [[0, 0, 0]] * (8 if engine == "d2" else 4),
                                       "reactor_gun_dir": [[0, 0, 1]] * (8 if engine == "d2" else 4)}),
            ]
            poly = {"model_num": 1, "subobj_flags": 0, "tmap_override": -1, "alt_textures": 0, "anim_angles": [[0, 0, 0]] * 10}
            for render in (1, 6):
                variants.append(({"render_type": render}, {"polyobj": poly}))
            variants.append(({"type": 12, "render_type": 0}, {"polyobj": poly}))
            for render in (2, 4, 5, 7):
                variants.append(({"render_type": render}, {"vclip": {"vclip_num": 1, "frametime": 2, "framenum": 3}}))
            local = schema_example(parity.AI_LOCAL_SCHEMA)
            local.update(dict.fromkeys(("next_action_time next_fire2" if engine == "d2" else "last_see_time last_attack_time wait_time").split(), 0))
            if engine == "d2":
                local["d1_saved"] = dict.fromkeys("last_see_time last_attack_time wait_time".split(), 0)
            for key in ("goal_angles", "delta_angles", "goal_state", "achieved_state"):
                local[key] = [schema_example(parity.AI_LOCAL_SCHEMA[key][0]) for _ in range(10)]
            ai = dict.fromkeys("behavior hide_segment hide_index path_length cur_path_index danger_laser_signature danger_laser_num".split(), 0)
            ai["flags"] = [0] * 11
            ai.update(dict.fromkeys(("dying_sound_playing dying_start_time" if engine == "d2" else "follow_path_start_seg follow_path_end_seg").split(), 0))
            if engine == "d2":
                ai["d1_saved"] = dict.fromkeys("follow_path_start_seg follow_path_end_seg".split(), 0)
            for control in (1, 11):
                variants.append(({"type": 2, "control_type": control}, {"ai": ai, "ai_local": local}))
            for tags, fields in variants:
                obj = {**schema_example(parity.OBJECT_SCHEMA), **tags, **copy.deepcopy(fields)}
                parity.validate_object(obj, engine, "$.object")
                for field in fields:
                    damaged = copy.deepcopy(obj)
                    del damaged[field]
                    with self.subTest(engine=engine, tags=tags, field=field), self.assertRaises(parity.EvidenceError):
                        parity.validate_object(damaged, engine, "$.object")

    def test_rng_labels_are_mapped_but_order_values_and_counts_are_not(self):
        a, b = self.rng("a"), self.rng("b")
        self.mutate(b, lambda rows: rows[1].update(file="imported.c", func="ported", line=22, stream=0))
        self.assertEqual(parity.compare_rng(a, b)["status"], "pass")
        self.mutate(b, lambda rows: rows[2].update(result=3))
        self.assertEqual(parity.compare_rng(a, b)["first_difference"]["event"], 1)
        self.mutate(b, lambda rows: rows[0].update(truncated=True))
        with self.assertRaises(parity.EvidenceError):
            parity.compare_rng(a, b)

    def test_missing_reference_and_corrupt_json_are_not_passes(self):
        missing = self.root / "missing"
        self.assertEqual(parity.safe_check(lambda: parity.compare_rng(missing, missing))["status"], "incomplete")
        bad = self.root / "broken"
        bad.write_text("{broken}")
        self.assertEqual(parity.safe_check(lambda: list(parity.records(bad)))["status"], "incomplete")

    def test_rng_context_does_not_mask_a_later_value_difference(self):
        a, b = self.rng("a"), self.rng("b")
        self.mutate(b, lambda rows: rows[1].update(ctx_obj=2, ctx_sig=3, ctx_id=4))
        report = parity.compare_rng(a, b)
        self.assertEqual(report["status"], "fail")
        self.assertIsNone(report["first_value_difference"])
        self.mutate(b, lambda rows: rows[2].update(result=3))
        self.assertEqual(parity.compare_rng(a, b)["first_value_difference"]["event"], 1)

    def test_effects_rng_is_checked_independently_of_simulation(self):
        a, b = self.rng("a"), self.rng("b")
        for path in (a, b):
            self.mutate(path, lambda rows: rows[2].update(stream=1))
        self.assertEqual(parity.compare_rng(a, b)["simulation_events_compared"], 1)
        self.assertEqual(parity.compare_rng(a, b, stream=1)["effects_events_compared"], 1)
        self.mutate(b, lambda rows: rows[2].update(result=3))
        self.assertEqual(parity.compare_rng(a, b)["status"], "pass")
        effects = parity.compare_rng(a, b, stream=1)
        self.assertEqual(effects["status"], "fail")
        self.assertEqual(effects["first_value_difference"]["event"], 0)
        self.mutate(b, lambda rows: rows[2].pop("result"))
        with self.assertRaises(parity.EvidenceError):
            parity.compare_rng(a, b, stream=1)

    def test_missing_rng_values_and_unknown_streams_are_incomplete(self):
        for mutation in (lambda rows: rows[1].pop("result"),
                         lambda rows: rows[1].update(stream=2)):
            path = self.rng("rng")
            self.mutate(path, mutation)
            with self.assertRaises(parity.EvidenceError):
                list(parity.rng_events(path))

    def test_orchestration_attempts_every_capture_and_archives_failures(self):
        data = self.root / "data"
        data.mkdir()
        for name in ("descent.hog", "descent.pig"):
            (data / name).write_bytes(b"isolated fixture")
        args = argparse.Namespace(repo=Path(__file__).resolve().parents[2], data=data,
                                  output=self.root / "output", native=__file__, imported=__file__, minimum_free_gb=0.01,
                                  demo=[self.root / "one.dximdemo", self.root / "two.dximdemo"])
        demo = {"header": self.header, "result": self.result}
        with mock.patch.object(parity, "read_demo", return_value=demo), \
             mock.patch.object(parity, "capture", side_effect=parity.EvidenceError("timeout")) as capture, \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertNotEqual(parity.run(args), 0)
        self.assertEqual(capture.call_count, 6)
        report = json.loads((args.output / "report.json").read_text())
        self.assertEqual(len(report["cases"]), 2)
        self.assertTrue(all(len(case["capture_errors"]) == 3 for case in report["cases"]))
        self.assertEqual(report["qualification"], "incomplete")

    def world_trace(self, name):
        example = schema_example
        state = example(parity.WORLD_SCHEMA)
        ai = state["ai"]
        ai["local_capacity"] = 1000
        ai["local_default"].update(last_see_time=0, last_attack_time=0, wait_time=0)
        ai["boss"].update(Boss_hit_this_frame=0, Boss_been_hit=0, teleport_segments=[0] * 100,
                          Num_boss_gate_segs=0, gate_segments=[0] * 100)
        for key in ("paths", "cloak", "awareness"):
            ai[key] = [example(parity.WORLD_SCHEMA["ai"][key][0]) for _ in range({"paths": 2500, "cloak": 8, "awareness": 64}[key])]
        for key in ("goal_angles", "delta_angles", "goal_state", "achieved_state"):
            ai["local_default"][key] = [example(parity.AI_LOCAL_SCHEMA[key][0]) for _ in range(10)]
        state["players"] = [{**example(parity.WORLD_SCHEMA["players"][0]), "shields": 100}]
        state["players"][0].update(primary_ammo=[0] * 5, secondary_ammo=[0] * 5)
        state["walls"] = [{**example(parity.WORLD_SCHEMA["walls"][0]), "hps": 200}]
        rows = [{"type": "meta", **self.header}]

        def boundary(phase, frame):
            rows.append({**self.object_storage(), "type": "object_boundary", "version": 2, "phase": phase,
                         "f": frame, "reset": True})
            rows.append({"type": "world_boundary", "version": 8, "phase": phase,
                         "f": frame, "reset": True, "state": copy.deepcopy(state)})

        boundary("restored", 0)
        for frame in range(2):
            rows.append({"type": "world_state", "version": 8, "f": frame,
                         "reset": frame == 0, "state": copy.deepcopy(state) if frame == 0 else {}})
        boundary("terminal", 2)
        return self.write(name, rows)

    def test_world_deltas_and_both_boundaries_are_required(self):
        a, b = self.world_trace("world-a"), self.world_trace("world-b")
        self.assertEqual(parity.compare_world(a, b, self.header)["status"], "pass")
        reconstructed = list(parity.world_states(a, self.header))
        self.assertEqual(reconstructed[3]["state"]["players"][0]["shields"], 100)
        for mutation in (lambda rows: rows.pop(),
                         lambda rows: rows.pop(2),
                         lambda rows: rows.pop(4),
                         lambda rows: rows[2]["state"].pop("reactor"),
                         lambda rows: rows[3].update(reset=False),
                         lambda rows: rows[-1].update(phase="aborted"),
                         lambda rows: rows.append(rows[-1])):
            b = self.world_trace("world-b")
            self.mutate(b, mutation)
            with self.assertRaises(parity.EvidenceError):
                parity.compare_world(a, b, self.header)

    def test_private_clocks_are_required_and_compared_at_every_boundary(self):
        clocks = ("Collision_delay_last_play_time", "Fusion_next_sound_time", "Fuelcen_last_sound_time")
        for clock in clocks:
            for index, phase in ((2, "world_boundary:restored"), (4, "world_state:frame"), (6, "world_boundary:terminal")):
                with self.subTest(clock=clock, phase=phase):
                    a, b = self.world_trace("clocks-a"), self.world_trace("clocks-b")
                    def change(rows):
                        if index == 4:
                            rows[index]["state"]["globals"] = copy.deepcopy(rows[2]["state"]["globals"])
                        rows[index]["state"]["globals"][clock] = -(1 << 40) + 17
                    self.mutate(b, change)
                    report = parity.compare_world(a, b, self.header)
                    self.assertIn(f"{phase}:$.state.globals.{clock}", report["first_field_differences"])
            a, b = self.world_trace("missing-a"), self.world_trace("missing-b")
            for path in (a, b):
                self.mutate(path, lambda rows: rows[2]["state"]["globals"].pop(clock))
            with self.assertRaises(parity.EvidenceError):
                parity.compare_world(a, b, self.header)

    def test_collision_clock_must_match_recorded_metadata_even_when_traces_agree(self):
        a, b = self.world_trace("metadata-a"), self.world_trace("metadata-b")
        demo = {"header": self.header, "checkpoint": {"collision_delay_last_play_time": 33286937}}
        self.assertEqual(parity.compare_world(a, b, self.header)["status"], "pass")
        for path in (a, b):
            self.assertEqual(parity.compare_checkpoint_collision_clock(demo, path, "d1"),
                             {"status": "fail", "applicable": True, "expected": 33286937, "actual": 0})
            self.mutate(path, lambda rows: rows[2]["state"]["globals"].update(Collision_delay_last_play_time=33286937))
            self.assertEqual(parity.compare_checkpoint_collision_clock(demo, path, "d1")["status"], "pass")
        demo["checkpoint"] = {}
        self.assertEqual(parity.compare_checkpoint_collision_clock(demo, self.world_trace("legacy"), "d1")["status"], "incomplete")
        for value in (None, True, "0"):
            demo["checkpoint"] = {"collision_delay_last_play_time": value}
            with self.assertRaises(parity.EvidenceError):
                parity.compare_checkpoint_collision_clock(demo, a, "d1")
        demo.pop("checkpoint")
        with self.assertRaises(parity.EvidenceError):
            parity.compare_checkpoint_collision_clock(demo, a, "d1")

    def import_world_ai(self, ai):
        for local in (ai["local_default"], *ai["locals"].values()):
            self.import_ai_storage(local, parity.AI_SAVED_LOCAL_FIELDS, next_action_time=0, next_fire2=0)
        ai.update(Believed_player_seg=-1, Ai_last_missile_camera=-1)
        ai["path_runtime"]["last_buddy_polish_path_tick"] = 0
        for cloak in ai["cloak"]:
            cloak["last_segment"] = -1
        ai["boss"]["d1_hit_pending"] = ai["boss"].pop("Boss_hit_this_frame")
        ai["boss"]["d1_been_hit"] = ai["boss"].pop("Boss_been_hit")
        ai["boss"]["Boss_hit_time"] = -10 * 65536

    def import_world_storage(self, state):
        self.import_world_ai(state["ai"])
        for player in state["players"]:
            player["primary_ammo"] += [0] * 5
            player["secondary_ammo"] += [0] * 5
        for center in state["robotcenters"]:
            center["robot_flags"] += [0]
        for wall in state["walls"]:
            wall.update(cloak_value=0, controlling_trigger=-1)

    def test_world_ai_mapping_covers_default_inactive_slots_and_both_boundaries(self):
        a = self.world_trace("mapped-world-native")
        def seed(rows):
            for row in rows:
                if row["type"] == "object_boundary":
                    row["slots"]["3"] = self.robot_object()
                    row["allocator"].update(num_objects=1, highest_object_index=3)
                ai = row.get("state", {}).get("ai")
                if ai is not None:
                    ai["local_default"].update(last_see_time=-123, last_attack_time=456, wait_time=789)
                    ai["locals"]["999"] = {**copy.deepcopy(ai["local_default"]), "wait_time": -987}
        self.mutate(a, seed)
        b = self.write("mapped-world-imported", list(parity.records(a)))
        def import_rows(rows):
            for row in rows:
                if row["type"] == "object_boundary":
                    self.import_robot(row["slots"]["3"])
                if "ai" in row.get("state", {}):
                    self.import_world_storage(row["state"])
        self.mutate(b, import_rows)
        before = b.read_bytes()
        report = parity.compare_world(a, b, self.header, "d2")
        self.assertEqual(report["records_compared"], 6)
        self.assertEqual(report["status"], "pass")
        for path in report["first_field_differences"]:
            self.assertFalse(any(field in path for field in (*parity.AI_SAVED_LOCAL_FIELDS, *parity.AI_SAVED_STATIC_FIELDS, "d1_saved")))
        self.assertEqual(b.read_bytes(), before)
        for index, phase in ((2, "world_boundary:restored"), (4, "world_state:frame"), (6, "world_boundary:terminal")):
            for slot in (None, "999"):
                for field in parity.AI_SAVED_LOCAL_FIELDS:
                    damaged = self.write("mapped-world-damaged", list(parity.records(b)))
                    def change(rows):
                        if index == 4:
                            rows[index]["state"] = copy.deepcopy(rows[2]["state"])
                        ai = rows[index]["state"]["ai"]
                        local = ai["local_default"] if slot is None else ai["locals"][slot]
                        local["d1_saved"][field] += 1
                    self.mutate(damaged, change)
                    report = parity.compare_world(a, damaged, self.header, "d2")
                    suffix = "local_default" if slot is None else f"locals.{slot}"
                    self.assertIn(f"{phase}:$.state.ai.{suffix}.{field}", report["first_field_differences"])
        for path, engine in ((a, "d2"), (b, "d1")):
            with self.assertRaises(parity.EvidenceError):
                list(parity.world_states(path, self.header, engine))

    def test_world_engine_identity_is_checked_before_mapping_empty_object_boundaries(self):
        a = self.world_trace("native-world-identity")
        with self.assertRaisesRegex(parity.EvidenceError, "declared executable"):
            list(parity.world_states(a, self.header, "d2"))
        b = self.write("imported-world-identity", list(parity.records(a)))
        self.mutate(b, lambda rows: [self.import_world_storage(row["state"])
                                    for row in rows if "ai" in row.get("state", {})])
        with self.assertRaisesRegex(parity.EvidenceError, "declared executable"):
            list(parity.world_states(b, self.header, "d1"))

    def test_world_neutral_capacity_and_wall_state_are_checked_at_every_boundary(self):
        a = self.world_trace("native-capacity")
        center = {**schema_example(parity.WORLD_SCHEMA["robotcenters"][0]), "robot_flags": [123]}
        self.mutate(a, lambda rows: [row["state"].update(robotcenters=[copy.deepcopy(center)]) for row in rows if "ai" in row.get("state", {})])
        b = self.write("imported-capacity", list(parity.records(a)))
        self.mutate(b, lambda rows: [self.import_world_storage(row["state"]) for row in rows if "ai" in row.get("state", {})])
        fields = parity.compare_world(a, b, self.header, "d2")["first_field_differences"]
        self.assertFalse(any("players" in path or "walls" in path or "robotcenters" in path for path in fields))
        mutations = (
            lambda s: s["players"][0]["primary_ammo"].__setitem__(9, 1),
            lambda s: s["players"][0]["secondary_ammo"].__setitem__(5, 1),
            lambda s: s["players"][0]["primary_ammo"].pop(),
            lambda s: s["robotcenters"][0]["robot_flags"].__setitem__(1, 1),
            lambda s: s["robotcenters"][0]["robot_flags"].pop(),
            lambda s: s["walls"][0].update(cloak_value=1),
            lambda s: s["walls"][0].update(controlling_trigger=0),
            lambda s: s["walls"][0].pop("cloak_value"),
            lambda s: s["ai"]["local_default"].update(next_fire2=1),
            lambda s: s["ai"]["boss"].update(Boss_hit_time=0),
            lambda s: s["ai"]["boss"].pop("Boss_hit_time"),
            lambda s: s["ai"]["path_runtime"].update(last_buddy_polish_path_tick=1),
            lambda s: s["ai"]["path_runtime"].pop("last_buddy_polish_path_tick"),
            lambda s: s["ai"]["locals"].update({"999": {**copy.deepcopy(s["ai"]["local_default"]), "next_action_time": 1}}),
        )
        for index in (2, 4, 6):
            for mutation in mutations:
                changed = self.write("invalid-capacity", list(parity.records(b)))
                def change(rows):
                    if index == 4:
                        rows[index]["state"] = copy.deepcopy(rows[2]["state"])
                    mutation(rows[index]["state"])
                self.mutate(changed, change)
                with self.assertRaises(parity.EvidenceError):
                    parity.compare_world(a, changed, self.header, "d2")

    def test_d2_ai_mirrors_are_typed_bounded_and_do_not_hide_unknown_state(self):
        a = self.world_trace("native-ai-mirrors")
        self.mutate(a, lambda rows: [row["state"].update(segments=[schema_example(parity.WORLD_SCHEMA["segments"][0]) for _ in range(2)])
                                    for row in rows if "ai" in row.get("state", {})])
        b = self.write("imported-ai-mirrors", list(parity.records(a)))
        def populate(rows):
            for row in rows:
                if "ai" not in row.get("state", {}):
                    continue
                self.import_world_storage(row["state"])
                ai = row["state"]["ai"]
                ai.update(Believed_player_seg=1, Ai_last_missile_camera=999)
                for index, cloak in enumerate(ai["cloak"]):
                    cloak["last_segment"] = index % 2
        self.mutate(b, populate)
        raw = b.read_bytes()
        self.assertEqual(parity.compare_world(a, b, self.header, "d2")["status"], "pass")
        self.assertEqual(b.read_bytes(), raw)
        for index, phase in ((2, "world_boundary:restored"), (4, "world_state:frame"), (6, "world_boundary:terminal")):
            for mutation in (
                    lambda ai: ai.update(Believed_player_seg=2),
                    lambda ai: ai.update(Believed_player_seg=-2),
                    lambda ai: ai.update(Believed_player_seg=True),
                    lambda ai: ai.update(Ai_last_missile_camera=1000),
                    lambda ai: ai.update(Ai_last_missile_camera=-2),
                    lambda ai: ai.update(Ai_last_missile_camera=False),
                    lambda ai: ai["cloak"][7].update(last_segment=2),
                    lambda ai: ai["cloak"][7].update(last_segment=-2),
                    lambda ai: ai["cloak"][7].update(last_segment=1.0)):
                bad = self.write("invalid-ai-mirror", list(parity.records(b)))
                def change(rows):
                    if index == 4:
                        rows[index]["state"] = copy.deepcopy(rows[2]["state"])
                    mutation(rows[index]["state"]["ai"])
                self.mutate(bad, change)
                with self.assertRaises(parity.EvidenceError):
                    parity.compare_world(a, bad, self.header, "d2")
            for group in (None, "cloak", "path_runtime"):
                bad = self.write("unknown-ai-cache", list(parity.records(b)))
                def change(rows):
                    if index == 4:
                        rows[index]["state"] = copy.deepcopy(rows[2]["state"])
                    ai = rows[index]["state"]["ai"]
                    target = ai if group is None else ai["cloak"][0] if group == "cloak" else ai[group]
                    target["unknown"] = 123
                self.mutate(bad, change)
                differences = parity.compare_world(a, bad, self.header, "d2")["first_field_differences"]
                suffix = "" if group is None else ".cloak[0]" if group == "cloak" else ".path_runtime"
                self.assertIn(f"{phase}:$.state.ai{suffix}.unknown", differences)
        for mutation in (
                lambda ai: ai.update(Believed_player_seg=1),
                lambda ai: ai.update(Ai_last_missile_camera=-1),
                lambda ai: ai["cloak"][0].update(last_segment=-1),
                lambda ai: ai["path_runtime"].update(last_buddy_polish_path_tick=0)):
            bad = self.write("mixed-native-ai-cache", list(parity.records(a)))
            self.mutate(bad, lambda rows: mutation(rows[2]["state"]["ai"]))
            with self.assertRaisesRegex(parity.EvidenceError, "D2-only AI caches"):
                list(parity.world_states(bad, self.header, "d1"))

    def test_boss_mapping_preserves_exact_integers_at_each_boundary(self):
        a = self.world_trace("native-boss-mapping")
        self.mutate(a, lambda rows: [row["state"]["ai"]["boss"].update(Boss_hit_this_frame=23456, Boss_been_hit=-12345)
                                    for row in rows if "ai" in row.get("state", {})])
        b = self.write("imported-boss-mapping", list(parity.records(a)))
        self.mutate(b, lambda rows: [self.import_world_storage(row["state"])
                                    for row in rows if "ai" in row.get("state", {})])
        report = parity.compare_world(a, b, self.header, "d2")
        self.assertFalse(any("Boss_been_hit" in path or "Boss_hit_this_frame" in path
                             for path in report["first_field_differences"]))
        self.assertFalse(any("Boss_hit_time" in path or "last_buddy_polish_path_tick" in path
                             for path in report["first_field_differences"]))
        for index, phase in ((2, "world_boundary:restored"), (4, "world_state:frame"), (6, "world_boundary:terminal")):
            for field, original in (("d1_been_hit", "Boss_been_hit"), ("d1_hit_pending", "Boss_hit_this_frame")):
                damaged = self.write("damaged-boss-mapping", list(parity.records(b)))
                def change(rows):
                    if index == 4:
                        rows[index]["state"] = copy.deepcopy(rows[2]["state"])
                    rows[index]["state"]["ai"]["boss"][field] = 1
                self.mutate(damaged, change)
                differences = parity.compare_world(a, damaged, self.header, "d2")["first_field_differences"]
                self.assertIn(f"{phase}:$.state.ai.boss.{original}", differences)
        for path, engine, field in ((a, "d1", "d1_been_hit"), (b, "d2", "Boss_been_hit")):
            self.mutate(path, lambda rows: rows[2]["state"]["ai"]["boss"].update({field: 0}))
            with self.assertRaisesRegex(parity.EvidenceError, "Mixed native and D2 boss"):
                list(parity.world_states(path, self.header, engine))

    @staticmethod
    def imported_trigger(flags):
        actions = (flags & 15) | ((flags >> 2) & 240)
        kind = next((kind for bit, kind in ((256, 4), (8, 3), (1, 0), (64, 2), (128, 5), (512, 6)) if flags & bit), 0)
        return {"type": kind, "flags": 128 | (64 if flags & 16 else 0) | (2 if flags & 32 else 0),
                "pad": actions if actions < 128 else actions - 256,
                "num_links": 1, "value": 12345, "time": -67890,
                "segments": list(range(10)), "sides": [0] * 10,
                "d1_saved": {"type": -127, "link_num": 126}}

    def test_trigger_mapping_retains_all_flag_combinations_and_original_storage(self):
        for flags in range(1024):
            raw = self.imported_trigger(flags)
            before = copy.deepcopy(raw)
            expected = {key: value for key, value in raw.items() if key not in ("pad", "d1_saved")}
            expected.update(type=-127, link_num=126, flags=flags)
            self.assertEqual(parity.canonical_d1_trigger(raw), expected)
            self.assertEqual(raw, before)
        raw = self.imported_trigger(0)
        raw["d1_saved"]["unknown"] = 8
        self.assertEqual(parity.canonical_d1_trigger(raw)["d1_saved"], {"unknown": 8})
        for mutation in (lambda t: t.update(flags=0), lambda t: t.update(flags=129),
                         lambda t: t.update(flags=132), lambda t: t.update(type=3)):
            raw = self.imported_trigger(0)
            mutation(raw)
            with self.assertRaises(parity.EvidenceError):
                parity.canonical_d1_trigger(raw)

    def test_trigger_mapping_checks_boundaries_and_rejects_incomplete_raw_records(self):
        a, b = self.world_trace("native-trigger"), self.world_trace("imported-trigger")
        raw = self.imported_trigger(1 | 16 | 32 | 64)
        native = {"type": -127, "flags": 113, "link_num": 126, "num_links": 1,
                  "value": 12345, "time": -67890, "segments": list(range(10)), "sides": [0] * 10}
        self.mutate(a, lambda rows: [row["state"].update(triggers=[copy.deepcopy(native)]) for row in rows if "ai" in row.get("state", {})])
        def import_rows(rows):
            for row in rows:
                if "ai" in row.get("state", {}):
                    self.import_world_storage(row["state"])
                    row["state"]["triggers"] = [copy.deepcopy(raw)]
        self.mutate(b, import_rows)
        fields = parity.compare_world(a, b, self.header, "d2")["first_field_differences"]
        self.assertFalse(any("triggers" in path for path in fields))
        for index, phase in ((2, "world_boundary:restored"), (4, "world_state:frame"), (6, "world_boundary:terminal")):
            for field in ("type", "link_num"):
                changed = self.write("changed-trigger", list(parity.records(b)))
                def change(rows):
                    rows[index]["state"]["triggers"] = [copy.deepcopy(raw)]
                    rows[index]["state"]["triggers"][0]["d1_saved"][field] += 1
                self.mutate(changed, change)
                fields = parity.compare_world(a, changed, self.header, "d2")["first_field_differences"]
                self.assertIn(f"{phase}:$.state.triggers[0].{field}", fields)
        for mutation in (lambda t: t.pop("d1_saved"), lambda t: t["d1_saved"].pop("link_num"),
                         lambda t: t["d1_saved"].update(type=128), lambda t: t["d1_saved"].update(link_num=False),
                         lambda t: t.update(pad=256), lambda t: t.update(link_num=-1),
                         lambda t: t["segments"].pop(), lambda t: t.update(num_links=11)):
            changed = self.write("invalid-trigger", list(parity.records(b)))
            self.mutate(changed, lambda rows: mutation(rows[2]["state"]["triggers"][0]))
            with self.assertRaises(parity.EvidenceError):
                parity.compare_world(a, changed, self.header, "d2")

    def test_endlevel_private_state_is_required_and_compared(self):
        a, b = self.world_trace("endlevel-a"), self.world_trace("endlevel-b")
        self.mutate(b, lambda rows: rows[2]["state"]["endlevel"]["frame"].update(sound_count=3))
        report = parity.compare_world(a, b, self.header)
        self.assertIn("world_boundary:restored:$.state.endlevel.frame.sound_count", report["first_field_differences"])
        for change in (lambda s: s.pop("endlevel"),
                       lambda s: s["endlevel"]["frame"].pop("explosion_wait1"),
                       lambda s: s["endlevel"]["fly"].pop(),
                       lambda s: s["endlevel"].update(explosion_playing=1),
                       lambda s: s["endlevel"]["exit_orientation"].pop()):
            paths = [self.world_trace("missing-endlevel-a"), self.world_trace("missing-endlevel-b")]
            for path in paths:
                self.mutate(path, lambda rows: change(rows[2]["state"]))
            with self.assertRaises(parity.EvidenceError):
                parity.compare_world(*paths, self.header)

    def test_external_explosion_requires_active_object_union(self):
        paths = [self.world_trace("explosion-a"), self.world_trace("explosion-b")]
        explosion = schema_example(parity.OBJECT_SCHEMA)
        explosion.update(type=1, control_type=2, render_type=2, explosion={})
        for path in paths:
            self.mutate(path, lambda rows: rows[2]["state"]["endlevel"].update(explosion_playing=1, explosion=[explosion]))
        with self.assertRaisesRegex(parity.EvidenceError, "explosion"):
            parity.compare_world(*paths, self.header)

    def test_restored_difference_cannot_hide_terminal_difference(self):
        a, b = self.world_trace("world-a"), self.world_trace("world-b")
        self.mutate(b, lambda rows: rows[2]["state"]["players"][0].update(shields=99))
        self.mutate(b, lambda rows: rows[-1]["state"]["walls"][0].update(hps=199))
        report = parity.compare_world(a, b, self.header)
        self.assertEqual(report["status"], "fail")
        fields = report["first_field_differences"]
        self.assertIn("world_boundary:restored:$.state.players[0].shields", fields)
        self.assertIn("world_boundary:terminal:$.state.walls[0].hps", fields)

    def test_terminal_before_cursor_advance_retains_raw_difference(self):
        a, b = self.world_trace("world-a"), self.world_trace("world-b")
        self.mutate(b, lambda rows: [row.update(f=1) for row in rows[-2:]])
        report = parity.compare_world(a, b, self.header)
        self.assertEqual(report["status"], "fail")
        self.assertEqual(report["first_field_differences"]["world_boundary:terminal:$.f"]["actual"], 1)
        self.mutate(b, lambda rows: rows.pop(4))
        with self.assertRaises(parity.EvidenceError):
            parity.compare_world(a, b, self.header)
        b = self.world_trace("world-b")
        self.mutate(b, lambda rows: rows[-1].update(f=0))
        with self.assertRaises(parity.EvidenceError):
            parity.compare_world(a, b, self.header)

    def test_world_frame_difference_and_unknown_fields_are_retained(self):
        a, b = self.world_trace("world-a"), self.world_trace("world-b")
        def change(rows):
            rows[4]["state"]["players"] = copy.deepcopy(rows[2]["state"]["players"])
            rows[4]["state"]["players"][0]["shields"] = 98
            rows[4]["state"]["unmapped"] = 7
        self.mutate(b, change)
        report = parity.compare_world(a, b, self.header)
        self.assertEqual(report["status"], "fail")
        self.assertEqual(report["first_field_differences"]["world_state:frame:$.state.players[0].shields"]["frame"], 1)
        self.assertIn("world_state:frame:$.state.unmapped", report["first_field_differences"])

    def test_world_fields_fail_closed_in_boundaries_and_deltas(self):
        a = self.world_trace("world-a")
        mutations = (
            lambda state: state.update(players={}),
            lambda state: state["globals"].pop("GameTime64"),
            lambda state: state["players"][0].update(shields=True),
            lambda state: state["players"][0].update(shields=100.0),
            lambda state: state["players"][0].update(primary_ammo=["0"]),
            lambda state: state.update(fuelcenters=[{"Type": 1}]),
            lambda state: state.update(doors=[{"n_parts": 1, "time": 0, "front": [0], "back": [0, 1]}]),
            lambda state: state["stuck"]["slots"].update({"0": [1, 2]}),
            lambda state: state.update(secrets=[[1, None]]),
            lambda state: state["ai"]["paths"].pop(),
            lambda state: state["ai"].update(path_free_index=2501),
            lambda state: state["ai"].update(Num_awareness_events=65),
            lambda state: state["ai"]["local_default"]["goal_state"].pop(),
            lambda state: state["ai"]["locals"].update({"0": copy.deepcopy(state["ai"]["local_default"])}),
            lambda state: state["ai"]["locals"].update({"1000": copy.deepcopy(state["ai"]["local_default"])}),
            lambda state: state["ai"].update(local_capacity=999),
            lambda state: state["ai"]["boss"]["teleport_segments"].pop(),
            lambda state: state["ai"]["boss"].update(Num_boss_gate_segs=101),
            lambda state: state["ai"]["boss"].pop("Num_boss_gate_segs"),
            lambda state: state["ai"]["boss"].pop("Boss_hit_this_frame"),
            lambda state: state["ai"]["local_default"].pop("last_attack_time"),
        )
        for index in (2, 4, 6):
            for mutation in mutations:
                with self.subTest(record=index, mutation=mutation):
                    b = self.world_trace("world-b")
                    def corrupt(rows):
                        if index == 4:
                            rows[index]["state"] = copy.deepcopy(rows[2]["state"])
                        mutation(rows[index]["state"])
                    self.mutate(b, corrupt)
                    with self.assertRaises(parity.EvidenceError):
                        parity.compare_world(a, b, self.header)

    def test_nonzero_ai_default_is_compared_without_losing_exceptions(self):
        a, b = self.world_trace("world-a"), self.world_trace("world-b")
        def baseline(rows):
            for row in rows:
                ai = row.get("state", {}).get("ai")
                if ai is not None:
                    ai["local_default"]["time_player_seen"] = -12345
                    ai["locals"]["999"] = {**copy.deepcopy(ai["local_default"]), "time_player_seen": 0}
        for path in (a, b):
            self.mutate(path, baseline)
        self.assertEqual(parity.compare_world(a, b, self.header)["status"], "pass")
        def change(rows):
            ai = copy.deepcopy(rows[2]["state"]["ai"])
            ai["local_default"]["time_player_seen"] += 1
            rows[4]["state"]["ai"] = ai
        self.mutate(b, change)
        report = parity.compare_world(a, b, self.header)
        self.assertEqual(report["status"], "fail")
        self.assertIn("world_state:frame:$.state.ai.local_default.time_player_seen", report["first_field_differences"])
        self.assertFalse(any("locals.999" in path for path in report["first_field_differences"]))

    def test_engine_specific_ai_fields_are_required_and_typed(self):
        rows = list(parity.records(self.world_trace("native-storage")))
        state = rows[2]["state"]
        ai = state["ai"]
        for key in ("last_see_time", "last_attack_time", "wait_time"):
            del ai["local_default"][key]
        ai["local_default"].update(next_action_time=0, next_fire2=0)
        ai["local_default"]["d1_saved"] = dict.fromkeys("last_see_time last_attack_time wait_time".split(), 0)
        ai.update(Believed_player_seg=-1, Ai_last_missile_camera=-1)
        ai["path_runtime"]["last_buddy_polish_path_tick"] = 0
        for cloak in ai["cloak"]:
            cloak["last_segment"] = -1
        del ai["boss"]["Boss_hit_this_frame"]
        del ai["boss"]["Boss_been_hit"]
        ai["boss"].update(Boss_hit_time=-10, d1_hit_pending=0, d1_been_hit=0)
        for player in state["players"]:
            player["primary_ammo"] += [0] * 5
            player["secondary_ammo"] += [0] * 5
        for wall in state["walls"]:
            wall.update(cloak_value=0, controlling_trigger=-1)
        ai["locals"]["999"] = {**copy.deepcopy(ai["local_default"]), "next_action_time": 12}
        parity.validate_world_state(state)
        for mutation in (
                lambda a: a.pop("Believed_player_seg"),
                lambda a: a["cloak"][0].pop("last_segment"),
                lambda a: a["path_runtime"].update(last_buddy_polish_path_tick=False),
                lambda a: a["local_default"].pop("next_fire2"),
                lambda a: a["local_default"].pop("d1_saved"),
                lambda a: a["local_default"]["d1_saved"].pop("last_see_time"),
                lambda a: a["locals"]["999"]["d1_saved"].update(wait_time=False),
                lambda a: a["locals"]["999"].update(next_action_time=12.0),
                lambda a: a["locals"]["999"].update(next_fire2=True),
                lambda a: a["locals"]["999"].update(wait_time=0),
                lambda a: a["boss"].pop("d1_hit_pending"),
                lambda a: a["boss"].pop("d1_been_hit"),
                lambda a: a["boss"].update(d1_been_hit=False)):
            damaged = copy.deepcopy(state)
            mutation(damaged["ai"])
            with self.assertRaises(parity.EvidenceError):
                parity.validate_world_state(damaged)

    def test_inactive_ai_slot_changes_remain_compared(self):
        a, b = self.world_trace("world-a"), self.world_trace("world-b")
        def change(rows):
            ai = copy.deepcopy(rows[2]["state"]["ai"])
            ai["locals"]["9"] = {**copy.deepcopy(ai["local_default"]), "next_fire": 17}
            rows[4]["state"]["ai"] = ai
        self.mutate(b, change)
        result = parity.compare_world(a, b, self.header)
        difference = result["first_field_differences"]["world_state:frame:$.state.ai.locals.9"]
        self.assertEqual(result["status"], "fail")
        self.assertEqual(difference["frame"], 1)
        self.assertEqual(difference["actual"]["next_fire"], 17)
        for index, field, value in ((2, "version", 1), (2, "version", 2), (4, "f", True), (4, "f", 1.0)):
            b = self.world_trace("world-b")
            self.mutate(b, lambda rows: rows[index].update({field: value}))
            with self.assertRaises(parity.EvidenceError):
                parity.compare_world(a, b, self.header)


if __name__ == "__main__":
    unittest.main()

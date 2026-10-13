"""Ordered input admission and lossless aliases match the native codec contract."""

import copy
import unittest

import ordered_public_replay as ordered
from d1_replay_parity import EvidenceError


def fixture(game="d1"):
    header = {"type": "header", "version": 5, "game": game, "frame_count": 2}
    frames = [
        {
            "type": "frame",
            "f": 0,
            "ft": 2000,
            "input": {"s": {"f2s": 1, "f": -50}, "p": {"f1": 2}},
            "rng": {"s": 101, "c": 3},
            "dispatch": {
                "pre_rng": {"s": 100, "c": 2},
                "ops": [
                    {"k": "reset", "ft": 0},
                    {"k": "sample", "ft": 3276, "s": {"f2s": 1, "f": -50}, "p": {"f2": 1}},
                    {
                        "k": "command",
                        "ft": 4096,
                        "event": {"kind": "direct_command", "command": "change_difficulty", "difficulty": 3},
                    },
                ],
            },
        },
        {
            "type": "frame",
            "f": 1,
            "input": {"s": {"f2s": 7}, "p": {"f1": 2}},
            "rng": {"s": 102, "c": 4},
            "dispatch": {"pre_rng": "=", "ops": [{"k": "sample", "ft": 1234, "s": "="}]},
        },
    ]
    tail = {
        "type": "input_tail",
        "f": 2,
        "input": {},
        "rng": {"s": 103, "c": 5},
        "dispatch": {"pre_rng": "=", "ops": [{"k": "sample", "ft": 4000, "s": "="}]},
    }
    return header, frames, tail


class OrderedInputTests(unittest.TestCase):
    def test_zero_frame_tail_only_and_malformed_counts_are_rejected(self):
        for game in ("d1", "d2"):
            for count in (0, -1, False, 0.0, "0", None, 2**32):
                for effectful in (False, True):
                    with self.subTest(game=game, count=count, effectful=effectful):
                        header, _, tail = fixture(game)
                        header["frame_count"] = count
                        tail["f"] = 0
                        if not effectful:
                            tail["dispatch"]["ops"] = []
                        with self.assertRaisesRegex(EvidenceError, "frame_count"):
                            ordered.validate(header, [], tail)
            header, _, tail = fixture(game)
            del header["frame_count"]
            with self.assertRaisesRegex(EvidenceError, "frame_count"):
                ordered.validate(header, [], tail)

    def test_current_frame_and_independent_tail_aliases(self):
        for game in ("d1", "d2"):
            header, frames, tail = fixture(game)
            decoded = ordered.validate(header, frames, tail)
            self.assertEqual([row["frame_time"] for row in decoded], [2000, 2000, None])
            self.assertEqual(decoded[0]["ops"][1]["ft"], 3276)
            self.assertEqual(decoded[0]["ops"][2]["ft"], 4096)
            self.assertEqual(decoded[1]["ops"][0]["s"]["f2s"], 7)
            self.assertEqual(decoded[1]["ops"][0]["s"]["f"], -50)
            self.assertEqual(decoded[1]["pre_rng"], {"s": 102, "c": 4})
            self.assertEqual(decoded[-1]["pre_rng"], {"s": 103, "c": 5})
            self.assertFalse(any(decoded[-1]["ops"][0]["s"].values()))
            self.assertFalse(any(decoded[1]["ops"][0]["p"].values()))
            self.assertEqual(decoded[1]["residual"]["p"]["f1"], 2)

    def test_held_zero_clears_prior_frame_and_pulses_never_inherit(self):
        header, frames, tail = fixture()
        frames[1]["input"] = {"s": {"f2s": 0}}
        decoded = ordered.validate(header, frames, tail)
        self.assertEqual(decoded[1]["residual"]["s"]["f2s"], 0)
        self.assertEqual(decoded[1]["residual"]["s"]["f"], -50)
        self.assertFalse(any(decoded[1]["residual"]["p"].values()))

    def test_compaction_is_lossless_and_does_not_mutate_input(self):
        header, frames, tail = fixture()
        frames[1]["dispatch"]["pre_rng"] = {"s": 102, "c": 4}
        frames[1]["dispatch"]["ops"] = [
            {"k": "sample", "ft": 9, "s": {"f2s": 7, "f": -50}, "p": {"f2": 0}},
            {"k": "sample", "ft": 11, "s": {}, "p": {"f1": 2}},
        ]
        before = copy.deepcopy((frames, tail))
        new_frames, new_tail = ordered.compact(header, frames, tail)
        self.assertEqual((frames, tail), before)
        self.assertEqual(new_frames[0]["dispatch"]["pre_rng"], {"s": 100, "c": 2})
        self.assertEqual(new_frames[0]["dispatch"]["ops"][1]["s"], "=")
        self.assertEqual(new_frames[0]["dispatch"]["ops"][1]["p"], {"f2": 1})
        self.assertEqual(new_frames[1]["dispatch"]["pre_rng"], "=")
        self.assertEqual(new_frames[1]["dispatch"]["ops"][0]["s"], "=")
        self.assertNotIn("p", new_frames[1]["dispatch"]["ops"][0])
        self.assertEqual(new_frames[1]["dispatch"]["ops"][1]["s"], {})
        self.assertEqual(new_frames[1]["dispatch"]["ops"][1]["p"], {"f1": 2})
        self.assertEqual(ordered.validate(header, new_frames, new_tail), ordered.validate(header, frames, tail))
        self.assertEqual(ordered.compact(header, new_frames, new_tail), (new_frames, new_tail))

    def test_rng_alias_requires_count_and_state_equality(self):
        header, frames, tail = fixture()
        for changed in ({"s": 102, "c": 3}, {"s": 101, "c": 4}):
            frames[1]["dispatch"]["pre_rng"] = changed
            compacted, _ = ordered.compact(header, frames, tail)
            self.assertEqual(compacted[1]["dispatch"]["pre_rng"], changed)

    def test_empty_batches_are_explicit_and_valid(self):
        header, frames, tail = fixture()
        for row in [*frames, tail]:
            row["dispatch"]["ops"] = []
        self.assertTrue(all(not row["ops"] for row in ordered.validate(header, frames, tail)))
        with self.assertRaises(EvidenceError):
            ordered.validate(header, frames, None)
        del frames[0]["dispatch"]
        with self.assertRaises(EvidenceError):
            ordered.validate(header, frames, tail)

    def test_all_supported_commands_and_game_restrictions(self):
        arguments = {
            "guidebot_goal": {"special_key": 3, "from_menu": False},
            "drop_marker": {"player_marker_num": 0, "message": "turn left"},
            "delete_marker": {"player_marker_num": 8, "expected_signature": ordered.INT32_MAX},
            "change_difficulty": {"difficulty": 4},
            "select_weapon_exact": {"weapon_class": 1, "weapon_index": 4},
        }
        for game in ("d1", "d2"):
            for command in ordered.COMMANDS:
                with self.subTest(game=game, command=command):
                    header, frames, tail = fixture(game)
                    event = {"kind": "direct_command", "command": command, **arguments.get(command, {})}
                    frames[0]["dispatch"]["ops"] = [{"k": "command", "ft": 1, "event": event}]
                    if game == "d1" and command not in ordered.D1_COMMANDS:
                        with self.assertRaises(EvidenceError):
                            ordered.validate(header, frames, tail)
                    else:
                        self.assertEqual(ordered.validate(header, frames, tail)[0]["ops"][0]["event"], event)

    def test_command_key_and_marker_domains(self):
        domains = (
            (
                "guidebot_goal",
                "special_key",
                {"from_menu": False},
                ordered.GUIDEBOT_KEYS,
                [-1, 0, 1, 0x0C, 0x12, 0x113, 0x202, 0x302, ordered.INT32_MAX],
            ),
            ("drop_marker", "player_marker_num", {"message": "ok"}, range(9), [-1, 9, 15, 16, ordered.INT32_MAX]),
            (
                "delete_marker",
                "player_marker_num",
                {"expected_signature": 42},
                range(9),
                [-1, 9, 15, 16, ordered.INT32_MAX],
            ),
            (
                "delete_marker",
                "expected_signature",
                {"player_marker_num": 0},
                (0, 42, ordered.INT32_MAX),
                [-1, ordered.INT32_MAX + 1],
            ),
        )
        for command, field, extra, supported, rejected in domains:
            for value in [*supported, *rejected]:
                header, frames, tail = fixture("d2")
                event = {"kind": "direct_command", "command": command, field: value, **extra}
                frames[0]["dispatch"]["ops"] = [{"k": "command", "ft": 0, "event": event}]
                with self.subTest(command=command, value=value):
                    if value in supported:
                        ordered.validate(header, frames, tail)
                    else:
                        with self.assertRaises(EvidenceError):
                            ordered.validate(header, frames, tail)

    def test_input_order_and_repeated_dispatches_are_retained(self):
        header, frames, tail = fixture()
        operations = [
            {"k": "reset", "ft": 4},
            {"k": "sample", "ft": 4, "s": {"f2s": 1}},
            {"k": "reset", "ft": 4},
            {"k": "sample", "ft": 4, "s": {}},
        ]
        frames[0]["dispatch"]["ops"] = operations
        decoded = ordered.validate(header, frames, tail)
        compacted, compact_tail = ordered.compact(header, frames, tail)
        self.assertEqual(len(compacted[0]["dispatch"]["ops"]), 4)
        self.assertEqual(ordered.validate(header, compacted, compact_tail), decoded)
        frames[0]["dispatch"]["ops"] = list(reversed(operations))
        self.assertNotEqual(ordered.validate(header, frames, tail), decoded)

    def test_clear_is_explicit_distinct_and_strict(self):
        for game in ("d1", "d2"):
            header, frames, tail = fixture(game)
            operations = [{"k": "reset", "ft": 0}, {"k": "clear", "ft": 1638}, {"k": "reset", "ft": 3276}]
            frames[0]["dispatch"]["ops"] = copy.deepcopy(operations)
            tail["dispatch"]["ops"] = [{"k": "clear", "ft": 0}]
            decoded = ordered.validate(header, frames, tail)
            self.assertEqual(decoded[0]["ops"], operations)
            compacted, compact_tail = ordered.compact(header, frames, tail)
            self.assertEqual(compacted[0]["dispatch"]["ops"], operations)
            self.assertEqual(compact_tail["dispatch"]["ops"], [{"k": "clear", "ft": 0}])
            for invalid in (
                {"k": "clear"},
                {"k": "clear", "ft": False},
                {"k": "clear", "ft": -1},
                {"k": "clear", "ft": ordered.MAX_FRAME_TIME + 1},
                {"k": "clear", "ft": 1, "s": {}},
                {"k": "clear", "ft": 1, "p": {}},
                {"k": "clear", "ft": 1, "event": {}},
            ):
                with self.subTest(game=game, operation=invalid):
                    frames[0]["dispatch"]["ops"] = [invalid]
                    with self.assertRaises(EvidenceError):
                        ordered.validate(header, frames, tail)

    def test_automap_controls_phases_and_aliases(self):
        for game in ("d1", "d2"):
            header, frames, tail = fixture(game)
            operations = [
                {"k": "controls", "ft": 1, "s": {"f2s": 7, "f": -50}, "p": {}},
                {"k": "automap", "ft": 2, "phase": "begin"},
                {"k": "clear", "ft": 3},
                {"k": "automap", "ft": 4, "phase": "depths"},
                {"k": "automap", "ft": 5, "phase": "active"},
                {"k": "controls", "ft": 6, "s": {}},
                {"k": "automap", "ft": 7, "phase": "close"},
            ]
            frames[1]["dispatch"]["ops"] = operations
            tail["dispatch"]["ops"] = [
                {"k": "automap", "ft": 0, "phase": phase} for phase in ("begin", "depths", "active")
            ] + [{"k": "controls", "ft": 65536, "s": "="}]
            decoded = ordered.validate(header, frames, tail)
            new_frames, new_tail = ordered.compact(header, frames, tail)
            self.assertEqual(new_frames[1]["dispatch"]["ops"][0], {"k": "controls", "ft": 1, "s": "="})
            self.assertEqual(decoded[1]["ops"][0]["s"]["f2s"], 7)
            self.assertFalse(any(decoded[-1]["ops"][-1]["s"].values()))
            self.assertEqual(ordered.validate(header, new_frames, new_tail), decoded)
            for sequence in (("begin", "close"), ("begin", "depths", "close")):
                frames[1]["dispatch"]["ops"] = [{"k": "automap", "ft": 0, "phase": phase} for phase in sequence]
                ordered.validate(header, frames, tail)

    def test_invalid_automap_phase_and_sequence(self):
        for game in ("d1", "d2"):
            header, frames, tail = fixture(game)
            for phase in (None, False, 1, "", "=", "open", "BEGIN", {}, []):
                frames[0]["dispatch"]["ops"] = [{"k": "automap", "ft": 0, "phase": phase}]
                with self.subTest(game=game, phase=phase), self.assertRaises(EvidenceError):
                    ordered.validate(header, frames, tail)
            for sequence in (
                ("depths",),
                ("active",),
                ("close",),
                ("begin",),
                ("begin", "depths"),
                ("begin", "active", "close"),
                ("begin", "begin", "close"),
                ("begin", "depths", "depths", "close"),
                ("begin", "depths", "active"),
                ("begin", "depths", "active", "active", "close"),
                ("begin", "close", "close"),
            ):
                frames[0]["dispatch"]["ops"] = [{"k": "automap", "ft": 0, "phase": phase} for phase in sequence]
                with self.subTest(game=game, sequence=sequence), self.assertRaises(EvidenceError):
                    ordered.validate(header, frames, tail)
            frames[0]["dispatch"]["ops"] = []
            for sequence in (("begin",), ("begin", "depths"), ("close",)):
                tail["dispatch"]["ops"] = [{"k": "automap", "ft": 0, "phase": phase} for phase in sequence]
                with self.subTest(tail=sequence), self.assertRaises(EvidenceError):
                    ordered.validate(header, frames, tail)

    def test_controls_and_automap_reject_unsupported_fields(self):
        for operation in (
            {"k": "controls", "ft": 0},
            {"k": "controls", "ft": 0, "s": "previous"},
            {"k": "controls", "ft": 0, "s": {}, "p": False},
            {"k": "controls", "ft": 0, "s": {}, "phase": "begin"},
            {"k": "automap", "ft": 0},
            {"k": "automap", "ft": False, "phase": "begin"},
            {"k": "automap", "ft": 0, "phase": "begin", "s": {}},
            {"k": "automap", "ft": 0, "phase": "begin", "event": {}},
        ):
            header, frames, tail = fixture()
            frames[0]["dispatch"]["ops"] = [operation]
            with self.subTest(operation=operation), self.assertRaises(EvidenceError):
                ordered.validate(header, frames, tail)

    def test_rear_view_baseline_and_action_roundtrip(self):
        for game in ("d1", "d2"):
            header, frames, tail = fixture(game)
            header["rear_view"] = {"active": 1, "leave_mode": 0, "current_cockpit": 1, "preferred_cockpit": 4}
            actions = ("arm", "release_off", "enter", "toggle_off", "reset")
            operations = [{"k": "rear_view", "ft": index, "action": action} for index, action in enumerate(actions)]
            frames[0]["dispatch"]["ops"] = operations
            tail["dispatch"]["ops"] = [{"k": "rear_view", "ft": 65536, "action": "enter"}]
            decoded = ordered.validate(header, frames, tail)
            compacted, compact_tail = ordered.compact(header, frames, tail)
            self.assertEqual(decoded[0]["ops"], operations)
            self.assertEqual(compacted[0]["dispatch"]["ops"], operations)
            self.assertEqual(ordered.validate(header, compacted, compact_tail), decoded)
            del header["rear_view"]
            ordered.validate(header, frames, tail)

    def test_rear_view_rejects_invalid_baseline_and_action(self):
        baseline = {"active": 1, "leave_mode": 0, "current_cockpit": 1, "preferred_cockpit": 4}
        for field in baseline:
            for value in (-1, 5, ordered.INT32_MAX, ordered.UINT32_MAX + 1, False, 1.0, "1", None):
                header, frames, tail = fixture()
                header["rear_view"] = {**baseline, field: value}
                with self.subTest(field=field, value=value), self.assertRaises(EvidenceError):
                    ordered.validate(header, frames, tail)
            header["rear_view"] = dict(baseline)
            del header["rear_view"][field]
            with self.assertRaises(EvidenceError):
                ordered.validate(header, frames, tail)
        header["rear_view"] = {**baseline, "entry_time": 1234}
        with self.assertRaises(EvidenceError):
            ordered.validate(header, frames, tail)
        for action in (False, None, 1, "", "toggle", "=", "ENTER", {}, []):
            header, frames, tail = fixture()
            frames[0]["dispatch"]["ops"] = [{"k": "rear_view", "ft": 0, "action": action}]
            with self.subTest(action=action), self.assertRaises(EvidenceError):
                ordered.validate(header, frames, tail)
        for extra in ({"phase": "active"}, {"s": {}}, {"event": {}}):
            frames[0]["dispatch"]["ops"] = [{"k": "rear_view", "ft": 0, "action": "enter", **extra}]
            with self.assertRaises(EvidenceError):
                ordered.validate(header, frames, tail)

    def test_resolved_cockpit_operations(self):
        for game in ("d1", "d2"):
            header, frames, tail = fixture(game)
            operations = [
                {"k": "cockpit", "ft": current * 100, "current_cockpit": current, "preferred_cockpit": preferred}
                for current in (0, 2, 3)
                for preferred in (0, 2, 3)
            ]
            frames[0]["dispatch"]["ops"] = operations
            tail["dispatch"]["ops"] = operations[-1:]
            decoded = ordered.validate(header, frames, tail)
            compacted, compact_tail = ordered.compact(header, frames, tail)
            self.assertEqual(decoded[0]["ops"], operations)
            self.assertEqual(compacted[0]["dispatch"]["ops"], operations)
            self.assertEqual(ordered.validate(header, compacted, compact_tail), decoded)
            for field in ("current_cockpit", "preferred_cockpit"):
                for value in (-1, 1, 4, 5, False, None, 1.0, "1", ordered.UINT32_MAX + 1):
                    frames[0]["dispatch"]["ops"] = [{**operations[0], field: value}]
                    with self.subTest(field=field, value=value), self.assertRaises(EvidenceError):
                        ordered.validate(header, frames, tail)
                frames[0]["dispatch"]["ops"] = [dict(operations[0])]
                del frames[0]["dispatch"]["ops"][0][field]
                with self.assertRaises(EvidenceError):
                    ordered.validate(header, frames, tail)
            frames[0]["dispatch"]["ops"] = [{**operations[0], "s": {}}]
            with self.assertRaises(EvidenceError):
                ordered.validate(header, frames, tail)

    def test_view_projection_contract(self):
        header, frames, tail = fixture()
        normal = {
            "screen_width": 640,
            "screen_height": 480,
            "aspect": 65536,
            "zoom": 0x9000,
            "depth": 9000,
            "classic_depth": 0,
            "missile_view_enabled": 0,
        }
        header["view_projection"] = normal
        decoded = ordered.validate(header, frames, tail)
        compacted, compact_tail = ordered.compact(header, frames, tail)
        self.assertEqual(header["view_projection"], normal)
        self.assertEqual(ordered.validate(header, compacted, compact_tail), decoded)
        maxima = {
            "screen_width": 16384,
            "screen_height": 16384,
            "aspect": 16 * 65536,
            "zoom": 16 * 65536,
            "depth": 9000,
            "classic_depth": 1,
            "missile_view_enabled": 0,
        }
        for field, maximum in maxima.items():
            header["view_projection"] = {**normal, field: maximum}
            ordered.validate(header, frames, tail)
            for value in (-1, maximum + 1, False, None, 1.0, "1", ordered.UINT32_MAX + 1):
                header["view_projection"] = {**normal, field: value}
                with self.subTest(field=field, value=value), self.assertRaises(EvidenceError):
                    ordered.validate(header, frames, tail)
            if field not in {"classic_depth", "missile_view_enabled"}:
                header["view_projection"] = {**normal, field: 0}
                with self.assertRaises(EvidenceError):
                    ordered.validate(header, frames, tail)
            header["view_projection"] = dict(normal)
            del header["view_projection"][field]
            with self.assertRaises(EvidenceError):
                ordered.validate(header, frames, tail)
        for change in ({"screen_width": 16384, "screen_height": 4097}, {"main_view_fov": 110}):
            header["view_projection"] = {**normal, **change}
            with self.assertRaises(EvidenceError):
                ordered.validate(header, frames, tail)
        header["view_projection"] = {**normal, "screen_width": 16384, "screen_height": 4096}
        ordered.validate(header, frames, tail)
        header["game"] = "d2"
        header["view_projection"]["missile_view_enabled"] = 1
        ordered.validate(header, frames, tail)

    def test_integer_boundaries(self):
        header, frames, tail = fixture("d2")
        frames[0]["input"] = {
            "s": {"p": ordered.INT32_MIN, "h": ordered.INT32_MAX, "f2s": 255, "ab": 255},
            "p": {"sw": 10, "hl": 255},
        }
        frames[0]["rng"] = {"s": ordered.UINT32_MAX, "c": ordered.UINT32_MAX}
        frames[0]["ft"] = ordered.MAX_FRAME_TIME
        frames[0]["dispatch"]["ops"] = [{"k": "sample", "ft": 0, "s": "=", "p": {"sw": 10}}]
        ordered.validate(header, frames, tail)

    def test_invalid_values_and_overflow(self):
        cases = [
            (("ft",), False),
            (("ft",), 0),
            (("ft",), ordered.MAX_FRAME_TIME + 1),
            (("f",), True),
            (("f",), 1),
            (("rng", "s"), True),
            (("rng", "s"), -1),
            (("rng", "c"), ordered.UINT32_MAX + 1),
            (("input", "s", "f2s"), 256),
            (("input", "s", "f2s"), False),
            (("input", "s", "f"), ordered.INT32_MIN - 1),
            (("input", "s", "f"), 1.0),
            (("input", "p", "sw"), 11),
            (("input", "p", "f1"), True),
            (("input", "p", "hl"), 0),
            (("input", "s", "ab"), 0),
            (("dispatch", "pre_rng"), "previous"),
            (("dispatch", "pre_rng"), None),
            (("dispatch", "pre_rng"), True),
            (("dispatch", "ops"), {}),
            (("dispatch", "ops", 0, "k"), "guess"),
            (("dispatch", "ops", 0, "ft"), -1),
            (("dispatch", "ops", 0, "ft"), True),
            (("dispatch", "ops", 0, "ft"), ordered.MAX_FRAME_TIME + 1),
            (("dispatch", "ops", 0, "extra"), 0),
            (("dispatch", "ops", 1, "s"), "previous"),
            (("dispatch", "ops", 1, "s"), None),
            (("dispatch", "ops", 1, "p"), "="),
            (("dispatch", "ops", 1, "p", "f2"), 256),
            (("dispatch", "ops", 1, "p", "f2"), False),
            (("dispatch", "ops", 2, "event", "command"), "unknown"),
            (("dispatch", "ops", 2, "event", "difficulty"), 5),
            (("dispatch", "ops", 2, "event", "difficulty"), True),
            (("dispatch", "ops", 2, "event", "difficulty"), 2**63),
        ]
        for path, value in cases:
            with self.subTest(path=path, value=value):
                header, frames, tail = fixture()
                target = frames[0]
                for key in path[:-1]:
                    target = target[key]
                target[path[-1]] = value
                with self.assertRaises(EvidenceError):
                    ordered.validate(header, frames, tail)

    def test_missing_fields_and_malformed_tail(self):
        for path in (("rng", "c"), ("dispatch", "ops"), ("dispatch", "pre_rng", "c"), ("dispatch", "ops", 1, "s")):
            header, frames, tail = fixture()
            target = frames[0]
            for key in path[:-1]:
                target = target[key]
            del target[path[-1]]
            with self.subTest(path=path), self.assertRaises(EvidenceError):
                ordered.validate(header, frames, tail)
        for key, value in (("ft", 1), ("events", {}), ("state", {}), ("diag", {}), ("type", "frame"), ("f", 1)):
            header, frames, tail = fixture()
            tail[key] = value
            with self.subTest(key=key), self.assertRaises(EvidenceError):
                ordered.validate(header, frames, tail)

    def test_no_commands_in_telemetry(self):
        header, frames, tail = fixture()
        frames[0]["events"] = [{"kind": "direct_command", "command": "death_abort"}]
        with self.assertRaises(EvidenceError):
            ordered.validate(header, frames, tail)
        frames[0]["events"] = [{"kind": "observational"}]
        ordered.validate(header, frames, tail)

    def test_tail_diagnostics_are_retained_and_never_executable(self):
        header, frames, tail = fixture()
        tail["events"] = [{"kind": "input_observation", "value": 2}]
        ordered.validate(header, frames, tail)
        _, compact_tail = ordered.compact(header, frames, tail)
        self.assertEqual(compact_tail["events"], tail["events"])
        for events in (None, {}, [1], [{"kind": "direct_command", "command": "death_abort"}]):
            tail["events"] = events
            with self.subTest(events=events), self.assertRaises(EvidenceError):
                ordered.validate(header, frames, tail)
        tail["events"] = [{"kind": "observational", "text": "x" * ordered.MAX_RECORD_BYTES}]
        with self.assertRaisesRegex(EvidenceError, "record limit"):
            ordered.validate(header, frames, tail)

    def test_count_and_byte_bounds(self):
        header, frames, tail = fixture()
        frames[0]["dispatch"]["ops"] = [{"k": "reset", "ft": 0}] * ordered.MAX_OPERATIONS
        ordered.validate(header, frames, tail)
        frames[0]["dispatch"]["ops"].append({"k": "reset", "ft": 0})
        with self.assertRaisesRegex(EvidenceError, "operation list"):
            ordered.validate(header, frames, tail)
        header, frames, tail = fixture("d2")
        sample = {
            "k": "sample",
            "ft": ordered.MAX_FRAME_TIME,
            "s": {key: ordered.INT32_MIN for key in ordered.AXES},
            "p": {key: 10 if key == "sw" else 255 for key in ordered.PULSES | ordered.D2_PULSES},
        }
        sample["s"].update({key: 255 for key in ordered.HELD | ordered.D2_HELD})
        frames[0]["dispatch"]["ops"] = [sample] * ordered.MAX_OPERATIONS
        self.assertGreater(len(ordered._encoded(frames[0]["dispatch"])), ordered.MAX_BATCH_BYTES)
        with self.assertRaisesRegex(EvidenceError, "byte limit"):
            ordered.validate(header, frames, tail)

    def test_marker_utf8_bytes_and_command_arguments(self):
        for message, accepted in (
            ("x" * 39, True),
            ("x" * 40, False),
            ("é" * 19, True),
            ("é" * 20, False),
            ("a\0b", False),
            ("\ud800", False),
        ):
            header, frames, tail = fixture("d2")
            frames[0]["dispatch"]["ops"] = [
                {
                    "k": "command",
                    "ft": 1,
                    "event": {
                        "kind": "direct_command",
                        "command": "drop_marker",
                        "player_marker_num": 0,
                        "message": message,
                    },
                }
            ]
            with self.subTest(message=repr(message)):
                if accepted:
                    ordered.validate(header, frames, tail)
                else:
                    with self.assertRaises(EvidenceError):
                        ordered.validate(header, frames, tail)
        for value in (0, 1, "true", None):
            header, frames, tail = fixture("d2")
            frames[0]["dispatch"]["ops"] = [
                {
                    "k": "command",
                    "ft": 1,
                    "event": {
                        "kind": "direct_command",
                        "command": "guidebot_goal",
                        "special_key": 3,
                        "from_menu": value,
                    },
                }
            ]
            with self.subTest(value=value), self.assertRaises(EvidenceError):
                ordered.validate(header, frames, tail)

    def test_misversioned_empty_and_count_mismatches(self):
        for key, value in (
            ("version", 4),
            ("version", True),
            ("game", "unknown"),
            ("frame_count", True),
            ("frame_count", 0),
            ("frame_count", 1),
            ("frame_count", ordered.UINT32_MAX + 1),
        ):
            header, frames, tail = fixture()
            header[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(EvidenceError):
                ordered.validate(header, frames, tail)
        header, frames, tail = fixture()
        with self.assertRaises(EvidenceError):
            ordered.validate(header, [], tail)


if __name__ == "__main__":
    unittest.main()

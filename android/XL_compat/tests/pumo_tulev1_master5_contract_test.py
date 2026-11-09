"""Public synthetic receipts: no mission assets, private saves, or native run.

Every negative starts from an independently accepted complete chain. Correlated
state edits update final/trace/history/alias together so rejection cannot rely
only on an incidental mismatch between copies of the same state.
"""

import copy
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import pumo_tulev1_master5_contract as c


def put(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value) + "\n")


def set_at(value, path, replacement):
    for key in path[:-1]:
        value = value[key]
    value[path[-1]] = replacement


def state(segment):
    return {
        "current_level_num": 7,
        "player_dead": False,
        "cheats_enabled": False,
        "game_time_raw": 100,
        "simulation_rng": {"state_available": True, "state": 42, "call_count": 0},
        "position": {"segment": segment, "x": 0.0, "y": 0.0, "z": 0.0, "orient_forward_fixed": [0, 0, 65536]},
        "player": {
            "invulnerable": False,
            "shields": 83.0,
            "energy": 100.0,
            "has_blue_key": False,
            "has_red_key": False,
            "has_gold_key": False,
            "cloaked": False,
            "laser_level": 0,
            "quad_lasers": False,
            "primary_weapon_flags": 65,
            "primary_weapon": 0,
            "secondary_weapon": 0,
            "secondary_weapon_flags": 65,
            "secondary_ammo": list(c.AMMO),
            "vulcan_ammo": 264,
        },
        "xl_pilot": {
            "remaining_objectives": 1,
            "operation_sequence": 0,
            "trigger_activated": {"5": False, "52": False, "71": False, "99": False},
            "wall_triggers": {},
            "doorways": {},
            "boost_player_locked": False,
            "spawn_count": 0,
            "runtime_active": True,
        },
        "level_scene": {
            "powerups": [{"object": 50, "id": 4, "segment": 1315}, {"object": 114, "id": 23, "segment": 618}]
        },
        "combat_observation": {
            "player_object": 0,
            "player_signature": 0,
            "owned_weapon_count": 0,
            "owned_weapons": [],
            "guided_control": None,
        },
        "primary_fire": {"held": 0, "count": 0},
        "secondary_fire": {"held": 0, "count": 0},
        "reactor_countdown": {"active": False, "timer": 0},
        "unknown_persisted_field": {"sentinel": "preserve me"},
    }


def walls(value, kind=5, status=0, indices=c.WALLS):
    for index in indices:
        value["xl_pilot"]["doorways"][str(index)] = {"type": kind, "state": status, "flags": 16, "keys": 1, "clip": -1}


def operations(value, operated=False):
    pilot = value["xl_pilot"]
    pilot["operation_sequence"] = 3 if operated else 0
    if operated:
        pilot["operation_counts"] = dict(c.COUNTS)
    for index, kind, number, shot in ((71, 9, 0, 0), (52, 33, 327680, 0), (5, 31, 0, 1)):
        pilot["trigger_activated"][str(index)] = operated
        pilot["wall_triggers"][str(index)] = {
            "actor": 0 if operated else -1,
            "attached": False,
            "delay_ms": -1,
            "flags": (7 if operated else 3) if index == 5 else 0,
            "operated": operated,
            "shot": shot if operated else 0,
            "type": kind,
            "value": number,
        }


def position(value, segment, raw=None, forward=None):
    value["position"]["segment"] = segment
    if raw:
        for axis, number in zip(("x", "y", "z"), raw):
            value["position"][axis] = number / 65536
    if forward:
        value["position"]["orient_forward_fixed"] = forward


class Fixture:
    def __init__(self, root):
        self.root = root
        self.engine = root / "engine"
        self.engine.write_bytes(b"asset-free engine identity, not executable")
        c.ENGINE_SHA = c.sha(self.engine)
        base = state(1277)
        walls(base)
        operations(base)
        for index in (138, 139):
            base["xl_pilot"]["doorways"][str(index)] = {"type": 2, "state": 0, "flags": 16, "keys": 1, "clip": 30}
        states = [copy.deepcopy(base) for _ in range(9)]
        for index, value in enumerate(states):
            position(value, 1277 if index < 2 else 1210 if index < 5 else 689)
            if index >= 5:
                value["game_time_raw"] += 20
                for wall in (138, 139):
                    value["xl_pilot"]["doorways"][str(wall)].update(flags=18, state=2)
        self.emit("door138", states)
        base = c.restored(states[-1])
        states = [copy.deepcopy(base) for _ in range(9)]
        for index, value in enumerate(states):
            position(value, 689 if index < 2 else 634 if index < 5 else 618)
            if index >= 5:
                value["game_time_raw"] += 20
                value["player"]["cloaked"] = True
                value["level_scene"]["powerups"] = [p for p in value["level_scene"]["powerups"] if p["object"] != 114]
        self.emit("cloak", states, "door138")
        base = c.restored(states[-1])
        states = [copy.deepcopy(base) for _ in range(19)]
        for index, value in enumerate(states):
            if index >= 2:
                position(value, 1329, [28455613, -8945664, -5779839], [-14265, -27184, -57891])
            if index >= 4:
                value["game_time_raw"] += 65536
            if index >= 7:
                value["game_time_raw"] += 1092
                value["player"]["energy"] = 99.75
            if index in (7, 8):
                value["combat_observation"]["owned_weapon_count"] = 2
                value["combat_observation"]["owned_weapons"] = [
                    {"object": obj, "signature": signature, "weapon_id": 0, "parent_object": 0, "parent_signature": 0}
                    for obj, signature in ((396, 496), (410, 497))
                ]
            if index >= 9:
                value["game_time_raw"] += 16384
                operations(value, True)
                walls(value, 4, indices=(148, 149))
                walls(value, 5, 5, indices=(159, 160))
            if index >= 10:
                value["game_time_raw"] += 68813
                walls(value, 4)
                value["player"]["shields"] = 78.5
            if index >= 12:
                position(value, 1313, [29816514, -6062080, 9426920], [0, 0, 65536])
            if index >= 15:
                value["game_time_raw"] += 90000
                position(value, 1315)
                value["player"]["has_blue_key"] = True
                value["level_scene"]["powerups"] = []
        self.emit("blue", states, "cloak")
        self.emit("cold", [c.restored(states[-1])], "blue")
        self.baseline = {path: path.read_bytes() for path in root.rglob("*") if path.is_file()}

    def path(self, name, kind):
        return self.root / name / f"{name}-{kind}"

    def emit(self, name, states, predecessor=None):
        root = self.root / name
        user = root / "preview-user"
        user.mkdir(parents=True)
        if predecessor:
            prior = c.doc(self.path(predecessor, "result.json"))["checkpoint_history"][0]
            input_name = prior["file"]
            (user / input_name).write_bytes((self.root / predecessor / "preview-user" / input_name).read_bytes())
        else:
            input_name = "spawn-input.sav"
            (user / input_name).write_bytes(b"synthetic upstream input")
        plan = c.template(name)
        nonce = {"door138": "a", "cloak": "b", "blue": "c", "cold": "d"}[name] * 32
        plan.update(resume_checkpoint=input_name, resume_sha256=c.sha(user / input_name), receipt_run_id=nonce)
        put(self.path(name, "executed-plan.json"), plan)
        events = [
            {"step": index, "action": action["action"], "state": value, "simulation_fps": 60}
            for index, (action, value) in enumerate(zip(plan["steps"], states))
        ]
        for index in reversed(
            [i for i, action in enumerate(plan["steps"]) if action["action"] in ("pose_view", "face_view")]
        ):
            events.insert(
                index + 1,
                {
                    "event": "position_assist",
                    "step": index,
                    "action": plan["steps"][index]["action"],
                    "position": copy.deepcopy(states[index + 1]["position"]),
                },
            )
        if name == "blue":
            event_index = next(i for i, event in enumerate(events) if event.get("step") == 9)
            for ordinal, index in enumerate(c.OPERATIONS, 1):
                value = copy.deepcopy(states[9])
                value["game_time_raw"] -= 1
                value["xl_pilot"]["operation_sequence"] = ordinal
                value["xl_pilot"]["operation_counts"] = {str(i): 1 for i in c.OPERATIONS[:ordinal]}
                events.insert(
                    event_index + ordinal - 1,
                    {
                        "event": "successful_wall_operation",
                        "index": index,
                        "sequence": ordinal,
                        "shot": 1 if index == 5 else 0,
                        "state": value,
                    },
                )
            changed = copy.deepcopy(value)
            changed["observation_boundary"] = "post_frame"
            events.insert(event_index + 3, {"event": "trigger_change", "state": changed})
            contacts = []
            for ordinal, (obj, signature) in enumerate(((396, 496), (410, 497))):
                contacts.append(
                    {
                        "level": 7,
                        "object": obj,
                        "signature": signature,
                        "parent_object": 0,
                        "parent_signature": 0,
                        "weapon_id": 0,
                        "wall": 158,
                        "trigger": 5,
                        "base": 148,
                        "overlay_raw": 414 - ordinal,
                        "blew_up": 1 - ordinal,
                        "eclip": 72 if ordinal == 0 else -1,
                        "effect_dest": 413 if ordinal == 0 else -1,
                        "effect_flags": 0 if ordinal == 0 else -1,
                        "hit_segment": 1335,
                        "hit_side": 3,
                        "hit_point_fixed": [28287560, -9592354, -7194351],
                    }
                )
            events.append(
                {
                    "event": "weapon_wall_contacts",
                    "result": {
                        "kind": "actual_contacts_reaching_effect_blowup_gate",
                        "truncated": False,
                        "contact_limit": 2,
                        "contact_count_lower_bound": 2,
                        "contacts": contacts,
                    },
                }
            )
        self.trace(name, events)
        put(self.path(name, "state.json"), states[-1])
        receipt = {
            "receipt_version": 2,
            "result": "PASS",
            "steps": len(states),
            "initial_level_setup": 7,
            "final_level": 7,
            "resumed_earned_checkpoint": True,
            "transitions": [],
            "rendered": False,
            "process_id": {"door138": 101, "cloak": 102, "blue": 103, "cold": 104}[name],
            "run_id": nonce,
            "plan_sha256": c.sha(self.path(name, "executed-plan.json")),
            "executable_sha256": c.sha(self.engine),
            "resume_sha256": plan["resume_sha256"],
            "position_assist_events": {"door138": 1, "cloak": 1, "blue": 2, "cold": 0}[name],
            "fixed_view_aim_events": 0,
            "operation_sequence": 3 if name == "blue" else 0,
            "checkpoint_history": [],
            "checkpoints": {},
        }
        if name != "cold":
            step = 17 if name == "blue" else 7
            filename = f"pumo-player-level7-{receipt['process_id']}-step{step}.sav"
            (user / filename).write_bytes(f"synthetic earned {name}".encode())
            (user / "pumo-player-level7.sav").write_bytes((user / filename).read_bytes())
            saved = {"step": step, "file": filename, "sha256": c.sha(user / filename), "state": states[-1]}
            receipt["checkpoint_history"] = [saved]
            receipt["checkpoints"] = {
                "7": {"file": "pumo-player-level7.sav", "sha256": saved["sha256"], "state": states[-1]}
            }
        put(self.path(name, "result.json"), receipt)

    def trace(self, name, events=None):
        path = self.path(name, "trace.jsonl")
        if events is None:
            return [json.loads(line) for line in path.read_text().splitlines()]
        path.write_text("".join(json.dumps(event) + "\n" for event in events))

    def edit(self, name, kind, path, value):
        filename = self.path(name, kind)
        document = c.doc(filename)
        set_at(document, path, value)
        put(filename, document)

    def edit_event(self, name, event_kind, path, value, index=0):
        events = self.trace(name)
        selected = [event for event in events if event.get("event") == event_kind][index]
        set_at(selected, path, value)
        self.trace(name, events)

    def edit_step(self, name, index, path, value):
        events = self.trace(name)
        selected = next(event for event in events if event.get("step") == index and "state" in event)
        set_at(selected["state"], path, value)
        self.trace(name, events)

    def all_final(self, name, path, value):
        self.edit(name, "state.json", path, value)
        self.edit_step(name, c.COUNTS_BY_PHASE[name] - 1, path, value)
        if name != "cold":
            self.edit(name, "result.json", ["checkpoint_history", 0, "state"] + path, value)
            self.edit(name, "result.json", ["checkpoints", "7", "state"] + path, value)

    def check(self, name):
        if name == "door138":
            c.producer(self.path(name, "result.json"), self.engine)
        else:
            prior = {"cloak": "door138", "blue": "cloak", "cold": "blue"}[name]
            c.phase(name, self.root / name, self.engine, self.path(prior, "result.json"))

    def restore(self):
        for path, value in self.baseline.items():
            if path.is_symlink():
                path.unlink()
            path.write_bytes(value)


def test_templates():
    ordinary = [
        "assert",
        "face_view",
        "assert",
        "send_axis",
        "wait_for",
        "send_axis",
        "assert",
        "set_debug",
        "introspect",
    ]
    for name in ("door138", "cloak"):
        assert [step["action"] for step in c.template(name)["steps"]] == ordinary
    blue = c.template("blue")
    assert [step["action"] for step in blue["steps"]] == [
        "assert",
        "pose_view",
        "assert",
        "wait_ms",
        "introspect",
        "send_button",
        "wait_ms",
        "send_button",
        "wait_ms",
        "wait_ms",
        "assert",
        "pose_view",
        "assert",
        "send_axis",
        "wait_for",
        "send_axis",
        "assert",
        "set_debug",
        "introspect",
    ]
    expected = {
        1: {
            "action": "pose_view",
            "segment": 1329,
            "position_fixed": [28455613, -8945664, -5779839],
            "pitch": 4462,
            "heading": -30248,
            "bank": 0,
        },
        3: {"action": "wait_ms", "ms": 1000},
        5: {"action": "send_button", "button": 100, "held": 1},
        6: {"action": "wait_ms", "ms": 1},
        7: {"action": "send_button", "button": 100, "held": 0},
        8: {"action": "wait_ms", "ms": 250},
        9: {"action": "wait_ms", "ms": 1050},
        11: {
            "action": "pose_view",
            "segment": 1313,
            "position_fixed": [29816514, -6062080, 9426920],
            "pitch": 0,
            "heading": 0,
            "bank": 0,
        },
        13: {"action": "send_axis", "axis": 1, "axis_value": -0.2},
        15: {"action": "send_axis", "axis": 1, "axis_value": 0},
        17: {"action": "set_debug", "field": "android_game_request", "value": "quick_save"},
    }
    for index, step in expected.items():
        assert c.same(blue["steps"][index], step)
    assert c.same(c.template("cold")["steps"], [{"action": "introspect"}])
    for name in ("cloak", "blue", "cold"):
        plan = c.template(name)
        assert all(field not in plan for field in ("resume_checkpoint", "resume_sha256", "receipt_run_id"))
    assert c.template("cloak")["steps"][4]["timeout_ms"] == 5000
    assert blue["steps"][14]["timeout_ms"] == 10000
    assert blue["observe_weapon_wall_contacts"] is True
    assert blue["realtime"] is False and blue["simulation_fps"] == 60


def test():
    test_templates()
    with tempfile.TemporaryDirectory() as temporary:
        fixture = Fixture(Path(temporary))
        for phase in ("door138", "cloak", "blue", "cold"):
            fixture.check(phase)
        rejected = 0

        def reject(label, phase, mutate):
            nonlocal rejected
            fixture.restore()
            mutate()
            try:
                fixture.check(phase)
            except (ValueError, KeyError, IndexError, TypeError, OSError):
                rejected += 1
                return
            raise AssertionError(f"accepted forgery: {label}")

        for value in (True, 1.0, "1", 0):
            reject(
                f"first blew_up {value!r}",
                "blue",
                lambda value=value: fixture.edit_event(
                    "blue", "weapon_wall_contacts", ["result", "contacts", 0, "blew_up"], value
                ),
            )
        for field, value in (
            ("wall", True),
            ("wall", 159),
            ("trigger", 5.0),
            ("overlay_raw", 413),
            ("weapon_id", 7),
            ("parent_signature", 9),
        ):
            reject(
                f"actual first contact {field}",
                "blue",
                lambda field=field, value=value: fixture.edit_event(
                    "blue", "weapon_wall_contacts", ["result", "contacts", 0, field], value
                ),
            )
        for field, value in (
            ("kind", "diagnostic_weapon_rays"),
            ("truncated", True),
            ("contact_limit", True),
            ("contact_count_lower_bound", 1),
        ):
            reject(
                f"contact witness {field}",
                "blue",
                lambda field=field, value=value: fixture.edit_event(
                    "blue", "weapon_wall_contacts", ["result", field], value
                ),
            )
        for field, value in (("index", 52), ("index", 71.0), ("sequence", True), ("shot", True)):
            reject(
                f"operation {field}",
                "blue",
                lambda field=field, value=value: fixture.edit_event(
                    "blue", "successful_wall_operation", [field], value
                ),
            )
        reject(
            "forged cumulative operation",
            "blue",
            lambda: fixture.edit_event(
                "blue", "successful_wall_operation", ["state", "xl_pilot", "operation_counts", "71"], True
            ),
        )
        for index in c.WALLS:
            for field, value in (("type", 5), ("type", 4.0), ("state", 5), ("state", False)):
                reject(
                    f"wall {index} {field} incomplete/typed",
                    "blue",
                    lambda index=index, field=field, value=value: fixture.all_final(
                        "blue", ["xl_pilot", "doorways", str(index), field], value
                    ),
                )
        for path, value in (
            (["player", "has_blue_key"], False),
            (["player", "has_blue_key"], 1),
            (["player", "cloaked"], False),
            (["player", "secondary_ammo", 6], 0),
            (["player", "secondary_ammo", 6], True),
            (["player", "shields"], 84),
            (["player", "energy"], 101),
            (["player", "invulnerable"], True),
            (["position", "segment"], 1313),
            (["level_scene", "powerups"], [{"object": 50, "id": 4, "segment": 1315}]),
        ):
            reject(
                f"unearned final {path}", "blue", lambda path=path, value=value: fixture.all_final("blue", path, value)
            )
        reject(
            "key granted before pickup", "blue", lambda: fixture.edit_step("blue", 14, ["player", "has_blue_key"], True)
        )
        reject(
            "wrong original key object",
            "blue",
            lambda: fixture.edit_step("blue", 12, ["level_scene", "powerups", 0, "id"], 5),
        )
        reject("pickup from behind gate", "blue", lambda: fixture.edit_step("blue", 12, ["position", "z"], 170.0))
        reject(
            "no emitted laser pair",
            "blue",
            lambda: fixture.edit_step("blue", 7, ["combat_observation", "owned_weapon_count"], 0),
        )
        reject(
            "not original cloak",
            "cloak",
            lambda: fixture.edit_step("cloak", 2, ["level_scene", "powerups", 1, "object"], 115),
        )
        reject(
            "cloak granted before collision",
            "cloak",
            lambda: fixture.edit_step("cloak", 4, ["player", "cloaked"], True),
        )
        reject("source wrong arrival", "door138", lambda: fixture.all_final("door138", ["position", "segment"], 458))
        for name in ("cloak", "blue"):
            for path, value in (
                (["checkpoint_history", 0, "step"], True),
                (["checkpoint_history", 0, "step"], 7 if name == "blue" else 17),
                (["checkpoint_history", 0, "file"], "pumo-player-level7.sav"),
                (["checkpoint_history", 0, "sha256"], "0" * 64),
                (["checkpoints", "7", "sha256"], "0" * 64),
                (["checkpoint_history"], []),
                (["steps"], 19.0 if name == "blue" else 9.0),
                (["run_id"], ""),
                (["process_id"], True),
                (["resume_sha256"], "0" * 64),
                (["executable_sha256"], "0" * 64),
            ):
                reject(
                    f"{name} provenance {path}",
                    name,
                    lambda name=name, path=path, value=value: fixture.edit(name, "result.json", path, value),
                )
        for name, step, field, value in (
            ("blue", 9, "ms", 0),
            ("blue", 13, "axis_value", -1),
            ("blue", 14, "timeout_ms", 99999),
            ("blue", 5, "action", "operate_trigger"),
            ("blue", 17, "value", "grant_key"),
            ("cloak", 1, "segment", 618),
            ("cold", 0, "action", "quick_save"),
        ):

            def mutate_plan(name=name, step=step, field=field, value=value):
                fixture.edit(name, "executed-plan.json", ["steps", step, field], value)
                fixture.edit(name, "result.json", ["plan_sha256"], c.sha(fixture.path(name, "executed-plan.json")))

            reject(f"rehash weakened plan {name} {step}", name, mutate_plan)
        for path, value in (
            (["game_time_raw"], 0),
            (["simulation_rng", "state"], 43),
            (["simulation_rng", "call_count"], 1),
            (["simulation_rng", "call_count"], False),
            (["position", "x"], 99.0),
            (["position", "orient_forward_fixed", 0], 1),
            (["player", "has_blue_key"], False),
            (["player", "energy"], 100),
            (["player", "secondary_ammo", 6], 2),
            (["xl_pilot", "operation_sequence"], 3),
            (["xl_pilot", "operation_counts"], dict(c.COUNTS)),
            (["xl_pilot", "operation_counts"], {}),
            (["xl_pilot", "trigger_activated", "71"], True),
            (["xl_pilot", "trigger_activated", "99"], True),
            (["xl_pilot", "wall_triggers", "5", "operated"], False),
            (["xl_pilot", "wall_triggers", "5", "flags"], 3),
            (["xl_pilot", "doorways", "149", "state"], 5),
            (["xl_pilot", "remaining_objectives"], 0),
            (["xl_pilot", "spawn_count"], 1),
            (["xl_pilot", "boost_player_locked"], True),
            (["unknown_persisted_field", "sentinel"], "silently ignored"),
            (["new_persisted_field"], "silently added"),
            (["reactor_countdown", "active"], True),
        ):
            reject(
                f"cold non-allowlisted mutation {path}",
                "cold",
                lambda path=path, value=value: fixture.all_final("cold", path, value),
            )
        for event_kind in ("successful_wall_operation", "trigger_change", "weapon_wall_contacts"):
            reject(
                f"missing {event_kind}",
                "blue",
                lambda event_kind=event_kind: fixture.trace(
                    "blue", [event for event in fixture.trace("blue") if event.get("event") != event_kind]
                ),
            )
        reject(
            "duplicate operation event",
            "blue",
            lambda: fixture.trace(
                "blue",
                fixture.trace("blue")
                + [
                    copy.deepcopy(
                        next(
                            event
                            for event in fixture.trace("blue")
                            if event.get("event") == "successful_wall_operation"
                        )
                    )
                ],
            ),
        )
        reject(
            "wrong actual first contact order",
            "blue",
            lambda: fixture.edit_event(
                "blue",
                "weapon_wall_contacts",
                ["result", "contacts"],
                list(
                    reversed(
                        next(event for event in fixture.trace("blue") if event.get("event") == "weapon_wall_contacts")[
                            "result"
                        ]["contacts"]
                    )
                ),
            ),
        )
        reject(
            "cold trace FPS boolean",
            "cold",
            lambda: fixture.trace("cold", [dict(fixture.trace("cold")[0], simulation_fps=True)]),
        )
        reject(
            "cold trace FPS missing",
            "cold",
            lambda: fixture.trace(
                "cold", [{key: value for key, value in fixture.trace("cold")[0].items() if key != "simulation_fps"}]
            ),
        )
        reject(
            "nonfinite resource", "blue", lambda: fixture.edit("blue", "state.json", ["player", "energy"], float("nan"))
        )
        reject(
            "duplicate JSON receipt field",
            "cold",
            lambda: fixture.path("cold", "result.json").write_text(
                fixture.path("cold", "result.json").read_text().rstrip()[:-1] + ', "result": "PASS"}'
            ),
        )
        reject(
            "unsafe save name",
            "cold",
            lambda: fixture.edit("cold", "executed-plan.json", ["resume_checkpoint"], "../escape.sav"),
        )

        def symlink_save():
            path = next((fixture.root / "blue" / "preview-user").glob("*-step17.sav"))
            path.unlink()
            path.symlink_to("pumo-player-level7.sav")

        reject("symlink instead of immutable save", "blue", symlink_save)
        reject("cold same process", "cold", lambda: fixture.edit("cold", "result.json", ["process_id"], 103))
        reject(
            "cold unexpected save",
            "cold",
            lambda: fixture.edit("cold", "result.json", ["checkpoint_history"], [{"step": 0}]),
        )
        reject("source engine bytes changed", "blue", lambda: fixture.engine.write_bytes(b"different engine"))
        reject(
            "versioned save bytes changed",
            "blue",
            lambda: next((fixture.root / "blue" / "preview-user").glob("*-step17.sav")).write_bytes(b"tampered"),
        )
        reject(
            "alias bytes changed",
            "blue",
            lambda: (fixture.root / "blue" / "preview-user" / "pumo-player-level7.sav").write_bytes(b"tampered"),
        )
        fixture.restore()
        for phase in ("door138", "cloak", "blue", "cold"):
            fixture.check(phase)
        # Exercise argument routing without weakening production's pinned hash
        module = str(Path(c.__file__).resolve())
        result = subprocess.run(
            [
                sys.executable,
                module,
                "phase",
                "cold",
                str(fixture.root / "cold"),
                str(fixture.engine),
                str(fixture.path("blue", "result.json")),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        assert result.returncode != 0 and "usage:" not in result.stderr
        print(f"master5 contract: full asset-free chain and {rejected} forgery controls passed")


if __name__ == "__main__":
    test()

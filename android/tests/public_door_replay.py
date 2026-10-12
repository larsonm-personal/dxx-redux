"""Qualify authored automatic doors, geometric obstruction and native D1/D2 lifecycle."""

import d1_replay_parity as parity

PROFILES = {
    "d1": {
        "level": 3,
        "target": 72,
        "door_back": 73,
        "front_segment": 75,
        "front_side": 5,
        "back_segment": 38,
        "back_side": 4,
    },
    "d2": {
        "level": 1,
        "target": 76,
        "door_back": 77,
        "front_segment": 6,
        "front_side": 0,
        "back_segment": 4,
        "back_side": 5,
    },
}
PROBE_KEYS = set("type version f phase front back front_doorway back_doorway blockers".split())
BLOCKER_KEYS = set("slot type id signature segment position radius".split())


def _require(condition, message):
    if not condition:
        raise parity.EvidenceError(message)


def verify(actions, header, states, state_trace):
    try:
        return _verify(actions, header, states, state_trace)
    except (KeyError, IndexError, TypeError) as error:
        raise parity.EvidenceError("Malformed or incomplete door-lifecycle evidence") from error


def _verify(actions, header, states, state_trace):
    game, count = header["game"], header["frame_count"]
    profile = PROFILES[game]
    _require(
        header["level"] == profile["level"] and header["difficulty"] == 2 and header["start_mode"] == "new_level",
        "Door lifecycle requires its inspected ordinary public spawn",
    )
    _require(
        all(type(actions.get(key)) is int and actions[key] == profile[key] for key in ("target", "door_back")),
        "Door lifecycle must use the identified authored wall pair",
    )
    events = actions["door_events"]
    expected_stages = [0, 1, 2, 4, 5, 6, 7] if game == "d1" else list(range(8))
    _require(
        isinstance(events, list)
        and [row["stage"] for row in events] == expected_stages
        and all(
            set(row) == {"stage", "f"} and type(row["f"]) is int and type(row["stage"]) is int and 0 <= row["f"] < count
            for row in events
        ),
        "Door lifecycle needs its exact ordered capture step boundaries",
    )
    step = {row["stage"]: row["f"] for row in events}
    _require(
        list(step.values()) == sorted(set(step.values()))
        and step[0] == 0
        and step[7] == count - 1
        and step[2] - step[1] >= 100,
        "Door obstruction must outlast the authored automatic wait",
    )
    front, back = actions["target"], actions["door_back"]
    objects = [dict(row["objects"]) for row in parity.object_states(state_trace, header, game)]
    worlds, restored = [], None
    for row in parity.world_states(state_trace, header, game):
        if row["type"] == "object_boundary":
            if row["phase"] == "terminal":
                objects.append(row["slots"])
        elif row["type"] == "world_state":
            worlds.append(dict(row["state"]))
        elif row["phase"] == "restored":
            restored = row["state"]
        else:
            worlds.append(row["state"])
    _require(
        len(worlds) == len(objects) == len(states) == count + 1 and restored is not None,
        "Door lifecycle needs complete object/world/frame boundaries",
    )
    for world in (restored, worlds[0]):
        for side, wall in (("front", front), ("back", back)):
            value = world["walls"][wall]
            _require(
                value["type"] == 2
                and value["state"] == 0
                and value["flags"] == 16
                and value["keys"] == 1
                and value["segnum"] == profile[side + "_segment"]
                and value["sidenum"] == profile[side + "_side"],
                "Both authored automatic door faces must start closed, unlocked and impassable",
            )
    player_slot = str(restored["players"][0]["objnum"])
    for index, world in enumerate(worlds):
        player = world["players"][0]
        _require(
            world["death"]["active"] == 0
            and player["shields"] >= 0
            and player["shields"] // 65536 == states[index]["player0"]["shields"],
            "Door lifecycle requires a surviving player and matching shield state",
        )
    probes = {}
    for row in parity.records(state_trace):
        if (
            row.get("type") != "door_probe"
            or (row.get("front"), row.get("back")) != (front, back)
            or row.get("phase") == "restored"
        ):
            continue
        _require(
            set(row) == PROBE_KEYS
            and row["version"] == 1
            and type(row["f"]) is int
            and 0 <= row["f"] <= count
            and row["phase"] == ("terminal" if row["f"] == count else "frame")
            and row["f"] not in probes,
            "Door probes require unique typed frame and terminal boundaries",
        )
        frame = row["f"]
        for side, wall in (("front", front), ("back", back)):
            flags = row[side + "_doorway"]
            _require(
                type(flags) is int
                and flags >= 0
                and bool(flags & 1) == bool(worlds[frame]["walls"][wall]["flags"] & 2),
                "Both collision flags must agree with their full wall state",
            )
        _require(isinstance(row["blockers"], list), "Door obstruction must identify actual objects")
        seen = set()
        for blocker in row["blockers"]:
            _require(
                set(blocker) == BLOCKER_KEYS
                and all(type(blocker[key]) is int for key in BLOCKER_KEYS - {"position"})
                and blocker["slot"] not in seen
                and blocker["radius"] > 0,
                "Door blockers need unique typed physical identities",
            )
            obj = objects[frame].get(str(blocker["slot"]))
            _require(
                obj is not None
                and all(blocker[key] == obj[key] for key in ("type", "id", "signature"))
                and blocker["segment"] == obj["segnum"]
                and blocker["position"] == obj["pos"]
                and blocker["radius"] == obj["size"],
                "Door blocker must match the exact live object pose and radius",
            )
            _require(
                game != "d2" or blocker["type"] not in (5, 1),
                "D2 weapons and fireballs cannot obstruct automatic doors",
            )
            seen.add(blocker["slot"])
        probes[frame] = row
    _require(
        all(frame in probes for frame in range(step[1], count + 1)),
        "Door lifecycle is missing continuous geometric observations",
    )

    def walls(frame):
        return worlds[frame]["walls"][front], worlds[frame]["walls"][back]

    def active(frame):
        found = [door for door in worlds[frame]["doors"] if front in door["front"] or front in door["back"]]
        _require(
            len(found) == 1
            and found[0]["n_parts"] == 1
            and found[0]["front"][0] == front
            and found[0]["back"][0] == back,
            "Active door must retain the authored front/back pair",
        )
        return found[0]

    first_open = actions["action_frame"] + 1
    _require(
        0 < first_open <= step[1] + 1
        and walls(first_open - 1)[0]["state"] == 0
        and all(wall["state"] == 1 for wall in walls(first_open)),
        "Door must first open at the captured normal-input boundary",
    )
    blocked_range = range(step[2] - 18, step[2] + 2)
    for frame in blocked_range:
        pair, door = walls(frame), active(frame)
        _require(
            any(str(obj["slot"]) == player_slot and obj["type"] == 4 for obj in probes[frame]["blockers"])
            and all(wall["flags"] & 2 for wall in pair),
            "The living player must physically obstruct the fully passable doorway for twenty frames",
        )
        if game == "d1":
            _require(
                [wall["state"] for wall in pair] == [3, 2] and door["time"] == 0,
                "D1 must freeze closing time with its back face waiting while obstructed",
            )
        else:
            _require(
                all(wall["state"] == 2 for wall in pair) and door["time"] > 5 * 65536,
                "D2 must wait beyond the automatic delay while obstructed",
            )
    clear = step[4 if game == "d1" else 3] + 1
    _require(
        not probes[clear]["blockers"] and walls(clear)[0]["state"] == 3,
        "Removing obstruction must permit normal automatic closing",
    )
    reversal = None
    if game == "d2":
        reversals = [
            frame
            for frame in range(clear + 1, step[4] + 2)
            if walls(frame - 1)[0]["state"] == 3 and walls(frame)[0]["state"] == 1
        ]
        _require(len(reversals) == 1, "D2 must automatically reopen when the player re-enters during closing")
        reversal = reversals[0]
        _require(
            any(str(obj["slot"]) == player_slot for obj in probes[reversal - 1]["blockers"])
            and all(probes[reversal - 1][side + "_doorway"] & 1 for side in ("front", "back")),
            "D2 reversal requires real player overlap while both faces remain passable",
        )
    closed, reopen = step[5] + 1, step[6] + 1
    _require(
        all(wall["state"] == 0 and not wall["flags"] & 2 for wall in walls(closed))
        and not probes[closed - 1]["blockers"]
        and not any(str(obj["slot"]) == player_slot for obj in probes[closed]["blockers"])
        and all(not probes[closed][side + "_doorway"] & 1 for side in ("front", "back")),
        "After clearance both faces must fully close and become impassable",
    )
    _require(
        not any(front in door["front"] or front in door["back"] for door in worlds[closed]["doors"]),
        "Completed automatic close must retire its active-door slot",
    )
    _require(
        all(wall["state"] == 0 for wall in walls(reopen - 1)) and all(wall["state"] == 1 for wall in walls(reopen)),
        "The fully closed door must reopen through normal player input",
    )
    _require(
        objects[count][player_slot]["segnum"] == profile["back_segment"]
        and not any(str(obj["slot"]) == player_slot for obj in probes[count]["blockers"])
        and all(probes[count][side + "_doorway"] & 1 for side in ("front", "back")),
        "The living player must traverse and clear both passable faces after reopening",
    )
    return {
        "door_first_open_frame": first_open - 1,
        "player_obstruction_frames": len(blocked_range),
        "door_clear_frame": clear - 1,
        "door_obstruction_reversal_frame": None if reversal is None else reversal - 1,
        "door_closed_frame": closed - 1,
        "door_reopen_frame": reopen - 1,
        "door_passage_frame": count - 1,
    }

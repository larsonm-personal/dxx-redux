"""Prove direct robot-projectile shield losses on the existing compact combat routes."""

import d1_replay_parity as parity

# Additional causal expectations from the original full live captures and verified
# object/world playback boundaries; the nine admitted replay/oracle pairs stay unchanged
# Each row is (impact frame, first-live boundary, projectile slot, signature)
PROFILES = {
    "d1": {"robot": (8, 3, 9), "weapon_id": 5, "damage": 393216, "hits": ((112, 70, 68, 76), (117, 76, 66, 78))},
    "d2": {
        "robot": (11, 37, 12),
        "weapon_id": 41,
        "damage": 131072,
        "hits": ((168, 157, 42, 112), (247, 237, 90, 150)),
    },
}
# Object flags shared by both engines: exploding, should-be-dead, destroyed, harmless
NON_DAMAGING_FLAGS = 1 | 2 | 4 | 32
EVENT_KEYS = set(
    "type version f player player_id player_signature weapon weapon_id weapon_signature "
    "parent parent_type parent_signature old_shields new_shields damage".split()
)


def _require(condition, message):
    if not condition:
        raise parity.EvidenceError(message)


def verify_trace(header, states, state_trace):
    """Exercise incoming damage independently of the outgoing kill gate."""
    objects = [dict(row["objects"]) for row in parity.object_states(state_trace, header, header["game"])]
    worlds, restored_world, restored_objects, terminal_objects = [], None, None, None
    for row in parity.world_states(state_trace, header, header["game"]):
        if row["type"] == "object_boundary":
            if row["phase"] == "restored":
                restored_objects = row["slots"]
            else:
                terminal_objects = row["slots"]
        elif row["type"] == "world_state":
            worlds.append(dict(row["state"]))
        elif row["phase"] == "restored":
            restored_world = row["state"]
        else:
            worlds.append(row["state"])
    _require(
        restored_world is not None and restored_objects is not None and terminal_objects is not None,
        "Missing incoming damage object/world boundaries",
    )
    return verify(
        header,
        states,
        objects + [terminal_objects],
        worlds,
        restored_objects,
        restored_world,
        [row for row in parity.records(state_trace) if row.get("type") == "player_weapon_damage"],
    )


def verify(header, states, objects, worlds, restored_objects, restored_world, events):
    try:
        return _verify(header, states, objects, worlds, restored_objects, restored_world, events)
    except (KeyError, IndexError, TypeError) as error:
        raise parity.EvidenceError("Malformed or incomplete incoming robot hit evidence") from error


def _verify(header, states, objects, worlds, restored_objects, restored_world, events):
    count = header["frame_count"]
    profile = PROFILES[header["game"]]
    _require(
        len(states) == len(objects) == len(worlds) == count + 1,
        "Incoming hits require complete per-frame and terminal object/world evidence",
    )
    robot_slot, robot_id, robot_signature = profile["robot"]
    player_slot = str(restored_world["players"][0]["objnum"])
    original_player = restored_objects[player_slot]
    player_identity = (4, 0, original_player["signature"])

    def identity(obj):
        return tuple(obj[key] for key in ("type", "id", "signature")) if obj is not None else None

    _require(
        identity(original_player) == player_identity and restored_world["players"][0]["shields"] == 100 * 65536,
        "Incoming hits must start with the original fresh player at 100 shields",
    )
    rows = [row for row in events if row.get("type") == "player_weapon_damage"]
    _require(len(rows) == len(profile["hits"]), "Missing or additional direct incoming projectile damage observations")
    shields = 100 * 65536
    by_frame = {}
    for row, (frame, birth, slot, signature) in zip(rows, profile["hits"]):
        _require(
            set(row) == EVENT_KEYS
            and all(type(row[key]) is int for key in EVENT_KEYS - {"type"})
            and row["version"] == 1
            and row["f"] == frame
            and 0 < birth <= frame < count,
            "Incoming damage requires the exact typed, ordered and bounded impact profile",
        )
        _require(
            (row["player"], row["player_id"], row["player_signature"])
            == (int(player_slot), 0, original_player["signature"])
            and (row["weapon"], row["weapon_id"], row["weapon_signature"]) == (slot, profile["weapon_id"], signature)
            and (row["parent"], row["parent_type"], row["parent_signature"]) == (robot_slot, 2, robot_signature)
            and row["damage"] == profile["damage"]
            and row["old_shields"] == shields
            and row["new_shields"] == shields - profile["damage"],
            "Incoming damage must identify the original robot, its projectile and exact player shield loss",
        )
        shields = row["new_shields"]
        by_frame[frame] = row
        weapon_identity = (5, profile["weapon_id"], signature)
        key = str(slot)
        _require(
            identity(restored_objects.get(key)) != weapon_identity
            and all(identity(boundary.get(key)) != weapon_identity for boundary in objects[:birth]),
            "Incoming projectile must be created during ordinary gameplay, not present at the start",
        )
        for boundary in objects[birth : frame + 1]:
            weapon = boundary.get(key)
            _require(
                identity(weapon) == weapon_identity
                and weapon["control_type"] == 9
                and weapon["flags"] & NON_DAMAGING_FLAGS == 0
                and weapon["lifeleft"] > 0
                and weapon["shields"] == profile["damage"]
                and weapon["weapon"]["multiplier"] == 65536
                and tuple(weapon["weapon"][field] for field in ("parent_num", "parent_type", "parent_signature"))
                == (robot_slot, 2, robot_signature),
                "Incoming projectile identity, ownership or live damaging state changed before impact",
            )
        for boundary in (objects[birth - 1], objects[birth]):
            robot = boundary.get(str(robot_slot))
            _require(
                identity(robot) == (2, robot_id, robot_signature) and robot["flags"] & 7 == 0 and robot["shields"] > 0,
                "Incoming projectile must originate from the identified living original robot",
            )
        # The owner may already be exploding when a previously fired shot arrives
        struck = objects[frame + 1].get(key)
        _require(
            identity(struck) == weapon_identity
            and struck["control_type"] == 9
            and struck["flags"] & NON_DAMAGING_FLAGS == 2
            and struck["shields"] == profile["damage"]
            and struck["weapon"]["multiplier"] == 65536
            and tuple(struck["weapon"][field] for field in ("parent_num", "parent_type", "parent_signature"))
            == (robot_slot, 2, robot_signature)
            and objects[frame + 1][player_slot]["shields"] == row["new_shields"],
            "Direct impact must mark its real projectile dead and mirror the player shield loss",
        )
        _require(
            all(identity(boundary.get(key)) != weapon_identity for boundary in objects[frame + 2 :]),
            "The incoming projectile must retire after its single direct hit",
        )
    for index, (state, boundary, world) in enumerate(zip(states, objects, worlds)):
        player = world["players"][0]
        _require(
            player["objnum"] == int(player_slot)
            and identity(boundary.get(player_slot)) == player_identity
            and boundary[player_slot]["flags"] & 7 == 0
            and world["death"]["active"] == 0
            and player["shields"] >= 0
            and state["player0"]["shields"] == player["shields"] // 65536
            and (index <= profile["hits"][0][0] or boundary[player_slot]["shields"] == player["shields"]),
            "Incoming damage must agree with every live player frame and full world shield value",
        )
        if index < count:
            hit = by_frame.get(index)
            expected = player["shields"] - (hit["damage"] if hit else 0)
            _require(
                (hit is None or hit["old_shields"] == player["shields"])
                and worlds[index + 1]["players"][0]["shields"] == expected,
                "Direct robot projectile events must explain every actual shield delta, without wall or other damage",
            )
    return {
        "incoming_robot_projectile_hits": len(rows),
        "incoming_hit_frames": list(by_frame),
        "incoming_shield_loss": 100 - shields // 65536,
        "incoming_robot": robot_slot,
    }

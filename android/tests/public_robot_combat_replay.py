"""Require identified, ordinary-start projectile kills and resolved public robot loot."""

from collections import defaultdict

import d1_replay_parity as parity

# Populated only from inspected ordinary starts using pinned public asset tables
TARGETS = {
    "d1": {
        "target": 8,
        "target_id": 3,
        "target_signature": 9,
        "target_shields": 1638400,
        "target_score": 200,
        "loot": (7, 1, 1, 3),
    },
    "d2": {
        "target": 11,
        "target_id": 37,
        "target_signature": 12,
        "target_shields": 786432,
        "target_score": 200,
        "loot": (7, 0, 0, 0),
    },
}
DAMAGE_KEYS = set(
    "type version f robot robot_id robot_signature weapon weapon_id weapon_signature parent parent_type "
    "parent_signature old_shields new_shields damage died score_value score kills_level kills_total".split()
)
LOOT_KEYS = set(
    "type version f robot robot_id robot_signature definition_type definition_id definition_count "
    "definition_probability chance_roll count_roll contains_type contains_id contains_count created "
    "created_type created_id created_signature".split()
)


def _require(condition, message):
    if not condition:
        raise parity.EvidenceError(message)


def verify(actions, header, states, state_trace):
    try:
        return _verify(actions, header, states, state_trace)
    except (KeyError, IndexError, TypeError) as error:
        raise parity.EvidenceError("Malformed or incomplete robot-combat outcome evidence") from error


def _verify(actions, header, states, state_trace):
    game, count = header["game"], header["frame_count"]
    profile = TARGETS[game]
    _require(
        header["level"] == 1 and header["difficulty"] == 2 and header["start_mode"] == "new_level",
        "Combat requires an ordinary public level-one start at authentic difficulty two",
    )
    _require(
        all(type(actions.get(key)) is int and actions[key] == profile[key] for key in profile if key != "loot"),
        "Combat target does not match the inspected original public robot",
    )
    damage, death, loot_frame, retirement = (
        actions.get(key) for key in ("damage_frame", "action_frame", "loot_frame", "retirement_frame")
    )
    _require(
        all(type(value) is int for value in (damage, death, loot_frame, retirement))
        and 0 <= damage <= death < loot_frame <= retirement < count,
        "Combat requires ordered damage, death, loot resolution and original-slot retirement",
    )
    slot, identity = str(actions["target"]), (2, actions["target_id"], actions["target_signature"])

    def matches(obj):
        return obj is not None and tuple(obj[key] for key in ("type", "id", "signature")) == identity

    # The trace iterators reuse their reconstruction maps; retain each boundary
    objects = [dict(row["objects"]) for row in parity.object_states(state_trace, header, game)]
    worlds, restored_world, restored_objects, terminal_objects = [], None, None, None
    for row in parity.world_states(state_trace, header, game):
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
        len(objects) == count
        and len(worlds) == count + 1
        and restored_world is not None
        and restored_objects is not None
        and terminal_objects is not None
        and len(states) == count + 1,
        "Combat requires complete original, per-frame and terminal object/world evidence",
    )
    objects.append(terminal_objects)
    for initial in (restored_objects, objects[0]):
        robot = initial.get(slot)
        _require(
            matches(robot)
            and robot["shields"] == actions["target_shields"]
            and robot["flags"] & 1 == 0
            and robot["contains_count"] == 0,
            "The identified robot must start alive with authored shields and no staged object contents",
        )
    _require(
        all(matches(row.get(slot)) for row in objects[: retirement + 1])
        and all(not matches(row.get(slot)) for row in objects[retirement + 1 :]),
        "The selected original robot must retire exactly at its declared post-frame boundary",
    )
    _require(
        all(objects[index][slot]["shields"] >= 0 for index in range(death + 1))
        and all(
            objects[index][slot]["shields"] < 0 and objects[index][slot]["flags"] & 1
            for index in range(death + 1, retirement + 1)
        ),
        "The selected robot must cross the real damage/death boundary and remain exploding until retirement",
    )
    first_damage = next(
        (index for index in range(death + 1) if objects[index + 1][slot]["shields"] < objects[index][slot]["shields"]),
        None,
    )
    _require(first_damage == damage, "Robot shields must fall at the exact declared first-damage boundary")
    player_slot = str(restored_world["players"][0]["objnum"])
    player = restored_objects[player_slot]
    _require(player["type"] == 4, "Combat projectile owner must be the original player")
    for initial in (restored_world, worlds[0]):
        p = initial["players"][0]
        _require(
            p["score"] == p["num_kills_level"] == p["num_kills_total"] == 0
            and p["laser_level"] == p["primary_weapon"] == 0
            and p["primary_weapon_flags"] == 1,
            "Combat starts with the authentic fresh player, starting lasers, zero score and zero kills",
        )
    for index, world in enumerate(worlds):
        p = world["players"][0]
        killed = int(index > death)
        _require(
            p["score"] == actions["target_score"] * killed
            and p["num_kills_level"] == p["num_kills_total"] == killed
            and world["death"]["active"] == 0
            and p["shields"] >= 0,
            "Exactly one identified robot must award its loaded score and kill counters to a living player",
        )
        _require(states[index]["player0"]["score"] == p["score"], "Frame/result score disagrees with world state")

    events = [row for row in parity.records(state_trace) if row.get("type") in ("robot_damage", "robot_loot")]
    hits, loots = [], []
    for row in events:
        keys = DAMAGE_KEYS if row["type"] == "robot_damage" else LOOT_KEYS
        _require(
            set(row) == keys
            and all(type(row[key]) is int for key in keys - {"type"})
            and row["version"] == 1
            and 0 <= row["f"] < count,
            "Combat observations need their exact typed schema and bounded frame",
        )
        _require(
            (row["robot"], row["robot_id"], row["robot_signature"])
            == (actions["target"], actions["target_id"], actions["target_signature"]),
            "Combat observations must refer to the selected original robot, without collateral kills",
        )
        (hits if row["type"] == "robot_damage" else loots).append(row)
    _require(hits and hits[0]["f"] == damage and len(loots) == 1, "Missing projectile damage or unique loot resolution")
    _require([row["f"] for row in hits] == sorted(row["f"] for row in hits), "Projectile damage is out of order")
    grouped = defaultdict(list)
    lethal = []
    for row in hits:
        frame = row["f"]
        _require(
            frame <= death
            and row["parent"] == int(player_slot)
            and row["parent_type"] == 4
            and row["parent_signature"] == player["signature"]
            and row["weapon_id"] == 0
            and row["weapon"] >= 0
            and row["weapon_signature"] > player["signature"]
            and row["damage"] > 0
            and row["old_shields"] >= 0
            and row["new_shields"] == row["old_shields"] - row["damage"]
            and row["died"] == int(row["new_shields"] < 0)
            and row["score_value"] == actions["target_score"]
            and row["score"] == row["died"] * actions["target_score"]
            and row["kills_level"] == row["kills_total"] == row["died"],
            "Each damage observation must be a real starting-laser hit with the correct player, damage and award",
        )
        grouped[frame].append(row)
        if row["died"]:
            lethal.append(frame)
    _require(lethal == [death], "Exactly one projectile hit must kill the target at the declared frame")
    for frame in range(death + 1):
        shields = objects[frame][slot]["shields"]
        for row in grouped[frame]:
            _require(row["old_shields"] == shields, "Damage observations do not chain from the original shields")
            shields = row["new_shields"]
        _require(
            shields == objects[frame + 1][slot]["shields"], "Damage observations do not explain actual robot shields"
        )

    loot = loots[0]
    _require(loot["f"] == loot_frame, "Loot resolution must occur at its declared post-death frame")
    _require(
        tuple(loot["definition_" + key] for key in ("type", "id", "count", "probability")) == profile["loot"],
        "Loot decision must use the robot's original loaded public drop definition",
    )
    chance, roll = loot["chance_roll"], loot["count_roll"]
    eligible = loot["definition_count"]
    _require((0 <= chance <= 32767) if eligible else chance == -1, "Missing or invented authentic loot chance roll")
    selected = eligible and ((chance * 16) >> 15) < loot["definition_probability"]
    _require((0 <= roll <= 32767) if selected else roll == -1, "Missing or invented authentic loot count roll")
    created_count = ((roll * eligible) >> 15) + 1 if selected else 0
    _require(created_count in (0, 1), "These compact scenes qualify only zero or one physical loot object")
    _require(
        type(actions.get("loot_count")) is int
        and actions["loot_count"] == created_count
        and loot["contains_count"] == created_count,
        "The realized loot count must follow the loaded rule and actual SIM rolls",
    )
    initial_pickups = {(obj["type"], obj["signature"]) for obj in restored_objects.values() if obj["type"] in (2, 7)}
    new_pickups = {
        (obj["type"], obj["signature"])
        for boundary in objects
        for obj in boundary.values()
        if obj["type"] in (2, 7) and (obj["type"], obj["signature"]) not in initial_pickups
    }
    if created_count:
        _require(
            loot["contains_type"] == loot["created_type"] == loot["definition_type"]
            and loot["contains_id"] == loot["created_id"] == loot["definition_id"]
            and loot["created"] >= 0
            and loot["created_signature"] > actions["target_signature"],
            "Physical loot must have its original definition's type and identity",
        )
        created = str(loot["created"])
        drop = objects[loot_frame + 1].get(created)
        _require(
            drop is not None
            and (drop["type"], drop["id"], drop["signature"])
            == (loot["created_type"], loot["created_id"], loot["created_signature"])
            and new_pickups == {(drop["type"], drop["signature"])},
            "Exactly the observed physical loot object must appear after normal death resolution",
        )
    else:
        _require(
            all(loot[key] == -1 for key in ("created", "created_type", "created_id", "created_signature"))
            and not new_pickups,
            "A failed or unavailable loot rule must create no robot or powerup objects",
        )
    return {
        "robot_damage_frame": damage,
        "robot_death_frame": death,
        "robot_retirement_frame": retirement,
        "player_projectile_hits": len(hits),
        "score_awarded": actions["target_score"],
        "loot_resolution_frame": loot_frame,
        "physical_loot_objects": created_count,
    }

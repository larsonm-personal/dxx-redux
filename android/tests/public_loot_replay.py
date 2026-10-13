"""Require an authored robot's real laser drop to be collected by ordinary flight."""

from collections import defaultdict

import d1_replay_parity as parity
from public_robot_combat_replay import DAMAGE_KEYS, LOOT_KEYS

CARRIER = {"target": 39, "target_id": 20, "target_signature": 40, "target_shields": 4587520, "target_score": 500}


def require(condition, message):
    if not condition:
        raise parity.EvidenceError(message)


def verify(actions, header, states, state_trace):
    try:
        return _verify(actions, header, states, state_trace)
    except (KeyError, IndexError, TypeError, StopIteration) as error:
        raise parity.EvidenceError("Malformed or incomplete authored robot-loot evidence") from error


def _verify(actions, header, states, state_trace):
    count = header["frame_count"]
    require(
        (header["game"], header["mission"], header["level"], header["difficulty"], header["start_mode"])
        == ("d1", "d1", 1, 2, "new_level"),
        "Authored loot requires the original public D1 level-one start",
    )
    require(
        all(type(actions.get(key)) is int and actions[key] == value for key, value in CARRIER.items()),
        "Loot carrier must be the original public laser-upgrade robot",
    )
    damage, death, loot_frame, retirement, gain_frame, pickup_frame = (
        actions.get(key)
        for key in (
            "damage_frame",
            "action_frame",
            "loot_frame",
            "retirement_frame",
            "pickup_gain_frame",
            "pickup_frame",
        )
    )
    require(
        all(type(frame) is int for frame in (damage, death, loot_frame, retirement, gain_frame, pickup_frame))
        and 0 <= damage <= death < loot_frame <= retirement < gain_frame < pickup_frame < count
        and pickup_frame == gain_frame + 1,
        "Loot needs ordered projectile damage, death, physical drop, robot retirement and collection",
    )
    require(
        type(actions.get("pickup")) is int
        and actions["pickup"] >= 0
        and type(actions.get("pickup_signature")) is int
        and actions["pickup_signature"] > 40,
        "Loot requires the created pickup's exact slot and new signature",
    )
    objects = [dict(row["objects"]) for row in parity.object_states(state_trace, header, "d1")]
    worlds, initial_objects, initial_world, terminal_objects = [], None, None, None
    for row in parity.world_states(state_trace, header, "d1"):
        if row["type"] == "object_boundary":
            if row["phase"] == "restored":
                initial_objects = row["slots"]
            else:
                terminal_objects = row["slots"]
        elif row["type"] == "world_state":
            worlds.append(dict(row["state"]))
        elif row["phase"] == "restored":
            initial_world = row["state"]
        else:
            worlds.append(row["state"])
    require(
        len(objects) == count
        and len(worlds) == count + 1
        and len(states) == count + 1
        and initial_objects is not None
        and initial_world is not None
        and terminal_objects is not None,
        "Loot requires complete original, per-frame and terminal object/world evidence",
    )
    objects.append(terminal_objects)
    slot = str(actions["target"])

    def robot(obj):
        return obj is not None and tuple(obj[key] for key in ("type", "id", "signature")) == (2, 20, 40)

    for boundary in (initial_objects, objects[0]):
        carrier = boundary[slot]
        require(
            robot(carrier)
            and carrier["shields"] == CARRIER["target_shields"]
            and not carrier["flags"] & 1
            and tuple(carrier[key] for key in ("contains_type", "contains_id", "contains_count")) == (7, 3, 1),
            "Laser carrier must start alive with its exact authored contents and health",
        )
    require(
        all(robot(row.get(slot)) for row in objects[: retirement + 1])
        and all(not robot(row.get(slot)) for row in objects[retirement + 1 :]),
        "Original loot carrier did not retire at the declared boundary",
    )
    require(
        all(objects[index][slot]["shields"] >= 0 for index in range(death + 1))
        and all(
            objects[index][slot]["shields"] < 0 and objects[index][slot]["flags"] & 1
            for index in range(death + 1, retirement + 1)
        ),
        "Loot carrier must die through its real negative-shield explosion lifecycle",
    )
    player_slot = str(initial_world["players"][0]["objnum"])
    owner = initial_objects[player_slot]
    require(
        owner["type"] == 4 and initial_world["players"][0]["score"] == 0, "Loot requires the original unscored player"
    )
    events = [row for row in parity.records(state_trace) if row.get("type") in ("robot_damage", "robot_loot")]
    hits, drops = [], []
    for row in events:
        keys = DAMAGE_KEYS if row["type"] == "robot_damage" else LOOT_KEYS
        require(
            set(row) == keys
            and all(type(row[key]) is int for key in keys - {"type"})
            and row["version"] == 1
            and 0 <= row["f"] < count,
            "Loot needs complete typed damage/drop observations",
        )
        if row["robot"] == actions["target"]:
            require((row["robot_id"], row["robot_signature"]) == (20, 40), "Loot carrier identity was reused")
            (hits if row["type"] == "robot_damage" else drops).append(row)
    require(
        hits and hits[0]["f"] == damage and len(drops) == 1, "Missing target projectile hits or unique authored drop"
    )
    grouped, lethal = defaultdict(list), []
    for row in hits:
        frame = row["f"]
        after = worlds[frame + 1]["players"][0]
        require(
            frame <= death
            and row["parent"] == int(player_slot)
            and row["parent_type"] == 4
            and row["parent_signature"] == owner["signature"]
            and row["weapon_id"] == 0
            and row["weapon"] >= 0
            and row["weapon_signature"] > owner["signature"]
            and row["damage"] > 0
            and row["old_shields"] >= 0
            and row["new_shields"] == row["old_shields"] - row["damage"]
            and row["died"] == int(row["new_shields"] < 0)
            and row["score_value"] == 500
            and row["score"] == after["score"]
            and row["kills_level"] == after["num_kills_level"]
            and row["kills_total"] == after["num_kills_total"],
            "Carrier damage must be explained by real player-owned starting lasers and actual awards",
        )
        grouped[frame].append(row)
        if row["died"]:
            lethal.append(frame)
    require(lethal == [death], "Exactly one identified player laser must kill the carrier")
    for frame in range(death + 1):
        shields = objects[frame][slot]["shields"]
        for row in grouped[frame]:
            require(row["old_shields"] == shields, "Carrier damage observations do not chain")
            shields = row["new_shields"]
        require(shields == objects[frame + 1][slot]["shields"], "Carrier shield change lacks projectile evidence")
    for key in ("num_kills_level", "num_kills_total"):
        require(
            worlds[death + 1]["players"][0][key] == worlds[death]["players"][0][key] + 1,
            "The carrier kill must increment actual player kill counters exactly once",
        )
    drop = drops[0]
    require(
        drop["f"] == loot_frame
        and drop["chance_roll"] == drop["count_roll"] == -1
        and tuple(drop["definition_" + key] for key in ("type", "id", "count", "probability")) == (7, 2, 1, 3)
        and tuple(drop[key] for key in ("contains_type", "contains_id", "contains_count")) == (7, 3, 1)
        and (drop["created"], drop["created_type"], drop["created_id"], drop["created_signature"])
        == (actions["pickup"], 7, 3, actions["pickup_signature"]),
        "Authored laser contents must create the identified physical drop without fallback chance/count rolls",
    )
    pickup_slot = str(actions["pickup"])

    def pickup(obj):
        return obj is not None and (obj["type"], obj["id"], obj["signature"]) == (7, 3, actions["pickup_signature"])

    require(
        not any(obj["signature"] == actions["pickup_signature"] for obj in initial_objects.values()),
        "Robot loot must not be an existing level pickup",
    )
    require(
        all(not pickup(row.get(pickup_slot)) for row in objects[: loot_frame + 1])
        and all(pickup(row.get(pickup_slot)) for row in objects[loot_frame + 1 : pickup_frame + 1])
        and all(not pickup(row.get(pickup_slot)) for row in objects[pickup_frame + 1 :]),
        "The exact new laser pickup must spawn, persist, then retire at collection",
    )
    require(
        all(
            not objects[index][pickup_slot]["flags"] & 2 and objects[index][pickup_slot]["lifeleft"] > 0
            for index in range(loot_frame + 1, gain_frame + 1)
        )
        and objects[gain_frame + 1][pickup_slot]["flags"] & 2
        and objects[gain_frame + 1][pickup_slot]["lifeleft"] > 0,
        "The live laser drop must be consumed at inventory gain, then retire on the following frame",
    )
    require(
        [
            row
            for row in events
            if row["type"] == "robot_loot"
            and row["created"] >= 0
            and (row["created_type"], row["created_id"]) == (7, 3)
        ]
        == [drop],
        "The identified carrier must create the scene's only new robot-dropped laser",
    )
    for other_slot, other in objects[gain_frame].items():
        if other_slot != pickup_slot and (other["type"], other["id"]) == (7, 3):
            after = objects[gain_frame + 1].get(other_slot)
            require(
                after is not None
                and (after["type"], after["id"], after["signature"]) == (other["type"], other["id"], other["signature"])
                and not after["flags"] & 2,
                "Another laser pickup must not explain the inventory gain",
            )
    for index, world in enumerate(worlds):
        player = world["players"][0]
        require(
            player["score"] == 500 * int(index > death)
            and player["laser_level"] == int(index > gain_frame)
            and player["primary_weapon"] == 0
            and player["primary_weapon_flags"] == 1
            and world["weapons"]["delayed_primary_autoselect_weapon_index"] == -1
            and player["shields"] >= 0
            and world["death"]["active"] == 0,
            "The living player must collect exactly one laser upgrade and retain the selected starting laser",
        )
        require(
            states[index]["player0"]["score"] == player["score"]
            and states[index]["player0"]["laser_level"] == player["laser_level"]
            and states[index]["player0"]["primary_weapon"] == player["primary_weapon"],
            "Loot inventory or selected weapon disagrees with recorded gameplay state",
        )
    return {
        "robot_death_frame": death,
        "physical_loot_frame": loot_frame,
        "loot_collection_frame": gain_frame,
        "loot_retirement_frame": pickup_frame,
        "laser_level_increase": 1,
        "selected_primary": 0,
        "player_projectile_hits": len(hits),
        "score_awarded": 500,
        "uncredited_kills_before_carrier": worlds[death]["players"][0]["num_kills_level"],
    }

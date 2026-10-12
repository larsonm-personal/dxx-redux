"""Require an ordinary-start projectile release of the original public Guide-Bot."""

import d1_replay_parity as parity

PROFILE = {
    "target": 74,
    "buddy": 28,
    "buddy_id": 33,
    "buddy_signature": 29,
    "cage_segment": 224,
}


def require(condition, message):
    if not condition:
        raise parity.EvidenceError(message)


def verify(actions, header, states, state_trace):
    try:
        return _verify(actions, header, states, state_trace)
    except (KeyError, IndexError, TypeError) as error:
        raise parity.EvidenceError("Malformed or incomplete Guide-Bot release evidence") from error


def _verify(actions, header, states, state_trace):
    count = header["frame_count"]
    require(
        header["game"] == "d2"
        and header["mission"] == "d2demo"
        and header["level"] == 1
        and header["difficulty"] == 2
        and header["start_mode"] == "new_level",
        "Guide-Bot release requires the authentic public D2 level-one start",
    )
    require(
        all(type(actions.get(key)) is int and actions[key] == value for key, value in PROFILE.items()),
        "Guide-Bot and cage must match the original public level identities",
    )
    damage, blast, release, exit_frame = (
        actions.get(key) for key in ("damage_frame", "blast_frame", "action_frame", "exit_frame")
    )
    require(
        all(type(value) is int for value in (damage, blast, release, exit_frame))
        and 0 <= damage < blast <= min(release, exit_frame)
        and max(release, exit_frame) < count,
        "Guide-Bot requires ordered, bounded cage release and physical exit boundaries",
    )
    objects = [dict(row["objects"]) for row in parity.object_states(state_trace, header, "d2")]
    worlds, original_world, original_objects, terminal_objects = [], None, None, None
    for row in parity.world_states(state_trace, header, "d2"):
        if row["type"] == "object_boundary":
            if row["phase"] == "restored":
                original_objects = row["slots"]
            else:
                terminal_objects = row["slots"]
        elif row["type"] == "world_state":
            worlds.append(dict(row["state"]))
        elif row["phase"] == "restored":
            original_world = row["state"]
        else:
            worlds.append(row["state"])
    require(
        len(objects) == count
        and len(worlds) == len(states) == count + 1
        and original_world is not None
        and original_objects is not None
        and terminal_objects is not None,
        "Guide-Bot needs complete original, per-frame and terminal native evidence",
    )
    objects.append(terminal_objects)
    for initial in (original_world, worlds[0]):
        for wall, seg, side in ((74, 222, 4), (75, 224, 5)):
            barrier = initial["walls"][wall]
            require(
                (barrier["segnum"], barrier["sidenum"], barrier["type"], barrier["hps"], barrier["flags"])
                == (seg, side, 1, 100 * 65536, 0),
                "Both authored cage sides must start intact at full strength",
            )
        require(
            initial["ai"]["escort"]["Buddy_objnum"] == 28 and initial["ai"]["escort"]["Buddy_allowed_to_talk"] == 0,
            "The original Guide-Bot must start unreleased in both raw native boundaries",
        )
        player = initial["players"][0]
        require(
            player["score"] == player["num_kills_level"] == player["num_kills_total"] == 0
            and player["laser_level"] == player["primary_weapon"] == 0
            and player["primary_weapon_flags"] == 1,
            "Guide-Bot release starts with authentic starting lasers and no fabricated kills",
        )
    for slots in (original_objects, *objects):
        bot = slots.get("28")
        require(
            bot is not None
            and (bot["type"], bot["id"], bot["signature"]) == (2, 33, 29)
            and bot["shields"] == 1000 * 65536
            and not bot["flags"] & 3,
            "The same original living Guide-Bot must survive the entire release",
        )
    require(original_objects["28"]["segnum"] == 224, "Guide-Bot must begin inside its authored cage")
    for index, world in enumerate(worlds):
        require(
            all(bool(world["walls"][wall]["flags"] & 1) == (index > blast) for wall in (74, 75)),
            "Both cage barriers must blast at the exact declared post-frame boundary",
        )
        escort = world["ai"]["escort"]
        require(
            escort["Buddy_objnum"] == 28
            and type(escort["Buddy_allowed_to_talk"]) is int
            and escort["Buddy_allowed_to_talk"] == int(index > release),
            "The native Guide-Bot release flag must change at the exact declared boundary",
        )
        require(
            world["death"]["active"] == 0 and world["players"][0]["shields"] >= 0,
            "The player must remain alive while releasing the Guide-Bot",
        )

    require(
        any(0 < worlds[index]["walls"][74]["hps"] < worlds[0]["walls"][74]["hps"] for index in range(blast + 1)),
        "The actual cage wall must take projectile damage before it blasts",
    )
    first_exit = next((index - 1 for index, slots in enumerate(objects) if slots["28"]["segnum"] != 224), None)
    require(first_exit == exit_frame, "The original Guide-Bot must physically exit its cage at the declared boundary")
    first_damage = next(
        (
            index
            for index in range(blast + 1)
            if worlds[index + 1]["walls"][74]["hps"] < worlds[index]["walls"][74]["hps"]
        ),
        None,
    )
    require(first_damage == damage, "The cage must take its first real damage at the declared boundary")
    return {"guidebot_released_frame": release, "guidebot_exited_frame": exit_frame, "original_guidebot_survived": True}

"""Fail-closed outcome evidence for the authored D2 demo level-two shoot switch."""

import d1_replay_parity as parity

# Authored public asset identities, verified against demo_import_oracles.json
SWITCH = {"target": 154, "trigger": 17, "switch_segment": 84, "switch_side": 2, "front_wall": 116, "back_wall": 117}
FRONT, BACK = (85, 4), (235, 5)
LOCKED, OPENED = 8, 2


def _require(condition, message):
    if not condition:
        raise parity.EvidenceError(message)


def verify(actions, header, states, state_trace):
    try:
        return _verify(actions, header, states, state_trace)
    except (KeyError, IndexError, TypeError) as error:
        raise parity.EvidenceError("Malformed or incomplete shoot-switch outcome evidence") from error


def _verify(actions, header, states, state_trace):
    _require(
        header["game"] == "d2" and header["mission"] == "d2demo" and header["level"] == 2,
        "Shoot-switch case requires the authored D2 public level-two start",
    )
    _require(
        all(type(actions.get(key)) is int and actions[key] == value for key, value in SWITCH.items()),
        "Shoot-switch identities do not match the authored public scene",
    )
    count = header["frame_count"]
    activation, passage = actions.get("action_frame"), actions.get("passage_frame")
    _require(
        type(activation) is int and type(passage) is int and 0 <= activation < passage < count,
        "Shoot-switch activation and passage require distinct ordered frame boundaries",
    )
    worlds, restored, terminal = {}, None, None
    for row in parity.world_states(state_trace, header, "d2"):
        if row["type"] == "object_boundary":
            continue
        state = row["state"]
        if row["type"] == "world_state":
            worlds[row["f"]] = {key: state[key] for key in ("walls", "triggers", "segments", "death")}
        elif row["phase"] == "restored":
            restored = state
        else:
            terminal = state
    _require(
        restored is not None and terminal is not None and set(worlds) == set(range(count)),
        "Shoot-switch requires complete initial, per-frame and terminal world evidence",
    )
    worlds[count] = terminal
    for initial in (restored, worlds[0]):
        walls, trigger = initial["walls"], initial["triggers"][17]
        _require(
            initial["segments"][84]["sides"][2]["wall"] == 154,
            "The original switch side must reference the authored trigger wall",
        )
        _require(
            len(walls) > 154
            and all(
                (
                    walls[index]["segnum"],
                    walls[index]["sidenum"],
                    walls[index]["type"],
                    walls[index]["state"],
                    walls[index]["keys"],
                )
                == (*location, 2, 0, 1)
                for index, location in ((116, FRONT), (117, BACK))
            ),
            "Both authored passage walls must initially be closed keyless doors",
        )
        _require(
            walls[116]["flags"] == 24 and walls[117]["flags"] == 16,
            "Authored forward door must start locked with its original unlocked reverse side",
        )
        _require(
            (walls[154]["segnum"], walls[154]["sidenum"], walls[154]["trigger"]) == (84, 2, 17)
            and trigger["type"] == 7
            and trigger["num_links"] == 2
            and list(zip(trigger["segments"][:2], trigger["sides"][:2])) == [FRONT, BACK],
            "Shoot switch must link the two original passage sides to its unlock trigger",
        )
    _require(
        all(worlds[index]["walls"][116]["flags"] & LOCKED for index in range(activation + 1))
        and all(
            not worlds[index]["walls"][side]["flags"] & LOCKED
            for index in range(activation + 1, count + 1)
            for side in (116, 117)
        ),
        "The real forward lock must clear at exactly the declared activation boundary",
    )
    _require(
        worlds[activation]["segments"][84]["sides"][2]["overlay"]
        != worlds[activation + 1]["segments"][84]["sides"][2]["overlay"],
        "The projectile must change the original shoot-switch overlay at activation",
    )
    activations = [
        row
        for row in parity.records(state_trace)
        if row.get("type") == "trigger_activation" and row.get("trigger") == 17
    ]
    _require(len(activations) == 1, "Exactly one successful authored shoot-switch activation is required")
    event = activations[0]
    expected = {
        "version": 1,
        "f": activation,
        "segment": 84,
        "side": 2,
        "trigger": 17,
        "shot": 1,
        "activator": 0,
        "activator_type": 4,
        "activator_signature": 0,
    }
    _require(
        all(type(event.get(key)) is int and event[key] == value for key, value in expected.items()),
        "The declared trigger must actually be activated by a player projectile",
    )
    _require(
        all(
            worlds[index]["walls"][side]["state"] == 0 and not worlds[index]["walls"][side]["flags"] & OPENED
            for index in range(activation + 1)
            for side in (116, 117)
        ),
        "Both passage sides must remain closed until the projectile activates the switch",
    )
    _require(
        all(
            worlds[index]["walls"][116]["state"] == worlds[index]["walls"][117]["state"]
            and bool(worlds[index]["walls"][116]["flags"] & OPENED)
            == bool(worlds[index]["walls"][117]["flags"] & OPENED)
            for index in range(count + 1)
        ),
        "Paired door animation and open flags must stay consistent",
    )
    openings = [
        index
        for index in range(activation + 1, passage + 2)
        if all(worlds[index]["walls"][side]["state"] in (1, 2) for side in (116, 117))
    ]
    _require(
        openings and all(worlds[passage + 1]["walls"][side]["flags"] & OPENED for side in (116, 117)),
        "Both linked door sides must open before the passage is traversed",
    )
    segments = [state["position"]["segment"] for state in states]
    _require(
        len(segments) == count + 1
        and segments[0] == 217
        and BACK[0] not in segments[: passage + 1]
        and segments[passage] == FRONT[0]
        and segments[passage + 1] == BACK[0],
        "Ordinary replay movement must cross the newly unlocked passage at its exact boundary",
    )
    _require(
        all(worlds[index]["death"]["active"] == 0 for index in range(count + 1)),
        "Shoot-switch traversal requires a living player throughout",
    )
    return {"projectile_trigger_frame": activation, "both_doors_opening_frame": openings[0], "passage_frame": passage}

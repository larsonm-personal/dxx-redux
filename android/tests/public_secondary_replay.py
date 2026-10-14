"""Prove the native two-tap cooldown boundary and both SDL dispatch orders."""

import d1_replay_parity as parity
import ordered_public_replay as ordered

SCENARIOS = ("secondary-release-first", "secondary-mouse-first")


def actions_for(scenario):
    if scenario not in SCENARIOS:
        raise parity.EvidenceError("Unknown secondary dispatch scenario")
    return {
        "first_fire_frame": 2,
        "second_press_frame": 12,
        "release_frame": 13,
        "initial_ammo": 5,
        "terminal_ammo": 3 if scenario.endswith("mouse-first") else 4,
        "cooldown_ticks": 32768,
        "second_press_ticks_early": 8,
    }


def capture_actions(report):
    expected = actions_for(report["scenario"])
    observations = report.get("observations", [])
    if (
        report.get("frames") != 25
        or report.get("initial_ammo") != 5
        or report.get("terminal_ammo") != expected["terminal_ammo"]
        or len(observations) != 25
        or observations[12]["cooldown_before"] - observations[12]["game_time"] != 8
        or observations[2]["ammo_after"] != 4
        or observations[12]["ammo_after"] != 4
        or observations[13]["ammo_after"] != expected["terminal_ammo"]
        or observations[13]["held"]
    ):
        raise parity.EvidenceError("Live SDL capture did not prove the native cooldown/order boundary")
    return expected


def verify(case, demo, header, frames, terminal):
    scenario = case["scenario"]
    expected = actions_for(scenario)
    if case["actions"] != expected or header["level"] != 1 or len(frames) != 25:
        raise parity.EvidenceError("Secondary scene must keep its exact ordinary-start two-tap contract")
    records = list(parity.records(demo))
    batches = ordered.validate(header, records[1:-2], records[-2])
    operations = batches[13]["ops"]
    held = [op["s"].get("f2s", 0) for op in operations if op["k"] == "sample"]
    if [op["k"] for op in operations] != ["reset", "sample", "reset", "sample"] or held != (
        [1, 0] if scenario.endswith("mouse-first") else [0, 0]
    ):
        raise parity.EvidenceError("Secondary recording lost the actual ordered SDL release batch")
    ammo = [frame["state"]["player0"]["secondary_ammo"][0] for frame in frames]
    expected_ammo = [5] * 2 + [4] * 11 + [expected["terminal_ammo"]] * 12
    if ammo != expected_ammo or terminal["player0"]["secondary_ammo"][0] != expected["terminal_ammo"]:
        raise parity.EvidenceError("Native secondary ammo did not match the exact two-tap event order")
    if (
        batches[2]["residual"]["s"].get("f2s") != 1
        or batches[12]["residual"]["s"].get("f2s") != 1
        or 10 * frames[0]["ft"] != expected["cooldown_ticks"] - expected["second_press_ticks_early"]
        or any(frame["ft"] != 3276 for frame in frames)
        or max(frame["diag"]["player_weapon_count"] for frame in frames) < 5 - expected["terminal_ammo"]
    ):
        raise parity.EvidenceError("Secondary scene lacks its held pulses, fixed cooldown boundary or live projectiles")
    return {
        "secondary_shots": 5 - expected["terminal_ammo"],
        "release_frame": 13,
        "second_press_ticks_early": 8,
        "ordered_input": True,
    }

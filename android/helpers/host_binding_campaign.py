#!/usr/bin/env python3
"""Real native keyboard rebind/cancel/persistence and before/after motion checks."""

import argparse
from collections import deque
import json
from pathlib import Path

from host_config_campaign import HostSession, ProbeFailure, freeze_binary, write_json


def observed_bindings(session):
    return session.wait(lambda s: s["kind"] == "bindings" and bool(s["items"]), "keyboard binding state")


def open_bindings(session):
    session.enter_options()
    session.open_menu("controls", exact=True)
    session.open_menu("customize keyboard", exact=True)
    return observed_bindings(session)


def select_binding(session, target):
    state = observed_bindings(session)
    frontier = deque([(state["selected"], [])])
    seen = set()
    while frontier:
        index, path = frontier.popleft()
        if index == target:
            for direction in path:
                session.key(direction)
                observed_bindings(session)
            return session.wait(
                lambda s: s["kind"] == "bindings" and s["selected"] == target, "selected binding action"
            )
        if index in seen:
            continue
        seen.add(index)
        for direction in ("up", "down", "left", "right"):
            neighbor = state["items"][index][direction]
            if 0 <= neighbor < len(state["items"]):
                frontier.append((neighbor, path + [direction]))
    raise ProbeFailure("binding action unreachable through observed navigation")


def change_binding(session, index, key, cancel=False):
    before = select_binding(session, index)
    session.key("enter")
    session.wait(lambda s: s["kind"] == "bindings" and s["subtitle"] == "changing", "binding capture active")
    session.key("escape" if cancel else key)
    after = session.wait(lambda s: s["kind"] == "bindings" and s["subtitle"] == "idle", "binding capture ended")
    if cancel and [i["value"] for i in after["items"]] != [i["value"] for i in before["items"]]:
        raise ProbeFailure("cancel changed a keyboard mapping")
    return after


def back_to_main(session):
    session.key("escape")
    session.wait(lambda s: session.has_item(s, "customize keyboard"), "binding window exited")
    session.back_to_options()
    session.key("escape")
    session.wait(lambda s: session.has_item(s, "new game", exact=True), "options exited to main")


def motion_check(session, key, moves):
    before = session.wait(lambda s: s["kind"] == "game", "motion baseline")
    session.key(key, hold_ms=600)
    after = session.wait(lambda s: s["kind"] == "game", "motion result")
    delta = sum(abs(a - b) for a, b in zip(before["ship"]["forward"], after["ship"]["forward"]))
    if (moves and delta < 8192) or (not moves and delta > 512):
        raise ProbeFailure(f"key {key} moves={moves} orientation delta={delta}")
    return {"key": key, "expected_motion": moves, "forward_delta": delta}


def run(executable, data, output, game, mission, version):
    session = HostSession(executable, data, output, game, timeout=240, config={"DigiVolume": 0, "MusicType": 0})
    phases = []
    try:
        session.start()
        original = open_bindings(session)
        candidates = [i for i, item in enumerate(original["items"]) if item["text"].lower() == "pitch forward"]
        target = next((i for i in candidates if original["items"][i]["binding"].strip().lower() == "up"), None)
        if target is None:
            raise ProbeFailure(f"no observed pitch-forward UP mapping: {original['items']}")
        previous = original["items"][target]["value"]
        if sum(i["value"] == previous for i in original["items"]) != 1:
            raise ProbeFailure("old key is not unique; old-key negative control would be ambiguous")
        used = {i["binding"].strip().lower() for i in original["items"]}
        key = next((key for key in ("u", "i", "o", "p", "y", "h", "j") if key not in used), None)
        if key is None:
            raise ProbeFailure("no unused observed letter binding available")
        change_binding(session, target, key, cancel=True)
        back_to_main(session)
        session.start_level(mission, version=version)
        phases.append(
            {"phase": "before", "checks": [motion_check(session, key, False), motion_check(session, "up", True)]}
        )
        session.leave_game()
        open_bindings(session)
        changed = change_binding(session, target, key)
        assigned = changed["items"][target]
        if assigned["value"] == previous or assigned["binding"].strip().lower() != key:
            raise ProbeFailure("new key not reflected in binding widget")
        back_to_main(session)
        session.start_level(mission, version=version)
        phases.append(
            {"phase": "rebound", "checks": [motion_check(session, "up", False), motion_check(session, key, True)]}
        )
        session.leave_game()
        session.close()
        session.start()
        reopened = open_bindings(session)
        if reopened["items"][target]["value"] != assigned["value"]:
            raise ProbeFailure("binding did not persist through process relaunch")
        change_binding(session, target, "up", cancel=True)
        restored = change_binding(session, target, "up")
        if [i["value"] for i in restored["items"]] != [i["value"] for i in original["items"]]:
            raise ProbeFailure("binding roundtrip did not restore the complete keyboard map")
        back_to_main(session)
        session.start_level(mission, version=version)
        phases.append(
            {"phase": "restored", "checks": [motion_check(session, key, False), motion_check(session, "up", True)]}
        )
        session.leave_game()
        session.close()
        return {
            "status": "pass",
            "game": game,
            "mission": mission,
            "version": version,
            "new_key": key,
            "target": target,
            "previous_value": previous,
            "assigned_value": assigned["value"],
            "phases": phases,
            "scope": "real binding/cancel/reopen/relaunch, unique old-key and unused new-key motion controls, full map roundtrip",
        }
    finally:
        session.stop()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--executable", required=True)
    p.add_argument("--data", required=True)
    p.add_argument("--game", choices=("d1", "d2"), required=True)
    p.add_argument("--mission", required=True)
    p.add_argument("--version", type=int, choices=(1, 2), required=True)
    p.add_argument("--output", required=True)
    args = p.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    executable = freeze_binary(args.executable, output)
    try:
        result = run(executable, args.data, output, args.game, args.mission, args.version)
    except Exception as error:
        result = {"status": "failed", "error": repr(error)}
    write_json(output / "result.json", result)
    print(json.dumps(result), flush=True)
    return 0 if result["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Bounded real-menu graphics/control combinations around gameplay and save restoration."""

import argparse
import datetime
import json
from pathlib import Path
import time

from host_config_campaign import HostSession, ProbeFailure, covering_cases, freeze_binary, write_json
from host_render_campaign import capture

FACTORS = {
    "AlphaEffects": [0, 1],
    "DynLightColor": [0, 1],
    "DisableCockpit": [0, 1],
    "StickyRearview": [0, 1],
    "MouseFSIndicator": [0, 1],
    "ClassicDepth": [0, 1],
    "ReticleType": [0, 2, 5],
    "ReticleSize": [0, 4],
    "KeyboardSens0": [0, 16],
    "MouseSens0": [0, 16],
    "Difficulty": [0, 2, 4],
}
TRIPLES = [("AlphaEffects", "DynLightColor", "ClassicDepth"), ("DisableCockpit", "ReticleType", "ReticleSize")]
RETICLES = {0: "Classic", 2: "None", 5: "Circle"}
DIFFICULTIES = {0: "Trainee", 2: "Hotshot", 4: "Insane"}
GRAPHICS = {
    "AlphaEffects": "Transparency Effects",
    "DynLightColor": "Colored Dynamic Light",
    "DisableCockpit": "Disable Cockpit View",
    "ClassicDepth": "Classic Depth Ordering",
}
CONTROLS = {"StickyRearview": "Sticky Rearview", "MouseFSIndicator": "Mouse FlightSim Indicator"}


def widget(session, label, value, verify, section=None):
    if not verify:
        session.set_item(label, value, exact=True, section=section)
    else:
        state = session.select(label, exact=True, section=section)
        actual = state["items"][state["selected"]].get("value")
        if actual != value:
            raise ProbeFailure(f"reopened widget {label}: expected {value}, got {actual}")


def settings(session, values, verify=False):
    """Enter and return to Options; Escape applies these native menus' changes."""
    session.open_menu("graphics options")
    for field, label in GRAPHICS.items():
        widget(session, label, values[field], verify)
    session.open_menu("reticle options", exact=True)
    widget(session, RETICLES[values["ReticleType"]], 1, verify)
    widget(session, "Reticle Size:", values["ReticleSize"], verify)
    session.key("escape")
    session.wait(lambda s: session.has_item(s, "transparency effects"), "return from reticle")
    session.back_to_options()
    session.open_menu("controls", exact=True)
    for field, label in CONTROLS.items():
        widget(session, label, values[field], verify)
    session.open_menu("sensitivity & deadzone", exact=True)
    widget(session, "Turn L/R", values["KeyboardSens0"], verify, "Keyboard Sensitivity")
    widget(session, "Turn L/R", values["MouseSens0"], verify, "Mouse Sensitivity")
    session.key("escape")
    session.wait(lambda s: session.has_item(s, "sensitivity & deadzone"), "return from sensitivity")
    session.back_to_options()
    session.assert_config({"ClassicDepth": values["ClassicDepth"]})
    session.assert_player({k: v for k, v in values.items() if k not in ("ClassicDepth", "Difficulty")})


def assert_restored(saved, changed, restored):
    if saved["mission"] != restored["mission"] or saved["game"]["level"] != restored["game"]["level"]:
        raise ProbeFailure("save restored the wrong mission or level")
    delta = sum(abs(a - b) for a, b in zip(saved["ship"]["forward"], changed["ship"]["forward"]))
    error = sum(abs(a - b) for a, b in zip(saved["ship"]["forward"], restored["ship"]["forward"]))
    if delta < 8192:
        raise ProbeFailure(f"real turn did not distinguish saved state: delta={delta}")
    if error > 512:
        raise ProbeFailure(f"saved orientation was not restored: error={error}, changed={delta}")
    return {"changed_forward_delta": delta, "restored_forward_error": error}


def run_case(executable, data, directory, game, mission, version, values):
    session = HostSession(
        executable,
        data,
        directory,
        game,
        timeout=300,
        config={"DigiVolume": 0, "MusicType": 0, "ResolutionX": 640, "ResolutionY": 480},
    )
    try:
        session.start()
        original = session.wait(lambda s: True, "initial player configuration")
        baseline = {k: original["config" if k == "ClassicDepth" else "player"][k] for k in FACTORS if k != "Difficulty"}
        session.enter_options()
        settings(session, values)
        settings(session, values, verify=True)
        session.key("escape")
        session.wait(lambda s: session.has_item(s, "new game", exact=True), "main menu")
        loaded = session.start_level(mission, difficulty=DIFFICULTIES[values["Difficulty"]], version=version)
        if loaded["difficulty"] != values["Difficulty"] or loaded["presentation_version"] != version:
            raise ProbeFailure("actual difficulty or committed content mode mismatch")
        session.automap_roundtrip()
        session.enter_game_options()
        settings(session, baseline)
        settings(session, values)
        session.key("escape")
        session.wait(lambda s: s["kind"] == "game", "resume after options")
        session.save_first_slot()
        saved = session.last_saved_state
        session.key("up", hold_ms=600)
        changed = session.wait(lambda s: s["kind"] == "game", "real ship turn")
        restored = session.load_first_slot()
        restoration = assert_restored(saved, changed, restored)
        _, rendering = capture(session, "restored-game")
        session.leave_game()
        session.close()
        session.start()
        session.enter_options()
        settings(session, values, verify=True)
        settings(session, baseline)
        settings(session, baseline, verify=True)
        session.key("escape")
        session.wait(lambda s: session.has_item(s, "new game", exact=True), "final main menu")
        session.close()
        return {
            "status": "pass",
            "values": values,
            "baseline": baseline,
            "restoration": restoration,
            "render": rendering,
            "scope": "native widget/effective state, actual difficulty/content, automap, real turn/save restore, relaunch and A-B-A",
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
    p.add_argument("--seed", type=int, default=20261014)
    p.add_argument("--limit", type=int)
    p.add_argument("--deadline", required=True, help="ISO-8601 scheduling cutoff with timezone")
    args = p.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    executable = freeze_binary(args.executable, output)
    rows = covering_cases(args.seed, FACTORS, TRIPLES)
    if args.limit is not None:
        rows = rows[: args.limit]
    write_json(output / "cases.json", {"seed": args.seed, "factors": FACTORS, "triples": TRIPLES, "cases": rows})
    cutoff = datetime.datetime.fromisoformat(args.deadline)
    if cutoff.tzinfo is None:
        p.error("--deadline must include a timezone")
    until = cutoff.timestamp()
    results = []
    for index, values in enumerate(rows):
        if time.time() >= until:
            break
        folder = output / f"case-{index:03d}"
        folder.mkdir()
        try:
            result = run_case(executable, args.data, folder, args.game, args.mission, args.version, values)
        except Exception as error:
            result = {"status": "failed", "error": repr(error), "values": values}
        write_json(folder / "result.json", result)
        results.append(result)
        print(json.dumps({"case": index, "status": result["status"], "error": result.get("error")}), flush=True)
        if result["status"] != "pass":
            break
    complete = len(results) == len(rows)
    summary = {
        "planned": len(rows),
        "executed": len(results),
        "passed": sum(r["status"] == "pass" for r in results),
        "complete": complete,
        "seed": args.seed,
        "results": results,
    }
    write_json(output / "summary.json", summary)
    return 0 if complete and summary["passed"] == len(rows) and rows else 1


if __name__ == "__main__":
    raise SystemExit(main())

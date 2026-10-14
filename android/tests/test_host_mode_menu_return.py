#!/usr/bin/env python3
"""Real main-menu return regression across native D1/D2 and imported D1 modes.

Uses only the explicit opt-in desktop test build and ordinary engine inputs.
No Android launcher, retail fixtures or external input interception is involved.
"""

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "helpers"))
from host_config_campaign import HostSession, freeze_binary, write_json
from host_render_campaign import capture


def nearby_returns(session, name, alternative=None):
    before = session.wait(lambda s: session.has_item(s, "new game", exact=True), "main menu before nearby returns")
    session.enter_options()
    session.key("escape")
    after = session.wait(lambda s: session.has_item(s, "new game", exact=True), "main menu after options")
    assert after["mission"] == before["mission"] and not after["game"]["active"]
    if alternative:
        session.open_menu("new game", exact=True)
        session.wait(lambda s: s["kind"] == "listbox", "mission picker")
        session.select(alternative)
        session.key("escape")
        after = session.wait(lambda s: session.has_item(s, "new game", exact=True), "cancel mission picker")
        assert after["mission"] == before["mission"] and not after["game"]["active"]
    _, pixels = capture(session, name)
    return {
        "mission_before": before["mission"],
        "mission_after": after["mission"],
        "render": pixels,
        "options_return": True,
        "mission_picker_cancel": bool(alternative),
    }


def run_route(executable, data, directory, game, route):
    session = HostSession(
        executable,
        data,
        directory,
        game,
        timeout=180,
        config={"MusicType": 0, "DigiVolume": 0, "ResolutionX": 640, "ResolutionY": 480, "WindowMode": 1},
    )
    states = []
    try:
        session.start()
        combined = any(mission.startswith("D1:") for mission, _ in route)
        nearby = [nearby_returns(session, "initial-nearby", "D1: Descent Demo" if combined else None)]
        for index, (mission, version) in enumerate(route):
            loaded = session.start_level(mission=mission, version=version)
            returned = session.leave_game()
            _, pixels = capture(session, f"return-{index}-v{version}")
            states.append({"loaded": loaded, "returned": returned, "render": pixels})
            alternative = ("Descent 2 Demo" if version == 1 else "D1: Descent Demo") if combined else None
            nearby.append(nearby_returns(session, f"nearby-{index}-v{version}", alternative))
        session.close()
        return {
            "status": "pass",
            "route": route,
            "states": states,
            "nearby": nearby,
            "scope": "same process and disposable profile; semantic mode/level, main menu, real rendered frame and clean quit",
        }
    finally:
        session.stop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--d1", required=True)
    parser.add_argument("--d2", required=True)
    parser.add_argument("--d1-data", required=True)
    parser.add_argument("--d2-data", required=True)
    parser.add_argument("--combined-data", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    binaries = {}
    for game in ("d1", "d2"):
        folder = output / game
        folder.mkdir()
        binaries[game] = freeze_binary(getattr(args, game), folder)
    cases = [
        ("native-d1", "d1", args.d1_data, [("Descent Demo", 1)]),
        ("native-d2", "d2", args.d2_data, [("Descent 2 Demo", 2)]),
        ("imported-d1", "d2", args.combined_data, [("D1: Descent Demo", 1)]),
        (
            "shared-d1-d2-d1",
            "d2",
            args.combined_data,
            [("D1: Descent Demo", 1), ("Descent 2 Demo", 2), ("D1: Descent Demo", 1)],
        ),
    ]
    results = []
    for name, game, data, route in cases:
        directory = output / name
        directory.mkdir()
        try:
            result = run_route(binaries[game], data, directory, game, route)
        except Exception as error:
            result = {"status": "failed", "error": repr(error), "route": route}
        result["name"] = name
        write_json(directory / "result.json", result)
        results.append(result)
        print(json.dumps({k: v for k, v in result.items() if k != "states"}), flush=True)
    write_json(
        output / "summary.json",
        {
            "passed": sum(r["status"] == "pass" for r in results),
            "planned": len(cases),
            "executed": len(results),
            "results": results,
        },
    )
    return 0 if all(r["status"] == "pass" for r in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())

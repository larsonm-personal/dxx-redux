#!/usr/bin/env python3
"""Run real ordinary-save timer boundaries using pinned public game data.

Seismic cases seed synthetic events on public levels, not retail weapon content.
The caller supplies the normal native runtime environment (SDL and libraries).
"""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("d1-exe", "d2-exe", "d1-assets", "d2-assets", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    cases = [("d1", "cooldowns", False)]
    cases += [("d2", scenario, False) for scenario in ("pickup", "cooldowns", "seismic", "ambient", "seismic-level")]
    cases += [("d2", scenario, True) for scenario in ("pickup", "cooldowns")]
    results = []
    for game, scenario, imported in cases:
        name = ("imported-d1" if imported else game) + "-" + scenario
        directory = output / name
        directory.mkdir(exist_ok=True)
        executable = getattr(args, game + "_exe").resolve()
        assets = (args.d1_assets if game == "d1" or imported else args.d2_assets).resolve()
        command = [str(executable), "--timer-save-runtime", str(assets), scenario]
        if imported:
            command.append("--imported-d1")
        with (directory / "stdout.log").open("w", encoding="utf-8") as log:
            try:
                result = subprocess.run(command, cwd=directory, stdout=log, stderr=subprocess.STDOUT, timeout=180, check=False)
                code = result.returncode
            except subprocess.TimeoutExpired:
                code = 124
        results.append({"case": name, "exit": code, "command": command})
        print(f"{name}: {'PASS' if code == 0 else 'FAIL'}", flush=True)
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    return int(any(result["exit"] != 0 for result in results))


if __name__ == "__main__":
    raise SystemExit(main())

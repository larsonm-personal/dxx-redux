"""Verify controlled, nonvacuous public station production without a video backend."""

import argparse
import json
import os
from pathlib import Path
import subprocess

from run_compact_public_replays import validate_assets
from d1_replay_parity import EvidenceError

PROFILES = {"d1": (3, ((0, 213), (1, 214)), 3), "d2": (2, ((5, 50), (14, 233), (16, 263)), 25)}


def validate_report(report, game):
    level, stations, energy_centers = PROFILES[game]
    if (
        any(type(report.get(key)) is not int for key in ("schema", "level", "matcens"))
        or report.get("schema") != 1
        or report.get("scope") != "controlled_public_station_loop"
        or report.get("game") != game
        or report.get("level") != level
        or report.get("matcens") != len(stations)
        or report.get("passed") is not True
    ):
        raise EvidenceError("Public matcen report must name its nonempty authored fixture and controlled scope")
    expected = [
        (difficulty, center, policy) for difficulty in (2, 4) for center in range(len(stations)) for policy in range(4)
    ]
    cases = report.get("cases", [])
    if [(row.get("difficulty"), row.get("center"), row.get("policy")) for row in cases] != expected:
        raise EvidenceError("Missing, reordered or duplicate public station cases")
    first_rounds = {}
    for row in cases:
        if any(
            type(row.get(key)) is not int
            for key in (
                "difficulty",
                "center",
                "policy",
                "station",
                "segment",
                "energy_centers",
                "first_births",
                "second_births",
                "first_lives",
                "second_lives",
                "activation_count",
            )
        ):
            raise EvidenceError("Station case counters and identities must be integers")
        cleared = row.get("cleared_authored_robots")
        if (
            not isinstance(cleared, list)
            or any(
                not isinstance(value, list)
                or len(value) != 3
                or any(type(item) is not int or item < 0 for item in value)
                for value in cleared
            )
            or len({tuple(value) for value in cleared}) != len(cleared)
        ):
            raise EvidenceError("Controlled fixture must disclose any cleared authored robot identities")
        difficulty, center, policy = row["difficulty"], row["center"], row["policy"]
        if (row.get("station"), row.get("segment"), row.get("energy_centers")) != (*stations[center], energy_centers):
            raise EvidenceError("Public station or energy-center identity changed")
        births = row.get("births", [])
        first = [birth for birth in births if birth.get("round") == 1]
        second = [birth for birth in births if birth.get("round") == 2]
        if births != first + second or (row.get("first_births"), row.get("second_births")) != (len(first), len(second)):
            raise EvidenceError("Incomplete native production rounds")
        if (policy < 2 and not 1 < len(first) <= difficulty + 3) or (policy >= 2 and first):
            raise EvidenceError("One-round must preserve multiple native births and pause must genuinely suppress them")
        if (policy == 0 and not 1 < len(second) <= difficulty + 3) or (policy != 0 and second):
            raise EvidenceError("Default must produce again while one-round/paused suppress reactivation")
        activations = (2, 1, 0, 1)[policy]
        consumes_life = game == "d1" or difficulty != 4 or policy == 1
        lives = 3 - int(policy != 2 and consumes_life)
        if (row.get("activation_count"), row.get("first_lives")) != (activations, lives):
            raise EvidenceError("Activation counts or intentional D1/D2 life rules changed")
        if row["second_lives"] != lives - int(policy == 0 and consumes_life):
            raise EvidenceError("Suppressed retriggers cannot consume station lives")
        if row.get("energy_state_preserved") is not True:
            raise EvidenceError("All policy transitions must preserve the loaded energy-center state")
        for birth in births:
            if (
                set(birth) != {"round", "frame", "object", "id", "signature", "shields"}
                or any(type(value) is not int for value in birth.values())
                or not 0 <= birth["frame"] < 2400
                or min(birth["object"], birth["id"], birth["signature"]) < 0
                or birth["shields"] <= 0
            ):
                raise EvidenceError("Each real robot needs a complete typed native identity")
        if len({birth["signature"] for birth in births}) != len(births) or any(
            [birth["frame"] for birth in production] != sorted({birth["frame"] for birth in production})
            for production in (first, second)
        ):
            raise EvidenceError("Native births require distinct identities and strictly ordered production frames")
        normalized = [
            {key: value for key, value in birth.items() if key not in ("object", "signature")} for birth in first
        ]
        if policy == 0:
            first_rounds[difficulty, center] = normalized
        elif policy == 1 and normalized != first_rounds[difficulty, center]:
            raise EvidenceError("One-round changed its otherwise ordinary first production round")
    return len(cases)


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    environment = dict(os.environ, SDL_VIDEODRIVER="invalid")
    environment.pop("DISPLAY", None)
    environment.pop("WAYLAND_DISPLAY", None)
    summary = {"schema": 1, "scope": "controlled_public_station_loop", "games": []}
    for game in PROFILES:
        data, executable = getattr(args, "data_" + game), getattr(args, "exe_" + game)
        validate_assets(game, data)
        reference = None
        for repeat in (1, 2):
            directory = args.output / f"{game}-{repeat}"
            directory.mkdir()
            command = [str(executable), "--public-matcen-stations", str(data), "stations.json"]
            with (directory / "process.log").open("w") as log:
                subprocess.run(
                    command,
                    cwd=directory,
                    env=environment,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    timeout=args.timeout,
                    check=True,
                )
            raw = (directory / "stations.json").read_bytes()
            count = validate_report(json.loads(raw), game)
            if reference is not None and raw != reference:
                raise EvidenceError("Repeated public matcen production differs")
            reference = raw
        directory = args.output / f"{game}-without-activation"
        directory.mkdir()
        completed = subprocess.run(
            command + ["--without-activation"],
            cwd=directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=args.timeout,
        )
        log = completed.stdout + completed.stderr
        (directory / "process.log").write_text(log)
        if (
            completed.returncode != 1
            or "FAIL: first activation produces multiple real robots or is genuinely suppressed" not in log
            or (directory / "stations.json").exists()
        ):
            raise EvidenceError("Omitting actual native activation did not fail the nonvacuous production assertion")
        summary["games"].append(
            {"game": game, "cases": count, "identical_repeats": 2, "without_activation_rejected": True}
        )
    (args.output / "summary.json").write_text(json.dumps(summary, indent=4) + "\n")
    print("PASS: actual public matcen production, policy suppression, energy centers and omitted-activation controls")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("exe-d1", "exe-d2", "data-d1", "data-d2", "output"):
        parser.add_argument("--" + name, required=True, type=lambda value: Path(value).resolve())
    parser.add_argument("--timeout", type=int, default=120)
    run(parser.parse_args())


if __name__ == "__main__":
    main()

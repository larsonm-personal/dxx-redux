"""Verify controlled, nonvacuous public station production without a video backend."""

import argparse
import json
import os
from pathlib import Path
import subprocess

from run_compact_public_replays import validate_assets
from d1_replay_parity import EvidenceError

PROFILES = {"d1": (3, ((0, 213), (1, 214)), 3), "d2": (2, ((5, 50), (14, 233), (16, 263)), 25)}


HISTORICAL_GOLDEN = Path(__file__).with_name("fixtures") / "public_matcen_bc19490fb.json"


def historical_signature(report):
    """Keep ordered gameplay outcomes; never sort births, mappings or triggers.

    Object slots and allocation signatures are process-local identities, affected
    by unrelated object allocation and capture setup. Semantic validation still
    checks their types, uniqueness and light allocation order; repeat runs still
    compare every byte. Cleared authored robot slots/signatures are likewise
    incidental; retain their robot types and order to disclose the intervention.
    """
    def births(items):
        return [{key: value for key, value in birth.items() if key not in ("object", "signature")}
                for birth in items]

    def cleared(row):
        return [robot[1] for robot in row["cleared_authored_robots"]]

    cases = []
    for row in report["cases"]:
        case = {key: value for key, value in row.items() if key not in ("births", "cleared_authored_robots")}
        case.update(births=births(row["births"]), cleared_robot_types=cleared(row))
        cases.append(case)
    dispatch = []
    for row in report["dispatch_cases"]:
        case = {key: value for key, value in row.items()
                if key not in ("births", "activation_lights", "mapping", "authored_triggers", "cleared_authored_robots")}
        case.update(births=births(row["births"]),
                    activation_light_segments=[light["segment"] for light in row["activation_lights"]],
                    cleared_robot_types=cleared(row))
        dispatch.append(case)
    # validate_dispatch_cases requires identical mappings and authored triggers
    # across all five scenarios, so store these repeated records only once.
    return dict(cases=cases, dispatch_cases=dispatch,
                mapping=report["dispatch_cases"][0]["mapping"],
                authored_triggers=report["dispatch_cases"][0]["authored_triggers"])


def validate_historical_report(report, game):
    """Mandatory independent historical gate, in addition to self-consistency."""
    count = validate_report(report, game)
    golden = json.loads(HISTORICAL_GOLDEN.read_text())
    if golden["schema"] != 1 or golden["provenance"]["gameplay_revision"] != "bc19490fb58d0db5f6ba0fd440db33b59b9861fe":
        raise EvidenceError("Invalid historical public matcen golden provenance")
    actual, expected = historical_signature(report), golden["games"][game]
    for section in ("mapping", "authored_triggers", "cases", "dispatch_cases"):
        if actual[section] != expected[section]:
            raise EvidenceError(f"Historical public matcen {game} {section} differs from bc19490fb")
    return count


def validate_dispatch_cases(rows, game):
    _, stations, _ = PROFILES[game]
    scenarios = ("wall_dispatch", "sub", "reverse", "no_dispatch", "empty_mask")
    if not isinstance(rows, list) or [row.get("scenario") for row in rows] != list(scenarios):
        raise EvidenceError("Missing or reordered actual trigger dispatch witnesses")
    baseline = None
    for row in rows:
        scenario = row["scenario"]
        if row.get("zeroed_robot_masks") is not (scenario == "empty_mask"):
            raise EvidenceError("Dispatch controls must disclose their exact robot-mask intervention")
        if row.get("synthetic_links") is not True or not row.get("authored_triggers"):
            raise EvidenceError("Dispatch witnesses must disclose synthetic links and preserve authored triggers")
        if any(type(row.get(key)) is not int or row[key] < 0 for key in ("source_segment", "source_side", "trigger")) or row["source_side"] >= 6:
            raise EvidenceError("Dispatch source must identify its actual crossing")
        mapping = row.get("mapping")
        if not isinstance(mapping, list) or len(mapping) != len(stations):
            raise EvidenceError("Dispatch mapping must retain every generated ordinal")
        for center, entry in enumerate(mapping):
            if any(type(entry.get(key)) is not int for key in ("ordinal", "station", "segment")) or (entry["ordinal"], entry["station"], entry["segment"]) != (center, *stations[center]):
                raise EvidenceError("Dispatch mapping changed generated ordinal, station or segment")
            flags = entry.get("robot_flags")
            if not isinstance(flags, list) or len(flags) != (1 if game == "d1" else 2) or any(type(word) is not int or not 0 <= word <= 0xffffffff for word in flags) or not any(flags):
                raise EvidenceError("Dispatch witness needs the original nonempty robot mask")
        if baseline is None:
            baseline = row
        elif mapping != baseline["mapping"] or row["authored_triggers"] != baseline["authored_triggers"]:
            raise EvidenceError("Fresh dispatch scenarios must preserve identical loaded mappings and authored links")
        cleared = row.get("cleared_authored_robots")
        if not isinstance(cleared, list) or any(not isinstance(value, list) or len(value) != 3 or any(type(item) is not int or item < 0 for item in value) for value in cleared):
            raise EvidenceError("Dispatch fixture must disclose cleared authored robots")
        first, second = (0, len(stations) - 1) if scenario == "reverse" else (len(stations) - 1, 0)
        links = [stations[center][1] for center in (first, second, first)]
        if row.get("links") != links or any(type(value) is not int for value in row["links"]):
            raise EvidenceError("Dispatch links must retain exact target order and duplicate")
        dispatched = scenario != "no_dispatch"
        counts = row.get("activation_counts")
        if counts != [int(dispatched and center in (first, second)) for center in range(len(stations))] or any(type(value) is not int for value in counts):
            raise EvidenceError("Duplicate and untargeted centers must not consume activations")
        lights = row.get("activation_lights")
        if not isinstance(lights, list) or [light.get("segment") for light in lights] != (links[:2] if dispatched else []):
            raise EvidenceError("Native activation lights must witness actual ordered unique dispatch")
        if any(set(light) != {"signature", "segment"} or any(type(value) is not int or value < 0 for value in light.values()) for light in lights) or [light["signature"] for light in lights] != sorted({light["signature"] for light in lights}):
            raise EvidenceError("Activation lights need distinct native signatures in allocation order")
        births = row.get("births")
        if not isinstance(births, list):
            raise EvidenceError("Dispatch production evidence is missing")
        for birth in births:
            if set(birth) != {"frame", "object", "signature", "id", "shields", "station", "segment", "ordinal"} or any(type(value) is not int for value in birth.values()) or not 0 <= birth["frame"] < 2400 or min(birth.values()) < 0 or birth["shields"] <= 0:
                raise EvidenceError("Dispatch births need exact typed origin, identity and frame")
            center = birth["ordinal"]
            if center not in (first, second) or (birth["station"], birth["segment"]) != stations[center]:
                raise EvidenceError("Dispatch birth was remapped to the wrong station or segment")
            robot = birth["id"]
            flags = mapping[center]["robot_flags"]
            if robot >= 32 * len(flags) or not flags[robot // 32] & (1 << (robot % 32)):
                raise EvidenceError("Dispatch robot is outside its legacy ordinal mask")
        if len({birth["signature"] for birth in births}) != len(births) or [birth["frame"] for birth in births] != sorted(birth["frame"] for birth in births):
            raise EvidenceError("Dispatch births must retain unique signatures and frame order")
        for center in (first, second):
            production = [birth for birth in births if birth["ordinal"] == center]
            if scenario in ("no_dispatch", "empty_mask"):
                if production:
                    raise EvidenceError("No-dispatch and empty-mask controls must produce no robots")
            elif not 1 < len(production) <= 5:
                raise EvidenceError("Each dispatched station must produce multiple actual robots")
            if [birth["frame"] for birth in production] != sorted({birth["frame"] for birth in production}):
                raise EvidenceError("Each station needs strictly ordered production frames")
        if scenario == "sub":
            normalize = lambda items: [{key: value for key, value in birth.items() if key not in ("object", "signature")} for birth in items]
            if normalize(births) != normalize(baseline["births"]):
                raise EvidenceError("Crossing and direct trigger dispatch disagree on native outcomes")


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
    validate_dispatch_cases(report.get("dispatch_cases"), game)
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
            count = validate_historical_report(json.loads(raw), game)
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
            {"game": game, "cases": count, "dispatch_cases": 5, "identical_repeats": 2, "historical_revision": "bc19490fb", "historical_match": True, "without_activation_rejected": True}
        )
    (args.output / "summary.json").write_text(json.dumps(summary, indent=4) + "\n")
    print("PASS: actual public matcen production, ordered trigger dispatch, mask controls, policy suppression, energy centers and omitted-activation controls")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("exe-d1", "exe-d2", "data-d1", "data-d2", "output"):
        parser.add_argument("--" + name, required=True, type=lambda value: Path(value).resolve())
    parser.add_argument("--timeout", type=int, default=120)
    run(parser.parse_args())


if __name__ == "__main__":
    main()

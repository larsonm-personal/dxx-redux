"""Measure an exact mission descriptor using an observed build of original D2X-XL.

Use a writable private installation made from copies. Original D2X-XL creates
configuration and cache files. On Windows acknowledge first-run optional asset
warnings for that installation. On headless Linux run under Xvfb.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

from prepare_xl_campaign_reference import REVISION
from xl_readiness import levels


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def classify(code, events, descriptor, advertised, ordinal):
    def values(key):
        return [fields[1:] for fields in events if fields[0] == key]

    if values("initialized") != [[]]:
        return "reference_startup_unverified"
    if code == 20 and values("listed") == [["0"]]:
        return "descriptor_not_listed"
    opened = values("descriptor")
    if len(opened) != 1 or len(opened[0]) != 1 or Path(opened[0][0]).resolve() != descriptor:
        return "descriptor_identity_unverified"
    if code == 21 and values("mission") == [["0"]]:
        return "mission_rejected"
    if values("listed") != [["1"]] or values("mission") != [["1"]]:
        return "mission_unverified"
    if ordinal == 0:
        return "mission_opened" if code == 0 else "lookup_unverified"
    requested = str(ordinal)
    expected = advertised.casefold()
    member = [v for v in values("member") if len(v) == 3 and v[0] == requested]
    if len(member) != 1 or member[0][1].casefold() != expected:
        return "advertised_filename_differs"
    start = values("starting")
    if len(start) != 1 or len(start[0]) != 2 or start[0][0] != requested or start[0][1].casefold() != expected:
        return "level_start_unverified"
    if code == 0 and values("started") == [["1", requested]] and values("frames") == [["120", requested, "0"]]:
        return "started_and_ran"
    if member[0][2] == "0":
        return "advertised_member_lookup_empty"
    return {22: "level_start_rejected", 23: "simulation_stopped", 25: "game_loop_ended"}.get(
        code, "simulation_unverified"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument(
        "--installation", required=True, type=Path, help="Writable private copy with data/ and missions/"
    )
    parser.add_argument("--descriptor", required=True, type=Path, help="Path below missions/, including the extension")
    parser.add_argument(
        "--level", type=int, required=True, help="Signed advertised ordinal, or zero for mission lookup"
    )
    parser.add_argument("--work", required=True, type=Path, help="New output directory")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--wall-bitmap", type=int, help="Read back every displayed frame of this original bitmap")
    args = parser.parse_args()
    if args.wall_bitmap is not None and (args.wall_bitmap <= 0 or not args.level):
        parser.error("A positive wall bitmap needs a gameplay ordinal")
    engine, install, work = (p.resolve() for p in (args.engine, args.installation, args.work))
    missions = install / "missions"
    descriptor = (missions / args.descriptor).resolve()
    if (
        not descriptor.is_relative_to(missions)
        or not descriptor.is_file()
        or descriptor.suffix.lower() not in (".msn", ".mn2")
    ):
        parser.error("Expected an existing mission descriptor beneath missions/")
    if not engine.is_file() or not (install / "data/descent2.hog").is_file() or args.timeout <= 0 or work.exists():
        parser.error("Check reference executable, private data, timeout and fresh work directory")
    case = next((row for row in levels(descriptor) if row["level"] == args.level), None)
    if args.level and not case:
        parser.error("Ordinal is absent from the specified descriptor")
    work.mkdir(parents=True)
    user, cache = install / "user", install / "cache"
    user.mkdir(exist_ok=True)
    cache.mkdir(exist_ok=True)
    receipt = work / "reference.tsv"
    parent = descriptor.parent.relative_to(missions).as_posix()
    env = dict(
        os.environ,
        XL_REFERENCE_DESCRIPTOR=descriptor.name,
        XL_REFERENCE_FOLDER="" if parent == "." else parent + "/",
        XL_REFERENCE_LEVEL=str(args.level),
        XL_REFERENCE_OUT=str(receipt),
    )
    env["XL_REFERENCE_WALL_BITMAP"] = str(args.wall_bitmap or 0)
    command = [
        engine,
        "-datadir",
        install.as_posix(),
        "-userdir",
        user.as_posix(),
        "-cachedir",
        cache.as_posix(),
        "-fullscreen",
        "0",
        "-nosound",
        "-nomovies",
        "1",
        "-briefings",
        "0",
        "-progress_bars",
        "0",
    ]
    with (work / "console.log").open("wb") as log:
        try:
            code = subprocess.run(
                command, cwd=install, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout
            ).returncode
        except subprocess.TimeoutExpired:
            code = None
    events = (
        [line.split("\t") for line in receipt.read_text(encoding="utf-8", errors="replace").splitlines()]
        if receipt.exists()
        else []
    )
    advertised = case["level_file"] if case else ""
    outcome = classify(code, events, descriptor, advertised, args.level)
    result = dict(
        schema="xl-original-campaign-probe-v1",
        reference_revision=REVISION,
        engine_sha256=digest(engine),
        descriptor=args.descriptor.as_posix(),
        descriptor_sha256=digest(descriptor),
        ordinal=args.level,
        advertised_file=advertised,
        exit_code=code,
        observation=outcome,
        timed_out=code is None,
        receipt_sha256=digest(receipt) if receipt.exists() else None,
    )
    if args.wall_bitmap:
        samples = [list(map(int, event[1:])) for event in events if event[0] == "wall_texture"]
        complete = (
            bool(samples)
            and all(
                len(row) == 6 and row[0] == args.wall_bitmap and row[2] == len(samples) and row[3] > 0 and row[4] > 0
                for row in samples
            )
            and sorted(row[1] for row in samples) == list(range(len(samples)))
        )
        result["wall_texture"] = {"bitmap": args.wall_bitmap, "complete": complete, "samples": samples}
    (work / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(outcome)
    return 0 if outcome in ("started_and_ran", "mission_opened") and (not args.wall_bitmap or complete) else 1


if __name__ == "__main__":
    raise SystemExit(main())

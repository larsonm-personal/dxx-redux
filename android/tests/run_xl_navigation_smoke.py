"""Bounded original-XL assisted navigation; never a campaign playability claim."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time

from xl_private_assets import checked, expected_files, manifest

# Qualified original levels only, grouped by mission so large campaigns do not
# dominate sampling. The selection seed does not change the engine's seed 1
POOL = {
    "d2-xl": {
        "directory": "missions/single",
        "levels": [(1, "d2leva-1.rl2", 15, 33, 20), (3, "d2leva-3.rl2", 15, 33, 20)],
    },
    "obsrv-xl": {"directory": "missions/kruel", "levels": [(1, "obsrv-xl.rl2", 17, 36, 20)]},
    "neptunxl": {"directory": "missions/spotlight", "levels": [(1, "neptunxl.rl2", 23, 40, 20)]},
}


def select(seed):
    def pick(label, values):
        digest = hashlib.sha256(f"xl-navigation-v1:{seed}:{label}".encode("ascii")).digest()
        return values[int.from_bytes(digest, "big") % len(values)]

    mission = pick("mission", sorted(POOL))
    return mission, pick(mission, POOL[mission]["levels"])


def validate(route, proof, mission, level):
    ordinal, filename, version, game, target = level
    if (
        route.get("schema") != "dxx-guidebot-route-confirmation-v1"
        or route.get("status") != "confirmed"
        or route.get("mission") != mission
        or route.get("level") != ordinal
        or route.get("seed") != 1
        or route.get("fixed_hz") != 60
        or not 0 < route.get("frames", 0) <= 30 * 60
        or route.get("verification_goal") != {"kind": "segment", "segment": target}
        or not any(row.get("kind") == "unexplored" for row in route.get("objectives", []))
        or route.get("last_actor_state", {}).get("segment") != target
        or proof.get("mission") != mission
        or proof.get("level") != ordinal
        or proof.get("level_file", "").lower() != filename
        or proof.get("xl_active") is not True
        or (proof.get("level_version"), proof.get("game_version")) != (version, game)
        or proof.get("segments", 0) < 3
        or proof.get("visited_segments", 0) < 3
    ):
        raise ValueError("XL smoke did not prove the requested original load and multi-segment route")


def run(engine, stage, data, work, seed=318):
    engine, stage, data, work = (Path(p).resolve() for p in (engine, stage, data, work))
    identities = expected_files(manifest(stage / "manifest.json"))
    mission, level = select(seed)
    work.mkdir(parents=True, exist_ok=False)
    (work / "user").mkdir()
    (work / "missions").mkdir()
    sources = []
    for suffix in (".hog", ".mn2"):
        relative = POOL[mission]["directory"] + "/" + mission + suffix
        size, digest = identities[relative]
        source = stage / relative
        checked(source, {"size": size, "sha256": digest})
        shutil.copyfile(source, work / "missions" / source.name)
        sources.append((source, size, digest))
    # This and raw engine output belong to private logs, not public artifacts
    selection = {"selection_seed": seed, "mission": mission, "level": level[0], "level_file": level[1]}
    (work / "selection.json").write_text(json.dumps(selection, indent=2) + "\n")
    started = time.monotonic()
    with (work / "engine.log").open("wb") as log:
        result = subprocess.run(
            [
                str(engine),
                "-mission",
                mission,
                "-level",
                str(level[0]),
                "-hogdir",
                str(data),
                "-extra-dir",
                str(work),
                "-route-confirm-user-dir",
                str(work / "user"),
                "-route-confirm-segment",
                str(level[4]),
                "-route-confirm-timeout-seconds",
                "30",
                "-route-confirm-json-out",
                str(work / "route.json"),
                "-route-confirm-load-proof",
                str(work / "load.json"),
            ],
            cwd=work,
            stdout=log,
            stderr=subprocess.STDOUT,
            timeout=15,
            check=False,
        )
    if result.returncode:
        raise ValueError(f"XL smoke failed (exit {result.returncode}); see private selection.json and engine.log")
    validate(
        json.loads((work / "route.json").read_text()), json.loads((work / "load.json").read_text()), mission, level
    )
    for source, size, digest in sources:
        checked(source, {"size": size, "sha256": digest})
    return {"passed": 1, "selection_seed": seed, "seconds": round(time.monotonic() - started, 3)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("engine", "stage", "data", "work"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--seed", type=int, default=318)
    args = parser.parse_args()
    print(json.dumps(run(args.engine, args.stage, args.data, args.work, args.seed), sort_keys=True))

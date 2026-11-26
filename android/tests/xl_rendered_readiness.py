"""Supplemental rendered load/tick probe; never publishes official readiness."""

import argparse
import json
import os
from pathlib import Path
import subprocess

from xl_readiness import digest, level_file_matches


def entered(proof, mission, level, level_file, staged_level_sha256=None):
    return (
        isinstance(proof, dict)
        and proof.get("schema") == "dxx-xl-rendered-load-tick-v1"
        and proof.get("phase") in ("loaded", "simulation_failed", "rendering_failed", "loaded_ticked")
        and isinstance(proof.get("mission"), str)
        and proof["mission"].casefold() == mission.casefold()
        and type(proof.get("level")) is int
        and proof["level"] == level
        and level_file_matches(proof, {"level_file": level_file.lower()}, staged_level_sha256)
        and type(proof.get("segments")) is int
        and proof["segments"] > 0
        and all(type(proof.get(k)) is int and proof[k] == v for k, v in (("fixed_hz", 60), ("seed", 1)))
        and type(proof.get("difficulty", 2)) is int
        and proof.get("difficulty", 2) == 2
        and isinstance(proof.get("renderer"), str)
        and bool(proof["renderer"].strip())
        and type(proof.get("xl_active")) is bool
        and proof.get("assisted") is False
    )


def passed(code, proof, mission, level, level_file, staged_level_sha256=None):
    return (
        code == 0
        and entered(proof, mission, level, level_file, staged_level_sha256)
        and proof["phase"] == "loaded_ticked"
        and all(type(proof.get(k)) is int and proof[k] == 120 for k in ("frames", "rendered_frames"))
        and "gl_error" not in proof
    )


def run_probe(engine, data, extra, work, mission, level, level_file, engine_sha256, *, timeout=45, quiet=False):
    work = work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    (work / "user").mkdir()
    proof_path = work / "load.json"
    engine = engine.resolve()
    command = [
        str(engine),
        "-window",
        "-mission",
        mission,
        "-level",
        str(level),
        "-hogdir",
        str(data.resolve()),
        "-extra-dir",
        str(extra.resolve()),
        "-route-confirm-user-dir",
        str(work / "user"),
        "-route-confirm-json-out",
        str(work / "unused-route.json"),
        "-xl-readiness-proof",
        str(proof_path),
    ]
    if quiet:
        command.append("-nosound")
    with (work / "engine.log").open("wb") as log:
        try:
            code = subprocess.run(
                command,
                cwd=work,
                env=os.environ.copy(),
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=timeout,
                check=False,
            ).returncode
        except subprocess.TimeoutExpired:
            code = None
    proof = json.loads(proof_path.read_text()) if proof_path.exists() else {}
    if not isinstance(proof, dict):
        raise ValueError("Windowed receipt must be an object")
    loose_hash = None
    resolved = proof.get("level_file")
    if isinstance(resolved, str) and resolved.replace("\\", "/").casefold() == "missions/" + level_file.casefold():
        loose = extra / "missions" / Path(resolved.replace("\\", "/")).name
        if loose.is_file():
            loose_hash = digest(loose)
    success = passed(code, proof, mission, level, level_file, loose_hash)
    result = {
        "schema": "supplemental-rendered-xl-probe-v1",
        "passed": success,
        "entered_gameplay": entered(proof, mission, level, level_file, loose_hash),
        **({"staged_level_sha256": loose_hash} if loose_hash else {}),
        "exit_code": code,
        "proof": proof,
        "official_readiness_status_updated": False,
        "engine_sha256": engine_sha256,
    }
    (work / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("engine", "data", "extra", "work"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--mission", required=True)
    parser.add_argument("--level", type=int, required=True)
    parser.add_argument("--level-file", required=True)
    args = parser.parse_args()
    result = run_probe(
        args.engine, args.data, args.extra, args.work, args.mission, args.level, args.level_file, digest(args.engine)
    )
    print(
        json.dumps(
            {"passed": result["passed"], "phase": result["proof"].get("phase"), "exit_code": result["exit_code"]}
        )
    )
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

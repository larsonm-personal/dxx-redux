"""Supplemental rendered load/tick probe; never publishes official readiness."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def passed(code, proof, mission, level, level_file):
    return (
        code == 0
        and isinstance(proof, dict)
        and proof.get("schema") == "dxx-xl-rendered-load-tick-v1"
        and proof.get("phase") == "loaded_ticked"
        and proof.get("mission") == mission
        and type(proof.get("level")) is int
        and proof["level"] == level
        and all(
            type(proof.get(k)) is int and proof[k] == v
            for k, v in (("frames", 120), ("rendered_frames", 120), ("fixed_hz", 60), ("seed", 1))
        )
        and type(proof.get("segments")) is int
        and proof["segments"] > 0
        and isinstance(proof.get("level_file"), str)
        and proof["level_file"].lower() == level_file.lower()
        and isinstance(proof.get("renderer"), str)
        and bool(proof["renderer"].strip())
        and type(proof.get("xl_active")) is bool
        and proof.get("assisted") is False
        and "gl_error" not in proof
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("engine", "data", "extra", "work"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--mission", required=True)
    parser.add_argument("--level", type=int, required=True)
    parser.add_argument("--level-file", required=True)
    args = parser.parse_args()
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    (work / "user").mkdir()
    proof_path = work / "load.json"
    engine = args.engine.resolve()
    command = [
        str(engine),
        "-mission",
        args.mission,
        "-level",
        str(args.level),
        "-hogdir",
        str(args.data.resolve()),
        "-extra-dir",
        str(args.extra.resolve()),
        "-route-confirm-user-dir",
        str(work / "user"),
        "-route-confirm-json-out",
        str(work / "unused-route.json"),
        "-xl-readiness-proof",
        str(proof_path),
    ]
    with (work / "engine.log").open("wb") as log:
        try:
            code = subprocess.run(
                command, cwd=work, env=os.environ.copy(), stdout=log, stderr=subprocess.STDOUT, timeout=45, check=False
            ).returncode
        except subprocess.TimeoutExpired:
            code = None
    proof = json.loads(proof_path.read_text()) if proof_path.exists() else {}
    success = passed(code, proof, args.mission, args.level, args.level_file)
    result = {
        "schema": "supplemental-rendered-xl-probe-v1",
        "passed": success,
        "exit_code": code,
        "proof": proof,
        "official_readiness_status_updated": False,
        "engine_sha256": hashlib.sha256(engine.read_bytes()).hexdigest(),
    }
    (work / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"passed": success, "phase": proof.get("phase"), "exit_code": code}))
    return 0 if success else 1


if __name__ == "__main__":
    raise SystemExit(main())

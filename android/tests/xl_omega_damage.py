"""Assisted actual Omega target/charge and fail-closed replay controls, not readiness."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from xl_render_clock import REVISION, verify_reference


def passed(code, proof, mission, level, xl):
    del level  # This fixture reports mission and controlled rate, not level progression.
    if type(code) is not int or code != 0 or not isinstance(proof, dict):
        return False
    if set(proof) != {"schema", "assisted", "complete", "cases", "renderer", "mission", "input_policy"}:
        return False
    if (
        proof["schema"] != "xl-omega-damage-v1"
        or proof["assisted"] is not True
        or proof["complete"] is not True
        or proof["mission"] != mission
        or not isinstance(proof["renderer"], str)
        or not proof["renderer"].strip()
    ):
        return False
    expected_policy = {
        "omega_free_recording": True,
        "xl": xl,
        "rejected_paths": ["headless", "rendered", "paused"] if xl else [],
    }
    expected_policy["acquired_recording_rejected" if xl else "classic_recording_allowed"] = True
    if proof["input_policy"] != expected_policy or any(
        proof["input_policy"].get(key) is not value for key, value in expected_policy.items() if isinstance(value, bool)
    ):
        return False
    rows = proof["cases"]
    if not isinstance(rows, list) or len(rows) != 8:
        return False
    keys = {
        "hz",
        "multiplayer",
        "xl",
        "shots",
        "charge_used",
        "target_damage",
        "charge_remaining",
        "target_remaining",
        "strength",
    }
    for row, (multi, hz) in zip(rows, ((m, h) for m in (False, True) for h in (20, 30, 60, 120))):
        if (
            not isinstance(row, dict)
            or set(row) != keys
            or any(type(row[k]) is not int for k in keys - {"multiplayer", "xl"})
        ):
            return False
        active = xl and not multi
        if row["multiplayer"] is not multi or row["xl"] is not active or row["hz"] != hz or row["strength"] <= 0:
            return False
        shots = hz if active else (15 if hz == 30 else 20)
        charge = 65536 if active else shots * 3276
        if active:
            frames = [((i + 1) * 65536 // hz) - (i * 65536 // hz) for i in range(hz)]
            damage = sum((32 * frame * row["strength"]) >> 16 for frame in frames)
        else:
            damage = shots * ((32 * 3276 * row["strength"]) >> 16)
        if (
            row["shots"],
            row["charge_used"],
            row["target_damage"],
            row["charge_remaining"],
            row["target_remaining"],
        ) != (shots, charge, damage, 65536 - charge, 10000 * 65536 - damage):
            return False
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("engine", "data", "extra", "work", "reference-root"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--mission", required=True)
    parser.add_argument("--level", required=True, type=int)
    parser.add_argument("--xl", action="store_true")
    args = parser.parse_args()
    pins = verify_reference(args.reference_root)
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    (work / "user").mkdir()
    proof_path = work / "proof.json"
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
        "-xl-omega-damage-proof",
        str(proof_path),
        "-nosound",
        "-nomusic",
    ]
    with (work / "engine.log").open("wb") as log:
        try:
            code = subprocess.run(
                command, cwd=work, stdout=log, stderr=subprocess.STDOUT, timeout=45, check=False
            ).returncode
        except subprocess.TimeoutExpired:
            code = None
    proof = json.loads(proof_path.read_text()) if proof_path.exists() else {}
    success = passed(code, proof, args.mission, args.level, args.xl)
    result = {
        "passed": success,
        "exit_code": code,
        "proof": proof,
        "reference_revision": REVISION,
        "reference_hashes": pins,
        "engine_sha256": hashlib.sha256(engine.read_bytes()).hexdigest(),
        "official_readiness_status_updated": False,
    }
    (work / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"passed": success, "exit_code": code, "cases": len(proof.get("cases", []))}))
    return 0 if success else 1


if __name__ == "__main__":
    raise SystemExit(main())

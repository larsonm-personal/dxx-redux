"""Group a completed full-survey run by its first recorded blocker."""

import argparse
from collections import Counter
import json
from pathlib import Path

import xl_readiness as readiness
from xl_readiness_report import validate_rows


# Only these labels leave the private run directory. One case can encounter
# a different blocker once the first one is repaired.
FAMILIES = {
    "Unsupported level format": ("unsupported level version", "unsupported level/game version pair"),
    "Mission archive lookup": ("cannot read mission archive", "cannot activate mission archive"),
    "Advertised level file missing": ("level file is missing",),
    "Mission selection": ("could not load mission", "outside mission range"),
    "Absent trigger section": ("no trigger section to inspect",),
    "Robot contents": ("unknown contained object dependency",),
    "Robot definitions": ("missing robot definition", "mission declares custom robots", "hxm definitions"),
    "Light index bounds": ("invalid packed light index", "invalid light index"),
    "Texture IDs": ("texture_id at",),
    "Classic level in XL mission": ("stock-format levels",),
    "Companion lighting": ("clr companion",),
    "Animated replacement textures": ("clocked animation frames", "qualified wall effect clock"),
    "Replacement bitmap directory": ("pog bitmap directory",),
    "Texture memory budget": ("scoped texture decoded pixels",),
    "OOF model decoding": ("unexpected bytes in oof",),
    "ASE model rendering": ("ase conditional submodel",),
    "Custom base definitions": ("overrides base ham",),
    "Segment functions": ("segment_function", "invalid station type"),
    "Reactor triggers": ("invalid reactor",),
    "Trigger targets": ("invalid trigger target",),
    "Object trigger actions": ("object triggers at record",),
    "Wall types": ("wall_type at",),
    "Object placement": ("invalid object segment",),
    "Non-hexahedral geometry": ("segment_geometry",),
    "Multiplayer-only objects": ("multiplayer_only_object",),
    "Fluid behavior": ("water physics",),
    "Lightning effects": ("effect_lightning",),
}


def first_blocker(row, proof, log):
    outcome = row["outcome"]
    if outcome == "simulation_failed":
        if proof.get("phase") == "loaded_ticked" and proof.get("frames") == 120:
            if str(proof.get("level_file", "")).casefold() != row["level_file"].casefold():
                return "Resolved level filename differs"
            return "Completed frames with invalid readiness proof"
        return "Simulation stopped"
    if outcome in ("timeout", "crash", "infrastructure_error", "untested"):
        return outcome
    for line in log.splitlines():
        folded = line.casefold()
        for group, matches in FAMILIES.items():
            if any(phrase in folded for phrase in matches):
                return group
    return "Unsupported level format" if outcome == "unsupported_format" else "Unclassified load failure"


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def summarize(work, catalog_path):
    catalog = read(catalog_path)
    identity = read(work / "survey.json")
    result = read(work / "results.json")
    if (
        identity["schema"] != "dxx-xl-full-survey-v1"
        or identity["inventory_sha256"] != readiness.digest(catalog_path)
        or identity["asset_commit"] != catalog["asset_commit"]
        or identity["resources"] != readiness.resource_identity()
        or any(result.get(key) != value for key, value in identity.items())
        or len(set(identity["selected"])) != len(identity["selected"])
    ):
        raise ValueError("Survey identity mismatch")
    cases = {case["id"]: case for case in catalog["cases"]}
    rows = result["rows"]
    summary = validate_rows(rows, {**catalog, "selected": identity["selected"]})
    failures = [row for row in rows if row["outcome"] != "loaded_ticked"]
    if result["summary"] != summary or failures != read(work / "failures.json"):
        raise ValueError("Survey summary or failure list differs from measured rows")
    reported = []
    for index, row in enumerate(rows):
        directory = work / str(index)
        if row != read(directory / "result.json"):
            raise ValueError(f"Case receipt differs at index {index}")
        proof_path = directory / "load.json"
        proof = read(proof_path) if proof_path.exists() else {}
        if row["outcome"] == "loaded_ticked":
            if readiness.classify(0, proof, "", cases[row["id"]]) != "loaded_ticked":
                raise ValueError(f"Missing 120-frame proof at index {index}")
            continue
        path = directory / "engine.log"
        log = path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""
        reported.append(
            {
                "id": row["id"],
                "level_file": row["level_file"],
                "outcome": row["outcome"],
                "frames": row["frames"],
                "first_blocker": first_blocker(row, proof, log),
            }
        )
    return {
        "target_sha": identity["target_sha"],
        "engine_sha256": identity["engine_sha256"],
        "inventory_sha256": identity["inventory_sha256"],
        "summary": summary,
        "groups": dict(Counter(row["first_blocker"] for row in reported).most_common()),
        "failures": reported,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Output already exists")
    readiness.write(args.output, summarize(args.work, args.catalog))

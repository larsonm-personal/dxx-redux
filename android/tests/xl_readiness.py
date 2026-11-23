"""Fixed uploaded-asset survey. Load/tick readiness is not campaign playability."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import time

from nightly_corpus_stage import extract_archive, member_path, reject_links

ASSET_COMMIT = "9dad21bd65ba05665ac6fe315f37fe7461e85725"
RESOURCE_COMMIT = "dc6565f6b5400d7c700fa8378e766b2354b6ceb6"
RESOURCE_FILES = (
    (
        "descent_2_vertigo/d2x.hog",
        "d2x.hog",
        4371791,
        "97a4433833ec1f2fbdd2becc144a1e9726ca78b1210a71a49ddbec6182665fe6",
    ),
    (
        "xl-resources/d2x-xl.hog",
        "d2x-xl.hog",
        3815963,
        "e7351d66d534c3fdb400f03b91b771ad2d0c0d2b0100cf48718d504008ada7e6",
    ),
)
NEW_CAMPAIGNS = {"BoilingPoint-XL.zip", "dinter-1.5.7z", "sphere-1.6.zip", "wshock20.7z"}
SEED = "d2x-readiness-cohort-v1"
OUTCOMES = {
    "loaded_ticked",
    "unsupported_format",
    "load_failed",
    "simulation_failed",
    "crash",
    "timeout",
    "infrastructure_error",
    "untested",
}


def write(path, value):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def resource_identity():
    return {
        "profile": "authentic-vertigo-xl-v1",
        "repository": "larsonm-personal/dxx-redux-test-assets",
        "commit": RESOURCE_COMMIT,
        "files": [dict(source=source, path=name, size=size, sha256=sha) for source, name, size, sha in RESOURCE_FILES],
    }


def verify_resources(root, source_paths=False):
    for source, name, size, sha in RESOURCE_FILES:
        path = Path(root) / (source if source_paths else name)
        if path.is_symlink() or path.stat().st_size != size or digest(path) != sha:
            raise ValueError("Readiness resource identity mismatch: " + name)
    return resource_identity()


def stage_resources(source, runtime):
    verify_resources(source, source_paths=True)
    for relative, name, _, _ in RESOURCE_FILES:
        target = Path(runtime) / name
        if target.exists():
            raise ValueError("Readiness resource staging collision: " + name)
        shutil.copyfile(Path(source) / relative, target)
    return verify_resources(runtime)


def rank(value):
    return hashlib.sha256((SEED + ":" + value).encode()).hexdigest()


def levels(descriptor):
    """Enumerate advertised entries only; do not decide engine support here."""
    lines = [
        line.split(";", 1)[0].strip()
        for line in descriptor.read_bytes().decode("latin1").replace("\x1a", "").splitlines()
    ]
    rows = []
    for index, line in enumerate(lines):
        key, separator, count = line.partition("=")
        if separator and key.strip().lower() in ("num_levels", "num_secrets"):
            count = int(count.strip())
            if not 0 <= count <= 1000 or index + count >= len(lines):
                raise ValueError("Invalid advertised level count")
            sign = 1 if key.strip().lower() == "num_levels" else -1
            for ordinal, entry in enumerate(lines[index + 1 : index + count + 1], 1):
                name = entry.split(",", 1)[0].strip()
                if len(member_path(name).parts) != 1 or Path(name).suffix.lower() not in (
                    ".rl2",
                    ".rdl",
                    ".sl2",
                    ".sdl",
                ):
                    raise ValueError("Invalid advertised level name")
                rows.append({"level": ordinal * sign, "level_file": name.lower()})
    if not rows or len({r["level"] for r in rows}) != len(rows):
        raise ValueError("Missing or duplicate advertised levels")
    return rows


def inventory(downloads, tree, work, seven_zip):
    archives, cases = [], []
    for source in sorted(tree, key=lambda r: r["path"]):
        if not source["path"].endswith((".zip", ".7z")):
            continue
        name = Path(source["path"]).name
        path = Path(downloads) / name
        payload = path.read_bytes()
        blob = hashlib.sha1(f"blob {len(payload)}\0".encode() + payload).hexdigest()
        if len(payload) != source["size"] or blob != source["sha"]:
            raise ValueError("Archive does not match pinned Git tree: " + name)
        record = {"path": source["path"], "size": len(payload), "sha256": hashlib.sha256(payload).hexdigest()}
        archives.append(record)
        root = Path(work) / name
        extract_archive(path, root, seven_zip)
        descriptors = sorted(p for p in root.rglob("*") if p.suffix.lower() in (".mn2", ".msn") and p.is_file())
        if not descriptors:
            raise ValueError("Archive has no advertised mission inventory: " + name)
        for descriptor in descriptors:
            relative = descriptor.relative_to(root).as_posix()
            for row in levels(descriptor):
                identity = f"{name}/{relative}:{row['level']}"
                cases.append(
                    {
                        **row,
                        "id": identity,
                        "archive": source["path"],
                        "descriptor": relative,
                        "descriptor_sha256": digest(descriptor),
                        "mission": descriptor.stem.lower(),
                    }
                )
    # One fixed mission-first sample per archive, four new campaigns plus four
    # additional archives selected by the published seed, never by outcomes.
    names = {Path(a["path"]).name for a in archives}
    if not NEW_CAMPAIGNS <= names:
        raise ValueError("New campaigns missing from inventory")
    selected_archives = NEW_CAMPAIGNS | set(sorted(names - NEW_CAMPAIGNS, key=rank)[:4])
    selected = []
    for name in sorted(selected_archives):
        candidates = [r for r in cases if Path(r["archive"]).name == name]
        mission = min({r["descriptor"] for r in candidates}, key=lambda d: rank(name + ":" + d))
        selected.append(min((r for r in candidates if r["descriptor"] == mission), key=lambda r: rank(r["id"]))["id"])
    return {
        "schema": "dxx-xl-readiness-inventory-v1",
        "asset_commit": ASSET_COMMIT,
        "selection_seed": SEED,
        "scope": "All advertised normal/secret levels in the 23 uploaded archives, including classic/imported and multiplayer content; no engine-admission filtering",
        "archives": archives,
        "cases": cases,
        "selected": selected,
    }


def summary(rows):
    counts = Counter(row["outcome"] for row in rows)
    if not rows or any(key not in OUTCOMES for key in counts):
        raise ValueError("Invalid readiness outcomes")
    incomplete = counts["infrastructure_error"] + counts["untested"]
    passed = counts["loaded_ticked"]
    return {
        "criterion": "Requested original level loads and executes 120 unassisted simulation frames at 60 Hz without death or level transition",
        "denominator": len(rows),
        "passed": passed,
        "counts": dict(sorted(counts.items())),
        "complete": incomplete == 0,
        "readiness_percent": round(100 * passed / len(rows), 1) if not incomplete else None,
    }


def extraction_identity(root):
    identity = {}
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise ValueError("Readiness extraction cache contains a symbolic link")
        name = path.relative_to(root).as_posix()
        if path.is_dir():
            identity[name] = {"directory": True}
        elif path.is_file():
            identity[name] = {"size": path.stat().st_size, "sha256": digest(path)}
        else:
            raise ValueError("Readiness extraction cache contains a nonregular entry")
    return identity


def cached_extraction(source, archive, cache, seven_zip):
    # The supervisor owns a fresh cache outside uploaded evidence for this run only
    entry = Path(cache) / archive["sha256"]
    reject_links(entry)
    if not entry.exists():
        building = entry.with_name(entry.name + ".building")
        building.mkdir()
        extract_archive(source, building / "unpack", seven_zip)
        write(
            building / "identity.json",
            {"archive_sha256": archive["sha256"], "entries": extraction_identity(building / "unpack")},
        )
        if source.stat().st_size != archive["size"] or digest(source) != archive["sha256"]:
            raise ValueError("Pinned archive changed during extraction")
        building.rename(entry)
        return entry / "unpack"
    unpack = entry / "unpack"
    identity = entry / "identity.json"
    if unpack.is_symlink() or identity.is_symlink() or not unpack.is_dir() or not identity.is_file():
        raise ValueError("Incomplete readiness extraction cache")
    expected = json.loads(identity.read_text())
    if expected != {"archive_sha256": archive["sha256"], "entries": extraction_identity(unpack)}:
        raise ValueError("Readiness extraction cache identity changed")
    return unpack


def stage_case(assets, archive, case, work, seven_zip, unpack_cache=None):
    source = Path(assets) / member_path(archive["path"])
    reject_links(source)
    if source.stat().st_size != archive["size"] or digest(source) != archive["sha256"]:
        raise ValueError("Pinned archive identity mismatch")
    if unpack_cache is None:
        unpack = work / "unpack"
        extract_archive(source, unpack, seven_zip)
    else:
        unpack = cached_extraction(source, archive, unpack_cache, seven_zip)
    descriptor = unpack / member_path(case["descriptor"])
    if digest(descriptor) != case["descriptor_sha256"] or {
        "level": case["level"],
        "level_file": case["level_file"],
    } not in levels(descriptor):
        raise ValueError("Selected descriptor identity changed")

    extra = work / "extra"
    # Preserve package data/mods and sibling resources. Only normalize the
    # mission search location; never edit descriptor or HOG bytes.
    package = descriptor.parent
    for parent in descriptor.parents:
        if parent == unpack.parent:
            break
        if parent.name.lower() == "missions":
            package = parent.parent
            break
    shutil.copytree(package, extra)
    missions = extra / "missions"
    missions.mkdir(exist_ok=True)
    for old in list(extra.rglob("*")):
        if old.is_file() and old.suffix.lower() in (".mn2", ".msn"):
            old.unlink()
    for path in sorted(descriptor.parent.iterdir()):
        if not path.is_file() or (path.suffix.lower() in (".mn2", ".msn") and path != descriptor):
            continue
        target = missions / path.name
        if not target.exists():
            shutil.copyfile(path, target)
        elif digest(target) != digest(path):
            raise ValueError("Mission staging collision")
    return extra


def level_file_matches(proof, case, staged_level_sha256=None):
    resolved = proof.get("level_file", "")
    if not isinstance(resolved, str):
        return False
    if resolved.lower() == case["level_file"]:
        return True
    return bool(
        staged_level_sha256
        and resolved.replace("\\", "/").lower() == "missions/" + case["level_file"]
        and proof.get("level_source_sha256") == staged_level_sha256
    )


def classify(returncode, proof, log, case, staged_level_sha256=None):
    if returncode is not None and returncode < 0:
        return "crash"
    if returncode is None:
        return "timeout"
    if returncode == 0 and (
        proof.get("schema") == "dxx-xl-load-tick-v1"
        and proof.get("phase") == "loaded_ticked"
        and isinstance(proof.get("mission"), str)
        and proof["mission"].lower() == case["mission"].lower()
        and proof.get("level") == case["level"]
        and level_file_matches(proof, case, staged_level_sha256)
        and proof.get("frames") == 120
        and proof.get("segments", 0) > 0
        and proof.get("fixed_hz") == 60
        and proof.get("seed") == 1
        and proof.get("assisted") is False
    ):
        return "loaded_ticked"
    if not proof or proof.get("phase") == "initializing":
        return "infrastructure_error"
    if re.search(r"unsupported level(?:/game version pair| version)\b", log, re.I):
        return "unsupported_format"
    if proof.get("phase") in ("loaded", "simulation_failed", "loaded_ticked"):
        return "simulation_failed"
    return "load_failed"


def run(
    engine,
    assets,
    data,
    catalog,
    work,
    seven_zip,
    target_sha,
    require_resources=False,
    assisted_routes=True,
    unpack_cache=None,
):
    work = Path(work)
    work.mkdir(parents=True, exist_ok=False)
    if len(set(catalog["selected"])) != len(catalog["selected"]):
        raise ValueError("Duplicate cohort membership")
    cases = {r["id"]: r for r in catalog["cases"]}
    archives = {r["path"]: r for r in catalog["archives"]}
    rows = [{"id": key, "outcome": "untested", "assisted_navigation": "not_run"} for key in catalog["selected"]]
    report = {
        "schema": "dxx-xl-readiness-v1",
        "target_sha": target_sha,
        "asset_commit": catalog["asset_commit"],
        "selection_seed": catalog["selection_seed"],
        "inventory_levels": len(cases),
        "inventory_archives": len(archives),
        "rows": rows,
        "limitations": "Fixed stratified sample, not a statistical estimate or full playability; no rendering or player input; camera addon subchecks not covered",
    }
    if require_resources:
        report["resources"] = verify_resources(data)

    def checkpoint():
        report["summary"] = summary(rows)
        write(work / "results.json", report)

    checkpoint()
    for index, row in enumerate(rows):
        case = cases[row["id"]]
        row.update({key: case[key] for key in ("mission", "level", "level_file", "archive")})
        area = work / str(index)
        area.mkdir()
        (area / "user").mkdir()
        started = time.monotonic()
        try:
            if case["descriptor"].lower().endswith(".msn") and any(
                not (Path(data) / name).is_file() for name in ("descent.hog", "descent.pig")
            ):
                raise ValueError("Registered D1 test inputs missing")
            extra = stage_case(assets, archives[case["archive"]], case, area, seven_zip, unpack_cache)
            command = [
                str(Path(engine).resolve()),
                "-mission",
                case["mission"],
                "-level",
                str(case["level"]),
                "-hogdir",
                str(Path(data).resolve()),
                "-extra-dir",
                str(extra.resolve()),
                "-route-confirm-user-dir",
                str((area / "user").resolve()),
                "-route-confirm-json-out",
                str((area / "unused-route.json").resolve()),
                "-xl-readiness-proof",
                str((area / "load.json").resolve()),
            ]
            with (area / "engine.log").open("wb") as log:
                try:
                    result = subprocess.run(
                        command, cwd=area, stdout=log, stderr=subprocess.STDOUT, timeout=45, check=False
                    )
                    code = result.returncode
                except subprocess.TimeoutExpired:
                    code = None
            proof = json.loads((area / "load.json").read_text()) if (area / "load.json").exists() else {}
            staged_level_sha256 = None
            resolved = proof.get("level_file")
            if isinstance(resolved, str) and resolved.replace("\\", "/").lower() == "missions/" + case["level_file"]:
                staged_level = extra / "missions" / Path(resolved.replace("\\", "/")).name
                if staged_level.is_file():
                    staged_level_sha256 = digest(staged_level)
                    row["staged_level_sha256"] = staged_level_sha256
            # Raw engine messages remain private; publish only fixed classifications.
            with (area / "engine.log").open("rb") as log:
                log.seek(max(0, (area / "engine.log").stat().st_size - 65536))
                output = log.read(65536).decode("utf-8", errors="replace")
            row["outcome"] = classify(code, proof, output, case, staged_level_sha256)
            row["frames"] = (
                proof.get("frames", 0) if type(proof.get("frames")) is int and 0 <= proof["frames"] <= 120 else 0
            )
            row["xl_active"] = proof.get("xl_active") if type(proof.get("xl_active")) is bool else None
            if row["outcome"] == "loaded_ticked" and assisted_routes:
                target = min(20, proof["segments"] - 1)
                route_path, load_path = area / "route.json", area / "route-load.json"
                route_command = command[:-2] + [
                    "-route-confirm-segment",
                    str(target),
                    "-route-confirm-timeout-seconds",
                    "30",
                    "-route-confirm-load-proof",
                    str(load_path.resolve()),
                ]
                route_command[route_command.index("-route-confirm-json-out") + 1] = str(route_path.resolve())
                with (area / "route.log").open("wb") as log:
                    try:
                        route_run = subprocess.run(
                            route_command, cwd=area, stdout=log, stderr=subprocess.STDOUT, timeout=15, check=False
                        )
                        route = json.loads(route_path.read_text()) if route_path.exists() else {}
                        loaded = json.loads(load_path.read_text()) if load_path.exists() else {}
                        confirmed = (
                            route_run.returncode == 0
                            and route.get("status") == "confirmed"
                            and route.get("mission") == case["mission"]
                            and route.get("level") == case["level"]
                            and route.get("verification_goal") == {"kind": "segment", "segment": target}
                            and route.get("last_actor_state", {}).get("segment") == target
                            and route.get("schema") == "dxx-guidebot-route-confirmation-v1"
                            and route.get("fixed_hz") == 60
                            and route.get("seed") == 1
                            and 0 < route.get("frames", 0) <= 1800
                            and loaded.get("mission") == case["mission"]
                            and loaded.get("level") == case["level"]
                            and level_file_matches(loaded, case, staged_level_sha256)
                            and loaded.get("visited_segments", 0) >= 3
                        )
                        row["assisted_navigation"] = (
                            "confirmed" if confirmed else ("crash" if route_run.returncode < 0 else "unconfirmed")
                        )
                    except subprocess.TimeoutExpired:
                        row["assisted_navigation"] = "timeout"
                    except (OSError, ValueError, TypeError, subprocess.SubprocessError):
                        row["assisted_navigation"] = "infrastructure_error"
                row["navigation_target_segment"] = target
        except (OSError, ValueError, subprocess.SubprocessError):
            row["outcome"] = "infrastructure_error"
        row["seconds"] = round(time.monotonic() - started, 3)
        checkpoint()
    if require_resources:
        verify_resources(data)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("inventory", "prepare", "run"))
    for name in ("downloads", "tree", "work", "catalog", "engine", "assets", "data", "seven-zip"):
        parser.add_argument("--" + name, type=Path)
    parser.add_argument("--target-sha")
    parser.add_argument("--unpack-cache", type=Path, help="Supervisor-owned per-run verified extraction cache")
    parser.add_argument("--no-assisted-routes", action="store_true", help="Measure primary load/tick only")
    parser.add_argument(
        "--resources", type=Path, help="Pinned private checkout containing authentic Vertigo and XL resources"
    )
    parser.add_argument(
        "--require-resources", action="store_true", help="Verify and record the authentic runtime resource profile"
    )
    args = parser.parse_args()
    if args.command == "inventory":
        write(args.catalog, inventory(args.downloads, json.loads(args.tree.read_text()), args.work, args.seven_zip))
    elif args.command == "prepare":
        catalog = json.loads(args.catalog.read_text())
        if catalog["asset_commit"] != ASSET_COMMIT:
            raise ValueError("Unexpected uploaded asset revision")
        cases = {row["id"]: row for row in catalog["cases"]}
        paths = {cases[key]["archive"] for key in catalog["selected"]}
        args.work.mkdir(parents=True, exist_ok=False)
        if args.data:
            runtime = args.work / "runtime"
            runtime.mkdir()
            for game in ("d1", "d2"):
                for source in (args.data / game).iterdir():
                    if source.is_file():
                        shutil.copyfile(source, runtime / source.name)
        if args.resources:
            if not args.data:
                raise ValueError("Readiness resources require staged retail data")
            stage_resources(args.resources, runtime)
        for archive in catalog["archives"]:
            if archive["path"] not in paths:
                continue
            source = args.assets / member_path(archive["path"])
            if source.stat().st_size != archive["size"] or digest(source) != archive["sha256"]:
                raise ValueError("Pinned input identity mismatch")
            target = args.work / member_path(archive["path"])
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
    else:
        report = run(
            args.engine,
            args.assets,
            args.data,
            json.loads(args.catalog.read_text()),
            args.work,
            args.seven_zip,
            args.target_sha,
            args.require_resources,
            not args.no_assisted_routes,
            args.unpack_cache,
        )
        print(json.dumps(report["summary"], sort_keys=True))
        # This command means measurement completed, not compatibility passed.
        # The trusted reporter publishes failure unless every sampled case passes.
        return 0 if report["summary"]["complete"] else 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

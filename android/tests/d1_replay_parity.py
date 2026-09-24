"""Paired D1 observations, with explicit gaps before full simulation qualification.

Uses the existing replay wrapper and engine checkpoint readers. This module never
interprets a DGSS payload or substitutes gameplay values from an actual result.
"""

import argparse
import copy
import gzip
import hashlib
import itertools
import json
import platform
import shutil
import struct
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


class EvidenceError(ValueError):
    pass


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def digest(path):
    sha = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            sha.update(block)
    return sha.hexdigest()


def records(path):
    path = Path(path)
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rt", encoding="utf-8-sig") as source:
        for number, line in enumerate(source, 1):
            if not line.strip() or line.lstrip().startswith("//"):
                continue
            try:
                value = json.loads(line)
            except ValueError as error:
                raise EvidenceError(f"{path}:{number}: invalid JSON") from error
            if not isinstance(value, dict):
                raise EvidenceError(f"{path}:{number}: expected an object")
            yield value


def read_demo(path):
    header = checkpoint = result = None
    count = 0
    for record in records(path):
        kind = record.get("type")
        if header is None and kind != "header":
            raise EvidenceError("Recording must begin with its header")
        if kind == "header":
            if header is not None:
                raise EvidenceError("Duplicate recording header")
            header = record
        elif kind == "checkpoint":
            if checkpoint is not None or count:
                raise EvidenceError("Unexpected checkpoint placement")
            # Pin the engine's declared decoded checksum and the untouched record
            checkpoint = {key: value for key, value in record.items() if key != "data"}
            checkpoint["record_sha256"] = hashlib.sha256(
                json.dumps(record, sort_keys=True, separators=(",", ":")).encode()
            ).hexdigest()
        elif kind == "frame":
            if result is not None or record.get("f") != count:
                raise EvidenceError("Nonsequential recording frames")
            count += 1
        elif kind == "result":
            if result is not None:
                raise EvidenceError("Duplicate recording result")
            result = record["result"]
    if not header or header.get("game") != "d1" or not count:
        raise EvidenceError("A nonempty D1 recording is required")
    if count != header.get("frame_count") or result is None:
        raise EvidenceError("Recording frame count or terminal result is incomplete")
    if header.get("start_mode") == "save_checkpoint" and checkpoint is None:
        raise EvidenceError("Recording checkpoint is missing")
    return {"path": str(Path(path).resolve()), "sha256": digest(path), "header": header,
            "checkpoint": checkpoint, "result": result}


def imported_mission(header):
    mission = header["mission"]
    return "descent" if mission in ("", "d1", "descent") else mission


def validate_identity(value, header, engine):
    mission = imported_mission(header) if engine == "d2" else header["mission"]
    for key, expected in (("game", engine), ("mission", mission),
                          ("difficulty", header["difficulty"]), ("frame_count", header["frame_count"])):
        if value.get(key) != expected:
            raise EvidenceError(f"Unexpected {key}: {value.get(key)!r}; expected {expected!r}")


def canonical_result(value, header, engine):
    validate_identity(value, header, engine)
    value = copy.deepcopy(value)
    # Only executable/content naming differs; no actual supplies expected values
    value["game"] = "d1"
    value["mission"] = header["mission"]
    return value


def difference(expected, actual, path="$"):
    if type(expected) is not type(actual):
        return {"path": path, "expected": expected, "actual": actual}
    if isinstance(expected, dict):
        for key in sorted(expected.keys() | actual.keys()):
            if key not in expected or key not in actual:
                return {"path": f"{path}.{key}", "expected": expected.get(key, "<missing>"),
                        "actual": actual.get(key, "<missing>")}
            found = difference(expected[key], actual[key], f"{path}.{key}")
            if found:
                return found
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            return {"path": path + ".length", "expected": len(expected), "actual": len(actual)}
        for index, (left, right) in enumerate(zip(expected, actual)):
            found = difference(left, right, f"{path}[{index}]")
            if found:
                return found
    elif expected != actual:
        return {"path": path, "expected": expected, "actual": actual}
    return None


def frames(path, header, engine, recorded=False):
    count = 0
    last_ft = None
    meta_seen = False
    for record in records(path):
        if record.get("type") == ("header" if recorded else "meta"):
            if meta_seen:
                raise EvidenceError("Duplicate frame metadata")
            # start_replay emits the recording identity, including in D2.
            # Executable identity is validated separately in the terminal result.
            validate_identity(record, header, header["game"])
            if record.get("level") != header["level"] or record.get("start_mode") != header["start_mode"]:
                raise EvidenceError("Replay did not start from the recorded level/checkpoint mode")
            meta_seen = True
            continue
        if record.get("type") not in ("frame", "frame_state"):
            continue
        if not meta_seen or record.get("f") != count:
            raise EvidenceError("Frame metadata missing or frame order changed")
        count += 1
        last_ft = record.get("ft", last_ft)
        if last_ft is None or "state" not in record or "rng" not in record or "diag" not in record:
            raise EvidenceError(f"Required observations missing at frame {record.get('f')}")
        record["ft"] = last_ft
        yield record
    if count != header["frame_count"]:
        raise EvidenceError(f"Missing frame evidence: expected {header['frame_count']}, got {count}")


def compare_frames(left, right, header, right_engine, recorded=False):
    first = {}
    diagnostics = {}
    count = 0
    for a, b in itertools.zip_longest(frames(left, header, "d1", recorded), frames(right, header, right_engine)):
        if a is None or b is None:
            raise EvidenceError("Unequal trace lengths")
        for key in ("f", "ft", "state", "rng", "diag"):
            if key not in first:
                found = difference(a[key], b[key], f"$.{key}")
                if found:
                    first[key] = {"frame": count, **found}
        if not isinstance(a["diag"], dict) or not isinstance(b["diag"], dict):
            raise EvidenceError(f"Diagnostic observations must be objects at frame {count}")
        # An early representation difference must not hide a later gameplay
        # difference elsewhere in the diagnostics. Preserve the strict verdict
        # and inventory each field independently, without an exclusion list
        for key in sorted(a["diag"].keys() | b["diag"].keys()):
            entry = diagnostics.setdefault(key, {"frames_compared": 0, "frames_different": 0,
                                                 "first_difference": None})
            entry["frames_compared"] += 1
            found = difference({key: a["diag"][key]} if key in a["diag"] else {},
                               {key: b["diag"][key]} if key in b["diag"] else {}, "$.diag")
            if found:
                entry["frames_different"] += 1
                if entry["first_difference"] is None:
                    entry["first_difference"] = {"frame": count, **found}
        count += 1
    return {"frames_compared": count, "first_differences": first,
            "diagnostics": diagnostics,
            "status": "fail" if first else "pass"}


def rng_events(path, stream=0):
    count = 0
    meta = None
    for record in records(path):
        if record.get("type") == "meta":
            if meta is not None or record.get("truncated") is not False:
                raise EvidenceError("RNG trace is duplicate or truncated")
            meta = record
            continue
        if meta is None or record.get("type") not in ("rand", "srand"):
            raise EvidenceError("Missing RNG metadata or unsupported event")
        if record.get("seq") != count:
            raise EvidenceError("RNG event sequence is incomplete")
        count += 1
        # SIM and FX are independently seeded, but both traces must be complete
        if record.get("stream", 0) not in (0, 1):
            raise EvidenceError("Unsupported RNG stream")
        if record.get("stream", 0) != stream:
            continue
        required = ("frame", "gt", "call_count", "state_before", "state_after",
                    "seed" if record["type"] == "srand" else "result")
        if not all(key in record for key in required):
            raise EvidenceError("Incomplete RNG event")
        # Global seq interleaves FX; source function/path/line are engine labels
        yield {key: value for key, value in record.items()
               if key not in ("seq", "file", "func", "line", "stream")}
    if meta is None or count != meta.get("events"):
        raise EvidenceError("RNG trace event count is incomplete")


def compare_rng(left, right, stream=0):
    first = None
    first_values = None
    count = 0
    for a, b in itertools.zip_longest(rng_events(left, stream), rng_events(right, stream)):
        if first is None:
            found = difference(a, b)
            if found:
                first = {"event": count, **found}
        if first_values is None:
            # Keep object-context differences in the strict verdict, but also
            # expose algorithm divergence that an earlier annotation can mask
            values = lambda event: None if event is None else {
                key: value for key, value in event.items() if key not in ("ctx_obj", "ctx_sig", "ctx_id")}
            found = difference(values(a), values(b))
            if found:
                first_values = {"event": count, **found}
        count += 1
    count_key = "simulation_events_compared" if stream == 0 else "effects_events_compared"
    return {"status": "fail" if first else "pass", count_key: count,
            "first_difference": first, "first_value_difference": first_values}


def object_states(path, header):
    """Reconstruct the engine's lossless slot deltas, including deletion/reuse."""
    objects = {}
    count = 0
    capacity = None
    meta_seen = False
    for row in records(path):
        if row.get("type") == "meta":
            if meta_seen:
                raise EvidenceError("Duplicate object trace metadata")
            validate_identity(row, header, header["game"])
            if row.get("level") != header["level"] or row.get("start_mode") != header["start_mode"]:
                raise EvidenceError("Object trace does not describe the recorded start")
            meta_seen = True
        if row.get("type") != "object_state":
            continue
        if not meta_seen or row.get("version") != 1 or row.get("f") != count or row.get("reset") is not (count == 0):
            raise EvidenceError("Invalid object trace schema, reset or frame sequence")
        if type(row.get("capacity")) is not int or row["capacity"] <= 0:
            raise EvidenceError("Invalid object trace capacity")
        if capacity is not None and capacity != row["capacity"]:
            raise EvidenceError("Object trace capacity changed")
        capacity = row["capacity"]
        if not isinstance(row.get("slots"), dict):
            raise EvidenceError("Missing object slot deltas")
        for key, value in row["slots"].items():
            if not key.isdecimal() or str(int(key)) != key or not 0 <= int(key) < capacity:
                raise EvidenceError("Invalid object slot identity")
            if value is None:
                if key not in objects:
                    raise EvidenceError("Deleting an absent object slot")
                del objects[key]
            elif not isinstance(value, dict) or not all(type(value.get(k)) is int for k in ("signature", "type", "id")):
                raise EvidenceError("Incomplete live object identity")
            else:
                objects[key] = value
        for key, kind in (("allocator", dict), ("segment_heads", list), ("clock", dict), ("rng", list)):
            if not isinstance(row.get(key), kind):
                raise EvidenceError(f"Missing object trace {key}")
        count += 1
        yield {**{k: row[k] for k in ("capacity", "allocator", "segment_heads", "clock", "rng")},
               "objects": objects}
    if count != header["frame_count"]:
        raise EvidenceError(f"Missing object evidence: expected {header['frame_count']}, got {count}")


def field_differences(expected, actual, path="$"):
    """Visit every differing named field; unrelated layout fields cannot mask it."""
    if isinstance(expected, (dict, list)) and type(expected) is type(actual) and expected == actual:
        # Python considers True == 1; JSON encodings retain that type distinction
        if json.dumps(expected, sort_keys=True) == json.dumps(actual, sort_keys=True):
            return
    if type(expected) is not type(actual):
        yield {"path": path, "expected": expected, "actual": actual}
    elif isinstance(expected, dict):
        for key in sorted(expected.keys() | actual.keys()):
            name = f"{path}.{key}"
            if key not in expected or key not in actual:
                yield {"path": name, "expected": expected.get(key, "<missing>"), "actual": actual.get(key, "<missing>")}
            else:
                yield from field_differences(expected[key], actual[key], name)
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            yield {"path": path + ".length", "expected": len(expected), "actual": len(actual)}
        for i, (a, b) in enumerate(zip(expected, actual)):
            yield from field_differences(a, b, f"{path}[{i}]")
    elif expected != actual:
        yield {"path": path, "expected": expected, "actual": actual}


def compare_objects(left, right, header):
    first = {}
    count = 0
    for a, b in itertools.zip_longest(object_states(left, header), object_states(right, header)):
        if a is None or b is None:
            raise EvidenceError("Unequal object trace lengths")
        # Name fields independently of slot, while preserving the first concrete
        # slot/frame in each example. All raw values remain in the archived trace
        for group in ("capacity", "allocator", "segment_heads", "clock", "rng"):
            for found in field_differences(a[group], b[group], f"$.{group}"):
                first.setdefault(found["path"], {"frame": count, **found})
        for slot in sorted(a["objects"].keys() | b["objects"].keys(), key=int):
            for found in field_differences(a["objects"].get(slot), b["objects"].get(slot), "$.object"):
                first.setdefault(found["path"], {"frame": count, "slot": int(slot), **found})
        count += 1
    return {"status": "fail" if first else "pass", "frames_compared": count,
            "first_field_differences": first}


def safe_check(operation):
    try:
        return operation()
    except (EvidenceError, OSError, ValueError, KeyError) as error:
        return {"status": "incomplete", "error": str(error)}


def compare_pair(demo, left, right, engine, recorded=False):
    header = demo["header"]

    def terminal():
        expected = demo["result"] if recorded else json.loads(Path(left["result"]).read_text())
        actual = json.loads(Path(right["result"]).read_text())
        first = difference(canonical_result(expected, header, "d1"), canonical_result(actual, header, engine))
        return {"status": "fail" if first else "pass", "first_difference": first}

    checks = {
        "terminal_result": safe_check(terminal),
        "frames": safe_check(lambda: compare_frames(left["state"], right["state"], header, engine, recorded)),
        "simulation_rng": safe_check(lambda: compare_rng(left["rng"], right["rng"])),
        "effects_rng": safe_check(lambda: compare_rng(left["rng"], right["rng"], stream=1)),
    }
    if not recorded:
        checks["object_states"] = safe_check(lambda: compare_objects(left["state"], right["state"], header))
    statuses = [item["status"] for item in checks.values()]
    return {"status": "fail" if "fail" in statuses else "incomplete" if "incomplete" in statuses else "pass",
            "checks": checks}


def compress_trace(path):
    if path.suffix == ".gz":
        return path
    target = path.with_suffix(path.suffix + ".gz")
    # A failed compression must leave the source intact, never a valid-looking
    # truncated .gz beside it
    partial = target.with_name(target.name + ".partial")
    with path.open("rb") as source, gzip.open(partial, "wb", compresslevel=1) as output:
        shutil.copyfileobj(source, output)
    with gzip.open(partial, "rb") as check:
        actual = hashlib.sha256()
        for block in iter(lambda: check.read(1024 * 1024), b""):
            actual.update(block)
    if actual.hexdigest() != digest(path):
        raise EvidenceError(f"Trace compression verification failed: {path}")
    partial.replace(target)
    path.unlink()
    return target


def require_disk_space(path, minimum_gb):
    directory = Path(path).resolve()
    while not directory.exists():
        directory = directory.parent
    free = shutil.disk_usage(directory).free
    if free < minimum_gb * 1024 ** 3:
        raise EvidenceError(f"Insufficient output disk space at {directory}: "
                            f"{free / 1024 ** 3:.2f} GiB free, {minimum_gb:g} GiB reserve required")


def executable_info(path):
    path = Path(path)
    with path.open("rb") as source:
        magic = source.read(64)
        if magic[:2] == b"MZ":
            source.seek(struct.unpack_from("<I", magic, 60)[0])
            pe = source.read(6)
            if pe[:4] != b"PE\0\0":
                raise EvidenceError("Invalid executable header")
            architecture = {0x14C: "x86", 0x8664: "x86_64", 0xAA64: "arm64"}.get(struct.unpack_from("<H", pe, 4)[0], "unknown")
        elif magic[:4] == b"\x7fELF":
            architecture = "ELF64" if magic[4] == 2 else "ELF32"
        else:
            architecture = "unclassified"
    return {"path": str(path.resolve()), "sha256": digest(path), "architecture": architecture}


def snapshot_executable(path, directory):
    """Keep every capture on the same executable and colocated runtime libraries."""
    source = Path(path).resolve()
    directory.mkdir(parents=True)
    files = [source] + sorted(p for p in source.parent.iterdir()
                              if p.is_file() and p.suffix.lower() == ".dll" and p != source)
    original = {p: digest(p) for p in files}
    dependencies = []
    for item in files:
        target = directory / item.name
        shutil.copy2(item, target)
        if digest(target) != original[item]:
            raise EvidenceError(f"Executable package changed while staging: {item}")
        dependencies.append({"source": str(item), "path": str(target.resolve()), "sha256": original[item]})
    if any(digest(item) != expected for item, expected in original.items()):
        raise EvidenceError("Executable package changed while staging; finish the build before capture")
    info = executable_info(directory / source.name)
    info.update(source=str(source), package=dependencies)
    return info


def capture(args, demo, directory, name, imported, assets, executable):
    require_disk_space(args.output, args.minimum_free_gb)
    require_disk_space(args.repo, args.minimum_free_gb)
    run = directory / name
    run.mkdir()
    paths = {key: run / filename for key, filename in
             (("result", "result.json"), ("state", "state.jsonl.gz"), ("rng", "rng.jsonl"))}
    command = [args.pwsh, "-NoProfile", "-File", str(args.repo / "android/tests/run_input_demo_replay.ps1"),
               "-DemoPath", demo["path"], "-DataDir", str(assets), "-Runner", "fast", "-Mode", "accelerated",
               "-RenderProfile", "default", "-ReplayRobotLabels", "hide", "-SkipExpectedChecks",
               "-SandboxSuffix", args.output.name + "-" + name, "-TimeoutSeconds", str(args.timeout),
               "-ResultCopyPath", str(paths["result"]), "-StateLogPath", str(paths["state"]),
               "-RngLogPath", str(paths["rng"]), "-MinimumFreeSpaceGB", str(args.minimum_free_gb)]
    command += ["-ExecutablePath", executable["path"]]
    command += ["-D1InD2"] if imported else ["-Game", "d1"]
    info = {"command": command, "executable": executable, "artifacts": {key: str(path) for key, path in paths.items()}}
    write_json(run / "launch.json", info)
    print(f"{directory.name}: {name}", flush=True)
    with (run / "runner.log").open("w", encoding="utf-8") as log:
        completed = subprocess.run(command, cwd=args.repo, stdout=log, stderr=subprocess.STDOUT, check=False)
    info["exit_code"] = completed.returncode
    if any(digest(item["path"]) != item["sha256"] for item in executable["package"]):
        info["error"] = "Staged executable package changed during capture"
    for key in ("state", "rng"):
        if paths[key].is_file():
            paths[key] = compress_trace(paths[key])
    info["artifacts"] = {key: str(path) for key, path in paths.items()}
    write_json(run / "launch.json", info)
    if completed.returncode or "error" in info:
        raise EvidenceError(f"{name} capture failed; see {run / 'runner.log'}")
    for path in paths.values():
        if not path.is_file():
            raise EvidenceError(f"Missing required capture: {path}")
    return paths


def run(args):
    require_disk_space(args.output, args.minimum_free_gb)
    args.output.mkdir(parents=True, exist_ok=False)
    # Held for staging, captures and comparison. Retention skips open leases,
    # including another run still comparing its completed engine captures
    with (args.output / "producer.lock").open("w", encoding="ascii") as lease:
        if sys.platform != "win32":
            import fcntl
            fcntl.flock(lease.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        lease.write("paired D1 replay capture and comparison\n")
        lease.flush()
        return run_with_lease(args)


def run_with_lease(args):
    assets = args.output / "d1-assets"
    assets.mkdir()
    asset_manifest = []
    for name in ("descent.hog", "descent.pig"):
        require_disk_space(args.output, args.minimum_free_gb)
        matches = [path for path in args.data.iterdir() if path.name.lower() == name and path.is_file()]
        if len(matches) != 1:
            raise EvidenceError(f"Exactly one {name} required in {args.data}")
        target = assets / name
        shutil.copyfile(matches[0], target)
        asset_manifest.append({"source": str(matches[0]), "staged": str(target), "sha256": digest(target)})
    native = snapshot_executable(args.native, args.output / "binaries" / "native")
    imported = snapshot_executable(args.imported, args.output / "binaries" / "imported")
    manifest = {"schema": 1, "created_utc": datetime.now(timezone.utc).isoformat(), "host": platform.platform(),
                "assets": asset_manifest, "executables": {"native": native, "imported": imported},
                "settings": {"runner": "fast", "render_profile": "default", "companion": False,
                             "cameras": "checkpoint settings", "player_cfg": "unchanged recording header"},
                "demos": []}
    manifest["git_revision"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=args.repo, text=True).strip()
    working_diff = subprocess.check_output(["git", "diff", "HEAD"], cwd=args.repo)
    (args.output / "source.patch").write_bytes(working_diff)
    manifest["working_diff_sha256"] = hashlib.sha256(working_diff).hexdigest()
    harness = args.output / "harness"
    harness.mkdir()
    manifest["harness_sha256"] = {}
    for name in ("d1_replay_parity.py", "test_d1_replay_parity.ps1", "run_input_demo_replay.ps1",
                 "input_demo_host_build_guard.ps1"):
        shutil.copyfile(args.repo / "android/tests" / name, harness / name)
        manifest["harness_sha256"][name] = digest(harness / name)
    for name in ("output_disk_space.ps1", "retain-recent-artifacts.ps1", "clean-old-artifacts.ps1"):
        shutil.copyfile(args.repo / "android/helpers" / name, harness / name)
        manifest["harness_sha256"][name] = digest(harness / name)
    report = {"schema": 1, "cases": [], "qualification": "incomplete", "coverage_gaps": [
        "Existing frame diagnostics have not been fully mapped to a cross-engine semantic schema",
        "A complete pre-advance restored-state snapshot is not emitted",
        "A complete pre-retirement world/transition snapshot is not emitted",
        "Named object states are observed, but complete world/player/global state and presentation are not yet covered",
    ]}
    for path in args.demo:
        case = {"demo": str(path), "status": "incomplete"}
        report["cases"].append(case)
        try:
            demo = read_demo(path)
            rng_reference = Path(str(path) + ".rngtrace.jsonl")
            demo["recorded_rng"] = {"path": str(rng_reference),
                                    "sha256": digest(rng_reference) if rng_reference.is_file() else None}
            manifest["demos"].append(demo)
            write_json(args.output / "manifest.json", manifest)
            if demo["header"]["mission"] not in ("", "d1", "descent"):
                raise EvidenceError("Custom mission staging is not yet qualified by this base-campaign runner")
            directory = args.output / path.stem
            directory.mkdir()
            runs = {}
            # Always attempt all three captures, even if one is incomplete
            for name in ("native-a", "native-repeat", "imported"):
                try:
                    runs[name] = capture(args, demo, directory, name, name == "imported", assets,
                                         imported if name == "imported" else native)
                except (OSError, EvidenceError) as error:
                    case.setdefault("capture_errors", {})[name] = str(error)
            recorded = {"state": path, "rng": Path(str(path) + ".rngtrace.jsonl")}
            for relationship, left_name, right_name, engine in (
                ("recording_to_native", None, "native-a", "d1"),
                ("native_repeatability", "native-a", "native-repeat", "d1"),
                ("native_to_imported", "native-a", "imported", "d2"),
            ):
                if right_name not in runs or (left_name is not None and left_name not in runs):
                    case[relationship] = {"status": "incomplete", "error": "Required capture failed"}
                else:
                    print(f"{directory.name}: comparing {relationship}", flush=True)
                    case[relationship] = compare_pair(demo, runs[left_name] if left_name else recorded,
                                                      runs[right_name], engine, left_name is None)
            case["status"] = "fail" if any(case[key]["status"] == "fail" for key in
                                            ("recording_to_native", "native_repeatability", "native_to_imported")) else "incomplete"
        except (OSError, EvidenceError, ValueError, KeyError) as error:
            case["error"] = str(error)
        write_json(args.output / "report.json", report)
    for asset in asset_manifest:
        if digest(asset["staged"]) != asset["sha256"]:
            report["asset_error"] = "Staged assets changed during capture"
    write_json(args.output / "report.json", report)
    print(f"Report: {args.output / 'report.json'}", flush=True)
    print("Full fidelity qualification: INCOMPLETE (see explicit coverage gaps)", flush=True)
    return 1 if any(case["status"] == "fail" for case in report["cases"]) else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--imported", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pwsh", required=True)
    parser.add_argument("--timeout", type=int, default=300)
    parser.add_argument("--minimum-free-gb", type=float, default=4)
    parser.add_argument("--demo", type=Path, action="append", required=True)
    args = parser.parse_args()
    if not 0.01 <= args.minimum_free_gb <= 1048576:
        parser.error("--minimum-free-gb must be between 0.01 and 1048576")
    return run(args)


if __name__ == "__main__":
    sys.exit(main())

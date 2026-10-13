"""Build-gated native D1 fault captures; preserve raw evidence and never replace an oracle.

Requires an existing Ninja/Makefiles D1 build and a clean, exact source revision.
Run under Xvfb when needed; this helper never configures a new build directory.
Successful output is a candidate, not a parity or fault-admission verdict.
"""

import argparse
import gzip
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

import compact_public_replay as compact
from run_compact_public_replays import validate_assets

RECORDER = "android/app/src/main/cpp/shared/input_demo_recorder.cpp"
FIXTURE = "android/tests/test_upstream_compat.cpp"
RECIPES = (
    FIXTURE,
    "android/tests/public_level_replay_fixture.hpp",
    "android/tests/public_effect_phase_fixture.hpp",
    "android/tests/public_missile_fault_fixture.hpp",
    RECORDER,
)
SCENARIOS = ("missile-self-splash", "mine-blast", "effect-phase")
Error = compact.parity.EvidenceError


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def git(repo, *arguments):
    return subprocess.run(["git", "-C", str(repo), *arguments], capture_output=True, check=True).stdout


def source_identity(repo, revision):
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise Error("Expected source revision must be a full 40-character lowercase commit hash")
    if git(repo, "rev-parse", "HEAD").decode().strip() != revision:
        raise Error("Source HEAD differs from the expected revision")
    if git(repo, "status", "--porcelain", "--untracked-files=all").strip():
        raise Error("Source is dirty; commit the final recipes before capture")
    hashes = {}
    for name in RECIPES:
        raw = (repo / name).read_bytes()
        if raw != git(repo, "show", f"{revision}:{name}"):
            raise Error(f"Source recipe differs from the expected revision: {name}")
        hashes[name] = hashlib.sha256(raw).hexdigest()
    return hashes


def clean_stamp(stamp, revision):
    if not re.fullmatch(r"[0-9a-f]{7,40}", stamp) or not revision.startswith(stamp):
        raise Error("Recorder build stamp differs from the expected clean source revision")


def build_cache(repo, build, revision):
    cache = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        match = re.match(r"([^#/:][^:]*):[^=]+=(.*)$", line)
        if match:
            cache[match[1]] = match[2]
    if Path(cache.get("CMAKE_HOME_DIRECTORY", "")).resolve() != repo / "d1":
        raise Error("Build CMAKE_HOME_DIRECTORY must be the selected repository's d1 directory")
    if cache.get("CMAKE_GENERATOR") not in ("Ninja", "Unix Makefiles"):
        raise Error("Capture requires an existing single-config Ninja or Unix Makefiles build")
    if "DXX_INPUT_DEMO_GIT_VERSION" in cache:
        clean_stamp(cache["DXX_INPUT_DEMO_GIT_VERSION"], revision)


def compiled_stamp(repo, build, revision):
    """Check the real fixture/recorder compile recipes, including native D1 sources."""
    wanted = {str((repo / name).resolve()) for name in (FIXTURE, RECORDER, "d1/main/fireball.c", "d1/main/effects.c")}
    found, stamps = set(), set()
    for row in json.loads((build / "compile_commands.json").read_text()):
        arguments = row.get("arguments") or shlex.split(row["command"])
        output = row.get("output", "")
        if "-o" in arguments:
            output = arguments[arguments.index("-o") + 1]
        if "CMakeFiles/test_upstream_compat.dir/" not in output.replace("\\", "/"):
            continue
        path = str((Path(row["directory"]) / row["file"]).resolve())
        if path not in wanted:
            continue
        defines = [arg for arg in arguments if arg.startswith("-DDXX_INPUT_DEMO_GIT_VERSION=")]
        if len(defines) != 1 or any(arg.startswith("-DDXX_BUILD_DESCENT_II") for arg in arguments):
            raise Error("Fixture compile recipe lacks unique native D1 recorder metadata")
        stamp = defines[0].split("=", 1)[1].strip('"')
        clean_stamp(stamp, revision)
        stamps.add(stamp)
        found.add(path)
    if found != wanted or len(stamps) != 1:
        raise Error("Missing or inconsistent native D1 fixture/recorder compile recipes")
    return stamps.pop()


def run_logged(command, log, **kwargs):
    with log.open("w") as stream:
        stream.write(shlex.join(command) + "\n")
        stream.flush()
        subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=True, **kwargs)


def verify_capture(raw, scenario, revision, stamp, no_missile):
    with (raw / "capture.dximdemo").open() as stream:
        header = compact._json(next(stream))
        if (
            header.get("type") != "header"
            or header.get("game") != "d1"
            or header.get("mission") != "d1"
            or header.get("start_mode") != "new_level"
            or header.get("level") != (3 if scenario == "mine-blast" else 1)
        ):
            raise Error("Recording must be a native D1 ordinary new-level start")
        clean_stamp(header.get("git_version", ""), revision)
        if header["git_version"] != stamp:
            raise Error("Actual recorder source stamp differs from the rebuilt fixture")
        compact._no_save(header)
        for line in stream:
            if line.strip():
                compact._no_save(compact._json(line))
    report = compact._json((raw / "capture.json").read_bytes())
    if report.get("game") != "d1" or report.get("scenario") != scenario:
        raise Error("Capture report identity differs from the requested native D1 recipe")
    if scenario == "effect-phase":
        if report.get("capture_role") != "native_d1_reference":
            raise Error("Effect oracle source must be native D1")
    elif report.get("negative_no_missile") is not no_missile:
        raise Error("Capture report differs from the requested live missile control")
    if not (raw / "capture.dximdemo.rngtrace.jsonl").is_file():
        raise Error("Missing live recorder RNG trace")
    return header


def write_manifest(output, manifest):
    (output / "manifest.json").write_text(json.dumps(manifest, indent=4, sort_keys=True) + "\n")


def hash_evidence(output):
    return {
        str(path.relative_to(output)): {"sha256": digest(path), "bytes": path.stat().st_size}
        for path in sorted(output.rglob("*"))
        if path.is_file() and path.name != "manifest.json"
    }


def capture(args):
    args.output.mkdir(parents=True, exist_ok=False)
    manifest = {
        "schema": 1,
        "status": "in_progress",
        "stage": "preflight",
        "capture_invoked": False,
        "source_revision": args.revision,
        "scenario": args.scenario,
        "no_missile": args.no_missile,
        "screenshots": args.screenshots,
    }
    try:
        if args.timeout <= 0:
            raise Error("Capture timeout must be positive")
        if args.gzip_raw and not args.pack:
            raise Error("--gzip-raw requires successful --pack; raw originals are always retained")
        if args.no_missile and args.scenario == "effect-phase":
            raise Error("--no-missile applies only to missile and mine recipes")
        hashes = source_identity(args.repo, args.revision)
        build_cache(args.repo, args.build, args.revision)
        validate_assets("d1", args.assets)
        manifest.update(source_recipe_sha256=hashes, asset_sha256=compact.pinned_assets("d1"))
        manifest["stage"] = "configure"
        run_logged(
            [
                "cmake",
                "-S",
                str(args.repo / "d1"),
                "-B",
                str(args.build),
                "-U",
                "DXX_INPUT_DEMO_GIT_VERSION",
                "-U",
                "DXX_INPUT_DEMO_BUILD_NUMBERi",
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            ],
            args.output / "configure.log",
        )
        build_cache(args.repo, args.build, args.revision)
        stamp = compiled_stamp(args.repo, args.build, args.revision)
        manifest["stage"] = "build"
        run_logged(["cmake", "--build", str(args.build), "--target", "test_upstream_compat"], args.output / "build.log")
        manifest["stage"] = "verify_build"
        if source_identity(args.repo, args.revision) != hashes:
            raise Error("Source recipes changed during build")
        build_cache(args.repo, args.build, args.revision)
        if compiled_stamp(args.repo, args.build, args.revision) != stamp:
            raise Error("Recorder compile metadata changed during build")
        exe = args.build / "main/test_upstream_compat"
        if exe.is_symlink() or not exe.is_file() or not os.access(exe, os.X_OK):
            raise Error("Build did not produce its native test_upstream_compat executable")
        if stamp.encode() + b"\0" not in exe.read_bytes():
            raise Error("Built executable lacks the expected recorder source stamp")
        validate_assets("d1", args.assets)
        manifest.update(
            capture_executable_sha256=digest(exe),
            recorder_git_version=stamp,
            compile_commands_sha256=digest(args.build / "compile_commands.json"),
        )
        raw = args.output / "raw"
        raw.mkdir()
        home = raw / "home"
        home.mkdir()
        # Clear ambient exploration knobs, retaining display and native library settings
        environment = {key: value for key, value in os.environ.items() if not key.startswith("DXX_PUBLIC_")}
        environment["HOME"] = str(home)
        if args.screenshots:
            environment["DXX_PUBLIC_CAPTURE_SCREENSHOTS"] = "1"
        if args.no_missile:
            environment["DXX_PUBLIC_CAPTURE_NO_MISSILE"] = "1"
        command = (
            [str(exe), "--public-effect-phase", str(args.assets)]
            if args.scenario == "effect-phase"
            else [str(exe), "--public-level-replay", str(args.assets), args.scenario]
        )
        manifest.update(stage="capture", capture_invoked=True, command=command)
        write_manifest(args.output, manifest)
        run_logged(command, args.output / "capture.log", cwd=raw, env=environment, timeout=args.timeout)
        manifest["stage"] = "verify_capture"
        manifest["recording_header"] = verify_capture(raw, args.scenario, args.revision, stamp, args.no_missile)
        if digest(exe) != manifest["capture_executable_sha256"] or source_identity(args.repo, args.revision) != hashes:
            raise Error("Source or executable changed during capture")
        validate_assets("d1", args.assets)
        if args.pack:
            manifest["stage"] = "pack"
            recipe = RECIPES[2] if args.scenario == "effect-phase" else RECIPES[3]
            compact.pack(
                raw / "capture.dximdemo",
                raw / "capture.dximdemo.rngtrace.jsonl",
                args.output / "candidate.dximdemo",
                args.output / "candidate.oracle.json",
                {
                    "source_revision": args.revision,
                    "capture_executable_sha256": manifest["capture_executable_sha256"],
                    "capture_source_sha256": hashes[recipe],
                    "asset_sha256": manifest["asset_sha256"],
                },
            )
            if (
                max((args.output / name).stat().st_size for name in ("candidate.dximdemo", "candidate.oracle.json"))
                > compact.MAX_BYTES
            ):
                raise Error("Each compact input and native oracle must fit 100000 uncompressed bytes")
            # A successful pack does not replace detailed effect/mine trajectory checking
            manifest["packed"] = True
        if args.gzip_raw:
            manifest["stage"] = "archive"
            manifest["files"] = hash_evidence(args.output)
            write_manifest(args.output, manifest)
            for path in sorted(raw.iterdir()):
                if path.is_file() and path.suffix in (".dximdemo", ".jsonl"):
                    with (
                        path.open("rb") as source,
                        path.with_name(path.name + ".gz").open("xb") as target,
                        gzip.GzipFile(fileobj=target, mode="wb", mtime=0) as compressed,
                    ):
                        shutil.copyfileobj(source, compressed)
        manifest.update(status="candidate", stage="complete")
    except BaseException as error:
        manifest.update(status="failed", error=str(error))
        if isinstance(error, subprocess.CalledProcessError):
            manifest.update(failed_command=error.cmd, returncode=error.returncode)
            if error.stderr:
                manifest["stderr"] = error.stderr.decode(errors="replace")
        raise
    finally:
        manifest["files"] = hash_evidence(args.output)
        write_manifest(args.output, manifest)
    return manifest


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "build", "assets", "output"):
        parser.add_argument("--" + name, required=True, type=lambda value: Path(value).resolve())
    parser.add_argument("--revision", required=True, help="Exact clean source commit, 40 lowercase hex characters")
    parser.add_argument("--scenario", choices=SCENARIOS, required=True)
    parser.add_argument("--screenshots", action="store_true")
    parser.add_argument("--no-missile", action="store_true")
    parser.add_argument(
        "--pack", action="store_true", help="Create new bounded compact input and native summary/RNG oracle"
    )
    parser.add_argument(
        "--gzip-raw", action="store_true", help="After packing, add lossless gzip copies without deleting originals"
    )
    parser.add_argument("--timeout", type=int, default=300, help="Capture timeout in seconds")
    args = parser.parse_args(argv)
    try:
        capture(args)
    except (Error, OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"Capture failed: {error}; evidence retained at {args.output}", file=sys.stderr)
        return 1
    print(f"Native D1 candidate retained at {args.output}; detailed comparison and repeat capture still required")
    return 0


if __name__ == "__main__":
    sys.exit(main())

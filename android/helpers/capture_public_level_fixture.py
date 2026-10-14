#!/usr/bin/env python3
"""Capture a public scenario after checking its clean source and native build label.

Run a logged reconfigure/rebuild from the clean published checkout first. These
checks reject stale revision labels; they are not a build-source attestation.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def checked_output(arguments: list[str], cwd: Path) -> str:
    return subprocess.check_output(arguments, cwd=cwd, text=True).strip()


def isolated_environment(directory: Path) -> dict[str, str]:
    environment = os.environ.copy()
    for variable in ("HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME"):
        path = directory / variable.lower()
        path.mkdir()
        environment[variable] = str(path)
    return environment


def verify_source(repo: Path, executable: Path, expected: str, probe_directory: Path) -> str:
    if not re.fullmatch(r"[0-9a-fA-F]{40}", expected):
        raise ValueError("Expected revision must be the full published commit SHA")
    revision = checked_output(["git", "rev-parse", "HEAD"], repo)
    if revision != expected.lower():
        raise ValueError("Checkout HEAD does not match the expected published revision")
    if checked_output(["git", "status", "--porcelain", "--untracked-files=all"], repo):
        raise ValueError("Capture source must be clean, including untracked source files")
    published = checked_output(
        ["git", "for-each-ref", f"--contains={revision}", "--format=%(refname)", "refs/remotes/origin"], repo
    )
    if not published:
        raise ValueError("Fetch the published origin branch before capture")
    # Older fixtures treat an unknown flag as a full test run. Keep that probe
    # away from the checkout and caller config, with its diagnostic log retained
    probe_directory.mkdir()
    environment = isolated_environment(probe_directory)
    log_path = probe_directory / "probe.log"
    with log_path.open("w", encoding="utf-8") as log:
        subprocess.run(
            [str(executable), "--capture-build-revision"],
            cwd=probe_directory,
            env=environment,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=30,
        )
    embedded = log_path.read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"[0-9a-f]{7,40}", embedded) or not revision.startswith(embedded):
        raise ValueError(f"Executable revision {embedded!r} differs from source {revision}; reconfigure and rebuild")
    return revision


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--assets", required=True, type=Path)
    parser.add_argument("--scenario", required=True)
    parser.add_argument("--expected-revision", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--pwsh", default="pwsh")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    executable, assets, output = args.executable.resolve(), args.assets.resolve(), args.output.resolve()
    executable_digest = hashlib.sha256(executable.read_bytes()).hexdigest()
    if output.exists():
        raise ValueError("Capture output must be a new directory; existing evidence is never overwritten")
    if not assets.is_dir():
        raise ValueError("Public assets directory does not exist")
    subprocess.run(
        [
            args.pwsh,
            "-NoProfile",
            "-File",
            str(repo / "android/helpers/retain-recent-artifacts.ps1"),
            "-Artifacts",
            str(output),
        ],
        cwd=repo,
        check=True,
    )
    output.mkdir(parents=True)
    revision = verify_source(repo, executable, args.expected_revision, output / "revision-probe-before")
    environment = isolated_environment(output)
    with (output / "capture.log").open("w", encoding="utf-8") as log:
        subprocess.run(
            [str(executable), "--public-level-replay", str(assets), args.scenario],
            cwd=output,
            env=environment,
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
        )
    with (output / "capture.dximdemo").open(encoding="utf-8") as recording:
        header = json.loads(next(recording))
    recorded_revision = header.get("git_version", "")
    if not isinstance(recorded_revision, str) or not re.fullmatch(r"[0-9a-f]{7,40}", recorded_revision):
        raise ValueError("Recorder metadata has no valid embedded revision")
    if not revision.startswith(recorded_revision):
        raise ValueError("Recorder metadata does not match the verified executable revision")
    if hashlib.sha256(executable.read_bytes()).hexdigest() != executable_digest:
        raise ValueError("Executable changed during capture; retain this run as diagnostic evidence only")
    verify_source(repo, executable, revision, output / "revision-probe-after")
    provenance = {
        "source_revision": revision,
        "embedded_revision": header["git_version"],
        "executable_sha256": executable_digest,
        "scenario": args.scenario,
    }
    (output / "capture-source.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    print(f"Captured {args.scenario} from verified revision {revision}: {output}")


if __name__ == "__main__":
    main()

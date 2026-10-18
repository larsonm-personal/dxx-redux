"""Pinned private recordings: admission, staging and strict headless replay owner.

No recording bytes or generated replay diagnostics belong in public artifacts.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "helpers"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

ASSETS_COMMIT = "d7999de42cf5d94cdbfd18f5eddbfc6301c1eafd"
CASES = (
    {
        "name": "d2_descent2_level7_20261015_151655.dximdemo",
        "level": 7,
        "frames": 2161,
        "size": 899361,
        "sha256": "e9b83907fac3ea58e7687fd93e5cdaae598342955b2431d39a451074816dd7d3",
    },
    {
        "name": "d2_descent2_level9_20261016_074810.dximdemo",
        "level": 9,
        "frames": 7372,
        "size": 3280120,
        "sha256": "c2d7f29819381f94a53c749c3f74bb7d5af85b506194bf30f61c4ec8cc9f4af4",
    },
)


def validate(directory):
    """Identity is checked before parsing; originals and their oracles stay immutable."""
    from retail_ci_assets import file_identity

    results = []
    for case in CASES:
        path = Path(directory) / case["name"]
        if file_identity(path) != (case["size"], case["sha256"]):
            raise ValueError("Private recording identity mismatch")
        rows = [
            json.loads(line)
            for line in path.read_text().splitlines()
            if line.strip() and not line.lstrip().startswith("//")
        ]
        header, terminal = rows[0], rows[-1]
        if (
            header.get("type") != "header"
            or header.get("version") != 5
            or header.get("game") != "d2"
            or header.get("mission") != "d2"
            or header.get("level") != case["level"]
            or header.get("frame_count") != case["frames"]
            or header.get("start_mode") != "new_level"
            or header.get("asset_fingerprints", {}).get("status") != "complete"
            or terminal.get("type") != "result"
            or terminal.get("result", {}).get("frame_count") != case["frames"]
            or [row.get("f") for row in rows if row.get("type") == "frame"] != list(range(case["frames"]))
        ):
            raise ValueError("Private recording contract mismatch")
        results.append(terminal["result"])
    return results


def stage(source, destination):
    from retail_ci_assets import reject_links

    validate(source)
    reject_links(destination)
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=False)
    for case in CASES:
        shutil.copyfile(Path(source) / case["name"], destination / case["name"])
    validate(destination)


def command(args, case, result, executable=None):
    return [
        str(executable if executable is not None else args.exe),
        "-hogdir",
        str(args.data),
        "-inputdemo-replay",
        str(args.demos / case["name"]),
        "-inputdemo-actual-result",
        str(result),
        "-headless-console-output",
        "2",
    ]


def assert_expected(expected, actual, path="$"):
    # The native engine compares the embedded oracle too. Keep an independent
    # owner check so missing results, truncated frames and ignored failures fail
    if type(expected) is not type(actual):
        raise TypeError("Result type differs at " + path)
    if isinstance(expected, dict):
        for key, value in expected.items():
            if key not in actual:
                raise ValueError("Missing result field at " + path + "." + key)
            assert_expected(value, actual[key], path + "." + key)
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            raise ValueError("Result length differs at " + path)
        for index, (left, right) in enumerate(zip(expected, actual)):
            assert_expected(left, right, path + "[" + str(index) + "]")
    elif expected != actual:
        raise ValueError("Result differs at " + path)


def run(args):
    expected = validate(args.demos)
    args.output.mkdir(parents=True, exist_ok=False)
    failures = []
    for case, oracle in zip(CASES, expected):
        previous = None
        for repeat in (1, 2):
            stem = "level" + str(case["level"]) + "-" + str(repeat)
            result = args.output / (stem + ".json")
            try:
                environment = os.environ.copy()
                sandbox = args.output / (stem + ".sandbox")
                sandbox.mkdir()
                # The supported native CLI owns its private files here
                for key in (
                    "HOME",
                    "XDG_CONFIG_HOME",
                    "XDG_CACHE_HOME",
                    "XDG_DATA_HOME",
                ):
                    directory = sandbox / key.lower()
                    directory.mkdir()
                    environment[key] = str(directory)
                executable = args.exe
                if os.name == "nt":
                    # Windows PhysFS uses the executable directory for settings
                    executable = sandbox / args.exe.name
                    shutil.copy2(args.exe, executable)
                    for library in args.exe.parent.glob("*.dll"):
                        shutil.copy2(library, sandbox / library.name)
                if shutil.disk_usage(args.output).free < args.minimum_free_gb * 1024**3:
                    raise OSError("Insufficient private output disk reserve")
                with (args.output / (stem + ".log")).open("w") as log:
                    completed = subprocess.run(
                        command(args, case, result, executable=executable),
                        stdout=log,
                        stderr=subprocess.STDOUT,
                        timeout=args.timeout,
                        check=False,
                        cwd=sandbox,
                        env=environment,
                    )
                if completed.returncode:
                    raise ValueError("Native replay failed")
                actual = json.loads(result.read_text())
                assert_expected(oracle, actual)
                if previous is not None and actual != previous:
                    raise ValueError("Repeated terminal states differ")
                previous = actual
            except (OSError, TypeError, ValueError, subprocess.TimeoutExpired) as error:
                failures.append(stem + ": " + str(error))
    # Fixed status only on stdout: detailed diagnostics stay in the private output
    (args.output / "summary.json").write_text(
        json.dumps({"status": "FAIL" if failures else "PASS", "failures": failures}, indent=2) + "\n"
    )
    print(
        "FAIL: private input demo replay" if failures else "PASS: both private demos, strict oracle and repeat checks"
    )
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", action="store_true")
    parser.add_argument("--demos", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--data", type=Path)
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--timeout", type=int, default=300)
    parser.add_argument("--minimum-free-gb", type=float, default=4)
    args = parser.parse_args()
    for name in ("demos", "output", "data", "exe"):
        path = getattr(args, name)
        if path is not None:
            setattr(args, name, path.resolve())
    if args.stage:
        stage(args.demos, args.output)
        return 0
    if not args.data or not args.exe:
        parser.error("--data and --exe are required for replay")
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())

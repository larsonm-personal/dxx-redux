#!/usr/bin/env python3
"""Build and qualify the download-free Linux host profile in a supplied build directory."""

import argparse
import json
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if sys.platform != "linux":
        parser.error("this inventory check qualifies the Linux host profile")
    source = Path(__file__).resolve().parent
    build = args.build_dir.resolve()
    if build == source or source in build.parents:
        parser.error("use a build directory outside the extraction source tree")
    build.mkdir(parents=True, exist_ok=True)
    cache = build / "unused-dependency-cache"
    subprocess.run(
        [
            args.cmake,
            "-S",
            str(source),
            "-B",
            str(build),
            "-DCMAKE_BUILD_TYPE=Release",
            "-DDXX_EXTRACT_DEPENDENCY_LIGHT=ON",
            f"-DDXX_DEPENDENCY_CACHE_DIR={cache}",
        ],
        check=True,
    )
    if cache.exists() or (build / "_deps").exists():
        raise RuntimeError(
            "light profile unexpectedly populated third-party dependencies"
        )
    inventory = json.loads(
        subprocess.check_output(
            [args.ctest, "--test-dir", str(build), "--show-only=json-v1"],
            text=True,
        )
    )
    if len(inventory["tests"]) != 58:
        raise RuntimeError(
            "update the documented Linux inventory: expected 58 registrations"
        )
    subprocess.run(
        [args.cmake, "--build", str(build), "--parallel", str(args.jobs)], check=True
    )
    junit = build / "dependency-light-results.xml"
    subprocess.run(
        [
            args.ctest,
            "--test-dir",
            str(build),
            "--output-on-failure",
            "--output-junit",
            str(junit),
            "--parallel",
            str(args.jobs),
        ],
        check=True,
    )
    cases = ET.parse(junit).getroot().findall("testcase")
    skipped = sum(case.find("skipped") is not None for case in cases)
    failed = sum(case.find("failure") is not None for case in cases)
    if (len(cases), skipped, failed) != (58, 22, 0):
        raise RuntimeError(
            f"unexpected inventory: {len(cases)} cases, {skipped} skipped, {failed} failed"
        )
    print("Dependency-light Linux profile: 36 passed, 22 explicitly skipped, 0 failed.")
    print(
        "HFS/STI2 normally skip 8 internal real-media checks; light builds do not stage source media."
    )


if __name__ == "__main__":
    main()

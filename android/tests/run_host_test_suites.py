#!/usr/bin/env python3
"""Allowlisted host-suite adapter for a separately qualified target checkout."""

import argparse
import importlib.util
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

REGISTRY = Path(__file__).with_name("host_test_suites.json")
MEDIA_SKIP = re.compile(
    r"SKIP: sample media not present|skip missing fixture:|"
    r"StuffIt demo (?:archive|oracle file) missing, skipping|"
    r"SKIP: SOW real-media fixtures unavailable"
)


class SuiteCancelled(Exception):
    """A termination request that must unwind the active process group."""


def handle_termination(signum, frame):
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    raise SuiteCancelled("Suite cancelled by SIGTERM")


def write_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def load_suites():
    data = json.loads(REGISTRY.read_text(encoding="utf-8"))
    suites = data["suites"]
    if data["schema"] != 1 or len({s["id"] for s in suites}) != len(suites):
        raise ValueError("invalid or duplicate host suite IDs")
    return {s["id"]: s for s in suites}


def target_file(repo, relative):
    path = (repo / relative).resolve()
    if not path.is_relative_to(repo):
        raise ValueError("target path escapes checkout")
    return path


def classify_cases(cases, *, light=False):
    # Owner execution and leaf coverage are separate. Missing licensed media must not
    # cause an endless rerun of public leaves in the same commit's coverage sweep.
    failed = [c for c in cases if c["status"] == "FAIL"]
    exclusions = [c for c in cases if c["status"] in {"SKIP", "BLOCKED"}]
    ran = any(c["status"] in {"PASS", "FAIL", "BLOCKED"} for c in cases)
    return ("FAIL" if failed else "PASS" if ran else "BLOCKED"), exclusions


def read_ctest_results(path):
    cases = []
    for case in ET.parse(path).getroot().iter("testcase"):
        output = "\n".join(case.itertext())
        status = (
            "FAIL"
            if case.find("failure") is not None or case.find("error") is not None
            else "PASS"
        )
        reason = ""
        if case.find("skipped") is not None:
            status, reason = "SKIP", "CTest explicitly skipped this registration"
        elif status == "PASS" and MEDIA_SKIP.search(output):
            status, reason = (
                "BLOCKED",
                "Test internally skipped unavailable real-media fixtures",
            )
        cases.append({"id": case.get("name"), "status": status, "reason": reason})
    return cases


def unittest_worker(repo, relative, output):
    """Runs in a child process; imports target test code only there."""
    path = target_file(repo, relative)
    suite = unittest.defaultTestLoader.discover(str(path.parent), pattern=path.name)

    class RecordingResult(unittest.TextTestResult):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.passed = []
            self.expected = []

        def addSuccess(self, test):
            super().addSuccess(test)
            self.passed.append({"id": test.id(), "status": "PASS", "reason": ""})

        def addExpectedFailure(self, test, error):
            super().addExpectedFailure(test, error)
            self.expected.append(
                {
                    "id": test.id(),
                    "status": "SKIP",
                    "reason": "Expected failure remains unqualified",
                }
            )

    result = unittest.TextTestRunner(verbosity=2, resultclass=RecordingResult).run(
        suite
    )
    skipped = [
        {"id": test.id(), "status": "SKIP", "reason": reason}
        for test, reason in result.skipped
    ] + result.expected
    failures = [
        {"id": test.id(), "status": "FAIL", "reason": detail}
        for test, detail in result.failures + result.errors
    ] + [
        {
            "id": test.id(),
            "status": "FAIL",
            "reason": "Unexpected success of an expected-failure test",
        }
        for test in result.unexpectedSuccesses
    ]
    write_json(
        output,
        {
            "tests": result.testsRun,
            "skipped": skipped,
            "failures": failures,
            "successful": result.wasSuccessful(),
            "passed": result.passed,
        },
    )
    return 0 if result.wasSuccessful() else 1


def native_configure_arguments(repo, build, game):
    """Match run-linux-build.sh Debug test flags in a caller-owned cache."""
    return [
        "cmake",
        "-S",
        str(target_file(repo, game)),
        "-B",
        str(build),
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DBUILD_TESTING=ON",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-DCMAKE_C_FLAGS_DEBUG=-O0 -g1",
        "-DCMAKE_CXX_FLAGS_DEBUG=-O0 -g1",
        "-DOPENGL=ON",
        "-DSDLMIXER=ON",
        "-DUDP=ON",
        "-DOpenGL_GL_PREFERENCE=LEGACY",
    ]


def public_profile_arguments(suite, repo, build_root, work):
    """Only reviewed profile names construct argv; plans cannot supply commands."""
    profile = suite["profile"]

    def binary(game, suffix):
        name = "dxx-redux-" + game + suffix if suffix else game + "x-redux"
        return str(build_root / ("engine-" + game) / "main" / name)

    if profile in {"metadata-d1", "metadata-d2", "metadata-imported-d1"}:
        game = "d1" if profile == "metadata-d1" else "d2"
        args = [
            "-Executable",
            binary(game, "-metadata-worker"),
            "-Game",
            game,
            "-DataEdition",
            "shareware",
            "-OutputDirectory",
            str(work / "metadata"),
        ]
        if profile == "metadata-imported-d1":
            args += ["-ContentGame", "d1", "-CheckProfileSwitch"]
        return args
    if profile == "metadata-headers":
        return [
            "-NoBuild",
            "-DataEdition",
            "shareware",
            "-Game",
            "both",
            "-D1Exe",
            binary("d1", "-metadata-worker"),
            "-D2Exe",
            binary("d2", "-metadata-worker"),
            "-OutputDirectory",
            str(work / "headers"),
        ]
    if profile == "public-flight":
        return [
            "-DataEdition",
            "shareware",
            "-Game",
            "both",
            "-D1Exe",
            binary("d1", ""),
            "-D2Exe",
            binary("d2", ""),
            "-OutputDirectory",
            str(work / "flight"),
        ]

    def fixture(game):
        return str(build_root / ("engine-" + game) / "main" / "test_upstream_compat")

    if profile == "shareware-data":
        return ["-RequireData"]
    if profile == "public-save":
        return [
            "-D1Exe",
            fixture("d1"),
            "-D2Exe",
            fixture("d2"),
            "-D1Assets",
            str(repo / "game_data/shareware/d1"),
            "-D2Assets",
            str(repo / "game_data/shareware/d2"),
            "-Output",
            str(work / "save"),
        ]
    if profile == "public-pickup":
        return [
            "-DataEdition",
            "shareware",
            "-Mode",
            "pickup",
            "-NativeExe",
            fixture("d1"),
            "-ImportedExe",
            fixture("d2"),
            "-Output",
            str(work / "pickup"),
        ]
    if profile == "public-secret-baseline":
        return [
            "-DataEdition",
            "shareware",
            "-Game",
            "both",
            "-RequireAssets",
            "-D1Exe",
            binary("d1", "-headless-metadata"),
            "-D2Exe",
            binary("d2", "-headless-metadata"),
            "-OutputDir",
            str(work / "secret-baseline"),
        ]
    if profile == "public-ordered-input":
        return [
            "-D1Exe",
            fixture("d1"),
            "-D2Exe",
            fixture("d2"),
            "-D1Headless",
            binary("d1", "-headless"),
            "-D2Headless",
            binary("d2", "-headless"),
            "-D1Graphics",
            binary("d1", ""),
            "-D2Graphics",
            binary("d2", ""),
            "-Output",
            str(work / "ordered-input"),
        ]
    if profile == "public-all-weapons":
        return [
            "-D1BuildDir",
            str(build_root / "engine-d1"),
            "-D2BuildDir",
            str(build_root / "engine-d2"),
            "-Output",
            str(work / "all-weapons"),
        ]
    if profile == "benchmark-policy":
        return ["-PolicySelfTest"]
    raise ValueError("Unknown reviewed PowerShell profile: " + profile)


def run_suite(suite, repo, build_root, output, timeout):
    started = time.monotonic()
    result = {
        "schema": 1,
        "suite": suite["id"],
        "status": "BLOCKED",
        "reason": "",
        "exit_code": None,
        "elapsed_seconds": 0,
        "coverage_scope": suite["coverage_scope"],
        "coverage_complete": False,
        "exclusions": [],
        "cases": [],
        "phase": "preflight",
        "attempted_tests": False,
        "retryable": False,
        "blocker_kind": None,
    }
    work = output.parent / (output.stem + ".artifacts")
    work.mkdir(parents=True, exist_ok=True)
    temporary_root = work / "tmp"
    temporary_root.mkdir(parents=True, exist_ok=True)
    log = work / "suite.log"
    result["log"] = str(log)

    setup_seconds = 0.0

    def finish(status, reason, code=None, *, retryable=None, blocker_kind=None):
        if retryable is None:
            retryable = status == "BLOCKED" and result["phase"] != "tests"
        result.update(
            retryable=retryable,
            blocker_kind=blocker_kind
            or ("transient-prerequisite" if retryable else None),
            status=status,
            reason=reason,
            exit_code=code,
            elapsed_seconds=round(time.monotonic() - started - setup_seconds, 3),
            setup_seconds=round(setup_seconds, 3),
            elapsed_wall_seconds=round(time.monotonic() - started, 3),
            coverage_complete=status == "PASS" and not result["exclusions"],
        )
        write_json(output, result)
        return 0 if status == "PASS" else 1 if status == "FAIL" else 2

    def run(argv, *, native=False, setup=False, virtual_display=False):
        nonlocal setup_seconds
        command_started = time.monotonic()
        result["phase"] = "setup" if setup else "tests"
        remaining = timeout - (time.monotonic() - started)
        if remaining <= 0:
            raise subprocess.TimeoutExpired(argv, timeout)
        with log.open("a", encoding="utf-8") as stream:
            stream.write("COMMAND " + json.dumps([str(a) for a in argv]) + "\n")
            stream.flush()
            process = subprocess.Popen(
                argv,
                cwd=repo,
                stdout=stream,
                stderr=subprocess.STDOUT,
                env={
                    **os.environ,
                    "TMPDIR": str(temporary_root),
                    "TEMP": str(temporary_root),
                    "TMP": str(temporary_root),
                    **({"SDL_VIDEODRIVER": "x11"} if virtual_display else {}),
                    **(
                        {"SDL_AUDIODRIVER": "dummy", "SDL_VIDEODRIVER": "dummy"}
                        if native
                        else {}
                    ),
                },
                start_new_session=True,
            )
            if not setup:
                result["attempted_tests"] = True
            try:
                return process.wait(timeout=remaining)
            except BaseException:
                # Compiler/test descendants must not survive a cancelled owner.
                try:
                    os.killpg(process.pid, signal.SIGTERM)
                    process.wait(timeout=5)
                except (ProcessLookupError, subprocess.TimeoutExpired):
                    pass
                finally:
                    # A parent can exit before its stubborn children. Reap the entire
                    # session even when waiting for the direct child already succeeded.
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    process.wait()
                raise
            finally:
                if setup:
                    setup_seconds += time.monotonic() - command_started

    missing = [tool for tool in suite.get("executables", []) if not shutil.which(tool)]
    missing += [
        module
        for module in suite.get("python_modules", [])
        if importlib.util.find_spec(module) is None
    ]
    if missing:
        return finish("BLOCKED", "Missing prerequisites: " + ", ".join(missing))
    if sys.platform != "linux":
        return finish(
            "BLOCKED", "This initial host adapter is qualified for Linux only"
        )
    try:
        if suite["kind"] == "public-powershell":
            script = target_file(repo, suite["path"])
            if not script.is_file():
                return finish(
                    "BLOCKED",
                    "Profile script absent from target commit",
                    retryable=False,
                    blocker_kind="target-capability",
                )
            for game in suite.get("games", []):
                build = build_root / ("engine-" + game)
                code = run(native_configure_arguments(repo, build, game), setup=True)
                if code:
                    return finish(
                        "BLOCKED",
                        "Native profile configuration failed; see log",
                        code,
                        retryable=True,
                    )
                targets = [
                    name.format(game=game)
                    for name in suite.get(
                        "build_targets", ["dxx-redux-" + game + suite["target_suffix"]]
                    )
                ]
                code = run(
                    [
                        "cmake",
                        "--build",
                        str(build),
                        "--parallel",
                        "2",
                        "--target",
                        *targets,
                    ],
                    setup=True,
                )
                if code:
                    return finish(
                        "BLOCKED",
                        "Native profile build failed; see log",
                        code,
                        retryable=True,
                    )
            if suite.get("public_data"):
                setup = target_file(
                    repo, "android/helpers/setup_shareware_test_data.ps1"
                )
                code = run(
                    [
                        "pwsh",
                        "-NoProfile",
                        "-NonInteractive",
                        "-File",
                        str(setup),
                        "-Game",
                        "both",
                        "-Download",
                        "-BuildDirectory",
                        str(build_root / "shareware-fixture"),
                    ],
                    setup=True,
                )
                if code:
                    return finish(
                        "BLOCKED",
                        "Verified public fixture preparation failed; see log",
                        code,
                        retryable=True,
                    )
            argv = ["pwsh", "-NoProfile", "-NonInteractive", "-File", str(script)]
            argv += public_profile_arguments(suite, repo, build_root, work)
            if suite.get("virtual_display"):
                argv = ["xvfb-run", "-a"] + argv
            code = run(argv, virtual_display=suite.get("virtual_display", False))
            status = "PASS" if code == 0 else "BLOCKED" if code == 2 else "FAIL"
            result["cases"] = [
                {
                    "id": suite["id"],
                    "status": status,
                    "reason": "Existing owner assertions for the selected public profile; aggregate receipt",
                }
            ]
            result["exclusions"] = [
                {
                    "id": suite["id"] + ":" + item["id"],
                    "status": "SKIP",
                    "reason": item["reason"],
                }
                for item in suite.get("profile_exclusions", [])
            ]
            if code == 2:
                result["exclusions"] += result["cases"]
            return finish(
                status,
                "Reviewed profile completed; other profiles remain explicit exclusions",
                code,
                retryable=code == 2,
            )

        if suite["kind"] == "python-script":
            script = target_file(repo, suite["path"])
            if not script.is_file():
                return finish(
                    "BLOCKED",
                    "Test script absent from target commit",
                    retryable=False,
                    blocker_kind="target-capability",
                )
            code = run([sys.executable, str(script)])
            result["cases"] = [
                {
                    "id": suite["id"],
                    "status": "PASS" if code == 0 else "FAIL",
                    "reason": "Aggregate assertion-script result; no internal case count",
                }
            ]
            return finish(
                "PASS" if code == 0 else "FAIL", "Assertion script completed", code
            )

        if suite["kind"] == "python-unittest":
            if not target_file(repo, suite["path"]).is_file():
                return finish(
                    "BLOCKED",
                    "Test module absent from target commit",
                    retryable=False,
                    blocker_kind="target-capability",
                )
            child = work / "unittest-results.json"
            if child.exists():
                return finish("BLOCKED", "Use a fresh build root; prior result exists")
            code = run(
                [
                    sys.executable,
                    str(Path(__file__).resolve()),
                    "--unittest-worker",
                    str(repo),
                    suite["path"],
                    str(child),
                ]
            )
            if not child.exists():
                return finish("FAIL", "Test process did not produce results", code)
            data = json.loads(child.read_text(encoding="utf-8"))
            result["counts"] = {
                "total": data["tests"],
                "failed": len(data["failures"]),
                "skipped": len(data["skipped"]),
            }
            result["cases"] = data["passed"] + data["failures"] + data["skipped"]
            result["exclusions"] = data["skipped"]
            status = (
                "FAIL"
                if code or not data["successful"]
                else "BLOCKED"
                if not data["tests"]
                else "PASS"
            )
            return finish(
                status,
                "Unittest module completed; skipped cases are incomplete coverage",
                code,
            )

        if suite["kind"] == "standalone-cmake":
            light = False
            source = target_file(repo, suite["source"])
            if not (source / "CMakeLists.txt").is_file():
                return finish(
                    "BLOCKED",
                    "Standalone CMake owner absent from target commit",
                    retryable=False,
                    blocker_kind="target-capability",
                )
            if suite.get("public_soundfont"):
                helper = target_file(repo, "android/get_deps/helpers/get_soundfont.sh")
                code = run(["bash", str(helper)], setup=True)
                if code:
                    return finish(
                        "BLOCKED",
                        "Verified public SoundFont setup failed",
                        code,
                        retryable=True,
                    )
            build = build_root / suite["build_subdirectory"]
            code = run(
                [
                    "cmake",
                    "-S",
                    str(source),
                    "-B",
                    str(build),
                    "-DCMAKE_BUILD_TYPE=Release",
                ],
                setup=True,
            )
            if code:
                return finish(
                    "BLOCKED",
                    "Standalone CMake configuration failed; see log",
                    code,
                    retryable=True,
                )
            code = run(
                [
                    "cmake",
                    "--build",
                    str(build),
                    "--parallel",
                    "2",
                    "--target",
                    suite["build_target"],
                ],
                setup=True,
            )
            if code:
                return finish(
                    "BLOCKED",
                    "Standalone CMake build failed; see log",
                    code,
                    retryable=True,
                )
        elif suite["kind"] == "extractor":
            source = target_file(repo, "android/app/src/main/cpp/extract")
            cmake_file = source / "CMakeLists.txt"
            if not cmake_file.is_file():
                return finish("BLOCKED", "Standalone extractor entry point is absent")
            if suite.get("extra_test") == "strict-fingerprint":
                script = target_file(
                    repo, "android/tests/test_fingerprint_match_strict.py"
                )
                albums = target_file(
                    repo, "android/app/src/main/assets/known_albums.jsonc"
                )
                if not script.is_file() or "--matcher" not in script.read_text(
                    encoding="utf-8"
                ):
                    return finish(
                        "BLOCKED",
                        "Target strict-fingerprint script lacks portable --matcher support",
                        retryable=False,
                        blocker_kind="target-capability",
                    )
                if not albums.is_file():
                    return finish(
                        "BLOCKED", "Maintained known_albums.jsonc fixture is absent"
                    )
            light = suite["profile"] == "light"
            if light and "DXX_EXTRACT_DEPENDENCY_LIGHT" not in cmake_file.read_text(
                encoding="utf-8"
            ):
                return finish(
                    "BLOCKED",
                    "Target commit does not support the optional light profile",
                    retryable=False,
                    blocker_kind="target-capability",
                )
            build = (
                build_root
                / ("extractor-light" if light else "extractor-full")
                / "build"
            )
            args = [
                "cmake",
                "-S",
                str(source),
                "-B",
                str(build),
                "-DCMAKE_BUILD_TYPE=Release",
            ]
            if light:
                args.append("-DDXX_EXTRACT_DEPENDENCY_LIGHT=ON")
            else:
                args.append("-DDXX_EXTRACT_DEPENDENCY_LIGHT=OFF")
            code = run(args, setup=True)
            if code:
                text = log.read_text(encoding="utf-8", errors="replace")
                unavailable = any(
                    s in text
                    for s in [
                        "Failed to download",
                        "Couldn't resolve host",
                        "Could NOT find",
                    ]
                )
                return finish(
                    "BLOCKED" if unavailable else "FAIL",
                    "Extractor configuration failed; see log",
                    code,
                )
            build_args = ["cmake", "--build", str(build), "--parallel", "2"]
            if suite.get("extra_test") == "strict-fingerprint":
                build_args += ["--target", "fingerprint_match"]
            code = run(build_args, setup=True)
            if code:
                return finish("FAIL", "Extractor build failed; see log", code)
            if suite.get("extra_test") == "strict-fingerprint":
                code = run(
                    [
                        sys.executable,
                        str(script),
                        "--matcher",
                        str(build / "fingerprint_match"),
                        "--known-discs",
                        str(albums),
                    ]
                )
                result["cases"] = [
                    {
                        "id": suite["id"],
                        "status": "PASS" if code == 0 else "FAIL",
                        "reason": "Existing strict database admission assertions",
                    }
                ]
                return finish(
                    "PASS" if code == 0 else "FAIL",
                    "Strict fingerprint assertions completed",
                    code,
                )
        else:
            light = False
            build = build_root / suite["build_subdirectory"]
            game = suite["id"].rsplit("-", 1)[1]
            source = target_file(repo, game)
            if not (source / "CMakeLists.txt").is_file():
                return finish(
                    "BLOCKED", "Native game source is absent from target checkout"
                )
            # Same assertion-preserving options as run-linux-build.sh --test, with a
            # caller-owned build directory and separately captured leaf results.
            args = native_configure_arguments(repo, build, game)
            code = run(args, setup=True)
            if code:
                text = log.read_text(encoding="utf-8", errors="replace")
                unavailable = any(
                    s in text
                    for s in [
                        "Failed to download",
                        "Couldn't resolve host",
                        "Could NOT find",
                    ]
                )
                return finish(
                    "BLOCKED" if unavailable else "FAIL",
                    "Native configuration failed; see log",
                    code,
                )
            code = run(["cmake", "--build", str(build), "--parallel", "2"], setup=True)
            if code:
                return finish("FAIL", "Native build failed; see log", code)
        junit = work / "ctest-results.xml"
        if junit.exists():
            return finish(
                "BLOCKED", "Use a fresh result directory; prior CTest result exists"
            )
        code = run(
            [
                "ctest",
                "--test-dir",
                str(build),
                "--output-on-failure",
                "--output-junit",
                str(junit),
                "--parallel",
                "1" if suite["kind"] == "configured-ctest" else "2",
                "--no-tests=error",
                "--timeout",
                "120",
            ],
            native=suite["kind"] == "configured-ctest",
        )
        if not junit.exists():
            return finish(
                "FAIL", "CTest did not produce machine-readable results", code
            )
        cases = read_ctest_results(junit)
        status, exclusions = classify_cases(cases, light=light)
        if code:
            status = "FAIL"
        result["cases"], result["exclusions"] = cases, exclusions
        result["counts"] = {
            "total": len(cases),
            "passed": sum(c["status"] == "PASS" for c in cases),
            "excluded": len(exclusions),
            "failed": sum(c["status"] == "FAIL" for c in cases),
        }
        return finish(
            status, "CTest completed; exclusions remain outside proven coverage", code
        )
    except (SuiteCancelled, KeyboardInterrupt):
        return finish(
            "BLOCKED",
            "Suite cancelled; child process group terminated",
            retryable=True,
            blocker_kind="interrupted",
        )
    except subprocess.TimeoutExpired:
        return finish(
            "BLOCKED" if not result["attempted_tests"] else "FAIL",
            "Suite exceeded its wall-clock timeout",
            retryable=not result["attempted_tests"],
            blocker_kind="setup-timeout"
            if not result["attempted_tests"]
            else "test-timeout",
        )
    except (OSError, ValueError, ET.ParseError) as error:
        return finish("FAIL", str(error))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--unittest-worker":
        return unittest_worker(
            Path(sys.argv[2]).resolve(), sys.argv[3], Path(sys.argv[4])
        )
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list-json", action="store_true")
    parser.add_argument("--suite")
    parser.add_argument("--repo", type=Path)
    parser.add_argument("--build-root", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--timeout-seconds", type=int, default=1800)
    args = parser.parse_args()
    suites = load_suites()
    if args.list_json:
        print(json.dumps({"schema": 1, "suites": list(suites.values())}, indent=2))
        return 0
    if args.suite not in suites or not all([args.repo, args.build_root, args.output]):
        parser.error(
            "select a known --suite and provide --repo, --build-root, and --output"
        )
    repo = args.repo.resolve()
    if not repo.is_dir():
        parser.error("target checkout does not exist")
    if args.timeout_seconds <= 0:
        parser.error("timeout must be positive")
    previous_sigterm = signal.signal(signal.SIGTERM, handle_termination)
    try:
        return run_suite(
            suites[args.suite],
            repo,
            args.build_root.resolve(),
            args.output.resolve(),
            args.timeout_seconds,
        )
    finally:
        signal.signal(signal.SIGTERM, previous_sigterm)


if __name__ == "__main__":
    raise SystemExit(main())

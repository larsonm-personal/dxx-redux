"""Asset-free checks for the opt-in D2 developer compile CI job.

This qualifies compilation and linking only, not runtime behavior or gameplay.
"""

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
WORKFLOW = ROOT / ".github/workflows/xl-compat.yml"
TARGET = "dxx-redux-d2-headless-metadata"
SOURCE = "android/XL_compat/runtime/lifecycle_test.cpp"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def check_workflow(contents):
    require("  developer-compile:\n" in contents, "developer compile job missing")
    job = contents.split("  developer-compile:\n", 1)[1].split(
        "  developer-release-compile:\n", 1
    )[0]
    require("    needs: stack\n" in job, "stack gate missing")
    require("if: needs.stack.outputs.run == 'true'" in job, "XL profile gate missing")
    require("runs-on: ubuntu-24.04" in job, "hosted Linux runner missing")
    require("timeout-minutes: 45" in job, "job timeout missing")
    require(
        "permissions:\n  contents: read" in contents, "read-only permissions missing"
    )
    require(
        "persist-credentials: false" in job, "checkout credentials must not persist"
    )
    require(
        job.count("persist-credentials: false") == 2,
        "both checkouts must drop credentials",
    )
    require(
        "ref: ${{ github.workflow_sha }}" in job
        and "sparse-checkout: .github/workflows/xl-compat.yml" in job
        and "sparse-checkout-cone-mode: false" in job,
        "executed workflow checkout missing",
    )
    require(
        'test "$(git -C workflow-source rev-parse HEAD)" = "$WORKFLOW_SHA"' in job,
        "executed workflow SHA not checked",
    )
    require(
        'test "$(sha256sum .github/workflows/xl-compat.yml | cut -d\' \' -f1)" = "$EXECUTED_WORKFLOW_HASH"'
        in job,
        "merge-ref and head workflow content not compared",
    )
    head = "${{ github.event.pull_request.head.sha || github.sha }}"
    require(
        job.count(head) == 3, "candidate head must bind checkout, receipt and artifact"
    )
    require(
        'test "$(git rev-parse HEAD)" = "$EXPECTED_HEAD"' in job, "head receipt missing"
    )
    require("-DDXX_XL_DEVELOPER_TESTS=ON" in job, "developer option missing")
    require("-DCMAKE_BUILD_TYPE=Debug" in job, "debug build option missing")
    require("test_probe_artifact.py" in job, "artifact policy tests missing")
    require("-DOPENGL=ON" in job, "production renderer option missing")
    require("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" in job, "compile receipt missing")
    require("--parallel 2 --target " + TARGET in job, "bounded target build missing")
    require(
        "--verify-build build-xl-developer-ci" in job, "build receipt check missing"
    )
    require(
        "libphysfs-dev" in job
        and "libsdl-mixer1.2-dev" in job
        and "libglew-dev" in job,
        "production dependencies missing",
    )
    forbidden = (
        "setup_shareware_test_data",
        "run_xl_originals",
        "--download",
        "xvfb-run",
        "ctest",
        "./build-xl-developer-ci/main/",
        "secrets.",
    )
    require(
        not any(token in job for token in forbidden),
        "runtime, fixture, secret or artifact route in job",
    )
    require(
        job.count("actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02")
        == 1,
        "pinned official artifact uploader missing",
    )
    for token in (
        "probe_artifact.py create",
        '--head "$EXPECTED_HEAD"',
        '--run-id "$PROBE_RUN_ID"',
        '--run-attempt "$PROBE_RUN_ATTEMPT"',
        "path: temp/xl-developer-probe-packet/",
        "if-no-files-found: error",
        "retention-days: 1",
    ):
        require(token in job, f"artifact policy missing: {token}")
    require(
        "name: xl-developer-probe-${{ github.event.pull_request.head.sha || github.sha }}-${{ github.run_attempt }}"
        in job,
        "artifact identity not bound to candidate head and attempt",
    )
    check_release_workflow(contents)


def check_release_workflow(contents):
    require("  developer-release-compile:\n" in contents, "Release job missing")
    job = contents.split("  developer-release-compile:\n", 1)[1]
    for token in (
        "needs: stack",
        "if: needs.stack.outputs.run == 'true' && "
        "github.event_name == 'pull_request' && "
        "(github.head_ref == 'fix/pumo-release-probe-packet-20261108' || "
        "github.head_ref == 'fix/pumo-accepted-robot-damage-20261108')",
        "name: XL developer Release source compile (no game assets)",
        "runs-on: ubuntu-24.04",
        "timeout-minutes: 45",
        "ref: ${{ github.workflow_sha }}",
        "sparse-checkout: .github/workflows/xl-compat.yml",
        "sparse-checkout-cone-mode: false",
        'test "$(git -C workflow-source rev-parse HEAD)" = "$WORKFLOW_SHA"',
        'test "$(sha256sum .github/workflows/xl-compat.yml | cut -d\' \' -f1)" = "$EXECUTED_WORKFLOW_HASH"',
        "ref: ${{ github.event.pull_request.head.sha || github.sha }}",
        'test "$(git rev-parse HEAD)" = "$EXPECTED_HEAD"',
        "-DCMAKE_BUILD_TYPE=Release",
        "-DOPENGL=ON",
        "-DOpenGL_GL_PREFERENCE=LEGACY",
        "-DDXX_XL_DEVELOPER_TESTS=ON",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "--parallel 2 --target " + TARGET,
        "--verify-build build-xl-developer-release-ci --mode Release",
        "probe_artifact.py create",
        '--head "$EXPECTED_HEAD"',
        '--run-id "$PROBE_RUN_ID"',
        '--run-attempt "$PROBE_RUN_ATTEMPT" --mode Release',
        "path: temp/xl-developer-release-probe-packet/",
        "if-no-files-found: error",
        "retention-days: 1",
        "test_developer_compile_ci.py",
        "test_probe_artifact.py",
        "libphysfs-dev",
        "libsdl-mixer1.2-dev",
        "libglew-dev",
    ):
        require(token in job, f"Release policy missing: {token}")
    require(
        job.count("persist-credentials: false") == 2, "Release checkout credentials"
    )
    require(
        job.count("actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02")
        == 1,
        "Release uploader missing",
    )
    require(
        "name: xl-developer-release-probe-${{ github.event.pull_request.head.sha || github.sha }}-${{ github.run_attempt }}"
        in job,
        "Release artifact identity missing",
    )
    for forbidden in (
        "setup_shareware_test_data",
        "run_xl_originals",
        "--download",
        "xvfb-run",
        "ctest",
        "secrets.",
    ):
        require(forbidden not in job, "Release runtime, fixture or secret route")


def verify_build(build, mode="Debug"):
    require(mode in ("Debug", "Release"), "unsupported build mode")
    cache = (build / "CMakeCache.txt").read_text()
    for key, cache_type, value in (
        ("CMAKE_BUILD_TYPE", "STRING", mode),
        ("DXX_XL_DEVELOPER_TESTS", "BOOL", "ON"),
        ("OPENGL", "BOOL", "ON"),
        ("BUILD_TESTING", "BOOL", "OFF"),
        ("CMAKE_EXPORT_COMPILE_COMMANDS", "(?:BOOL|UNINITIALIZED)", "ON"),
    ):
        require(
            re.search(rf"^{key}:{cache_type}={value}$", cache, re.MULTILINE)
            is not None,
            f"{key} must be {value}",
        )
    if mode == "Release":
        require(
            re.search(
                r"^OpenGL_GL_PREFERENCE:(?:STRING|UNINITIALIZED)=LEGACY$",
                cache,
                re.MULTILINE,
            )
            is not None,
            "Release OpenGL preference must be LEGACY",
        )
    commands = json.loads((build / "compile_commands.json").read_text())
    entries = [
        entry for entry in commands if Path(entry["file"]).as_posix().endswith(SOURCE)
    ]
    require(len(entries) == 1, "expected exactly one lifecycle source compile")
    command = entries[0]["command"]
    require(
        "CMakeFiles/" + TARGET + ".dir/" in command,
        "lifecycle source bound to wrong target",
    )
    for macro in (
        "DXX_XL_DEVELOPER_PROBE",
        "DXX_PUMO_CONTACT_DEVELOPER_PROBE",
        "DXX_XL_CHECKPOINT_EXPORT",
        "DXX_XL_DESCRIPTOR_TEST",
    ):
        require(
            re.search(r"(?:^|\s)-D" + macro + r"(?:\s|$)", command) is not None,
            f"missing {macro} in lifecycle compile",
        )
    require(
        (build / "main/xl-preview-lifecycle").is_file(),
        "linked developer target missing",
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--check-workflow", action="store_true")
    group.add_argument("--verify-build", type=Path)
    parser.add_argument("--mode", choices=("Debug", "Release"), default="Debug")
    args = parser.parse_args()
    if args.check_workflow:
        check_workflow(WORKFLOW.read_text())
    else:
        verify_build(args.verify_build, args.mode)
    print("XL developer compile CI contract passed (compile only, no runtime proof)")


if __name__ == "__main__":
    main()

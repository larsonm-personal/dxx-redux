"""Build and check an asset-free developer probe packet, never execute it.

Local packet checks are not a substitute for authenticated GitHub run/artifact
metadata or private-fixture runtime qualification.
"""

import argparse
import hashlib
import json
import os
import platform
import re
import shlex
import shutil
import stat
import struct
import subprocess
import zipfile
from pathlib import Path, PurePosixPath

from developer_compile_ci import ROOT, SOURCE, TARGET, verify_build

BINARY = "xl-preview-lifecycle"
NOTICES = ("COPYING.txt", "d2/COPYING.txt")
RECEIPT = "provenance.json"
FILES = frozenset((BINARY, RECEIPT, *NOTICES))
MAX_BINARY = 160 * 1024 * 1024
MAX_ARCHIVE = 170 * 1024 * 1024
WORKFLOW = ".github/workflows/xl-compat.yml"
REPO = "larsonm-personal/dxx-redux"
JOB = "XL developer source compile (no game assets)"
WORKFLOW_ID = 394846952


def compile_identity(build):
    """Record public, path-free properties of the actual lifecycle TU compile."""
    commands = json.loads((build / "compile_commands.json").read_text())
    entries = [
        entry for entry in commands if Path(entry["file"]).as_posix().endswith(SOURCE)
    ]
    require(len(entries) == 1, "lifecycle compile identity missing")
    args = shlex.split(entries[0]["command"])
    macros = sorted(
        arg[2:]
        for arg in args
        if re.fullmatch(r"-D[A-Za-z_][A-Za-z_0-9]*(?:=[A-Za-z_0-9]+)?", arg)
    )
    switches = sorted(
        arg
        for arg in args
        if re.fullmatch(
            r"-(?:O[0-3sgz]|g[0-9]?|f[A-Za-z_0-9=-]+|m[A-Za-z_0-9=-]+|std=[A-Za-z_0-9+]+)",
            arg,
        )
    )
    require(
        "DXX_XL_DEVELOPER_PROBE" in macros
        and "DXX_PUMO_CONTACT_DEVELOPER_PROBE" in macros,
        "developer macro identity missing",
    )
    require("NDEBUG" not in macros, "release macro in Debug probe")
    require(platform.machine() == "x86_64", "unexpected runner ABI")
    compiler_files = list((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    require(len(compiler_files) == 1, "CMake compiler identity missing")
    compiler_info = compiler_files[0].read_text()

    def cmake_value(key):
        match = re.search(
            r"^set\(" + key + r' "([A-Za-z0-9.]+)"\)$', compiler_info, re.MULTILINE
        )
        require(match is not None, f"CMake {key} identity missing")
        return match.group(1)

    return {
        "abi": "linux-x86_64",
        "compiler": {
            "id": cmake_value("CMAKE_CXX_COMPILER_ID"),
            "version": cmake_value("CMAKE_CXX_COMPILER_VERSION"),
        },
        "macros": macros,
        "switches": switches,
    }


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def file_digest(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def preflight_zip(archive):
    """Reject oversized ZIP directory metadata before ZipFile allocates entries."""
    size = archive.stat().st_size
    require(22 <= size <= MAX_ARCHIVE, "archive outside size bound")
    with archive.open("rb") as source:
        source.seek(max(0, size - 65577))
        tail = source.read(65577)
    offset = tail.rfind(b"PK\x05\x06")
    require(offset >= 0 and offset + 22 <= len(tail), "ZIP end record missing")
    require(
        offset < 20 or tail[offset - 20 : offset - 16] != b"PK\x06\x07",
        "ZIP64 locator is not allowed",
    )
    (
        _,
        disk,
        directory_disk,
        disk_entries,
        entries,
        directory_size,
        directory_offset,
        comment,
    ) = struct.unpack_from("<4s4H2LH", tail, offset)
    require(
        offset + 22 + comment == len(tail)
        and disk == directory_disk == 0
        and disk_entries == entries == len(FILES)
        and directory_size <= 4096
        and directory_offset + directory_size <= size - 22,
        "ZIP directory outside packet bounds",
    )


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def exact_head(head):
    require(re.fullmatch(r"[0-9a-f]{40}", head) is not None, "invalid head")
    require(git("rev-parse", "HEAD") == head, "checkout head mismatch")
    return git("rev-parse", "HEAD^{tree}")


def create(build, output, head, run_id, run_attempt):
    require(re.fullmatch(r"[1-9][0-9]*", run_id) is not None, "invalid run id")
    require(re.fullmatch(r"[1-9][0-9]*", run_attempt) is not None, "invalid attempt")
    tree = exact_head(head)
    verify_build(build)
    source = build / "main" / BINARY
    require(source.is_file() and not source.is_symlink(), "binary must be regular")
    size = source.stat().st_size
    require(0 < size <= MAX_BINARY, "binary outside size bound")
    require(not output.exists(), "packet destination must be new")
    output.mkdir(parents=True)
    shutil.copyfile(source, output / BINARY)
    os.chmod(output / BINARY, 0o755)
    for notice in NOTICES:
        (output / notice).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / notice, output / notice)
    receipt = {
        "schema": 1,
        "repository": REPO,
        "workflow": WORKFLOW,
        "run_id": int(run_id),
        "run_attempt": int(run_attempt),
        "head": head,
        "tree": tree,
        "platform": "ubuntu-24.04",
        "target": TARGET,
        "compile": compile_identity(build),
        "mode": {
            "build_type": "Debug",
            "opengl": True,
            "developer_tests": True,
            "build_testing": False,
        },
        "files": {
            name: {
                "size": (output / name).stat().st_size,
                "sha256": file_digest(output / name),
            }
            for name in (BINARY, *NOTICES)
        },
        "qualification": "source-compile-and-link-only; no engine execution or game assets",
        "workflow_binding": "runner comparison only; GitHub REST does not independently attest executed YAML bytes",
    }
    (output / RECEIPT).write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    check_manifest(
        {
            name: {
                "size": (output / name).stat().st_size,
                "sha256": file_digest(output / name),
            }
            for name in FILES - {RECEIPT}
        },
        receipt,
        repository=REPO,
        workflow=WORKFLOW,
        run_id=int(run_id),
        run_attempt=int(run_attempt),
        head=head,
        tree=tree,
        platform="ubuntu-24.04",
    )


def check_files(
    files,
    receipt,
    *,
    repository,
    workflow,
    run_id,
    run_attempt,
    head,
    tree,
    platform,
    expected_compile=None,
):
    require(set(files) == FILES, "missing or unexpected packet files")
    return check_manifest(
        {
            name: {"size": len(files[name]), "sha256": sha256(files[name])}
            for name in FILES - {RECEIPT}
        },
        receipt,
        repository=repository,
        workflow=workflow,
        run_id=run_id,
        run_attempt=run_attempt,
        head=head,
        tree=tree,
        platform=platform,
        expected_compile=expected_compile,
    )


def check_manifest(
    manifest,
    receipt,
    *,
    repository,
    workflow,
    run_id,
    run_attempt,
    head,
    tree,
    platform,
    expected_compile=None,
):
    require(set(manifest) == FILES - {RECEIPT}, "missing or unexpected packet files")
    require(receipt.get("schema") == 1, "unknown receipt schema")
    for key, expected in (
        ("repository", repository),
        ("workflow", workflow),
        ("run_id", run_id),
        ("run_attempt", run_attempt),
        ("head", head),
        ("tree", tree),
        ("platform", platform),
        ("target", TARGET),
    ):
        require(receipt.get(key) == expected, f"{key} mismatch")
    require(
        receipt.get("mode")
        == {
            "build_type": "Debug",
            "opengl": True,
            "developer_tests": True,
            "build_testing": False,
        },
        "mode mismatch",
    )
    compile_data = receipt.get("compile")
    require(
        isinstance(compile_data, dict) and compile_data.get("abi") == "linux-x86_64",
        "target ABI mismatch",
    )
    compiler = compile_data.get("compiler")
    require(
        isinstance(compiler, dict)
        and compiler.get("id") in ("GNU", "Clang")
        and re.fullmatch(r"[0-9]+(?:\.[0-9]+)*", compiler.get("version", ""))
        is not None,
        "compiler identity mismatch",
    )
    require(
        isinstance(compile_data.get("macros"), list)
        and "DXX_XL_DEVELOPER_PROBE" in compile_data["macros"]
        and "DXX_PUMO_CONTACT_DEVELOPER_PROBE" in compile_data["macros"]
        and "NDEBUG" not in compile_data["macros"],
        "compile macro mismatch",
    )
    require(isinstance(compile_data.get("switches"), list), "compile flags missing")
    if expected_compile is not None:
        require(compile_data == expected_compile, "reviewed compiler identity mismatch")
    require(
        receipt.get("qualification")
        == "source-compile-and-link-only; no engine execution or game assets",
        "qualification mismatch",
    )
    require(
        receipt.get("workflow_binding")
        == "runner comparison only; GitHub REST does not independently attest executed YAML bytes",
        "workflow binding limitation missing",
    )
    require(
        set(receipt.get("files", {})) == FILES - {RECEIPT}, "file manifest mismatch"
    )
    for name in FILES - {RECEIPT}:
        require(
            receipt["files"][name] == manifest[name],
            f"{name} digest mismatch",
        )
    require(0 < manifest[BINARY]["size"] <= MAX_BINARY, "binary outside size bound")
    require(
        all(0 < manifest[name]["size"] <= 64 * 1024 for name in (*NOTICES,)),
        "license notice outside size bound",
    )


def verify_zip(
    archive,
    *,
    repository,
    workflow,
    run_id,
    run_attempt,
    head,
    tree,
    platform,
    artifact_name,
    artifact_digest,
    expected_compile=None,
):
    """Verify bytes after a caller authenticates run and artifact metadata via GitHub.

    The caller must separately check exact repository, workflow path, event PR
    head, run attempt, successful job, artifact id/name/expiry and GitHub's ZIP
    digest. An artifact receipt cannot authenticate itself.
    """
    require(repository == REPO and workflow == WORKFLOW, "wrong authenticated source")
    require(re.fullmatch(r"[0-9a-f]{40}", head) is not None, "invalid head")
    require(re.fullmatch(r"[0-9a-f]{40}", tree) is not None, "invalid tree")
    require(run_id > 0 and run_attempt > 0, "invalid run identity")
    require(
        artifact_name == f"xl-developer-probe-{head}-{run_attempt}",
        "artifact name mismatch",
    )
    preflight_zip(archive)
    require(
        "sha256:" + file_digest(archive) == artifact_digest,
        "GitHub archive digest mismatch",
    )
    manifest = {}
    receipt_data = None
    with zipfile.ZipFile(archive) as z:
        for info in z.infolist():
            name = info.filename
            path = PurePosixPath(name)
            require(
                name in FILES
                and not path.is_absolute()
                and ".." not in path.parts
                and name not in manifest
                and not (name == RECEIPT and receipt_data is not None),
                "unsafe or unexpected ZIP member",
            )
            mode = info.external_attr >> 16
            require(
                stat.S_IFMT(mode) in (0, stat.S_IFREG) and not info.is_dir(),
                "nonregular ZIP member",
            )
            limit = MAX_BINARY if name == BINARY else 64 * 1024
            require(
                0 < info.file_size <= limit and info.compress_size <= MAX_ARCHIVE,
                "ZIP member too large",
            )
            digest = hashlib.sha256()
            size = 0
            with z.open(info) as source:
                for block in iter(lambda: source.read(1024 * 1024), b""):
                    size += len(block)
                    require(size <= limit, "decompressed ZIP member too large")
                    digest.update(block)
                    if name == RECEIPT:
                        receipt_data = (receipt_data or b"") + block
            if name != RECEIPT:
                manifest[name] = {"size": size, "sha256": digest.hexdigest()}
    require(
        set(manifest) == FILES - {RECEIPT} and receipt_data is not None,
        "missing ZIP members",
    )
    receipt = json.loads(receipt_data)
    check_manifest(
        manifest,
        receipt,
        repository=repository,
        workflow=workflow,
        run_id=run_id,
        run_attempt=run_attempt,
        head=head,
        tree=tree,
        platform=platform,
        expected_compile=expected_compile,
    )
    return receipt


def authenticate_metadata(
    run, artifacts, jobs, *, head, tree, pr_number, executed_workflow, reviewed_workflow
):
    """Check official GitHub API objects before treating a ZIP as input.

    The caller fetches the run by id, artifact list by run id, and full
    jobs JSON and the workflow at the official merge SHA using authenticated
    github_fetch. The simplified jobs wrapper
    omits run_attempt and must not be used. Receipt data is not
    authoritative. Even after comparing independently fetched merge-ref and
    reviewed head YAML, REST metadata alone is not an execution attestation.
    This function never downloads or executes anything.
    """
    repository = run.get("repository", {})
    require(repository.get("full_name") == REPO, "run repository mismatch")
    require(run.get("workflow_id") == WORKFLOW_ID, "workflow identity mismatch")
    merge_ref = f"refs/pull/{pr_number}/merge"
    references = [
        r
        for r in run.get("referenced_workflows", [])
        if r.get("ref") == merge_ref
        and r.get("path", "").startswith(REPO + "/.github/workflows/stack-ci.yml@")
    ]
    require(len(references) == 1, "official merge workflow reference missing")
    merge_sha = references[0].get("sha", "")
    require(
        re.fullmatch(r"[0-9a-f]{40}", merge_sha) is not None
        and references[0]["path"]
        == REPO + "/.github/workflows/stack-ci.yml@" + merge_sha,
        "official merge workflow SHA mismatch",
    )
    require(
        isinstance(executed_workflow, bytes)
        and isinstance(reviewed_workflow, bytes)
        and executed_workflow == reviewed_workflow
        and sha256(reviewed_workflow) == sha256((ROOT / WORKFLOW).read_bytes()),
        "executed workflow differs from reviewed candidate source",
    )
    require(
        run.get("path") == WORKFLOW and run.get("event") == "pull_request",
        "run workflow/event mismatch",
    )
    require(
        run.get("status") == "completed" and run.get("conclusion") == "success",
        "run not successful",
    )
    require(
        run.get("head_sha") == head
        and run.get("head_commit", {}).get("tree_id") == tree,
        "run source mismatch",
    )
    matches = [
        pr for pr in run.get("pull_requests", []) if pr.get("number") == pr_number
    ]
    require(
        len(matches) == 1
        and matches[0].get("head", {}).get("sha") == head
        and matches[0].get("head", {}).get("repo", {}).get("id")
        == repository.get("id"),
        "PR head mismatch",
    )
    run_id, attempt = run.get("id"), run.get("run_attempt")
    require(
        isinstance(run_id, int)
        and run_id > 0
        and isinstance(attempt, int)
        and attempt > 0,
        "invalid run identity",
    )
    require(isinstance(artifacts, list), "artifact listing missing")
    matches = [
        a for a in artifacts if a.get("name") == f"xl-developer-probe-{head}-{attempt}"
    ]
    require(len(matches) == 1, "missing or multiple probe artifacts")
    artifact = matches[0]
    require(
        artifact.get("name") == f"xl-developer-probe-{head}-{attempt}"
        and artifact.get("expired") is False
        and artifact.get("workflow_run", {}).get("id") == run_id
        and artifact.get("workflow_run", {}).get("head_sha") == head
        and isinstance(artifact.get("id"), int)
        and artifact.get("id") > 0,
        "artifact identity/expiry mismatch",
    )
    require(
        re.fullmatch(r"sha256:[0-9a-f]{64}", artifact.get("digest", "")) is not None
        and 0 < artifact.get("size_in_bytes", 0) <= MAX_ARCHIVE,
        "artifact digest/size mismatch",
    )
    matches = [job for job in jobs if job.get("name") == JOB]
    require(
        len(matches) == 1
        and matches[0].get("run_id") == run_id
        and matches[0].get("run_attempt") == attempt
        and matches[0].get("head_sha") == head
        and "ubuntu-24.04" in matches[0].get("labels", [])
        and matches[0].get("status") == "completed"
        and matches[0].get("conclusion") == "success"
        and any(
            step.get("name") == "Upload source-only developer probe packet"
            and step.get("conclusion") == "success"
            for step in matches[0].get("steps", [])
        ),
        "developer packet job not successful",
    )
    return {
        "repository": REPO,
        "workflow": WORKFLOW,
        "run_id": run_id,
        "run_attempt": attempt,
        "head": head,
        "tree": tree,
        "platform": "ubuntu-24.04",
        "artifact_name": artifact["name"],
        "artifact_digest": artifact["digest"],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    create_parser = sub.add_parser("create")
    for flag in ("build", "output", "head", "run-id", "run-attempt"):
        create_parser.add_argument("--" + flag, required=True)
    args = parser.parse_args()
    if args.command == "create":
        create(
            Path(args.build),
            Path(args.output),
            args.head,
            args.run_id,
            args.run_attempt,
        )
    print("Source-only developer probe packet prepared; no runtime qualification")


if __name__ == "__main__":
    main()

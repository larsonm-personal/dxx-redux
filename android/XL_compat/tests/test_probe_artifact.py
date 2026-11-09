"""Source-only artifact negative checks; never run the probe."""

import json
import struct
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path

from probe_artifact import (
    BINARY,
    MAX_BINARY,
    NOTICES,
    RECEIPT,
    REPO,
    ROOT,
    WORKFLOW,
    authenticate_metadata,
    check_files,
    compile_identity,
    create,
    packet_name,
    sha256,
    verify_zip,
)

HEAD = "a" * 40
TREE = "b" * 40
NAME = f"xl-developer-probe-{HEAD}-2"


def packet():
    files = {
        BINARY: b"source-derived-executable",
        NOTICES[0]: b"license one",
        NOTICES[1]: b"license two",
    }
    receipt = {
        "schema": 1,
        "repository": REPO,
        "workflow": WORKFLOW,
        "run_id": 123,
        "run_attempt": 2,
        "head": HEAD,
        "tree": TREE,
        "platform": "ubuntu-24.04",
        "target": "dxx-redux-d2-headless-metadata",
        "compile": {
            "abi": "linux-x86_64",
            "compiler": {"id": "GNU", "version": "13.3.0"},
            "macros": ["DXX_XL_DEVELOPER_PROBE", "DXX_PUMO_CONTACT_DEVELOPER_PROBE"],
            "switches": ["-g"],
        },
        "mode": {
            "build_type": "Debug",
            "opengl": True,
            "developer_tests": True,
            "build_testing": False,
        },
        "files": {
            name: {"size": len(data), "sha256": sha256(data)}
            for name, data in files.items()
        },
        "qualification": "source-compile-and-link-only; no engine execution or game assets",
        "workflow_binding": "runner comparison only; GitHub REST does not independently attest executed YAML bytes",
    }
    files[RECEIPT] = json.dumps(receipt).encode()
    return files, receipt


class ArtifactTests(unittest.TestCase):
    def test_release_mode_is_caller_bound(self):
        files, receipt = packet()
        release = dict(receipt)
        release["mode"] = {
            **receipt["mode"], "build_type": "Release", "gl_preference": "LEGACY"
        }
        release["compile"] = {
            **receipt["compile"],
            "macros": [*receipt["compile"]["macros"], "NDEBUG"],
        }
        files[RECEIPT] = json.dumps(release).encode()
        args = {
            "repository": REPO, "workflow": WORKFLOW, "run_id": 123,
            "run_attempt": 2, "head": HEAD, "tree": TREE,
            "platform": "ubuntu-24.04",
        }
        check_files(files, release, **args, mode="Release")
        with self.assertRaises(ValueError):
            check_files(files, release, **args)
        with self.assertRaises(ValueError):
            check_files(files, release, **{**args, "head": "c" * 40}, mode="Release")
        for bad in (
            {**release["mode"], "gl_preference": "GLVND"},
            {**release["mode"], "build_type": "Debug"},
        ):
            with self.assertRaises(ValueError):
                check_files(files, {**release, "mode": bad}, **args, mode="Release")
        self.assertEqual(packet_name(HEAD, 2, "Release"), f"xl-developer-release-probe-{HEAD}-2")
        with self.assertRaises(ValueError):
            packet_name(HEAD, 2, "RelWithDebInfo")
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "release.zip"
            with zipfile.ZipFile(archive, "w") as output:
                for name, data in files.items():
                    output.writestr(name, data)
            zip_args = {
                **args,
                "artifact_name": packet_name(HEAD, 2, "Release"),
                "artifact_digest": "sha256:" + sha256(archive.read_bytes()),
            }
            verify_zip(archive, **zip_args, mode="Release")
            with self.assertRaises(ValueError):
                verify_zip(archive, **zip_args)
            with self.assertRaises(ValueError):
                verify_zip(
                    archive, **{**zip_args, "artifact_digest": "sha256:" + "0" * 64},
                    mode="Release"
                )

    def test_create_source_only_packet(self):
        with tempfile.TemporaryDirectory() as temp:
            build = Path(temp) / "build"
            (build / "main").mkdir(parents=True)
            (build / "main" / BINARY).write_bytes(b"synthetic binary; never executed")
            compiler_dir = build / "CMakeFiles" / "3.31.6"
            compiler_dir.mkdir(parents=True)
            (compiler_dir / "CMakeCXXCompiler.cmake").write_text(
                'set(CMAKE_CXX_COMPILER_ID "GNU")\nset(CMAKE_CXX_COMPILER_VERSION "13.3.0")\n'
            )
            (build / "CMakeCache.txt").write_text(
                "CMAKE_BUILD_TYPE:STRING=Debug\nDXX_XL_DEVELOPER_TESTS:BOOL=ON\n"
                "OPENGL:BOOL=ON\nBUILD_TESTING:BOOL=OFF\n"
                "CMAKE_EXPORT_COMPILE_COMMANDS:BOOL=ON\n"
            )
            (build / "compile_commands.json").write_text(
                json.dumps(
                    [
                        {
                            "file": "/runner/android/XL_compat/runtime/lifecycle_test.cpp",
                            "command": "c++ -DDXX_XL_DEVELOPER_PROBE -DDXX_PUMO_CONTACT_DEVELOPER_PROBE "
                            "-DDXX_XL_CHECKPOINT_EXPORT -DDXX_XL_DESCRIPTOR_TEST -g "
                            "-o main/CMakeFiles/dxx-redux-d2-headless-metadata.dir/lifecycle_test.cpp.o",
                        }
                    ]
                )
            )
            head = subprocess.check_output(
                ["git", "rev-parse", "HEAD"], text=True
            ).strip()
            output = Path(temp) / "packet"
            create(build, output, head, "123", "2")
            self.assertEqual(
                {
                    p.relative_to(output).as_posix()
                    for p in output.rglob("*")
                    if p.is_file()
                },
                {BINARY, RECEIPT, *NOTICES},
            )
            self.assertEqual(json.loads((output / RECEIPT).read_text())["head"], head)
            with (build / "main" / BINARY).open("wb") as binary:
                binary.truncate(MAX_BINARY + 1)
            with self.assertRaises(ValueError):
                create(build, Path(temp) / "over-limit", head, "123", "2")

    def test_compiler_identity(self):
        with tempfile.TemporaryDirectory() as temp:
            build = Path(temp)
            compiler_dir = build / "CMakeFiles" / "3.31.6"
            compiler_dir.mkdir(parents=True)
            (compiler_dir / "CMakeCXXCompiler.cmake").write_text(
                'set(CMAKE_CXX_COMPILER_ID "GNU")\nset(CMAKE_CXX_COMPILER_VERSION "13.3.0")\n'
            )
            (build / "compile_commands.json").write_text(
                json.dumps(
                    [
                        {
                            "file": "/runner/android/XL_compat/runtime/lifecycle_test.cpp",
                            "command": "c++ -DDXX_XL_DEVELOPER_PROBE -DDXX_PUMO_CONTACT_DEVELOPER_PROBE -g -O0 -I/private/path -c lifecycle_test.cpp",
                        }
                    ]
                )
            )
            identity = compile_identity(build)
            self.assertEqual(identity["abi"], "linux-x86_64")
            self.assertEqual(identity["compiler"], {"id": "GNU", "version": "13.3.0"})
            self.assertEqual(identity["switches"], ["-O0", "-g"])
            self.assertNotIn("private", str(identity))

    def test_authenticated_metadata_negatives(self):
        run = {
            "id": 123,
            "workflow_id": 394846952,
            "run_attempt": 2,
            "repository": {"id": 1, "full_name": REPO},
            "path": WORKFLOW,
            "event": "pull_request",
            "status": "completed",
            "conclusion": "success",
            "head_sha": HEAD,
            "head_branch": "fix/pumo-release-probe-packet-20261108",
            "head_commit": {"tree_id": TREE},
            "referenced_workflows": [
                {
                    "ref": "refs/pull/50/merge",
                    "sha": "f" * 40,
                    "path": REPO + "/.github/workflows/stack-ci.yml@" + "f" * 40,
                }
            ],
            "pull_requests": [{"number": 50, "head": {"sha": HEAD, "repo": {"id": 1}}}],
        }
        artifact = {
            "id": 456,
            "name": NAME,
            "expired": False,
            "workflow_run": {"id": 123, "head_sha": HEAD},
            "size_in_bytes": 1024,
            "digest": "sha256:" + "e" * 64,
        }
        jobs = [
            {
                "name": "XL developer source compile (no game assets)",
                "run_id": 123,
                "run_attempt": 2,
                "head_sha": HEAD,
                "labels": ["ubuntu-24.04"],
                "status": "completed",
                "conclusion": "success",
                "steps": [
                    {
                        "name": "Upload source-only developer probe packet",
                        "conclusion": "success",
                    }
                ],
            }
        ]
        workflow = (ROOT / WORKFLOW).read_bytes()
        args = {
            "head": HEAD,
            "tree": TREE,
            "pr_number": 50,
            "executed_workflow": workflow,
            "reviewed_workflow": workflow,
        }
        self.assertEqual(
            authenticate_metadata(run, [artifact], jobs, **args)["artifact_digest"],
            artifact["digest"],
        )
        for field, value in (
            ("id", 124),
            ("run_attempt", 3),
            ("workflow_id", 12),
            ("path", "other.yml"),
            ("event", "workflow_dispatch"),
            ("conclusion", "failure"),
            ("head_sha", "f" * 40),
        ):
            with self.subTest(field=field), self.assertRaises(ValueError):
                authenticate_metadata({**run, field: value}, [artifact], jobs, **args)
        for field, value in (
            ("name", "old"),
            ("expired", True),
            ("digest", "wrong"),
            ("size_in_bytes", 180 * 1024 * 1024),
        ):
            with self.subTest(field=field), self.assertRaises(ValueError):
                authenticate_metadata(run, [{**artifact, field: value}], jobs, **args)
        with self.assertRaises(ValueError):
            authenticate_metadata(
                run, [artifact], [{**jobs[0], "conclusion": "failure"}], **args
            )
        with self.assertRaises(ValueError):
            authenticate_metadata(
                run, [artifact], [{**jobs[0], "run_attempt": 1}], **args
            )
        with self.assertRaises(ValueError):
            authenticate_metadata(run, [artifact, artifact], jobs, **args)
        with self.assertRaises(ValueError):
            authenticate_metadata(run, [artifact], jobs, **{**args, "pr_number": 51})
        with self.assertRaises(ValueError):
            authenticate_metadata(
                run, [artifact], jobs, **{**args, "executed_workflow": b"wrong"}
            )
        release_artifact = {**artifact, "name": packet_name(HEAD, 2, "Release")}
        release_job = {
            **jobs[0],
            "name": "XL developer Release source compile (no game assets)",
            "steps": [{"name": "Upload source-only Release developer probe packet", "conclusion": "success"}],
        }
        authenticate_metadata(run, [release_artifact], [release_job], **args, mode="Release")
        authenticate_metadata(
            {**run, "head_branch": "fix/pumo-accepted-robot-damage-20261108"},
            [release_artifact], [release_job], **args, mode="Release"
        )
        for bad_artifact, bad_job, bad_args in (
            (artifact, release_job, args),
            (release_artifact, jobs[0], args),
            (release_artifact, {**release_job, "conclusion": "failure"}, args),
            (release_artifact, release_job, {**args, "reviewed_workflow": b"tampered"}),
        ):
            with self.assertRaises(ValueError):
                authenticate_metadata(
                    run, [bad_artifact], [bad_job], **bad_args, mode="Release"
                )
        with self.assertRaises(ValueError):
            authenticate_metadata(
                {**run, "head_branch": "other"}, [release_artifact],
                [release_job], **args, mode="Release"
            )

    def test_packet_negative_fields(self):
        files, receipt = packet()
        args = {
            "repository": REPO,
            "workflow": WORKFLOW,
            "run_id": 123,
            "run_attempt": 2,
            "head": HEAD,
            "tree": TREE,
            "platform": "ubuntu-24.04",
        }
        check_files(files, receipt, **args)
        for key, value in (
            ("repository", "wrong/repo"),
            ("workflow", "other.yml"),
            ("run_id", 124),
            ("run_attempt", 3),
            ("head", "c" * 40),
            ("tree", "d" * 40),
            ("platform", "windows-2022"),
        ):
            with self.subTest(key=key), self.assertRaises(ValueError):
                check_files(files, receipt, **{**args, key: value})
        for name in (BINARY, *NOTICES):
            with self.subTest(name=name), self.assertRaises(ValueError):
                check_files({**files, name: b"changed"}, receipt, **args)
        with self.assertRaises(ValueError):
            check_files({**files, "private.pig": b"x"}, receipt, **args)

    def test_zip_negative_paths_and_digest(self):
        files, _ = packet()
        args = {
            "repository": REPO,
            "workflow": WORKFLOW,
            "run_id": 123,
            "run_attempt": 2,
            "head": HEAD,
            "tree": TREE,
            "platform": "ubuntu-24.04",
            "artifact_name": NAME,
        }
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "artifact.zip"

            def write(extra=()):
                with zipfile.ZipFile(archive, "w") as z:
                    for name, data in [*files.items(), *extra]:
                        z.writestr(name, data)
                return "sha256:" + sha256(archive.read_bytes())

            digest = write()
            verify_zip(archive, artifact_digest=digest, **args)
            for field, bad in (
                ("repository", "wrong/repo"),
                ("workflow", "wrong.yml"),
                ("run_id", 124),
                ("run_attempt", 3),
                ("head", "c" * 40),
                ("tree", "d" * 40),
                ("platform", "windows-2022"),
                ("artifact_name", "stale"),
                ("artifact_digest", "sha256:" + "0" * 64),
            ):
                with self.subTest(field=field), self.assertRaises(ValueError):
                    verify_zip(
                        archive, artifact_digest=digest, **{**args, field: bad}
                    ) if field != "artifact_digest" else verify_zip(
                        archive, artifact_digest=bad, **args
                    )
            for member in ("../secret", "/absolute", "private.pig", NOTICES[0]):
                with self.subTest(member=member), self.assertRaises(ValueError):
                    with zipfile.ZipFile(archive, "w") as z:
                        for name, data in files.items():
                            z.writestr(member if name == BINARY else name, data)
                    verify_zip(
                        archive,
                        artifact_digest="sha256:" + sha256(archive.read_bytes()),
                        **args,
                    )
            with self.assertRaises(ValueError):
                verify_zip(archive, artifact_digest=write([("extra", b"x")]), **args)
            with zipfile.ZipFile(archive, "w") as z:
                for name, data in files.items():
                    z.writestr(name, b"x" * 65537 if name == NOTICES[0] else data)
            with self.assertRaises(ValueError):
                verify_zip(
                    archive,
                    artifact_digest="sha256:" + sha256(archive.read_bytes()),
                    **args,
                )
            write()
            data = archive.read_bytes()
            eocd = data.rfind(b"PK\x05\x06")
            archive.write_bytes(
                data[:eocd]
                + struct.pack("<4sLQL", b"PK\x06\x07", 0, 0, 1)
                + data[eocd:]
            )
            with self.assertRaises(ValueError):
                verify_zip(
                    archive,
                    artifact_digest="sha256:" + sha256(archive.read_bytes()),
                    **args,
                )
            write()
            data = archive.read_bytes()
            eocd = data.rfind(b"PK\x05\x06")
            end = bytearray(data[eocd:])
            end[20:22] = struct.pack("<H", 65535)
            archive.write_bytes(
                data[:eocd]
                + struct.pack("<4sLQL", b"PK\x06\x07", 0, 0, 1)
                + end
                + b"x" * 65535
            )
            with self.assertRaises(ValueError):
                verify_zip(
                    archive,
                    artifact_digest="sha256:" + sha256(archive.read_bytes()),
                    **args,
                )
            link = zipfile.ZipInfo(BINARY)
            link.external_attr = 0o120777 << 16
            with zipfile.ZipFile(archive, "w") as z:
                for name, data in files.items():
                    if name != BINARY:
                        z.writestr(name, data)
                z.writestr(link, b"target")
            with self.assertRaises(ValueError):
                verify_zip(
                    archive,
                    artifact_digest="sha256:" + sha256(archive.read_bytes()),
                    **args,
                )


if __name__ == "__main__":
    unittest.main()

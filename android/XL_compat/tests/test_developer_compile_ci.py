"""Negative source checks for the asset-free developer compile job."""

import json
import tempfile
import unittest
from pathlib import Path

from developer_compile_ci import WORKFLOW, check_workflow, verify_build


class DeveloperCompileCiTests(unittest.TestCase):
    def test_workflow_and_negative_routes(self):
        workflow = WORKFLOW.read_text()
        check_workflow(workflow)
        for original, replacement in (
            ("-DDXX_XL_DEVELOPER_TESTS=ON", "-DDXX_XL_DEVELOPER_TESTS=OFF"),
            ("--parallel 2 --target dxx-redux-d2-headless-metadata", "--parallel 8"),
            ("persist-credentials: false", "persist-credentials: true"),
            (
                "ref: ${{ github.event.pull_request.head.sha || github.sha }}",
                "ref: ${{ github.sha }}",
            ),
            ("--verify-build build-xl-developer-ci", "--version"),
        ):
            with self.subTest(original=original), self.assertRaises(ValueError):
                check_workflow(workflow.replace(original, replacement))
        with self.assertRaises(ValueError):
            check_workflow(
                workflow + "\n      - run: ctest --test-dir build-xl-developer-ci\n"
            )
        with self.assertRaises(ValueError):
            check_workflow(
                workflow + "\n      - run: setup_shareware_test_data --download\n"
            )
        for original, replacement in (
            ("-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_BUILD_TYPE=Debug"),
            ("-DOpenGL_GL_PREFERENCE=LEGACY", "-DOpenGL_GL_PREFERENCE=GLVND"),
            ("--mode Release", "--mode Debug"),
            ("xl-developer-release-probe-", "xl-developer-probe-"),
            ("XL developer Release source compile", "XL developer source compile"),
            ("github.head_ref == 'fix/pumo-release-probe-packet-20261108'", "true"),
            ("github.head_ref == 'fix/pumo-accepted-robot-damage-20261108'", "true"),
        ):
            with self.subTest(original=original), self.assertRaises(ValueError):
                check_workflow(workflow.replace(original, replacement))

    def test_build_receipt_and_negative_bindings(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "main").mkdir()
            (build / "main/xl-preview-lifecycle").touch()
            cache = "\n".join(
                f"{key}:BOOL={value}"
                for key, value in (
                    ("DXX_XL_DEVELOPER_TESTS", "ON"),
                    ("OPENGL", "ON"),
                    ("BUILD_TESTING", "OFF"),
                    ("CMAKE_EXPORT_COMPILE_COMMANDS", "ON"),
                )
            )
            cache += "\nCMAKE_BUILD_TYPE:STRING=Debug"
            (build / "CMakeCache.txt").write_text(cache)
            command = (
                "c++ -DDXX_XL_DEVELOPER_PROBE -DDXX_PUMO_CONTACT_DEVELOPER_PROBE "
                "-DDXX_XL_CHECKPOINT_EXPORT -DDXX_XL_DESCRIPTOR_TEST "
                "-o main/CMakeFiles/dxx-redux-d2-headless-metadata.dir/lifecycle_test.cpp.o"
            )
            entry = {
                "file": "/repo/android/XL_compat/runtime/lifecycle_test.cpp",
                "command": command,
            }
            commands = build / "compile_commands.json"
            commands.write_text(json.dumps([entry]))
            verify_build(build)
            cache = cache.replace(
                "CMAKE_EXPORT_COMPILE_COMMANDS:BOOL=ON",
                "CMAKE_EXPORT_COMPILE_COMMANDS:UNINITIALIZED=ON",
            )
            (build / "CMakeCache.txt").write_text(cache)
            verify_build(build)
            for bad in (
                command.replace("-DDXX_XL_DEVELOPER_PROBE ", ""),
                command.replace(
                    "dxx-redux-d2-headless-metadata.dir", "dxx-redux-d2-headless.dir"
                ),
            ):
                with self.subTest(command=bad), self.assertRaises(ValueError):
                    commands.write_text(json.dumps([{**entry, "command": bad}]))
                    verify_build(build)
            commands.write_text(json.dumps([entry]))
            with self.assertRaises(ValueError):
                (build / "CMakeCache.txt").write_text(
                    cache.replace(
                        "CMAKE_BUILD_TYPE:STRING=Debug",
                        "CMAKE_BUILD_TYPE:STRING=Release",
                    )
                )
                verify_build(build)
            (build / "CMakeCache.txt").write_text(cache)
            with self.assertRaises(ValueError):
                (build / "CMakeCache.txt").write_text(
                    cache.replace(
                        "DXX_XL_DEVELOPER_TESTS:BOOL=ON",
                        "DXX_XL_DEVELOPER_TESTS:BOOL=OFF",
                    )
                )
                verify_build(build)
            release_cache = cache.replace(
                "CMAKE_BUILD_TYPE:STRING=Debug", "CMAKE_BUILD_TYPE:STRING=Release"
            ) + "\nOpenGL_GL_PREFERENCE:STRING=LEGACY"
            (build / "CMakeCache.txt").write_text(release_cache)
            verify_build(build, "Release")
            with self.assertRaises(ValueError):
                verify_build(build, "Debug")
            with self.assertRaises(ValueError):
                (build / "CMakeCache.txt").write_text(
                    release_cache.replace("LEGACY", "GLVND")
                )
                verify_build(build, "Release")


if __name__ == "__main__":
    unittest.main()

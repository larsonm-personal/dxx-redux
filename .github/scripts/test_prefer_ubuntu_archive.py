#!/usr/bin/env python3
"""Synthetic mirror and workflow tests; never change this machine's apt config."""

from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import prefer_ubuntu_archive as helper

ROOT = Path(__file__).resolve().parents[2]
ENV = {"GITHUB_ACTIONS": "true", "RUNNER_ENVIRONMENT": "github-hosted", "ImageOS": "ubuntu24"}
RELEASE = {"ID": "ubuntu", "VERSION_ID": "24.04"}
SOURCES = b"Types: deb\nURIs: mirror+file:/etc/apt/apt-mirrors.txt\nSuites: noble noble-updates noble-security\nComponents: main universe restricted multiverse\nSigned-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n"


class MirrorTests(unittest.TestCase):
    def transform(self, data, sources=SOURCES, env=None, release=None, machine="x86_64"):
        return helper.prefer_archive(
            data, sources, ENV if env is None else env, RELEASE if release is None else release, machine
        )

    def test_only_priorities_change_and_second_run_is_identical(self):
        result = self.transform(helper.ORIGINAL)
        self.assertEqual(
            result,
            b"http://azure.archive.ubuntu.com/ubuntu/\tpriority:2\nhttps://archive.ubuntu.com/ubuntu/\tpriority:1\nhttps://security.ubuntu.com/ubuntu/\tpriority:3\n",
        )
        self.assertEqual(self.transform(result), result)

    def test_unknown_custom_and_malformed_lists_fail(self):
        for data in (
            b"",
            helper.ORIGINAL + b"\n",
            helper.ORIGINAL * 2,
            helper.ORIGINAL.replace(b"archive.ubuntu.com", b"custom.example"),
            helper.ORIGINAL.replace(b"priority:1", b"priority:9"),
            helper.ORIGINAL.replace(b"\t", b" "),
            helper.ORIGINAL.replace(b"\n", b"\r\n"),
            helper.ORIGINAL.replace(b"http:", b"https:"),
            helper.ORIGINAL.splitlines(keepends=True)[0],
            helper.ORIGINAL + b"# unexpected additions\n",
        ):
            with self.subTest(data=data), self.assertRaises(ValueError):
                self.transform(data)

    def test_unknown_sources_fail(self):
        for sources in (
            b"",
            SOURCES.replace(b"mirror+file:", b"https:"),
            SOURCES + b"URIs: https://custom.example/ubuntu\n",
            SOURCES.replace(b"apt-mirrors.txt", b"other-mirrors.txt"),
        ):
            with self.subTest(sources=sources), self.assertRaises(ValueError):
                self.transform(helper.ORIGINAL, sources=sources)

    def test_non_hosted_non_noble_and_arm_runners_fail(self):
        for key in ENV:
            env = dict(ENV)
            env.pop(key)
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.transform(helper.ORIGINAL, env=env)
        for args in (
            {"env": dict(ENV, RUNNER_ENVIRONMENT="self-hosted")},
            {"release": dict(RELEASE, VERSION_ID="22.04")},
            {"release": dict(RELEASE, ID="debian")},
            {"machine": "aarch64"},
        ):
            with self.subTest(args=args), self.assertRaises(ValueError):
                self.transform(helper.ORIGINAL, **args)

    def test_write_is_verified_idempotent_and_failures_propagate(self):
        with tempfile.TemporaryDirectory() as directory:
            mirrors, sources = Path(directory, "mirrors"), Path(directory, "sources")
            mirrors.write_bytes(helper.ORIGINAL)
            sources.write_bytes(SOURCES)

            def write(args, **kwargs):
                self.assertEqual(args, ["sudo", "tee", str(mirrors)])
                self.assertTrue(kwargs["check"])
                mirrors.write_bytes(kwargs["input"])

            with (
                patch.object(helper, "MIRRORS", mirrors),
                patch.object(helper, "SOURCES", sources),
                patch.dict(helper.os.environ, ENV, clear=True),
                patch.object(helper.platform, "freedesktop_os_release", return_value=RELEASE),
                patch.object(helper.platform, "machine", return_value="x86_64"),
                patch.object(helper.subprocess, "run", side_effect=write) as run,
            ):
                helper.main()
                helper.main()
                self.assertEqual(run.call_count, 1)
                self.assertEqual(sources.read_bytes(), SOURCES)
                mirrors.write_bytes(b"custom")
                with self.assertRaises(ValueError):
                    helper.main()
                self.assertEqual(mirrors.read_bytes(), b"custom")
                self.assertEqual(run.call_count, 1)
                mirrors.write_bytes(helper.ORIGINAL)
                run.side_effect = subprocess.CalledProcessError(1, "sudo")
                with self.assertRaises(subprocess.CalledProcessError):
                    helper.main()
                run.side_effect = None
                with self.assertRaises(RuntimeError):
                    helper.main()

    def test_host_and_android_use_controller_before_apt(self):
        for name in ("nightly-tooling-sweep.yml", "android-nightly-tooling.yml"):
            source = (ROOT / ".github/workflows" / name).read_text()
            call = "python3 control/.github/scripts/prefer_ubuntu_archive.py"
            self.assertEqual(source.count(call), 1)
            self.assertLess(source.index("path: control"), source.index(call))
            self.assertLess(source.index(call), source.index("sudo apt-get"))
            self.assertNotIn("python3 target/.github/scripts/prefer_ubuntu_archive.py", source)
        android = (ROOT / ".github/workflows/android-nightly-tooling.yml").read_text()
        self.assertIn("expected='7:6.1.1-3ubuntu5'", android)
        smoke = (ROOT / ".github/workflows/engine-smoke.yml").read_text()
        self.assertEqual(smoke.count("python3 .github/scripts/prefer_ubuntu_archive.py"), 2)
        self.assertIn("python3 .github/scripts/test_prefer_ubuntu_archive.py", smoke)


if __name__ == "__main__":
    unittest.main()

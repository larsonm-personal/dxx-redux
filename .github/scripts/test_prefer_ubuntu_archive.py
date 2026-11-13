#!/usr/bin/env python3
"""Synthetic mirror and workflow tests; never change this machine's apt config."""

import re
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import prefer_ubuntu_archive as helper

ROOT = Path(__file__).resolve().parents[2]
ENV = {"GITHUB_ACTIONS": "true", "RUNNER_ENVIRONMENT": "github-hosted", "ImageOS": "ubuntu24"}
RELEASE = {"ID": "ubuntu", "VERSION_ID": "24.04"}
SOURCES = b"Types: deb\nURIs: mirror+file:/etc/apt/apt-mirrors.txt\nSuites: noble noble-updates noble-security\nComponents: main universe restricted multiverse\nSigned-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n"


class MirrorTests(unittest.TestCase):
    def apt_jobs(self, source):
        jobs = source.split("\njobs:\n", 1)[1]
        return {
            name: body
            for name, body in re.findall(r"(?ms)^  ([\w-]+):\n(.*?)(?=^  [\w-]+:|\Z)", jobs)
            if "sudo apt-get" in body
        }

    def assert_controller_before_apt(self, source, job_name):
        match = re.search(rf"(?ms)^  {re.escape(job_name)}:\n(.*?)(?=^  [\w-]+:|\Z)", source)
        self.assertIsNotNone(match, f"Missing {job_name} job")
        job = match.group(1)
        call = "python3 control/.github/scripts/prefer_ubuntu_archive.py"
        self.assertEqual(job.count(call), 1)
        trusted_ref = "ref: ${{ github.workflow_sha }}"
        self.assertIn(trusted_ref, job)
        self.assertLess(job.index(trusted_ref), job.index(call))
        self.assertLess(job.index("path: control"), job.index(call))
        self.assertLess(job.index(call), job.index("sudo apt-get"))
        self.assertNotIn("python3 target/.github/scripts/prefer_ubuntu_archive.py", job)

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

    def test_official_direct_sources_are_unchanged(self):
        for scheme in (b"http", b"https"):
            for host in (b"archive", b"security", b"azure.archive"):
                for slash in (b"", b"/"):
                    sources = SOURCES.replace(
                        b"mirror+file:/etc/apt/apt-mirrors.txt",
                        scheme + b"://" + host + b".ubuntu.com/ubuntu" + slash,
                    )
                    with self.subTest(sources=sources):
                        self.assertEqual(self.transform(helper.ORIGINAL, sources), helper.ORIGINAL)
        mixed = SOURCES + b"URIs: http://archive.ubuntu.com/ubuntu/\n"
        with self.assertRaises(ValueError):
            self.transform(helper.ORIGINAL, mixed)

    def test_direct_sources_never_invoke_a_writer(self):
        with tempfile.TemporaryDirectory() as directory:
            mirrors, sources = Path(directory, "mirrors"), Path(directory, "sources")
            direct = SOURCES.replace(b"mirror+file:/etc/apt/apt-mirrors.txt", b"http://archive.ubuntu.com/ubuntu")
            mirrors.write_bytes(helper.ORIGINAL)
            sources.write_bytes(direct)
            with (
                patch.object(helper, "MIRRORS", mirrors),
                patch.object(helper, "SOURCES", sources),
                patch.dict(helper.os.environ, ENV, clear=True),
                patch.object(helper.platform, "freedesktop_os_release", return_value=RELEASE),
                patch.object(helper.platform, "machine", return_value="x86_64"),
                patch.object(helper.subprocess, "run") as run,
            ):
                helper.main()
                run.assert_not_called()
                self.assertEqual(sources.read_bytes(), direct)
                self.assertEqual(mirrors.read_bytes(), helper.ORIGINAL)

    def test_unmanaged_sources_never_write_or_block_apt(self):
        with tempfile.TemporaryDirectory() as directory:
            mirrors, sources = Path(directory, "mirrors"), Path(directory, "sources")
            unmanaged = SOURCES + b"URIs: https://custom.example/ubuntu\n"
            mirrors.write_bytes(helper.ORIGINAL)
            sources.write_bytes(unmanaged)
            with (
                patch.object(helper, "MIRRORS", mirrors),
                patch.object(helper, "SOURCES", sources),
                patch.dict(helper.os.environ, ENV, clear=True),
                patch.object(helper.platform, "freedesktop_os_release", return_value=RELEASE),
                patch.object(helper.platform, "machine", return_value="x86_64"),
                patch.object(helper.subprocess, "run") as run,
            ):
                helper.main()
                run.assert_not_called()
                self.assertEqual(sources.read_bytes(), unmanaged)
                self.assertEqual(mirrors.read_bytes(), helper.ORIGINAL)
                # Host authorization remains mandatory even for the no-write path
                with patch.dict(helper.os.environ, {"RUNNER_ENVIRONMENT": "self-hosted"}):
                    with self.assertRaises(ValueError):
                        helper.main()

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
        nightly = (ROOT / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        nightly_apt_jobs = self.apt_jobs(nightly)
        self.assertEqual(set(nightly_apt_jobs), {"host", "pumo_stage"})
        for job_name in nightly_apt_jobs:
            with self.subTest(job=job_name):
                self.assert_controller_before_apt(nightly, job_name)
        android = (ROOT / ".github/workflows/android-nightly-tooling.yml").read_text()
        android_apt_jobs = self.apt_jobs(android)
        self.assertEqual(set(android_apt_jobs), {"android"})
        for job_name in android_apt_jobs:
            self.assert_controller_before_apt(android, job_name)
        self.assertIn("expected='7:6.1.1-3ubuntu5'", android)
        smoke = (ROOT / ".github/workflows/engine-smoke.yml").read_text()
        self.assertEqual(smoke.count("python3 .github/scripts/prefer_ubuntu_archive.py"), 2)
        self.assertIn("python3 .github/scripts/test_prefer_ubuntu_archive.py", smoke)

    def test_pumo_stage_controller_contract_rejects_missing_or_late_call(self):
        nightly = (ROOT / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        call = "python3 control/.github/scripts/prefer_ubuntu_archive.py"
        prefix, marker, suffix = nightly.partition("  pumo_stage:\n")
        self.assertTrue(marker)
        job, next_marker, rest = suffix.partition("  execution-stack:\n")
        self.assertTrue(next_marker)
        first_apt = next(line for line in job.splitlines(keepends=True) if "sudo apt-get" in line)
        variants = {
            "missing": job.replace(call, "# controller omitted"),
            "duplicate": job.replace(call, call + "\n          " + call),
            "late": job.replace("          " + call + "\n", "", 1).replace(
                first_apt, first_apt + "          " + call + "\n", 1
            ),
            "target": job.replace(call, "python3 target/.github/scripts/prefer_ubuntu_archive.py"),
            "untrusted-ref": job.replace(
                "ref: ${{ github.workflow_sha }}",
                "ref: ${{ needs.plan.outputs.target_sha }}",
            ),
        }
        for name, altered in variants.items():
            with self.subTest(name=name), self.assertRaises(AssertionError):
                self.assert_controller_before_apt(prefix + marker + altered + next_marker + rest, "pumo_stage")

    def test_new_apt_job_requires_explicit_review(self):
        nightly = (ROOT / ".github/workflows/nightly-tooling-sweep.yml").read_text()
        added = nightly + "\n  new_job:\n    steps:\n      - run: sudo apt-get update\n"
        self.assertIn("new_job", self.apt_jobs(added))
        with self.assertRaises(AssertionError):
            self.assert_controller_before_apt(added, "new_job")


if __name__ == "__main__":
    unittest.main()

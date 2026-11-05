"""Exercise CI policy with changing server metadata, without network access."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import stack_ci

ROOT = Path(__file__).resolve().parents[2]


class StackPolicyTests(unittest.TestCase):
    def test_native_positions_and_unknown_data(self):
        for stack, expected in [
            (None, True),
            ({}, True),
            ({"number": 7, "position": 1, "size": 2}, False),
            ({"number": 7, "position": 2, "size": 2}, True),
            ({"number": 7, "position": 1, "size": 1}, True),
            ({"number": 7, "position": 0, "size": 2}, True),
            ({"number": 7, "position": 3, "size": 2}, True),
            ({"number": 7, "position": True, "size": 2}, True),
            ({"number": 7, "position": "1", "size": 2}, True),
        ]:
            with self.subTest(stack=stack):
                self.assertEqual(stack_ci.eligible({"stack": stack}), expected)

    def test_refresh_transitions_and_isolation(self):
        pr = {"number": 12, "state": "open"}

        def fetch(url):
            self.assertEqual(url, "https://api.github.com/repos/owner/repo/pulls/12")
            return pr

        self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch))
        pr["stack"] = {"number": 7, "position": 1, "size": 2}
        self.assertFalse(stack_ci.refresh(12, "owner/repo", fetch))
        pr["stack"]["size"] = 1
        self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch))
        pr["number"] = 13
        self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch))

    def test_api_errors_run_tests(self):
        for error in (OSError("403"), ValueError("bad JSON"), TimeoutError()):
            with self.subTest(error=error):
                with patch.object(stack_ci.urllib.request, "urlopen", side_effect=error):
                    self.assertTrue(stack_ci.refresh(12, "owner/repo"))

    def test_cli_refreshes_even_without_event_stack(self):
        for original_stack in (None, {"number": 7, "position": 1, "size": 1}):
            with tempfile.TemporaryDirectory() as tmp:
                event, output, summary = [Path(tmp) / name for name in ("event", "output", "summary")]
                event.write_text(json.dumps({"pull_request": {"number": 12, "stack": original_stack}}))
                with (
                    patch.dict(
                        os.environ,
                        {
                            "GITHUB_EVENT_NAME": "pull_request",
                            "GITHUB_EVENT_PATH": str(event),
                            "GITHUB_REPOSITORY": "owner/repo",
                            "GITHUB_OUTPUT": str(output),
                            "GITHUB_STEP_SUMMARY": str(summary),
                        },
                    ),
                    patch.object(stack_ci, "refresh", return_value=False) as refresh,
                ):
                    stack_ci.main()
                    refresh.assert_called_once_with(
                        12, "owner/repo", profile="", event_pr={"number": 12, "stack": original_stack}
                    )
                self.assertEqual(output.read_text(), "run=false\n")
                self.assertIn("Skip", summary.read_text())

    def test_extended_selection_and_full_stack_coverage(self):
        pr = {"number": 12, "state": "open", "head": {"sha": "b" * 40}, "base": {"sha": "a" * 40}}
        comparison = {"files": [{"filename": "README.md"}]}

        def fetch(url):
            return comparison if "/compare/" in url else pr

        self.assertFalse(stack_ci.refresh(12, "owner/repo", fetch, "extended", pr))
        comparison["files"] = [{"filename": "old.txt", "previous_filename": "d2/main/network.cpp"}]
        self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "extended", pr))
        comparison["files"] = [{"filename": "README.md"}] * 300
        self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "extended", pr))
        comparison["files"] = []
        pr["stack"] = {}
        self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "extended", pr))
        pr["stack"] = {"number": 7, "position": 2, "size": 2}
        self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "extended", pr))
        pr["stack"]["size"] = 3
        self.assertFalse(stack_ci.refresh(12, "owner/repo", fetch, "extended", pr))

    def test_scripts_and_xl_selection(self):
        for profile in ("scripts", "xl"):
            pr = {"number": 12, "state": "open", "head": {"sha": "b" * 40}, "base": {"sha": "a" * 40}}
            comparison = {"files": [{"filename": "README.md"}]}

            def fetch(url):
                return comparison if "/compare/" in url else pr

            self.assertFalse(stack_ci.refresh(12, "owner/repo", fetch, profile, pr))
            pr["stack"] = {"number": 7, "position": 2, "size": 2}
            # A README-only tip must still cover lower-layer script/XL edits
            self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, profile, pr))
            pr["stack"]["size"] = 3
            self.assertFalse(stack_ci.refresh(12, "owner/repo", fetch, profile, pr))
            comparison["files"] = [{"filename": "android/tests/test_fixture.py"}]
            self.assertFalse(stack_ci.refresh(12, "owner/repo", fetch, profile, pr))
            pr["stack"] = {}
            self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, profile, pr))

    def test_script_paths_and_renames_preserve_standalone_coverage(self):
        pr = {"number": 12, "state": "open", "head": {"sha": "b" * 40}, "base": {"sha": "a" * 40}, "stack": None}
        for path in (
            "android/foo.py",
            "android/helpers/nested/foo.py",
            "android/run_all_tests.ps1",
            "android/helpers/foo.psm1",
            "android/tests/fixture.json",
            "android/test_fixtures/a.txt",
            "android/game_scripts/helper.py",
            "game_data/mods/d2x-xl/convert.ps1",
            ".github/workflows/android-tooling.yml",
            ".github/scripts/stack_ci.py",
            "cmake/test.cmake",
            "d2/main/bmread.c",
        ):
            for file in ({"filename": path}, {"filename": "removed.txt", "previous_filename": path}):
                with self.subTest(file=file):

                    def fetch(url):
                        return {"files": [file]} if "/compare/" in url else pr

                    self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "scripts", pr))
        for path in ("android/game_scripts/test_new.jsonc", "game_data/mods/d2x-xl/README.md"):

            def fetch(url):
                return {"files": [{"filename": path}]} if "/compare/" in url else pr

            self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "scripts", pr))
            standalone = dict(pr, stack=None)
            self.assertTrue(
                stack_ci.refresh(
                    12,
                    "owner/repo",
                    lambda url: fetch(url) if "/compare/" in url else standalone,
                    "scripts",
                    standalone,
                )
            )

    def test_scripts_uncertainty_keeps_coverage(self):
        pr = {"number": 12, "state": "open", "head": {"sha": "b" * 40}, "base": {"sha": "a" * 40}, "stack": None}
        for comparison in ({}, {"files": None}, {"files": [None]}, {"files": [{"filename": "README.md"}] * 300}):

            def fetch(url):
                return comparison if "/compare/" in url else pr

            self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "scripts", pr))
        self.assertTrue(stack_ci.refresh(12, "owner/repo", lambda url: pr, "scripts", {}))
        stale = dict(pr, head={"sha": "c" * 40})
        self.assertTrue(stack_ci.refresh(12, "owner/repo", lambda url: pr, "scripts", stale))
        stale_base = dict(pr, base={"sha": "c" * 40})
        self.assertTrue(stack_ci.refresh(12, "owner/repo", lambda url: pr, "scripts", stale_base))
        pr["stack"] = {"number": 7, "position": 1, "size": 2}
        self.assertTrue(stack_ci.refresh(12, "owner/repo", lambda url: pr, "unknown-profile", pr))

    def test_workflows_keep_cheap_checks_and_gate_matrix_work(self):
        scripts = (ROOT / ".github/workflows/android-tooling.yml").read_text()
        xl = (ROOT / ".github/workflows/xl-compat.yml").read_text()
        for workflow, profile in ((scripts, "scripts"), (xl, "xl")):
            self.assertIn("  pull_request:\n", workflow)
            self.assertNotIn("    paths:", workflow)
            self.assertIn("    needs: stack", workflow)
            self.assertIn("    if: needs.stack.outputs.run == 'true'", workflow)
            self.assertIn("      profile: " + profile, workflow)
            self.assertIn("          STACK_CI_PROFILE: " + profile, workflow)
            self.assertIn("if: steps.current.outputs.run == 'true'", workflow)
        self.assertTrue(scripts.startswith("name: scripts and ci tests\n"))
        self.assertIn("name: scripts and ci tests (${{ matrix.os }})", scripts)
        self.assertIn("  workflow-lint:", scripts)
        self.assertIn("  catalogs:", scripts)
        self.assertIn("./android/tests/test_validate_automation_catalog.ps1", scripts)

    def test_private_skip_does_not_publish_status(self):
        sys.path.insert(0, str(ROOT / "android/tests"))
        import nightly_pr_candidate as candidate

        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "output"
            with (
                patch.dict(os.environ, {"GITHUB_OUTPUT": str(output)}),
                patch.object(sys, "argv", ["nightly_pr_candidate.py", "admit"]),
                patch.object(candidate, "verify", return_value={"number": 12, "sha": "a" * 40}),
                patch.object(stack_ci, "refresh", return_value=False),
                patch.object(candidate, "status") as status,
            ):
                self.assertEqual(candidate.main(), 0)
                status.assert_not_called()
            self.assertEqual(output.read_text(), "eligible=false\n")

    def test_non_pr_events_never_query_or_skip(self):
        for name in ("push", "workflow_dispatch", "merge_group", "schedule", "workflow_run"):
            with tempfile.TemporaryDirectory() as tmp:
                event, output, summary = [Path(tmp) / file for file in ("event", "output", "summary")]
                event.write_text("{}")
                env = dict(
                    os.environ,
                    GITHUB_EVENT_NAME=name,
                    GITHUB_EVENT_PATH=str(event),
                    GITHUB_OUTPUT=str(output),
                    GITHUB_STEP_SUMMARY=str(summary),
                )
                subprocess.run([sys.executable, str(ROOT / ".github/scripts/stack_ci.py")], env=env, check=True)
                self.assertEqual(output.read_text(), "run=true\n")


if __name__ == "__main__":
    unittest.main()

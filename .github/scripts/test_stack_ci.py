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
    def public_lan_finish(self):
        workflow = (ROOT / ".github/workflows/public-lan-smoke.yml").read_text()
        return workflow.split("\n  finish:\n", 1)[1].split("\n  evidence:\n", 1)[0]

    def test_public_lan_finish_distinguishes_cancelled_selection_from_failure(self):
        finish = self.public_lan_finish()
        condition = next(line.strip() for line in finish.splitlines() if line.startswith("    if:"))
        condition = condition.removeprefix("if: ${{ ").removesuffix(" }}")
        # Evaluate the workflow's actual guard with only its supported context
        for source, target in (
            ("always()", "True"),
            ("!inputs.evidence_run_id", "not evidence"),
            ("needs.plan.result", "plan"),
            ("needs.execution-stack.outputs.run", "stack"),
            ("needs.android.result", "android"),
            ("&&", "and"),
            ("||", "or"),
        ):
            condition = condition.replace(source, target)
        for plan, stack, android, expected in (
            ("cancelled", "", "skipped", False),
            ("skipped", "", "skipped", False),
            ("failure", "", "skipped", True),
            ("success", "true", "success", True),
            ("success", "true", "failure", True),
            ("success", "true", "cancelled", True),
            ("success", "", "skipped", True),
            ("success", "false", "skipped", False),
            ("success", "false", "failure", True),
            ("success", "false", "cancelled", True),
        ):
            with self.subTest(plan=plan, stack=stack, android=android):
                context = {"plan": plan, "stack": stack, "android": android, "evidence": ""}
                self.assertEqual(eval(condition, {"__builtins__": {}}, context), expected)
                context["evidence"] = "123"
                self.assertFalse(eval(condition, {"__builtins__": {}}, context))

    def test_public_lan_missing_plan_artifact_remains_fatal(self):
        finish = self.public_lan_finish()
        download = next(step for step in finish.split("      - ") if "name: nightly-plan\n" in step)
        self.assertIn("uses: actions/download-artifact@", download)
        self.assertNotIn("continue-on-error:", download)
        self.assertNotIn("if:", download)

    def test_public_lan_finish_retains_plan_and_gameplay_failures(self):
        finish = self.public_lan_finish()
        self.assertIn('[[ "$PLAN_RESULT" == success && "$ANDROID_RESULT" == success ]] || status=$?', finish)
        self.assertIn("--require-selected --receipts", finish)
        self.assertIn("> summary.json || status=$?", finish)
        self.assertIn('exit "$status"', finish)

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

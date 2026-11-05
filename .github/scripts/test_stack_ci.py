"""Exercise CI policy with changing server metadata, without network access."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import textwrap
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
            self.assertNotIn("steps.current.outputs.run", workflow)
        self.assertTrue(scripts.startswith("name: scripts and ci tests\n"))
        self.assertIn("name: scripts and ci tests (${{ matrix.os }})", scripts)
        self.assertIn("  workflow-lint:", scripts)
        self.assertIn("  catalogs:", scripts)
        self.assertIn("./android/tests/test_validate_automation_catalog.ps1", scripts)

    def test_rust_full_stack_diff_and_uncertain_base(self):
        pr = {
            "number": 12,
            "state": "open",
            "head": {"sha": "b" * 40},
            "base": {"sha": "c" * 40},
            "stack": {"number": 7, "position": 2, "size": 2, "base": {"sha": "a" * 40}},
        }
        files = [{"filename": "server/src/main.rs"}]
        urls = []

        def fetch(url):
            urls.append(url)
            return {"files": files} if "/compare/" in url else pr

        self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "rust", pr))
        self.assertEqual(urls[-1], "https://api.github.com/repos/owner/repo/compare/" + "a" * 40 + "..." + "b" * 40)
        files[:] = [{"filename": "README.md"}]
        self.assertFalse(stack_ci.refresh(12, "owner/repo", fetch, "rust", pr))
        # An event opened standalone can now be the tip of a new native stack
        event = dict(pr, stack=None)
        files[:] = [{"filename": "removed.txt", "previous_filename": "server/src/main.rs"}]
        self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "rust", event))
        pr["stack"]["size"] = 3
        self.assertFalse(stack_ci.refresh(12, "owner/repo", fetch, "rust", event))
        pr["stack"]["size"] = 2
        for base in (None, {}, {"sha": "invalid"}, {"sha": ""}):
            pr["stack"]["base"] = base
            self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "rust", event))
        pr["stack"] = {}
        self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "rust", event))
        pr["stack"] = None
        files[:] = [{"filename": "server/Cargo.lock"}]
        self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "rust", event))
        self.assertEqual(urls[-1], "https://api.github.com/repos/owner/repo/compare/" + "c" * 40 + "..." + "b" * 40)
        files[:] = [{"filename": "README.md"}]
        self.assertFalse(stack_ci.refresh(12, "owner/repo", fetch, "rust", event))
        files[:] = [{"filename": "README.md"}] * 300
        self.assertTrue(stack_ci.refresh(12, "owner/repo", fetch, "rust", event))
        rust = (ROOT / ".github/workflows/server-rust.yml").read_text()
        self.assertIn("  server:\n    needs: stack\n    if: needs.stack.outputs.run == 'true'", rust)
        self.assertIn("      profile: rust", rust)
        self.assertNotIn("steps.current", rust)
        self.assertIn("BASE: ${{ github.event.before }}", rust)

    def test_rust_push_detection_and_admitted_pr_execution(self):
        workflow = (ROOT / ".github/workflows/server-rust.yml").read_text()
        block = workflow.split("      - name: Detect server changes", 1)[1]
        script = textwrap.dedent(block.split("        run: |\n", 1)[1].split("      - name:", 1)[0])
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            def git(*args):
                return subprocess.check_output(
                    ["git", "-c", "user.name=CI Test", "-c", "user.email=ci@example.invalid", *args],
                    cwd=root,
                    stderr=subprocess.DEVNULL,
                    text=True,
                ).strip()

            git("init")
            (root / "README.md").write_text("initial")
            git("add", ".")
            git("commit", "-m", "initial")
            base = git("rev-parse", "HEAD")
            (root / "README.md").write_text("docs")
            git("commit", "-am", "docs")
            docs_head = git("rev-parse", "HEAD")
            (root / "server").mkdir()
            (root / "server/main.rs").write_text("fn main() {}")
            git("add", ".")
            git("commit", "-m", "server")
            server_head = git("rev-parse", "HEAD")
            for event, head, deleted, expected in (
                ("push", docs_head, "false", "false"),
                ("push", server_head, "false", "true"),
                ("push", server_head, "true", "false"),
                ("pull_request", docs_head, "false", "true"),
            ):
                output = root / "output"
                output.write_text("")
                env = dict(
                    os.environ,
                    GITHUB_EVENT_NAME=event,
                    GITHUB_OUTPUT=str(output),
                    BASE=base,
                    HEAD=head,
                    DELETED=deleted,
                )
                subprocess.run(["bash", "-e", "-c", script], cwd=root, env=env, check=True)
                self.assertEqual(output.read_text(), "changed=" + expected + "\n")

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

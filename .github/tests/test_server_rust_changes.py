#!/usr/bin/env python3
"""Exercise the CI path filter against real temporary Git histories, without Rust."""

import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / ".github/scripts/server_rust_changes.py"
SPEC = importlib.util.spec_from_file_location("server_rust_changes", SCRIPT)
FILTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FILTER)


class ServerChanges(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        previous = Path.cwd()
        os.chdir(self.root)
        self.addCleanup(os.chdir, previous)
        self.git("init", "-q", "-b", "cmake")
        self.git("config", "user.name", "larsonm-personal")
        self.git("config", "user.email", "265118235+larsonm-personal@users.noreply.github.com")
        self.base = self.save("server/src/main.rs", "initial server")

    def git(self, *args):
        return subprocess.check_output(["git", *args], text=True, stderr=subprocess.PIPE).strip()

    def save(self, name, content="changed"):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        self.git("add", "--all")
        self.git("commit", "-qm", "filter fixture")
        return self.git("rev-parse", "HEAD")

    def push(self, before, after):
        return FILTER.server_changed("push", {"before": before, "after": after})

    def pr(self, base, head):
        return FILTER.server_changed("pull_request", {"pull_request": {"base": {"sha": base}, "head": {"sha": head}}})

    def test_server_change_runs_for_push_and_pr(self):
        head = self.save("server/src/main.rs")
        self.assertTrue(self.push(self.base, head))
        self.assertTrue(self.pr(self.base, head))

    def test_unrelated_paths_do_not_run(self):
        before = self.base
        for name in (
            "android/app/test.kt",
            "d2/main/test.c",
            "rust-toolchain.toml",
            ".github/workflows/server-rust.yml",
            "serverish/file",
        ):
            with self.subTest(path=name):
                head = self.save(name)
                self.assertFalse(self.push(before, head))
                self.assertFalse(self.pr(self.base, head))
                before = head

    def test_pr_uses_aggregate_but_push_uses_only_pushed_diff(self):
        server_head = self.save("server/src/main.rs")
        head = self.save("android/unrelated.txt")
        self.assertTrue(self.pr(self.base, head))
        self.assertFalse(self.push(server_head, head))

    def test_base_branch_only_changes_do_not_select_pr(self):
        self.git("checkout", "-qb", "feature")
        head = self.save("android/unrelated.txt")
        self.git("checkout", "-q", "cmake")
        base = self.save("server/src/main.rs", "base-only update")
        self.assertFalse(self.pr(base, head))

    def test_server_deletion_selects(self):
        (self.root / "server/src/main.rs").unlink()
        head = self.save("android/unrelated.txt")
        self.assertTrue(self.push(self.base, head))

    def test_rename_out_of_server_selects(self):
        self.git("mv", "server/src/main.rs", "moved.rs")
        head = self.save("android/unrelated.txt")
        self.assertTrue(self.push(self.base, head))

    def test_rename_into_server_selects(self):
        before = self.save("outside.rs")
        self.git("mv", "outside.rs", "server/moved.rs")
        head = self.save("android/unrelated.txt")
        self.assertTrue(self.push(before, head))

    def test_new_branch_and_deleted_branch(self):
        self.assertTrue(self.push("0" * 40, self.base))
        self.assertFalse(FILTER.server_changed("push", {"deleted": True}))

    def test_invalid_event_fails_instead_of_silently_skipping(self):
        with self.assertRaises(ValueError):
            FILTER.server_changed("workflow_dispatch", {})
        with self.assertRaises(ValueError):
            self.push("not-a-sha", self.base)

    def test_cli_emits_boolean_output(self):
        for path, expected in (("android/unrelated.txt", "false"), ("server/Cargo.lock", "true")):
            head = self.save(path)
            event = self.root / "event.json"
            event.write_text(json.dumps({"before": self.base, "after": head}))
            output = self.root / "output.txt"
            output.write_text("")
            subprocess.run(
                ["python3", str(SCRIPT)],
                check=True,
                capture_output=True,
                env=dict(os.environ, GITHUB_EVENT_NAME="push", GITHUB_EVENT_PATH=str(event), GITHUB_OUTPUT=str(output)),
            )
            self.assertEqual(output.read_text(), f"changed={expected}\n")

    def test_workflow_gates_every_rust_step_and_always_reports(self):
        workflow = (ROOT / ".github/workflows/server-rust.yml").read_text()
        self.assertNotIn("    paths:", workflow)
        self.assertIn("  pull_request:", workflow)
        self.assertIn("  push:\n    branches: [cmake]", workflow)
        self.assertNotIn("workflow_dispatch:", workflow)
        self.assertIn("fetch-depth: 0", workflow)
        self.assertIn("id: changes", workflow)
        self.assertIn("python3 .github/scripts/server_rust_changes.py", workflow)
        for step in workflow.split("      - name: ")[1:]:
            if any(
                command in step
                for command in ("cargo ", "bash android/get_deps/helpers/get_rust.sh", "python3 server/")
            ):
                self.assertIn("if: steps.changes.outputs.changed == 'true'", step)
        self.assertIn("if: steps.changes.outputs.changed != 'true'", workflow)
        self.assertNotIn("    if:", workflow.split("    steps:")[0])
        self.assertIn("cargo build --locked", workflow)
        self.assertIn("cargo test --locked", workflow)


if __name__ == "__main__":
    unittest.main()

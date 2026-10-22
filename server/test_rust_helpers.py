#!/usr/bin/env python3
"""Exercise upgrade/install orchestration without downloading or deploying anything."""

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent


class RustHelpers(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in (
            "server/rust_tools.conf",
            "rust-toolchain.toml",
            "server/rust_upgrade.sh",
            "server/rust_dependency_lint.sh",
            "server/check_exact_dependencies.py",
            "server/Cargo.toml",
            "android/get_deps/helpers/get_rust.sh",
        ):
            dest = self.root / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, dest)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.log = self.root / "commands"
        self.env = dict(
            os.environ, PATH=f"{self.bin}:{os.environ['PATH']}", CARGO_HOME=str(self.root / "cargo"), LOG=str(self.log)
        )
        self.mock("rustup", 'if [[ "$1" == --version ]]; then echo "rustup 1.29.2"; fi')
        self.mock("cargo", "true")
        self.mock("rustc", "true")
        self.mock("bindgen", "true")
        # Unexpected network activity must fail even on a connected test runner
        self.mock("curl", "exit 91")

    def mock(self, name, body):
        path = self.bin / name
        path.write_text(f'#!/usr/bin/env bash\nprintf "%s\\n" "{name} $*" >> "$LOG"\n{body}\n')
        path.chmod(0o755)

    def run_helper(self, name, *args):
        return subprocess.run(
            ["bash", str(self.root / name), *args], env=self.env, capture_output=True, text=True, check=False
        )

    def test_install_uses_exact_declared_toolchain(self):
        result = self.run_helper("android/get_deps/helpers/get_rust.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        commands = self.log.read_text()
        self.assertIn(
            "toolchain install 1.99.1 --profile minimal --component clippy,rustfmt --no-self-update", commands
        )
        self.assertNotIn("stable", commands)
        self.assertNotIn("curl", commands)

    def test_windows_bootstrap_uses_versioned_executable(self):
        self.mock("rustup", 'echo "rustup 1.28.0"')
        self.mock("uname", 'if [[ "$1" == -s ]]; then echo MINGW64_NT; else echo x86_64; fi')
        fixture = self.root / "installer-fixture"
        fixture.write_text('#!/usr/bin/env bash\nprintf "%s\\n" "installer $*" >> "$LOG"\n')
        self.env["BOOTSTRAP_FIXTURE"] = str(fixture)
        self.env["BOOTSTRAP_SHA256"] = hashlib.sha256(fixture.read_bytes()).hexdigest()
        self.mock(
            "curl",
            """
            target="${@: -1}"
            if [[ "$*" == *".sha256 "* ]]; then
                printf '%s\\n' "$BOOTSTRAP_SHA256" > "$target"
            else
                cp "$BOOTSTRAP_FIXTURE" "$target"
            fi
        """,
        )
        result = self.run_helper("android/get_deps/helpers/get_rust.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        commands = self.log.read_text()
        self.assertIn(
            "https://static.rust-lang.org/rustup/archive/1.29.2/x86_64-pc-windows-msvc/rustup-init.exe",
            commands,
        )
        self.assertIn("rustup-init.exe: OK", result.stdout)
        self.assertIn("installer -y --no-modify-path --profile minimal --default-toolchain 1.99.1", commands)

    def test_upgrade_locks_addons_and_dates_nightly(self):
        result = self.run_helper("server/rust_upgrade.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        commands = self.log.read_text()
        for spec in (
            "cargo-udeps --version =0.1.61 --locked",
            "cargo-edit --version =0.13.14 --locked",
            "bindgen-cli --version =0.73.2 --locked",
        ):
            self.assertIn("cargo install " + spec, commands)
        self.assertIn("toolchain install nightly-2026-10-22", commands)

    def test_failed_install_preserves_inventory(self):
        inventory = self.root / "server/rust_version.txt"
        inventory.write_text("old inventory\n")
        self.mock("cargo", "exit 42")
        self.assertEqual(self.run_helper("server/rust_upgrade.sh").returncode, 42)
        self.assertEqual(inventory.read_text(), "old inventory\n")

    def test_dependency_dry_run_does_not_update_or_build(self):
        result = self.run_helper("server/rust_dependency_lint.sh", "--dry-run")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.log.read_text(), "cargo upgrade --pinned --dry-run\n")

    def test_rejects_dependency_ranges_before_upgrade(self):
        manifest = self.root / "server/Cargo.toml"
        manifest.write_text(manifest.read_text().replace('"=1.', '"^1.', 1))
        result = self.run_helper("server/rust_dependency_lint.sh")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exact =version pin", result.stderr)
        self.assertFalse(self.log.exists())

    def test_dependency_check_uses_locked_dated_nightly(self):
        result = self.run_helper("server/rust_dependency_lint.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("cargo +nightly-2026-10-22 udeps --all-targets --locked", self.log.read_text())


if __name__ == "__main__":
    unittest.main()

"""Fail-fast capture gates without native builds, game execution, or public assets."""

import argparse
import gzip
import hashlib
import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import capture_public_d1_fault_probes as capture


class CaptureGateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        # Match the CLI contract, including aliased Windows temporary paths
        self.root = Path(self.temp.name).resolve()
        self.repo = self.root / "source"
        self.build = self.root / "independent-build"
        self.build.mkdir()
        self.assets = self.root / "assets"
        self.assets.mkdir()
        self.revision = "a" * 40
        self.stamp = self.revision[:9]
        self.sources = {}
        for name in capture.RECIPES:
            path = self.repo / name
            path.parent.mkdir(parents=True, exist_ok=True)
            self.sources[name] = (name + "\n").encode()
            path.write_bytes(self.sources[name])
        (self.repo / "d1").mkdir()
        self.write_cache()
        self.args = argparse.Namespace(
            repo=self.repo,
            build=self.build,
            assets=self.assets,
            output=self.root / "attempt",
            revision=self.revision,
            scenario="mine-blast",
            no_missile=False,
            no_flare=False,
            flare_delay=0,
            no_pixel_observation=False,
            screenshots=False,
            pack=False,
            gzip_raw=False,
            timeout=300,
        )
        self.commands = []
        self.dirty = False
        self.head = self.revision
        self.fail_stage = None
        self.binary_stamp = self.stamp
        self.compile_stamp = self.stamp
        self.record_stamp = self.stamp
        self.record_game = "d1"
        self.record_start = "new_level"
        self.inject_save = False
        self.capture_env = None
        self.runner = self.enterContext(mock.patch.object(capture.subprocess, "run", side_effect=self.run_command))
        self.asset_check = self.enterContext(mock.patch.object(capture, "validate_assets"))
        self.enterContext(mock.patch.object(capture.compact, "pinned_assets", return_value={"descent.hog": "b" * 64}))

    def write_cache(self, home=None, stamp=None):
        text = f"CMAKE_HOME_DIRECTORY:INTERNAL={home or self.repo / 'd1'}\nCMAKE_GENERATOR:INTERNAL=Ninja\n"
        if stamp is not None:
            text += f"DXX_INPUT_DEMO_GIT_VERSION:STRING={stamp}\n"
        (self.build / "CMakeCache.txt").write_text(text)

    def configure(self):
        rows = []
        for name in (capture.FIXTURE, capture.RECORDER, "d1/main/fireball.c", "d1/main/effects.c"):
            rows.append(
                {
                    "directory": str(self.build),
                    "file": str(self.repo / name),
                    "command": f'c++ -DDXX_INPUT_DEMO_GIT_VERSION=\\"{self.compile_stamp}\\" '
                    f"-o main/CMakeFiles/test_upstream_compat.dir/{Path(name).name}.o -c {self.repo / name}",
                }
            )
        (self.build / "compile_commands.json").write_text(json.dumps(rows))

    def run_command(self, command, **kwargs):
        self.assertTrue(kwargs["check"], "Every external process must fail closed")
        if command[0] == "git":
            arguments = command[3:]
            if arguments[0] == "rev-parse":
                result = self.head.encode() + b"\n"
            elif arguments[0] == "status":
                result = b" M d1/main/game.c\n" if self.dirty else b""
            elif arguments[0] == "show":
                result = self.sources[arguments[1].split(":", 1)[1]]
            else:
                self.fail(f"Unexpected git command: {command}")
            return subprocess.CompletedProcess(command, 0, stdout=result)
        stage = "build" if command[:2] == ["cmake", "--build"] else "configure" if command[0] == "cmake" else "capture"
        self.commands.append(stage)
        kwargs["stdout"].write(f"{stage} output\n")
        if stage == "capture":
            self.capture_env = kwargs["env"]
            self.assertEqual(self.commands[:2], ["configure", "build"])
            self.assertTrue((Path(kwargs["env"]["HOME"])).is_dir())
            prior = self.manifest()
            self.assertEqual(
                prior["capture_executable_sha256"], capture.digest(self.build / "main/test_upstream_compat")
            )
            self.assertIn(capture.RECIPES[3], prior["source_recipe_sha256"])
            self.assertEqual(prior["asset_sha256"], {"descent.hog": "b" * 64})
            self.write_recording(kwargs["cwd"])
        if self.fail_stage == stage:
            raise subprocess.CalledProcessError(7, command)
        if stage == "configure":
            self.configure()
        elif stage == "build":
            exe = self.build / "main/test_upstream_compat"
            exe.parent.mkdir()
            exe.write_bytes(b"fake executable\0" + self.binary_stamp.encode() + b"\0")
            exe.chmod(0o755)
        return subprocess.CompletedProcess(command, 0)

    def write_recording(self, directory):
        header = {
            "type": "header",
            "game": self.record_game,
            "mission": "d1",
            "level": 3 if self.args.scenario == "mine-blast" else 1,
            "difficulty": 0 if self.args.scenario == "fan-transparency" else 2,
            "git_version": self.record_stamp,
            "start_mode": self.record_start,
        }
        frame = {"type": "frame", "f": 0}
        if self.inject_save:
            frame["save_checkpoint"] = "bad"
        (directory / "capture.dximdemo").write_text(json.dumps(header) + "\n" + json.dumps(frame) + "\n")
        (directory / "capture.dximdemo.rngtrace.jsonl").write_text('{"type":"meta"}\n')
        (directory / "capture.json").write_text(
            json.dumps(
                {
                    "game": "d1",
                    "scenario": self.args.scenario,
                    "negative_no_missile": self.args.no_missile,
                    "no_fire": self.args.no_flare,
                    "fire_delay": self.args.flare_delay,
                    "pixel_observation": not self.args.no_pixel_observation,
                    "capture_role": "native_d1_reference",
                }
            )
        )

    def manifest(self):
        return json.loads((self.args.output / "manifest.json").read_text())

    def assert_blocked(self, pattern, commands=()):
        with self.assertRaisesRegex(capture.Error, pattern):
            capture.capture(self.args)
        self.assertEqual(self.commands, list(commands))
        self.assertFalse(self.manifest()["capture_invoked"])
        self.assertEqual(self.manifest()["status"], "failed")

    def test_failed_build_never_invokes_stale_executable(self):
        self.fail_stage = "build"
        stale = self.build / "main/test_upstream_compat"
        stale.parent.mkdir()
        stale.write_text("stale fixture must not run")
        with self.assertRaises(subprocess.CalledProcessError):
            capture.capture(self.args)
        self.assertEqual(self.commands, ["configure", "build"])
        manifest = self.manifest()
        self.assertEqual((manifest["stage"], manifest["returncode"]), ("build", 7))
        self.assertFalse(manifest["capture_invoked"])
        self.assertIn("build output", (self.args.output / "build.log").read_text())
        self.assertIn("build.log", manifest["files"])

    def test_stale_cached_revision_never_builds_or_captures(self):
        self.write_cache(stamp="b" * 9)
        self.assert_blocked("build stamp")

    def test_dirty_cached_stamp_never_builds_or_captures(self):
        self.write_cache(stamp=self.stamp + "-dirty")
        self.assert_blocked("build stamp")

    def test_wrong_source_home_never_builds_or_captures(self):
        self.write_cache(home=self.root / "other" / "d1")
        self.assert_blocked("CMAKE_HOME_DIRECTORY")

    def test_dirty_source_never_builds_or_captures(self):
        self.dirty = True
        self.assert_blocked("Source is dirty")

    def test_missing_build_cache_never_configures_or_captures(self):
        (self.build / "CMakeCache.txt").unlink()
        with self.assertRaises(FileNotFoundError):
            capture.capture(self.args)
        self.assertEqual(self.commands, [])
        self.assertFalse(self.manifest()["capture_invoked"])

    def test_missing_source_recipe_never_configures_or_captures(self):
        (self.repo / capture.RECIPES[1]).unlink()
        with self.assertRaises(FileNotFoundError):
            capture.capture(self.args)
        self.assertEqual(self.commands, [])
        self.assertFalse(self.manifest()["capture_invoked"])

    def test_wrong_head_never_builds_or_captures(self):
        self.head = "b" * 40
        self.assert_blocked("Source HEAD")

    def test_changed_recipe_never_builds_or_captures(self):
        (self.repo / capture.RECIPES[1]).write_text("changed despite clean status")
        self.assert_blocked("Source recipe differs")

    def test_failed_configure_never_builds_or_captures(self):
        self.fail_stage = "configure"
        with self.assertRaises(subprocess.CalledProcessError):
            capture.capture(self.args)
        self.assertEqual(self.commands, ["configure"])
        self.assertFalse(self.manifest()["capture_invoked"])

    def test_stale_compiled_revision_never_builds_or_captures(self):
        self.compile_stamp = "b" * 9
        self.assert_blocked("build stamp", ["configure"])

    def test_wrong_binary_stamp_never_captures(self):
        self.binary_stamp = "b" * 9
        self.assert_blocked("executable lacks", ["configure", "build"])

    def test_postcapture_stamp_mismatch_retains_invalid_evidence(self):
        self.record_stamp = "b" * 9
        with self.assertRaisesRegex(capture.Error, "build stamp"):
            capture.capture(self.args)
        self.assertEqual(self.manifest()["status"], "failed")
        self.assertTrue(self.manifest()["capture_invoked"])
        self.assertIn("raw/capture.dximdemo", self.manifest()["files"])

    def test_non_native_recording_fails(self):
        self.record_game = "d2"
        with self.assertRaisesRegex(capture.Error, "native D1"):
            capture.capture(self.args)

    def test_save_start_fails(self):
        self.record_start = "save_checkpoint"
        with self.assertRaisesRegex(capture.Error, "ordinary new-level"):
            capture.capture(self.args)

    def test_embedded_save_payload_fails(self):
        self.inject_save = True
        with self.assertRaisesRegex(capture.Error, "Save/checkpoint"):
            capture.capture(self.args)

    def test_capture_failure_retains_raw_evidence(self):
        self.fail_stage = "capture"
        with self.assertRaises(subprocess.CalledProcessError):
            capture.capture(self.args)
        self.assertEqual(self.manifest()["stage"], "capture")
        self.assertIn("raw/capture.dximdemo", self.manifest()["files"])

    def test_success_order_hashes_and_isolation(self):
        self.args.no_missile = True
        self.args.screenshots = True
        with mock.patch.dict(capture.os.environ, {"DXX_PUBLIC_MINE_LEVEL": "99", "HOME": "/wrong/home"}):
            result = capture.capture(self.args)
        self.assertEqual(self.commands, ["configure", "build", "capture"])
        self.assertEqual(result["status"], "candidate")
        self.assertEqual(
            result["source_recipe_sha256"][capture.FIXTURE], hashlib.sha256(self.sources[capture.FIXTURE]).hexdigest()
        )
        self.assertEqual(self.capture_env["HOME"], str(self.args.output / "raw/home"))
        self.assertNotIn("DXX_PUBLIC_MINE_LEVEL", self.capture_env)
        self.assertEqual(self.capture_env["DXX_PUBLIC_CAPTURE_NO_MISSILE"], "1")
        self.assertEqual(self.capture_env["DXX_PUBLIC_CAPTURE_SCREENSHOTS"], "1")
        raw = self.args.output / "raw/capture.dximdemo"
        self.assertEqual(result["files"]["raw/capture.dximdemo"]["sha256"], capture.digest(raw))

    def test_effect_uses_dedicated_fixture(self):
        self.args.scenario = "effect-phase"
        result = capture.capture(self.args)
        self.assertEqual(result["command"][1:], ["--public-effect-phase", str(self.assets)])

    def test_pack_preserves_raw_and_only_then_archives(self):
        self.args.pack = self.args.gzip_raw = True

        def pack(full, rng, demo, oracle, provenance):
            self.assertTrue(full.is_file())
            self.assertTrue(rng.is_file())
            self.assertFalse(full.with_name(full.name + ".gz").exists())
            self.assertEqual(provenance["source_revision"], self.revision)
            self.assertEqual(
                provenance["capture_source_sha256"], hashlib.sha256(self.sources[capture.RECIPES[3]]).hexdigest()
            )
            demo.write_text("compact")
            oracle.write_text("native oracle")

        with mock.patch.object(capture.compact, "pack", side_effect=pack):
            result = capture.capture(self.args)
        raw = self.args.output / "raw/capture.dximdemo"
        self.assertEqual(gzip.decompress(raw.with_name(raw.name + ".gz").read_bytes()), raw.read_bytes())
        self.assertTrue(result["packed"])
        self.assertIn("candidate.oracle.json", result["files"])

    def test_pack_failure_never_archives_or_deletes_evidence(self):
        self.args.pack = self.args.gzip_raw = True
        with (
            mock.patch.object(capture.compact, "pack", side_effect=capture.Error("invalid pack")),
            self.assertRaisesRegex(capture.Error, "invalid pack"),
        ):
            capture.capture(self.args)
        self.assertEqual(self.manifest()["stage"], "pack")
        self.assertTrue((self.args.output / "raw/capture.dximdemo").is_file())
        self.assertFalse((self.args.output / "raw/capture.dximdemo.gz").exists())

    def test_oversized_pack_never_archives_or_deletes_evidence(self):
        self.args.pack = self.args.gzip_raw = True

        def oversized(full, rng, demo, oracle, provenance):
            demo.write_bytes(b"x" * (capture.compact.MAX_BYTES + 1))
            oracle.write_text("oracle")

        with (
            mock.patch.object(capture.compact, "pack", side_effect=oversized),
            self.assertRaisesRegex(capture.Error, "100000"),
        ):
            capture.capture(self.args)
        self.assertTrue((self.args.output / "raw/capture.dximdemo").is_file())
        self.assertFalse((self.args.output / "raw/capture.dximdemo.gz").exists())

    def test_fan_no_fire_uses_only_its_explicit_environment_option(self):
        self.args.scenario = "fan-transparency"
        self.args.no_flare = True
        result = capture.capture(self.args)
        self.assertTrue(result["no_flare"])
        self.assertEqual(self.capture_env["DXX_PUBLIC_FAN_NO_FIRE"], "1")
        self.assertNotIn("DXX_PUBLIC_CAPTURE_NO_MISSILE", self.capture_env)
        self.assertNotIn("DXX_PUBLIC_FAN_DELAY", self.capture_env)

    def test_fan_one_frame_delay_is_explicit(self):
        self.args.scenario = "fan-transparency"
        self.args.flare_delay = 1
        result = capture.capture(self.args)
        self.assertEqual(result["flare_delay"], 1)
        self.assertEqual(self.capture_env["DXX_PUBLIC_FAN_DELAY"], "1")

    def test_fan_pixel_observation_control_is_explicit(self):
        self.args.scenario = "fan-transparency"
        self.args.no_pixel_observation = True
        result = capture.capture(self.args)
        self.assertFalse(result["pixel_observation"])
        self.assertEqual(self.capture_env["DXX_PUBLIC_FAN_NO_PIXEL_OBSERVATION"], "1")

    def test_fan_options_reject_other_recipes_before_build(self):
        self.args.no_flare = True
        self.assert_blocked("only to fan-transparency")

    def test_fan_missile_option_is_rejected_before_build(self):
        self.args.scenario = "fan-transparency"
        self.args.no_missile = True
        self.assert_blocked("only to missile and mine")

    def test_fan_combined_controls_rejected_before_build(self):
        self.args.scenario = "fan-transparency"
        self.args.no_flare = True
        self.args.flare_delay = 1
        self.assert_blocked("captured separately")

    def test_fan_invalid_delay_rejected_before_build(self):
        self.args.scenario = "fan-transparency"
        self.args.flare_delay = 2
        self.assert_blocked("zero or one frame")

    def test_existing_output_is_never_replaced(self):
        self.args.output.mkdir()
        sentinel = self.args.output / "manifest.json"
        sentinel.write_text("old evidence")
        with self.assertRaises(FileExistsError):
            capture.capture(self.args)
        self.assertEqual(sentinel.read_text(), "old evidence")
        self.runner.assert_not_called()

    def test_cli_canonicalizes_paths_before_capture(self):
        names = ("repo", "build", "assets", "output")
        argv = []
        for name in names:
            expected = getattr(self.args, name)
            self.assertEqual(expected, expected.resolve(), "Fixture paths must match the CLI contract")
            lexical = expected.parent / "unused-component" / ".." / expected.name
            self.assertNotEqual(lexical, expected)
            argv.extend(("--" + name, str(lexical)))
        argv.extend(("--revision", self.revision, "--scenario", self.args.scenario))
        with mock.patch.object(capture, "capture") as invoke, mock.patch("sys.stdout"):
            self.assertEqual(capture.main(argv), 0)
        invoke.assert_called_once()
        parsed = invoke.call_args.args[0]
        for name in names:
            self.assertEqual(getattr(parsed, name), getattr(self.args, name))
        capture.build_cache(parsed.repo, parsed.build, parsed.revision)
        self.runner.assert_not_called()

    def test_cli_returns_nonzero_on_build_failure(self):
        self.fail_stage = "build"
        argv = [
            value
            for name in ("repo", "build", "assets", "output", "revision", "scenario")
            for value in ("--" + name, str(getattr(self.args, name)))
        ]
        with mock.patch("sys.stderr"):
            self.assertEqual(capture.main(argv), 1)
        self.assertNotIn("capture", self.commands)


if __name__ == "__main__":
    unittest.main()

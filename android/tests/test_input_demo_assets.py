"""Fail-closed replay asset admission and verified-shareware selection controls."""

import copy
import hashlib
import json
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest

import input_demo_assets as assets


class AssetPreflightTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.data = self.root / "data"
        self.data.mkdir()

    def tearDown(self):
        self.temporary.cleanup()

    def header(self, mode="native_d1"):
        names = {
            "d1_hog": "descent.hog",
            "d1_pig": "descent.pig",
            "d2_hog": "descent2.hog",
            "d2_ham": "descent2.ham",
            "d2_pig": "groupa.pig",
        }
        files = []
        for role in sorted(assets.REQUIRED[mode]):
            path = self.data / names[role]
            path.write_bytes((role + " real fixture bytes").encode())
            files.append({"role": role, "name": path.name, "size": path.stat().st_size, "sha256": assets.digest(path)})
        return {
            "type": "header",
            "game": "d1" if mode == "native_d1" else "d2",
            "asset_fingerprints": {"status": "complete", "mode": mode, "files": files},
        }

    def public_header(self, game="d1", mode=None):
        return {
            "type": "header",
            "game": "d1" if (mode or "native_" + game) == "native_d1" else "d2",
            "asset_fingerprints": {
                "status": "complete",
                "mode": mode or "native_" + game,
                "files": [
                    {
                        "role": game + "_" + name.split(".")[-1],
                        "name": name,
                        "size": row["size"],
                        "sha256": row["sha256"],
                    }
                    for name, row in assets.pinned_assets(game).items()
                    if name.endswith((".hog", ".ham", ".pig"))
                ],
            },
        }

    def demo(self, name, header):
        path = self.root / name
        path.write_text(json.dumps(header) + "\n")
        return path

    def test_native_d1_d2_and_imported_d1_match_actual_bytes(self):
        for mode in assets.REQUIRED:
            with self.subTest(mode=mode):
                header = self.header(mode)
                result = assets.verify_header(header, self.data, header["game"])
                self.assertEqual(result["status"], "verified")
                self.assertIsNone(result["shareware_game"])
        header = self.header("native_d1")
        self.assertEqual(assets.verify_header(header, self.data, "d2", imported=True)["status"], "verified")
        with self.assertRaisesRegex(assets.AssetError, "engine differs"):
            assets.verify_header(self.header("d1_in_d2"), self.data, "d1")
        with self.assertRaisesRegex(assets.AssetError, "Imported replay"):
            assets.verify_header(self.header("native_d2"), self.data, "d2", imported=True)

    def test_wrong_source_same_size_is_rejected_even_with_opt_in(self):
        header = self.header()
        path = self.data / "descent.hog"
        path.write_bytes(b"!" * path.stat().st_size)
        for allow in (False, True):
            with self.assertRaisesRegex(assets.AssetError, "mismatch"):
                assets.verify_header(header, self.data, "d1", allow_unverified=allow)

    def test_missing_unknown_and_partial_unknown_are_explicitly_unverified(self):
        header = {"type": "header", "game": "d1"}
        with self.assertRaisesRegex(assets.AssetError, "UNVERIFIED"):
            assets.verify_header(header, self.data, "d1")
        self.assertEqual(assets.verify_header(header, self.data, "d1", allow_unverified=True)["status"], "unverified")
        header = self.header()
        header["asset_fingerprints"].update(status="unknown", error="PIG source unavailable")
        header["asset_fingerprints"]["files"].pop()
        self.assertIsNone(assets.shareware_game(header))
        with self.assertRaisesRegex(assets.AssetError, "UNVERIFIED"):
            assets.verify_header(header, self.data, "d1")
        self.assertEqual(assets.verify_header(header, self.data, "d1", allow_unverified=True)["status"], "unverified")
        (self.data / header["asset_fingerprints"]["files"][0]["name"]).write_bytes(b"wrong")
        with self.assertRaisesRegex(assets.AssetError, "mismatch"):
            assets.verify_header(header, self.data, "d1", allow_unverified=True)

    def test_archive_contained_base_member_remains_explicitly_unverified(self):
        header = self.header("native_d2")
        # Native retains the container and supported roles, omitting a base HAM
        # loaded from an untracked archive that has no recorded container identity
        fingerprint = header["asset_fingerprints"]
        fingerprint.update(status="unknown", error="base asset owner is outside recorded HOGs: d2_ham")
        fingerprint["files"] = [row for row in fingerprint["files"] if row["role"] != "d2_ham"]
        # This loose file was not the recorded HAM and must not be substituted
        (self.data / "descent2.ham").write_bytes(b"different loose HAM")
        with self.assertRaisesRegex(assets.AssetError, "UNVERIFIED"):
            assets.verify_header(header, self.data, "d2")
        result = assets.verify_header(header, self.data, "d2", allow_unverified=True)
        self.assertEqual(result, {"status": "unverified", "shareware_game": None})
        with self.assertRaisesRegex(assets.AssetError, "No verified"):
            assets.select_shareware([self.demo("shareware-archive-member.dximdemo", header)])
        # Known supported bytes are still checked; opt-in never hides their mismatch
        (self.data / "descent2.hog").write_bytes(b"changed recorded HOG")
        with self.assertRaisesRegex(assets.AssetError, "mismatch"):
            assets.verify_header(header, self.data, "d2", allow_unverified=True)

    def archive(self, name, members):
        path = self.data / name
        with path.open("wb") as stream:
            stream.write(b"DHF")
            for member, payload in members:
                stream.write(member.encode("ascii").ljust(13, b"\0"))
                stream.write(struct.pack("<I", len(payload)))
                stream.write(payload)
        return path

    def bind_archive(self, header, role, path):
        files = header["asset_fingerprints"]["files"]
        row = {"role": role, "name": path.name, "size": path.stat().st_size, "sha256": assets.digest(path)}
        files[:] = [item for item in files if item["role"] != role] + [row]
        return row

    def test_base_hog_member_matches_and_wrong_loose_shadow_fails(self):
        header = self.header("native_d2")
        ham = next(row for row in header["asset_fingerprints"]["files"] if row["role"] == "d2_ham")
        payload = (self.data / ham["name"]).read_bytes()
        (self.data / ham["name"]).unlink()
        hog = self.archive("descent2.hog", [("descent2.ham", payload)])
        self.bind_archive(header, "d2_hog", hog)
        ham["container"] = "d2_hog"
        ham["name"] = "renamed-member.data"
        self.assertEqual(assets.verify_header(header, self.data, "d2")["status"], "verified")
        (self.data / "descent2.ham").write_bytes(b"wrong preferred loose HAM")
        with self.assertRaisesRegex(assets.AssetError, "mismatch"):
            assets.verify_header(header, self.data, "d2")
        (self.data / "descent2.ham").write_bytes(payload)
        self.assertEqual(assets.verify_header(header, self.data, "d2")["status"], "verified")
        hog.write_bytes(hog.read_bytes() + b"tamper")
        with self.assertRaisesRegex(assets.AssetError, "Recorded asset mismatch"):
            assets.verify_header(header, self.data, "d2")

    def test_mission_hog_member_overrides_loose_but_not_preferred_filename(self):
        header = self.header("native_d2")
        pig = next(row for row in header["asset_fingerprints"]["files"] if row["role"] == "d2_pig")
        payload = (self.data / pig["name"]).read_bytes()
        hog = self.archive("custom.hog", [(pig["name"], payload)])
        self.bind_archive(header, "mission_hog", hog)
        pig["container"] = "mission_hog"
        (self.data / pig["name"]).write_bytes(b"lower-priority loose PIG")
        self.assertEqual(assets.verify_header(header, self.data, "d2")["status"], "verified")
        ham = next(row for row in header["asset_fingerprints"]["files"] if row["role"] == "d2_ham")
        ham_payload = (self.data / ham["name"]).read_bytes()
        self.archive("custom.hog", [(pig["name"], payload), ("d2demo.ham", ham_payload)])
        self.bind_archive(header, "mission_hog", hog)
        ham.update(name="d2demo.ham", container="mission_hog")
        (self.data / "descent2.ham").write_bytes(b"preferred-name loose HAM")
        with self.assertRaisesRegex(assets.AssetError, "mismatch"):
            assets.verify_header(header, self.data, "d2")

    def test_hog_member_parser_bounds_ambiguity_and_exact_payload(self):
        header = self.header("native_d2")
        ham = next(row for row in header["asset_fingerprints"]["files"] if row["role"] == "d2_ham")
        payload = (self.data / ham["name"]).read_bytes()
        (self.data / ham["name"]).unlink()
        ham["container"] = "d2_hog"
        variants = [
            b"DHF" + b"short",
            b"DHF" + b"descent2.ham".ljust(13, b"\0") + struct.pack("<I", 0xFFFFFFFF),
        ]
        for blob in variants:
            hog = self.data / "descent2.hog"
            hog.write_bytes(blob)
            self.bind_archive(header, "d2_hog", hog)
            with self.assertRaises(assets.AssetError):
                assets.verify_header(header, self.data, "d2")
        hog = self.archive("descent2.hog", [("descent2.ham", payload), ("DESCENT2.HAM", payload)])
        self.bind_archive(header, "d2_hog", hog)
        with self.assertRaisesRegex(assets.AssetError, "Ambiguous"):
            assets.verify_header(header, self.data, "d2")
        hog = self.archive("descent2.hog", [("descent2.ham", b"wrong member")])
        self.bind_archive(header, "d2_hog", hog)
        with self.assertRaisesRegex(assets.AssetError, "member mismatch"):
            assets.verify_header(header, self.data, "d2")
        ham["container"] = "mission_hog"
        with self.assertRaisesRegex(assets.AssetError, "container is missing"):
            assets.fingerprints(header)
        ham["container"] = "d2_pig"
        with self.assertRaisesRegex(assets.AssetError, "container must"):
            assets.fingerprints(header)

    def test_malformed_and_incomplete_cannot_be_bypassed(self):
        original = self.header()
        variants = []
        for key, value in (
            ("sha256", "A" * 64),
            ("sha256", "abc"),
            ("size", True),
            ("size", 0),
            ("name", "../descent.hog"),
            ("role", "other"),
        ):
            candidate = copy.deepcopy(original)
            candidate["asset_fingerprints"]["files"][0][key] = value
            variants.append(candidate)
        candidate = copy.deepcopy(original)
        candidate["asset_fingerprints"]["files"].pop()
        variants.append(candidate)
        candidate = copy.deepcopy(original)
        candidate["asset_fingerprints"]["files"].append(candidate["asset_fingerprints"]["files"][0])
        variants.append(candidate)
        for value in (
            None,
            {},
            {"status": "unknown", "mode": "native_d1", "files": []},
            {**original["asset_fingerprints"], "version": 1},
        ):
            variants.append({**original, "asset_fingerprints": value})
        for candidate in variants:
            with self.subTest(candidate=candidate), self.assertRaises(assets.AssetError):
                assets.verify_header(candidate, self.data, "d1", allow_unverified=True)

    def test_unknown_duplicate_json_fields_are_rejected(self):
        path = self.root / "duplicate.dximdemo"
        path.write_text('{"type":"header","game":"d1","game":"d2"}\n')
        with self.assertRaisesRegex(assets.AssetError, "Duplicate"):
            assets.read_header(path)

    def test_basename_is_diagnostic_but_preferred_wrong_source_fails(self):
        header = self.header()
        for row in header["asset_fingerprints"]["files"]:
            row["name"] = "renamed-" + row["name"] + ".data"
        self.assertEqual(assets.verify_header(header, self.data, "d1")["status"], "verified")
        # A matching spare file cannot conceal a different selected engine source
        (self.data / "spare.hog").write_bytes((self.data / "descent.hog").read_bytes())
        (self.data / "descent.hog").write_bytes(b"wrong preferred file")
        with self.assertRaisesRegex(assets.AssetError, "mismatch"):
            assets.verify_header(header, self.data, "d1")

    def test_case_ambiguity_and_retail_shadow_fail(self):
        header = self.header()
        (self.data / "DESCENT.HOG").write_bytes((self.data / "descent.hog").read_bytes())
        with self.assertRaisesRegex(assets.AssetError, "Ambiguous"):
            assets.verify_header(header, self.data, "d1")
        header = self.header("native_d2")
        for row in header["asset_fingerprints"]["files"]:
            if row["role"] in ("d2_hog", "d2_ham"):
                path = self.data / row["name"]
                row["name"] = "d2demo." + path.suffix[1:]
                path.rename(self.data / row["name"])
        (self.data / "descent2.hog").write_bytes(b"retail preferred source")
        with self.assertRaisesRegex(assets.AssetError, "mismatch"):
            assets.verify_header(header, self.data, "d2")

    def test_selection_uses_complete_hash_sets_not_demo_or_asset_names(self):
        expected = []
        for game, mode in (("d1", "native_d1"), ("d2", "native_d2"), ("d1", "d1_in_d2")):
            header = self.public_header(game, mode)
            for row in header["asset_fingerprints"]["files"]:
                row["name"] = "renamed." + row["name"].split(".")[-1]
            expected.append(self.demo(mode + "-retail-name.dximdemo", header))
        unknown = self.public_header()
        unknown["asset_fingerprints"].update(status="unknown", error="source unavailable")
        incomplete = self.public_header()
        incomplete["asset_fingerprints"]["files"].pop()
        paths = expected + [
            self.demo("shareware-named-retail.dximdemo", self.header()),
            self.demo(
                "shareware-missing.dximdemo", {"type": "header", "game": "d1", "mission": "d1", "edition": "shareware"}
            ),
            self.demo("shareware-unknown.dximdemo", unknown),
            self.demo("shareware-incomplete.dximdemo", incomplete),
        ]
        self.assertEqual(assets.select_shareware(paths), sorted(expected))
        self.assertEqual(assets.select_shareware(paths, "d1"), [expected[0]])
        with self.assertRaisesRegex(assets.AssetError, "No verified"):
            assets.select_shareware(paths[3:])

    def test_custom_mission_retains_shareware_base_and_is_verified(self):
        header = self.public_header()
        mission = {"role": "mission_hog", "name": "custom.hog", "size": 3, "sha256": hashlib.sha256(b"DHF").hexdigest()}
        header["asset_fingerprints"]["files"].append(mission)
        self.assertEqual(assets.shareware_game(header), "d1")
        header = self.header()
        header["asset_fingerprints"]["files"].append(mission)
        with self.assertRaisesRegex(assets.AssetError, "Missing"):
            assets.verify_header(header, self.data, "d1")
        (self.data / "missions").mkdir()
        (self.data / "missions/custom.hog").write_bytes(b"DHF")
        self.assertEqual(assets.verify_header(header, self.data, "d1")["status"], "verified")
        (self.data / "missions/custom.hog").write_bytes(b"bad")
        with self.assertRaisesRegex(assets.AssetError, "mismatch"):
            assets.verify_header(header, self.data, "d1")

    def test_imported_companions_must_all_be_shareware_and_complete(self):
        header = self.public_header("d1", "d1_in_d2")
        companions = self.public_header("d2")["asset_fingerprints"]["files"]
        header["asset_fingerprints"]["files"].extend(companions)
        self.assertEqual(assets.shareware_game(header), "d1")
        header["asset_fingerprints"]["files"][-1]["sha256"] = "0" * 64
        self.assertIsNone(assets.shareware_game(header))
        header["asset_fingerprints"]["files"].pop()
        with self.assertRaisesRegex(assets.AssetError, "D2 HOG, HAM and PIG"):
            assets.shareware_game(header)

    def test_source_manifest_identity_is_required(self):
        repo = self.root / "repo"
        (repo / "android/tests/fixtures").mkdir(parents=True)
        (repo / "game_data").mkdir()
        (repo / "android/tests/fixtures/demo_import_oracles.json").write_text(
            json.dumps({"archives": [{"archive": "desc14sw.exe", "sha256": "a" * 64, "files": []}]})
        )
        (repo / "game_data/test_data_manifest.json").write_text(
            json.dumps({"files": [{"path": "game_data/demo installers/desc14sw.exe", "sha256": "b" * 64}]})
        )
        with self.assertRaisesRegex(assets.AssetError, "disagree"):
            assets.shareware_game(self.public_header(), repo)

    def test_cli_nonempty_selection_and_mismatch_exit_codes(self):
        script = Path(assets.__file__)
        demo = self.demo("recording.dximdemo", self.header())
        command = [sys.executable, str(script), "verify", str(demo), "--data", str(self.data), "--game", "d1"]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        (self.data / "descent.pig").write_bytes(b"wrong")
        self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
        self.assertNotEqual(
            subprocess.run(
                [sys.executable, str(script), "select-shareware", str(demo)], capture_output=True
            ).returncode,
            0,
        )

    def test_android_active_set_uses_same_bytes_gate_before_launch(self):
        import importlib.util

        path = assets.ROOT / "android/helpers/generate_store_assets.py"
        spec = importlib.util.spec_from_file_location("asset_capture_test", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        capture = module.Capture.__new__(module.Capture)
        capture.allow_unverified_assets = False
        header = self.header()
        setup = {
            "active_set_path": "/data/user/0/com.dxxredux.app/files/test",
            "set_files_recursive": ["descent.hog", "descent.pig"],
        }
        capture.private = lambda command, remote, binary: (self.data / Path(remote).name).read_bytes()
        capture.verify_replay_assets(header, setup, "d1")
        (self.data / "descent.pig").write_bytes(b"wrong device bytes")
        with self.assertRaisesRegex(assets.AssetError, "mismatch"):
            capture.verify_replay_assets(header, setup, "d1")
        with self.assertRaisesRegex(assets.AssetError, "UNVERIFIED"):
            capture.verify_replay_assets({"game": "d1"}, setup, "d1")

    def test_host_and_android_gate_order_and_direct_runner_inventory(self):
        root = assets.ROOT
        host = (root / "android/tests/run_input_demo_replay.ps1").read_text()
        self.assertLess(
            host.index("-Command verify -Paths @($resolvedDemoPath)"), host.index("$sandbox = New-LaunchSandbox")
        )
        self.assertIn(
            "verify_replay_assets(header, setup,", (root / "android/helpers/generate_store_assets.py").read_text()
        )
        direct = (root / "android/tests/check_ordered_input_runtime.py").read_text()
        self.assertLess(
            direct.index("assets.verify_header(assets.read_header(demo), data, game)"),
            direct.index('"-inputdemo-replay"'),
        )
        batch = (root / "android/tests/run_input_demo_regressions.ps1").read_text()
        self.assertIn("-Command select-shareware", batch)
        self.assertIn("if ($SharewareOnly -and $AllowUnverifiedAssets)", batch)
        self.assertIn("No verified shareware recordings selected", batch)


if __name__ == "__main__":
    unittest.main()

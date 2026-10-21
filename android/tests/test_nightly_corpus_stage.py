"""Synthetic archive binding and rejection controls; contains no private assets."""

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import stat
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import nightly_corpus_stage as stage
import nightly_regression_catalog as catalog


class CorpusStageTests(unittest.TestCase):
    def setUp(self):
        self.temp = self.enterContext(tempfile.TemporaryDirectory())
        self.root = Path(self.temp)

    def archive(self, rows):
        path = self.root / "fixture.zip"
        with zipfile.ZipFile(path, "w") as output:
            for name, data in rows:
                output.writestr(name, data)
        return path

    def test_exact_variants_are_isolated_in_reverse_and_parallel_order(self):
        rows = [
            ("A/Mine.MN2", b"name=fixture\nnum_levels=1\nONE.RL2\n"),
            ("A/Mine.HOG", b"payload-A"),
            ("A/One.RL2", b"loose-A"),
            ("B/Mine.MN2", b"name=fixture\nnum_levels=1\nONE.RL2\n"),
            ("B/Mine.HOG", b"payload-B"),
            ("B/One.RL2", b"loose-B"),
        ]
        for order in (rows, list(reversed(rows))):
            archive = self.archive(order)

            def run(variant):
                destination = self.root / ("stage-" + variant)
                result = stage.stage_route(
                    archive,
                    {"mission_filename": "MINE.mn2", "mission_path": variant.lower() + "/mine.MN2"},
                    destination,
                )
                self.assertEqual((result / "missions/mine.hog").read_bytes(), ("payload-" + variant).encode())
                self.assertEqual((result / "missions/one.rl2").read_bytes(), ("loose-" + variant).encode())
                self.assertEqual(
                    sorted(p.name for p in (result / "missions").iterdir()), ["mine.hog", "mine.mn2", "one.rl2"]
                )
                return result

            with ThreadPoolExecutor(max_workers=2) as pool:
                results = list(pool.map(run, ("B", "A")))
            import shutil

            for result in results:
                shutil.rmtree(result)

    def test_ambiguous_distinct_payload_never_selects_first(self):
        for reverse in (False, True):
            rows = [("a/m.mn2", b"name=fixture"), ("a/m.hog", b"A"), ("b/m.mn2", b"name=fixture"), ("b/m.hog", b"B")]
            archive = self.archive(rows[::-1] if reverse else rows)
            with self.assertRaisesRegex(stage.StageUnavailable, "distinct payloads"):
                stage.stage_route(archive, {"mission_filename": "m.mn2"}, self.root / "stage")
            self.assertFalse((self.root / "stage").exists())

    def test_identical_variants_can_deduplicate_and_hog_override_is_associated(self):
        rows = [
            (folder + "/" + name, value)
            for folder in ("a", "b")
            for name, value in (
                ("m.mn2", b"name=fixture\nhog = PAYLOAD.HOG\n"),
                ("payload.hog", b"correct"),
                ("m.hog", b"wrong"),
            )
        ]
        result = stage.stage_route(self.archive(rows), {"mission_filename": "m.mn2"}, self.root / "stage")
        self.assertEqual((result / "missions/payload.hog").read_bytes(), b"correct")
        self.assertFalse((result / "missions/m.hog").exists())

    def test_hog_member_bytes_and_names_are_never_rewritten(self):
        import struct

        hog = b"DHF" + b"MiXeD.RL2".ljust(13, b"\0") + struct.pack("<I", 7) + b"payload"
        result = stage.stage_route(
            self.archive([("M.MN2", b"num_levels=1\nMiXeD.RL2\n"), ("M.HOG", hog)]),
            {"mission_filename": "m.mn2"},
            self.root / "stage",
        )
        self.assertEqual((result / "missions/m.hog").read_bytes(), hog)
        self.assertIn(b"MiXeD.RL2", (result / "missions/m.mn2").read_bytes())

    def test_unsafe_paths_collisions_links_and_limits_fail_before_publication(self):
        cases = [[(name, b"x")] for name in ("../escape", "/absolute", "C:\\evil", "a/../bad", "a\\..\\bad", "NUL.txt")]
        cases += [[("A", b"x"), ("a", b"y")], [("a", b"x"), ("a/b", b"y")]]
        link = zipfile.ZipInfo("link")
        link.create_system = 3
        link.external_attr = (stat.S_IFLNK | 0o777) << 16
        cases.append([(link, b"../outside")])
        for rows in cases:
            with self.subTest(rows=repr(rows)), self.assertRaises(stage.StageUnavailable):
                stage.stage_route(self.archive(rows), {"mission_filename": "m.mn2"}, self.root / "stage")
            self.assertFalse((self.root / "stage").exists())
        with patch.object(stage, "MAX_BYTES", 1), self.assertRaises(stage.StageUnavailable):
            stage.stage_route(self.archive([("m.mn2", b"two")]), {"mission_filename": "m.mn2"}, self.root / "stage")
        with patch.object(stage, "MAX_ENTRIES", 1), self.assertRaises(stage.StageUnavailable):
            stage.validate_entries([("a", 0, False), ("b", 0, False)])

    def test_no_cross_directory_hog_fallback_or_escaping_override(self):
        for rows in (
            [("a/m.mn2", b"name=x"), ("b/m.hog", b"wrong")],
            [("m.mn2", b"hog=../m.hog"), ("m.hog", b"wrong")],
        ):
            with self.assertRaises(stage.StageUnavailable):
                stage.stage_route(self.archive(rows), {"mission_filename": "m.mn2"}, self.root / "stage")

    def test_existing_stage_and_linked_parent_are_rejected(self):
        archive = self.archive([("m.mn2", b"name=x"), ("m.hog", b"correct")])
        target = self.root / "existing"
        target.mkdir()
        (target / "keep").write_bytes(b"untouched")
        with self.assertRaises(stage.StageUnavailable):
            stage.stage_route(archive, {"mission_filename": "m.mn2"}, target)
        self.assertEqual((target / "keep").read_bytes(), b"untouched")
        if hasattr(__import__("os"), "symlink"):
            link = self.root / "link"
            try:
                link.symlink_to(target, target_is_directory=True)
            except OSError:
                self.skipTest("Host cannot create test symlinks")
            with self.assertRaises(stage.StageUnavailable):
                stage.stage_route(archive, {"mission_filename": "m.mn2"}, link / "stage")
            self.assertFalse((target / "stage").exists())

    @unittest.skipUnless(shutil.which("pwsh"), "PowerShell unavailable")
    def test_prepared_route_rejects_multi_owner_reuse_before_execution(self):
        repo = Path(__file__).resolve().parents[2]
        prepared = self.root / "prepared"
        (prepared / "missions").mkdir(parents=True)
        command = [
            "pwsh",
            "-NoProfile",
            "-NonInteractive",
            "-File",
            str(repo / "android/helpers/regenerate_all_guidebot_simulations.ps1"),
            "-DryRun",
            "-NoBuild",
            "-ExactMissionJson",
            "-MissionJson",
            "FirstStrike.json",
            "-PreparedMissionStage",
            str(prepared),
        ]
        rejected = subprocess.run(command, capture_output=True, text=True, timeout=30)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("exactly one headless owner", rejected.stderr)
        accepted = subprocess.run(command + ["-Level", "1"], capture_output=True, text=True, timeout=30)
        self.assertEqual(accepted.returncode, 0, accepted.stderr)
        rejected = subprocess.run(
            command + ["-Level", "1", "-WriteRegression"], capture_output=True, text=True, timeout=30
        )
        self.assertNotEqual(rejected.returncode, 0)

    @unittest.skipUnless(shutil.which("7zz"), "Standalone 7-Zip unavailable")
    def test_real_seven_zip_round_trip(self):
        source = self.root / "input"
        source.mkdir()
        (source / "M.MN2").write_bytes(b"name=fixture")
        (source / "M.HOG").write_bytes(b"payload")
        archive = self.root / "fixture.7z"
        subprocess.run(["7zz", "a", str(archive), "."], cwd=source, capture_output=True, check=True, timeout=30)
        target = stage.stage_route(archive, {"mission_filename": "m.mn2"}, self.root / "stage")
        self.assertEqual((target / "missions/m.hog").read_bytes(), b"payload")

    def test_catalog_preserves_missing_and_vertigo_contract(self):
        self.assertTrue(catalog.unavailable_level({"problems": "Level file is missing"}))
        self.assertTrue(catalog.unavailable_level({"route_problem": "Unsupported level version 27"}))
        mission = {"game": "d2", "target_index": 0, "mission_filename": "d2x.mn2"}
        self.assertTrue(catalog.vertigo_campaign(catalog.VERTIGO_BASELINE, mission))
        self.assertFalse(catalog.vertigo_campaign("Vertigo Missions.json", mission))
        self.assertFalse(catalog.vertigo_campaign(catalog.VERTIGO_BASELINE, {**mission, "target_index": 1}))
        self.assertFalse(catalog.vertigo_campaign("ewithin-versions.json", mission))


if __name__ == "__main__":
    unittest.main()

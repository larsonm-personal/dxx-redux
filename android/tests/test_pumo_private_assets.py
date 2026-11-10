"""Asset-free rejection tests for the trusted full-Pumo staging boundary."""

import hashlib
import importlib.util
import json
import stat
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    "pumo_private_assets", Path(__file__).with_name("pumo_private_assets.py")
)
pumo = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pumo)


class Entry:
    def __init__(self, pathname, size=1, filetype=stat.S_IFREG):
        self.pathname, self.size, self.filetype = pathname, size, filetype


class PumoPrivateAssetsTest(unittest.TestCase):
    def test_staging_workflow_has_no_candidate_or_native_execution(self):
        workflow = (
            Path(__file__).parents[2] / ".github/workflows/nightly-tooling-sweep.yml"
        ).read_text()
        job = workflow.split("  pumo_stage:\n", 1)[1].split("  execution-stack:\n", 1)[
            0
        ]
        self.assertIn("name: Verify pinned Pumo staging", job)
        self.assertIn("inputs.campaign == 'pumo-stage'", job)
        self.assertIn("ref: ${{ github.workflow_sha }}", job)
        self.assertIn("nightly_retail.trusted_context()", job)
        self.assertIn(
            "not os.environ['CANDIDATE_PR'] and not os.environ['CANDIDATE_SHA']", job
        )
        self.assertIn("github.ref == 'refs/heads/cmake'", job)
        self.assertIn(
            "github.workflow_ref == 'larsonm-personal/dxx-redux/.github/workflows/nightly-tooling-sweep.yml@refs/heads/cmake'",
            job,
        )
        self.assertIn("ref: dc6565f6b5400d7c700fa8378e766b2354b6ceb6", job)
        self.assertIn("tools/pumo-full/archive.py", job)
        self.assertIn("xl-resources/d2x-xl.hog", job)
        self.assertIn("if: always()", job)
        self.assertLess(
            job.index("Reject candidate inputs"),
            job.index("Check private credential availability"),
        )
        self.assertLess(
            job.index("Check private credential availability"),
            job.index("Check out separate immutable"),
        )
        for forbidden in (
            "target_sha",
            "path: target",
            "run_pumo_full",
            "ctest",
            "xl-preview-lifecycle",
            "retail-input",
        ):
            self.assertNotIn(forbidden, job)
        policy = (Path(__file__).with_name("nightly_tooling_policy.json")).read_text()
        self.assertNotIn("pumo-stage", policy)

    def test_safe_path_rejects_escape_and_control_names(self):
        for name in (
            "",
            "/absolute",
            "../escape",
            "x/../escape",
            "C:/drive",
            "x\\y",
            "x//y",
            "x/./y",
            "x\ny",
            "x\0y",
        ):
            with self.subTest(name=name), self.assertRaises(ValueError):
                pumo.safe_path(name)

    def test_inventory_accepts_bounded_regular_fixture(self):
        entries = [
            Entry("missions", 0, stat.S_IFDIR),
            Entry("missions/single", 0, stat.S_IFDIR),
            Entry("missions/single/pmines.hog", 2),
            Entry("missions/single/pmines.mn2", 3),
            Entry("mods/pmines/models/model171.oof", 4),
        ]
        selected = pumo.inventory(entries, expected_counts=(3, 2, 9))
        self.assertEqual(
            selected["missions/single/pmines.hog"], ("missions/pmines.hog", 2)
        )

    def test_inventory_rejects_links_duplicates_overlap_limits_and_missing(self):
        cases = [
            [Entry("x", filetype=stat.S_IFLNK)],
            [Entry("x"), Entry("X")],
            [Entry("x"), Entry("x/y")],
            [Entry("x", pumo.MAX_EXPANDED + 1)],
            [Entry(f"x/{n}") for n in range(pumo.MAX_ENTRIES + 1)],
            [Entry("missions/single/pmines.hog")],
        ]
        for entries in cases:
            with self.subTest(case=entries[0].pathname), self.assertRaises(ValueError):
                pumo.inventory(
                    entries,
                    expected_counts=(len(entries), 0, sum(e.size for e in entries)),
                )

    def test_reconstruct_rejects_preexisting_output_before_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "archive.7z"
            output.write_bytes(b"keep")
            with self.assertRaises(FileExistsError):
                pumo.reconstruct(root / "missing", output)
            self.assertEqual(output.read_bytes(), b"keep")

    def test_reconstruct_rejects_missing_or_linked_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = root / "fixture"
            fixture.mkdir()
            with self.assertRaises((ValueError, FileNotFoundError)):
                pumo.reconstruct(fixture, root / "archive.7z")
            (fixture / "manifest.json").symlink_to(root / "missing")
            with self.assertRaises((ValueError, FileNotFoundError)):
                pumo.reconstruct(fixture, root / "archive.7z")

    def test_reconstruct_verifies_pins_chunks_and_no_replace(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = root / "fixture"
            (fixture / "chunks").mkdir(parents=True)
            payload = b"original"
            chunk = fixture / "chunks/pmines_v11.7z.000"
            chunk.write_bytes(payload)
            source = {
                "filename": "pmines_v11.7z",
                "size": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest(),
            }
            manifest = {
                "profile": "pumo-full-archive-v1",
                "source": source,
                "chunk_size": len(payload),
                "chunks": [
                    {
                        "path": "chunks/pmines_v11.7z.000",
                        "size": len(payload),
                        "sha256": source["sha256"],
                    }
                ],
            }
            raw = json.dumps(manifest).encode()
            (fixture / "manifest.json").write_bytes(raw)
            pins = {
                "ARCHIVE_SIZE": len(payload),
                "ARCHIVE_SHA256": source["sha256"],
                "CHUNK_SIZE": len(payload),
                "MANIFEST_SHA256": hashlib.sha256(raw).hexdigest(),
            }
            with patch.multiple(pumo, **pins):
                output = root / "archive.7z"
                pumo.reconstruct(fixture, output)
                self.assertEqual(output.read_bytes(), payload)
                with self.assertRaises(FileExistsError):
                    pumo.reconstruct(fixture, output)
                output.unlink()
                chunk.write_bytes(b"corrupt!")
                with self.assertRaises(ValueError):
                    pumo.reconstruct(fixture, output)
                self.assertFalse(output.exists())
                chunk.unlink()
                with self.assertRaises(ValueError):
                    pumo.reconstruct(fixture, output)


if __name__ == "__main__":
    unittest.main()

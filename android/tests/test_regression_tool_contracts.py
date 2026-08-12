import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
NORMALIZER = ROOT / "android/helpers/normalize_json.py"


def normalize(text: str, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(NORMALIZER), *args],
        input=text,
        text=True,
        capture_output=True,
        check=False,
    )


class RegressionToolContractsTest(unittest.TestCase):
    def test_desktop_targets_keep_public_executable_names(self) -> None:
        for game, public_name in (("d1", "d1x-redux"), ("d2", "d2x-redux")):
            text = (ROOT / game / "main/CMakeLists.txt").read_text(encoding="utf-8")
            start = text.index("if(ANDROID)")
            desktop = text[start : text.index("endif()", start)]
            self.assertIn(f"add_executable(dxx-redux-{game}", desktop)
            self.assertIn(
                f"set_target_properties(dxx-redux-{game} PROPERTIES OUTPUT_NAME {public_name})",
                desktop,
            )
            self.assertLess(desktop.index("add_executable"), desktop.index("OUTPUT_NAME"))

    def test_mission_batch_preflights_before_hash_or_inspection(self) -> None:
        source = (ROOT / "android/helpers/run_mission_zip_batch.ps1").read_text(encoding="utf-8")
        loop = source[source.index("foreach ($zip in $zips)") :]
        size = loop.index('$record["size_bytes"] = $zip.Length')
        gate = loop.index('$record["size_bytes"] -gt $LargeZipBytes')
        self.assertLess(size, gate)
        self.assertLess(gate, loop.index("Get-FileHash -Algorithm SHA256"))
        self.assertLess(gate, loop.index("Get-MissionZipGameHint -ZipPath"))
        catch = loop.index('"Archive preflight failed:')
        continuation = loop.index("continue", catch)
        failure = loop[catch - 500 : continuation + 20]
        for token in (
            '$record["status"] = "failed"',
            "Write-MissionZipFailureJson",
            '$results += [pscustomobject]$record',
            "Write-MissionZipBatchResult",
        ):
            self.assertIn(token, failure)

        start = source.index("function Get-MissionZipGameHint")
        function = source[start : source.index("function ", start + 20)]
        self.assertIn("Get-MissionArchiveEntryNames", function)
        self.assertNotIn("Could not inspect archive", function)

    def test_mission_metadata_json_gate(self) -> None:
        for text in ("", "{", "null", "42", '"text"'):
            with self.subTest(text=text):
                self.assertNotEqual(0, normalize(text, "--mission-metadata").returncode)
        for text in ('{"mine_volume":1}', '[{"mine_volume":1}]'):
            with self.subTest(text=text):
                result = normalize(text, "--mission-metadata")
                self.assertEqual(0, result.returncode, result.stderr)
                self.assertIn("1.0", result.stdout)

        source = (ROOT / "android/helpers/run_mission_zip_batch.ps1").read_text(encoding="utf-8")
        self.assertIn('throw "JSON text is empty"', source)
        self.assertIn("JSON formatter timed out after 30 seconds", source)
        self.assertIn("metadata JSON validation failed", source)
        self.assertNotIn("writing raw text", source.lower())

    def test_normalizer_rejects_ambiguous_json(self) -> None:
        for token in ("NaN", "Infinity", "-Infinity"):
            result = normalize(f'{{"value":{token}}}')
            self.assertNotEqual(0, result.returncode)
            self.assertEqual("", result.stdout)
            self.assertIn("non-finite", result.stderr)
        for text in ('{"x":1,"x":2}', '{"outer":{"x":1,"x":2}}', '{"a":1,"\\u0061":2}'):
            result = normalize(text)
            self.assertNotEqual(0, result.returncode)
            self.assertIn("duplicate JSON object member", result.stderr)
        self.assertEqual(0, normalize('[{"x":1},{"x":2}]').returncode)

    def test_normalizer_file_failure_preserves_original(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "ambiguous.json"
            original = b'{"x":1,"x":2}\n'
            path.write_bytes(original)
            result = subprocess.run(
                [sys.executable, str(NORMALIZER), str(path)],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertNotEqual(0, result.returncode)
            self.assertEqual(original, path.read_bytes())


if __name__ == "__main__":
    unittest.main()

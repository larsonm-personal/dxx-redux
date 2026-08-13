import hashlib
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / "cmake" / "dxx-verified-dependencies.cmake"
MANIFEST = ROOT / "android" / "get_deps" / "tool_versions.conf"
CMAKE = shutil.which("cmake") or str(Path("C:/local/android-sdk/cmake/3.31.6/bin/cmake.exe"))
PRODUCTION_PREFIXES = (
    "SDL12",
    "SDL_MIXER12",
    "TINYSOUNDFONT",
    "NLOHMANN_JSON",
    "PHYSFS",
    "LZMA_SDK",
    "CHROMAPRINT_ANDROID",
    "MINIMP3",
    "MINIMP3_EX",
    "STB_VORBIS",
    "DR_FLAC",
    "STB_IMAGE",
    "KTX_SOFTWARE",
    "INPUT_DEMO_PICOSHA2",
    "INPUT_DEMO_CPP_BASE64",
)


class VerifiedNativeDependenciesTest(unittest.TestCase):
    def setUp(self):
        temp_root = ROOT / "android" / "temp"
        temp_root.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=temp_root)
        self.work = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def write_manifest(self, source: Path, expected: bytes) -> Path:
        manifest = self.work / "manifest.conf"
        digest = hashlib.sha256(expected).hexdigest()
        manifest.write_text(f"FIXTURE_URL={source.as_uri()}\nFIXTURE_SHA256={digest}\n", encoding="ascii")
        return manifest

    def run_fixture(self, manifest: Path, expect_success: bool = True):
        script = self.work / "driver.cmake"
        output = self.work / "output.bin"
        cache = self.work / "cache"
        script.write_text(
            f'set(DXX_DEPENDENCY_MANIFEST "{manifest.as_posix()}")\n'
            f'set(DXX_DEPENDENCY_CACHE_DIR "{cache.as_posix()}")\n'
            f'include("{HELPER.as_posix()}")\n'
            f'dxx_verified_source(FIXTURE "{output.as_posix()}")\n',
            encoding="ascii",
        )
        result = subprocess.run(
            [CMAKE, "-P", str(script)],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        if expect_success and result.returncode != 0:
            self.fail(result.stdout + result.stderr)
        if not expect_success and result.returncode == 0:
            self.fail("verification unexpectedly accepted hostile bytes")
        return result, output, cache

    def test_manifest_covers_every_production_download(self):
        values = {
            line.split("=", 1)[0]
            for line in MANIFEST.read_text(encoding="utf-8").splitlines()
            if "=" in line and not line.startswith("#")
        }
        for prefix in PRODUCTION_PREFIXES:
            self.assertIn(f"{prefix}_URL", values)
            self.assertIn(f"{prefix}_SHA256", values)

        owners = (
            ROOT / "android/app/src/main/cpp/CMakeLists.txt",
            ROOT / "cmake/input-demo-codec-deps.cmake",
        )
        for owner in owners:
            text = owner.read_text(encoding="utf-8")
            self.assertNotIn("GIT_REPOSITORY", text)
            self.assertNotIn("file(\n        DOWNLOAD", text)
            self.assertNotIn(" URL https://", text)

    def test_verified_cache_supports_offline_reuse_and_rejects_corruption(self):
        source = self.work / "source.bin"
        expected = b"reviewed dependency bytes\n"
        source.write_bytes(expected)
        manifest = self.write_manifest(source, expected)

        _, output, cache = self.run_fixture(manifest)
        self.assertEqual(expected, output.read_bytes())

        source.unlink()
        output.unlink()
        self.run_fixture(manifest)
        self.assertEqual(expected, output.read_bytes())

        cached_file = next(cache.glob("FIXTURE-*.source"))
        cached_file.write_bytes(b"hostile cache body\n")
        result, _, _ = self.run_fixture(manifest, expect_success=False)
        self.assertIn("Cached FIXTURE SHA-256 mismatch", result.stdout + result.stderr)

    def test_hostile_download_and_changed_pin_are_checked(self):
        source = self.work / "source.bin"
        expected = b"expected bytes\n"
        source.write_bytes(b"hostile response\n")
        manifest = self.write_manifest(source, expected)
        self.run_fixture(manifest, expect_success=False)

        replacement = b"new reviewed generation\n"
        source.write_bytes(replacement)
        manifest = self.write_manifest(source, replacement)
        _, output, cache = self.run_fixture(manifest)
        self.assertEqual(replacement, output.read_bytes())
        self.assertEqual(1, len(tuple(cache.glob("FIXTURE-*.source"))))


if __name__ == "__main__":
    unittest.main()

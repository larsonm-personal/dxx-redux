import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "helpers"))
import recover_forest


def pog(rows):
    header = b"DPOG" + struct.pack("<II", 1, len(rows))
    directory = b"".join(struct.pack("<H", index) for index, _ in rows)
    pixels = bytearray()
    for ordinal, (_, color) in enumerate(rows):
        directory += struct.pack("<8s6BI", f"POG{ordinal:04d}".encode(), 0, 1, 1, 0, 0, color, len(pixels))
        pixels.append(color)
    return header + directory + pixels


def hog_member(name, data):
    return struct.pack("<13si", name.encode(), len(data)) + data


class ForestRecovery(unittest.TestCase):
    def test_local_recovery_preserves_sources_and_refuses_replacement(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "diedel").mkdir()
            (root / "single").mkdir()
            descriptor = b"d2x-name = The Forest\nnum_levels = 1\nforest.rl2\n"
            hog = b"DHF" + hog_member("forest.rl2", b"level")
            bank = pog([(7, 15), (3, 27), (12, 61)])
            expected = pog([(12, 61), (7, 15)])
            monolevel = b"DHF" + hog_member("dinter.pog", bank)
            files = {"diedel/forest.mn2": descriptor, "diedel/forest.hog": hog, "single/dinter.hog": monolevel}
            for name, data in files.items():
                (root / name).write_bytes(data)
            output = root / "recovered.zip"
            hashes = dict(FOREST_DESCRIPTOR=descriptor, FOREST_HOG=hog, MONOLEVEL_POG=bank, FOREST_POG=expected)
            with patch.multiple(
                recover_forest,
                FOREST_BITMAP_IDS=(12, 7),
                **{key: hashlib.sha256(value).hexdigest() for key, value in hashes.items()},
            ):
                recover_forest.recover(root, output)
                with zipfile.ZipFile(output) as archive:
                    self.assertEqual(archive.namelist(), ["forest.mn2", "forest.hog"])
                    self.assertEqual(archive.read("forest.mn2"), descriptor)
                    self.assertEqual(archive.read("forest.hog"), hog + hog_member("forest.pog", expected))
                for name, data in files.items():
                    self.assertEqual((root / name).read_bytes(), data)
                first = output.read_bytes()
                with self.assertRaises(FileExistsError):
                    recover_forest.recover(root, output)
                self.assertEqual(output.read_bytes(), first)
                (root / "single/dinter.hog").write_bytes(monolevel[:-1] + b"x")
                with self.assertRaisesRegex(ValueError, "dinter.pog differs"):
                    recover_forest.recover(root, root / "wrong.zip")
                self.assertFalse((root / "wrong.zip").exists())


if __name__ == "__main__":
    unittest.main()

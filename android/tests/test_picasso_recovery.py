import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "helpers"))
import recover_collider
import recover_picasso


class MissionSpellingRecovery(unittest.TestCase):
    def test_corrects_verified_descriptor_and_keeps_sibling_hog(self):
        for helper, stem, first_level, misspelled, corrected in (
            (recover_picasso, "picasso", None, "picasso.rl2", "picaso.rl2"),
            (recover_collider, "SPC-XL", "spc-xl.rl2", "sp-xxl.rl2", "spc-xxl.rl2"),
        ):
            with self.subTest(mission=stem):
                self.check_recovery(helper, stem, first_level, misspelled, corrected)

    def check_recovery(self, helper, stem, first_level, misspelled, corrected):
        prefix = b"name = recovery fixture\r\n"
        prefix += b"num_levels = 2\r\n" + first_level.encode() + b"\r\n" if first_level else b"num_levels = 1\r\n"
        descriptor = prefix + misspelled.encode() + b"\r\n"
        hog = b"DHF" + struct.pack("<13si", corrected.encode(), 5) + b"level"
        with (
            tempfile.TemporaryDirectory() as tmp,
            patch.multiple(
                helper,
                DESCRIPTOR_SHA256=hashlib.sha256(descriptor).hexdigest(),
                HOG_SHA256=hashlib.sha256(hog).hexdigest(),
            ),
        ):
            root = Path(tmp)
            source = root / (stem + ".mn2")
            sibling = root / (stem + ".hog")
            source.write_bytes(descriptor)
            sibling.write_bytes(hog)
            archive = root / "recovered.zip"
            helper.recover(source, archive)
            with zipfile.ZipFile(archive) as result:
                self.assertEqual(result.namelist(), [source.name, sibling.name])
                self.assertEqual(result.read(source.name), prefix + corrected.encode() + b"\r\n")
                self.assertEqual(result.read(sibling.name), hog)
            published = archive.read_bytes()
            with self.assertRaises(FileExistsError):
                helper.recover(source, archive)
            self.assertEqual(archive.read_bytes(), published)
            self.assertEqual(source.read_bytes(), descriptor)
            self.assertEqual(sibling.read_bytes(), hog)
            other = root / "wrong.zip"
            source.write_bytes(b"other mission")
            with self.assertRaisesRegex(ValueError, "Descriptor"):
                helper.recover(source, other)
            source.write_bytes(descriptor)
            sibling.write_bytes(b"other HOG")
            with self.assertRaisesRegex(ValueError, "Sibling HOG"):
                helper.recover(source, other)
            self.assertFalse(other.exists())


if __name__ == "__main__":
    unittest.main()

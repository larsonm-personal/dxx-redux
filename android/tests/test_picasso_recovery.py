import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "helpers"))
import recover_picasso


class PicassoRecovery(unittest.TestCase):
    def test_corrects_verified_descriptor_and_keeps_sibling_hog(self):
        descriptor = b"name = recovery fixture\r\nnum_levels = 1\r\npicasso.rl2\r\n"
        hog = b"DHF" + struct.pack("<13si", b"picaso.rl2", 5) + b"level"
        with (
            tempfile.TemporaryDirectory() as tmp,
            patch.multiple(
                recover_picasso,
                DESCRIPTOR_SHA256=hashlib.sha256(descriptor).hexdigest(),
                HOG_SHA256=hashlib.sha256(hog).hexdigest(),
            ),
        ):
            root = Path(tmp)
            source = root / "Picasso.mn2"
            sibling = root / "picasso.hog"
            source.write_bytes(descriptor)
            sibling.write_bytes(hog)
            archive = root / "recovered.zip"
            recover_picasso.recover(source, archive)
            with zipfile.ZipFile(archive) as result:
                self.assertEqual(result.namelist(), ["picasso.mn2", "picasso.hog"])
                self.assertEqual(result.read("picasso.mn2"), descriptor.replace(b"picasso.rl2", b"picaso.rl2"))
                self.assertEqual(result.read("picasso.hog"), hog)
            with self.assertRaises(FileExistsError):
                recover_picasso.recover(source, archive)
            self.assertEqual(source.read_bytes(), descriptor)
            self.assertEqual(sibling.read_bytes(), hog)

            other = root / "other.zip"
            sibling.write_bytes(hog + b"x")
            with self.assertRaisesRegex(ValueError, "Sibling HOG differs"):
                recover_picasso.recover(source, other)
            source.write_bytes(descriptor[:-2])
            with self.assertRaisesRegex(ValueError, "Descriptor differs"):
                recover_picasso.recover(source, other)
            self.assertFalse(other.exists())


if __name__ == "__main__":
    unittest.main()

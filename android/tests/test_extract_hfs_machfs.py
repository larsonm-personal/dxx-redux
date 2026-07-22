#!/usr/bin/env python3

import importlib.util
import pathlib
import tempfile
import unittest


SCRIPT_PATH = pathlib.Path(__file__).parents[1] / "helpers" / "extract_hfs_machfs.py"
SPEC = importlib.util.spec_from_file_location("extract_hfs_machfs", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class FakeFolder(dict):
    pass


class FakeFile:
    def __init__(self, data):
        self.data = data


class ExtractHfsMachfsTests(unittest.TestCase):
    def test_extracts_ordinary_nested_file(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp) / "root"
            volume = FakeFolder({"Games": FakeFolder({"descent.hog": FakeFile(b"hog")})})

            MODULE.extract_folder(volume, root, root, FakeFolder, FakeFile)

            self.assertEqual((root / "Games" / "descent.hog").read_bytes(), b"hog")

    def test_rejects_unsafe_components_before_writing(self):
        unsafe_names = ("", ".", "..", "../escape", "..\\escape", "/escape", "C:\\escape")
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp) / "root"
            sentinel = pathlib.Path(temp) / "escape"
            sentinel.write_bytes(b"safe")

            for name in unsafe_names:
                with self.subTest(name=name):
                    volume = FakeFolder({name: FakeFile(b"bad")})
                    with self.assertRaises(ValueError):
                        MODULE.extract_folder(volume, root, root, FakeFolder, FakeFile)
                    self.assertEqual(sentinel.read_bytes(), b"safe")


if __name__ == "__main__":
    unittest.main()

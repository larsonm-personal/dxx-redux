#!/usr/bin/env python3

import pathlib
import re
import unittest


REPO_ROOT = pathlib.Path(__file__).parents[2]
EXTRACT_DIR = REPO_ROOT / "android" / "app" / "src" / "main" / "cpp" / "extract"


class InnoCapabilityDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (EXTRACT_DIR / "inno_reader.c").read_text(encoding="utf-8")
        cls.header = (EXTRACT_DIR / "inno_reader.h").read_text(encoding="utf-8")
        cls.docs = (EXTRACT_DIR / "INNO_READER_CAPABILITIES.md").read_text(encoding="utf-8")

    def test_reader_files_link_the_capability_matrix(self):
        for text in (self.source, self.header):
            self.assertIn("INNO_READER_CAPABILITIES.md", text)
            self.assertIn("this is a single file AI-slop implementation", text)
            self.assertIn("the historical preamble above is preserved verbatim", text)

    def test_documented_version_gate_matches_the_parser(self):
        self.assertIn("version->major == 5", self.source)
        self.assertIn("version->minor >= 3", self.source)
        self.assertIn("version->minor <= 6", self.source)
        self.assertIn("version->patch <= 99", self.source)
        self.assertIsNone(re.search(r"#define\s+INNO_VER\s*\(", self.source))
        self.assertIn("5.3.0 through 5.6.99", self.docs)
        self.assertIn("5.3.8 MD5 and 5.3.9 SHA-1", self.docs)
        self.assertIn("5.5.7 and 5.6.2 Unicode", self.docs)
        self.assertIn("MD5 before 5.3.9 and SHA-1 from 5.3.9 onward", self.docs)

    def test_documented_data_methods_match_decoder_switches(self):
        implemented = ("STORED", "ZLIB", "LZMA1", "LZMA2")
        for method in implemented:
            self.assertIn(f"INNO_COMPRESS_{method}", self.header)
            self.assertIn(f"actual_method == INNO_COMPRESS_{method}", self.source)
        self.assertIn("BZip2 data chunks | Unsupported and rejected", self.docs)
        self.assertIn("BZip2 decompression not implemented", self.source)

    def test_encryption_and_callback_limitations_remain_explicit(self):
        self.assertIn("chunk_encrypted", self.header)
        self.assertIn("de->chunk_encrypted", self.source)
        self.assertIn("Encrypted chunks | Unsupported and rejected", self.docs)
        self.assertIn("informational and cannot cancel extraction", self.docs)
        self.assertIsNone(re.search(r"if\s*\(\s*(?:writer->)?progress\s*\(", self.source))


if __name__ == "__main__":
    unittest.main()

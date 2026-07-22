#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


SCRIPT_PATH = pathlib.Path(__file__).parents[1] / "helpers" / "run_bounded_extractor.py"
SPEC = importlib.util.spec_from_file_location("run_bounded_extractor", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class RunBoundedExtractorTests(unittest.TestCase):
    def run_child(self, code, output_dir, **limits):
        defaults = {
            "timeout_seconds": 2,
            "max_files": 4,
            "max_file_bytes": 64,
            "max_total_bytes": 128,
            "max_diagnostic_bytes": 128,
        }
        defaults.update(limits)
        return MODULE.run_bounded(
            [sys.executable, "-c", code, str(output_dir)],
            str(output_dir),
            **defaults,
        )

    def test_accepts_bounded_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            code = "import pathlib,sys; pathlib.Path(sys.argv[1],'ok').write_bytes(b'ok')"
            self.assertEqual(self.run_child(code, root), 0)

    def test_rejects_large_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            code = "import pathlib,sys,time; pathlib.Path(sys.argv[1],'large').write_bytes(b'x'*65); time.sleep(2)"
            self.assertNotEqual(self.run_child(code, root), 0)

    def test_rejects_excessive_files_and_total_output(self):
        cases = (
            "import pathlib,sys,time; root=pathlib.Path(sys.argv[1]); "
            "[(root/str(i)).write_bytes(b'x') for i in range(5)]; time.sleep(2)",
            "import pathlib,sys,time; root=pathlib.Path(sys.argv[1]); "
            "[(root/str(i)).write_bytes(b'x'*50) for i in range(3)]; time.sleep(2)",
        )
        for code in cases:
            with self.subTest(code=code), tempfile.TemporaryDirectory() as temp:
                self.assertNotEqual(self.run_child(code, temp), 0)

    def test_rejects_large_diagnostics(self):
        with tempfile.TemporaryDirectory() as temp:
            code = "import sys,time; sys.stdout.write('x'*129); sys.stdout.flush(); time.sleep(2)"
            self.assertNotEqual(self.run_child(code, temp), 0)

    def test_times_out(self):
        with tempfile.TemporaryDirectory() as temp:
            self.assertNotEqual(
                self.run_child("import time; time.sleep(2)", temp, timeout_seconds=0.1),
                0,
            )


if __name__ == "__main__":
    unittest.main()

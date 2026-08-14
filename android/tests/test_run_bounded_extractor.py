#!/usr/bin/env python3

import importlib.util
import pathlib
import socket
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock


SCRIPT_PATH = pathlib.Path(__file__).parents[1] / "helpers" / "run_bounded_extractor.py"
SPEC = importlib.util.spec_from_file_location("run_bounded_extractor", SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class RunBoundedExtractorTests(unittest.TestCase):
    def run_child(self, code, output_dir, **limits):
        defaults = {
            "timeout_seconds": 2,
            "max_files": 4,
            "max_file_bytes": 16384,
            "max_total_bytes": 32768,
            "max_diagnostic_bytes": 128,
        }
        defaults.update(limits)
        return MODULE.run_bounded(
            [sys.executable, "-c", code, str(output_dir)],
            str(output_dir),
            **defaults,
        )

    def assert_descendant_cleanup(self, mode):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            descendant_marker = root / "descendant-survived"
            sentinel_marker = root / "sentinel-survived"
            sentinel_code = (
                "import pathlib,sys,time; time.sleep(0.5); "
                "pathlib.Path(sys.argv[1]).write_text('alive')")
            sentinel = subprocess.Popen(
                [sys.executable, "-c", sentinel_code, str(sentinel_marker)])
            descendant_code = (
                "import pathlib,sys,time; time.sleep(0.8); "
                "pathlib.Path(sys.argv[1]).write_text('alive')")
            parent_code = (
                "import pathlib,subprocess,sys,time; "
                f"subprocess.Popen([sys.executable,'-c',{descendant_code!r},"
                "str(pathlib.Path(sys.argv[1])/'descendant-survived')]); "
                "pathlib.Path(sys.argv[1],'parent-ready').write_text('ready'); "
                + ({
                    "success": "sys.exit(0)",
                    "failure": "sys.exit(7)",
                    "timeout": "time.sleep(5)",
                }[mode])
            )
            try:
                result = self.run_child(
                    parent_code, root,
                    timeout_seconds=0.2 if mode == "timeout" else 2)
                self.assertEqual(result == 0, mode == "success")
                sentinel.wait(timeout=2)
                time.sleep(0.6)
                self.assertTrue(sentinel_marker.exists())
                self.assertFalse(descendant_marker.exists())
            finally:
                if sentinel.poll() is None:
                    sentinel.kill()
                    sentinel.wait()

    def test_accepts_bounded_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            code = "import pathlib,sys; pathlib.Path(sys.argv[1],'ok').write_bytes(b'ok')"
            self.assertEqual(self.run_child(code, root), 0)

    def test_measurement_ignores_a_file_removed_after_enumeration(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            stable = root / "stable"
            transient = root / "transient.tmp.0"
            stable.write_bytes(b"stable")
            transient.write_bytes(b"temporary")
            real_scandir = MODULE.os.scandir

            def racing_scandir(path):
                entries = list(real_scandir(path))
                transient.unlink()
                return iter(entries)

            with mock.patch.object(MODULE.os, "scandir", side_effect=racing_scandir):
                files, total = MODULE.measure_tree(root, 4, 16384, 16384)

            self.assertEqual(files, 1)
            self.assertGreaterEqual(total, len(b"stable"))

    def test_strict_measurement_rejects_a_disappearing_file(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            transient = root / "transient"
            transient.write_bytes(b"temporary")
            real_scandir = MODULE.os.scandir

            def racing_scandir(path):
                entries = list(real_scandir(path))
                transient.unlink()
                return iter(entries)

            with mock.patch.object(MODULE.os, "scandir", side_effect=racing_scandir):
                with self.assertRaisesRegex(RuntimeError, "changed during validation"):
                    MODULE.measure_tree(root, 4, 16384, 16384, strict=True)

    def test_accepts_an_ordinary_nested_tree(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            (root / "nested").mkdir()
            (root / "nested" / "payload").write_bytes(b"payload")
            files, total = MODULE.measure_tree(
                root, 4, 16384, 16384, strict=True)
            self.assertEqual(files, 1)
            self.assertGreaterEqual(total, len(b"payload"))

    def test_rejects_hard_link_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            source = root / "source"
            source.write_bytes(b"payload")
            os_link = getattr(MODULE.os, "link", None)
            if os_link is None:
                self.skipTest("hard links are unavailable")
            os_link(source, root / "alias")
            with self.assertRaisesRegex(RuntimeError, "multiply-linked|identity"):
                MODULE.measure_tree(root, 8, 16384, 32768, strict=True)

    def test_successful_child_is_failed_before_unsafe_output_can_publish(self):
        with tempfile.TemporaryDirectory() as temp:
            parent = pathlib.Path(temp)
            root = parent / "output"
            root.mkdir()
            sentinel = parent / "sentinel"
            sentinel.write_text("preserve")
            code = (
                "import os,pathlib,sys; "
                "os.link(sys.argv[2],pathlib.Path(sys.argv[1])/'alias')")
            result = MODULE.run_bounded(
                [sys.executable, "-c", code, str(root), str(sentinel)],
                str(root), 2, 4, 16384, 16384, 128)
            self.assertNotEqual(result, 0)
            self.assertEqual(sentinel.read_text(), "preserve")

    def test_rejects_symlink_escape_without_touching_sentinel(self):
        with tempfile.TemporaryDirectory() as temp:
            parent = pathlib.Path(temp)
            root = parent / "output"
            root.mkdir()
            sentinel = parent / "sentinel"
            sentinel.write_text("preserve")
            try:
                (root / "escape").symlink_to(sentinel)
            except OSError as error:
                self.skipTest(f"symlinks are unavailable: {error}")
            with self.assertRaisesRegex(RuntimeError, "link|reparse"):
                MODULE.measure_tree(root, 4, 16384, 16384, strict=True)
            self.assertEqual(sentinel.read_text(), "preserve")

    @unittest.skipUnless(MODULE.os.name == "nt", "Windows junction fixture")
    def test_rejects_junction_escape_without_touching_sentinel(self):
        with tempfile.TemporaryDirectory() as temp:
            parent = pathlib.Path(temp)
            root = parent / "output"
            target = parent / "outside"
            root.mkdir()
            target.mkdir()
            sentinel = target / "sentinel"
            sentinel.write_text("preserve")
            result = subprocess.run(
                ["cmd", "/d", "/c", "mklink", "/J", str(root / "escape"), str(target)],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
            if result.returncode:
                self.skipTest("junction creation is unavailable")
            with self.assertRaisesRegex(RuntimeError, "reparse"):
                MODULE.measure_tree(root, 4, 16384, 16384, strict=True)
            self.assertEqual(sentinel.read_text(), "preserve")

    @unittest.skipIf(MODULE.os.name == "nt", "POSIX special-file fixtures")
    def test_rejects_fifo_and_socket_output(self):
        makers = {
            "fifo": lambda path: MODULE.os.mkfifo(path),
            "socket": lambda path: socket.socket(socket.AF_UNIX),
        }
        for kind, maker in makers.items():
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as temp:
                root = pathlib.Path(temp)
                path = root / kind
                resource = maker(path)
                if kind == "socket":
                    resource.bind(str(path))
                try:
                    with self.assertRaisesRegex(RuntimeError, "link or special file"):
                        MODULE.measure_tree(root, 4, 16384, 16384, strict=True)
                finally:
                    if kind == "socket":
                        resource.close()

    @unittest.skipIf(MODULE.os.name == "nt", "POSIX device fixture")
    def test_rejects_device_output_when_creation_is_permitted(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            path = root / "device"
            try:
                MODULE.os.mknod(
                    path, MODULE.stat.S_IFCHR | 0o600, MODULE.os.makedev(1, 3))
            except (AttributeError, OSError) as error:
                self.skipTest(f"device creation is unavailable: {error}")
            with self.assertRaisesRegex(RuntimeError, "link or special file"):
                MODULE.measure_tree(root, 4, 16384, 16384, strict=True)

    @unittest.skipIf(MODULE.os.name == "nt", "POSIX allocation fixture")
    def test_counts_allocated_bytes_against_the_total(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            (root / "payload").write_bytes(b"x")
            allocated = MODULE.os.lstat(root / "payload").st_blocks * 512
            if allocated <= 1:
                self.skipTest("filesystem does not expose allocation blocks")
            with self.assertRaisesRegex(RuntimeError, "accounted bytes"):
                MODULE.measure_tree(root, 4, allocated, allocated - 1, strict=True)

    def test_rejects_sparse_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            path = root / "sparse"
            if MODULE.os.name == "nt":
                path.write_bytes(b"")
                import msvcrt
                with path.open("r+b") as stream:
                    handle = msvcrt.get_osfhandle(stream.fileno())
                    returned = MODULE.wintypes.DWORD()
                    if not MODULE.kernel32.DeviceIoControl(
                            handle, 0x000900C4, None, 0, None, 0,
                            MODULE.ctypes.byref(returned), None):
                        self.skipTest("filesystem does not permit sparse files")
                with path.open("r+b") as stream:
                    stream.truncate(1024 * 1024)
            else:
                with path.open("wb") as stream:
                    stream.seek(1024 * 1024 - 1)
                    stream.write(b"x")
                info = MODULE.os.lstat(path)
                if info.st_blocks * 512 >= info.st_size:
                    self.skipTest("filesystem did not create a sparse file")
            with self.assertRaisesRegex(RuntimeError, "sparse"):
                MODULE.measure_tree(root, 4, 2 * 1024 * 1024, 2 * 1024 * 1024,
                                    strict=True)

    @unittest.skipUnless(MODULE.os.name == "nt", "Windows stream fixture")
    def test_rejects_alternate_data_stream_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            path = root / "payload"
            path.write_bytes(b"payload")
            try:
                with open(str(path) + ":hidden", "wb") as stream:
                    stream.write(b"hidden")
            except OSError as error:
                self.skipTest(f"alternate streams are unavailable: {error}")
            with self.assertRaisesRegex(RuntimeError, "alternate data stream"):
                MODULE.measure_tree(root, 4, 16384, 16384, strict=True)

    def test_rejects_large_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            code = "import pathlib,sys,time; pathlib.Path(sys.argv[1],'large').write_bytes(b'x'*65); time.sleep(2)"
            self.assertNotEqual(self.run_child(code, root, max_file_bytes=64), 0)

    def test_rejects_excessive_files_and_total_output(self):
        cases = (
            "import pathlib,sys,time; root=pathlib.Path(sys.argv[1]); "
            "[(root/str(i)).write_bytes(b'x') for i in range(5)]; time.sleep(2)",
            "import pathlib,sys,time; root=pathlib.Path(sys.argv[1]); "
            "[(root/str(i)).write_bytes(b'x'*50) for i in range(3)]; time.sleep(2)",
        )
        for code in cases:
            with self.subTest(code=code), tempfile.TemporaryDirectory() as temp:
                limits = ({"max_files": 4} if "range(5)" in code else {
                    "max_total_bytes": 128,
                })
                self.assertNotEqual(self.run_child(code, temp, **limits), 0)

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

    def test_descendants_are_stopped_after_success(self):
        self.assert_descendant_cleanup("success")

    def test_descendants_are_stopped_after_failure(self):
        self.assert_descendant_cleanup("failure")

    def test_descendants_are_stopped_after_timeout(self):
        self.assert_descendant_cleanup("timeout")

    def test_descendants_are_stopped_after_cancellation(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            descendant_marker = root / "descendant-survived"
            sentinel_marker = root / "sentinel-survived"
            delayed_write = (
                "import pathlib,sys,time; time.sleep(0.8); "
                "pathlib.Path(sys.argv[1]).write_text('alive')")
            sentinel = subprocess.Popen(
                [sys.executable, "-c", delayed_write, str(sentinel_marker)])
            parent_code = (
                "import pathlib,subprocess,sys,time; "
                f"subprocess.Popen([sys.executable,'-c',{delayed_write!r},"
                "str(pathlib.Path(sys.argv[1])/'descendant-survived')]); "
                "pathlib.Path(sys.argv[1],'parent-ready').write_text('ready'); "
                "time.sleep(5)")
            supervisor = subprocess.Popen(
                [
                    sys.executable, "-I", str(SCRIPT_PATH),
                    "--output-dir", str(root),
                    "--timeout-seconds", "4",
                    "--max-files", "20",
                    "--max-file-bytes", "1024",
                    "--max-total-bytes", "4096",
                    "--max-diagnostic-bytes", "1024",
                    "--", sys.executable, "-c", parent_code, str(root),
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            try:
                deadline = time.monotonic() + 2
                while not (root / "parent-ready").exists():
                    if supervisor.poll() is not None or time.monotonic() >= deadline:
                        self.fail("cancelled supervisor did not start its process tree")
                    time.sleep(0.02)
                supervisor.terminate()
                supervisor.wait(timeout=3)
                sentinel.wait(timeout=2)
                time.sleep(0.4)
                self.assertTrue(sentinel_marker.exists())
                self.assertFalse(descendant_marker.exists())
            finally:
                for process in (supervisor, sentinel):
                    if process.poll() is None:
                        process.kill()
                        process.wait()

    def test_ownership_failure_does_not_start_the_command(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            marker = root / "started"
            code = "import pathlib,sys; pathlib.Path(sys.argv[1],'started').touch()"
            with mock.patch.object(
                    MODULE, "start_owned_process",
                    side_effect=RuntimeError("ownership unavailable")):
                with self.assertRaisesRegex(RuntimeError, "ownership unavailable"):
                    self.run_child(code, root)
            self.assertFalse(marker.exists())


if __name__ == "__main__":
    unittest.main()

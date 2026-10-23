#!/usr/bin/env python3
"""Exercise real partial HTTP transfers and fail-closed CMake cache publication."""

import hashlib
import http.server
from pathlib import Path
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]
PAYLOAD = b"pinned dependency fixture\n"
COUNTS = {}


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        count = COUNTS[self.path] = COUNTS.get(self.path, 0) + 1
        if self.path == "/busy" and count == 1:
            self.send_error(503)
            return
        if self.path in ("/missing", "/shell-missing"):
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(PAYLOAD)))
        self.end_headers()
        partial = self.path == "/partial" or (self.path == "/retry" and count == 1)
        self.wfile.write(PAYLOAD[:3] if partial else PAYLOAD)
        self.close_connection = True

    def log_message(self, *_args):
        pass


with tempfile.TemporaryDirectory(prefix="dxx-download-") as tmp:
    directory = Path(tmp)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        for route, attempts, success in [
            ("retry", 2, True),
            ("partial", 3, False),
            ("missing", 1, False),
            ("hash", 1, False),
        ]:
            cache = directory / route
            digest = hashlib.sha256(b"wrong" if route == "hash" else PAYLOAD).hexdigest()
            manifest = directory / "pins.conf"
            manifest.write_text(f"FIXTURE_URL=http://127.0.0.1:{server.server_port}/{route}\nFIXTURE_SHA256={digest}\n")
            script = directory / "fetch.cmake"
            script.write_text(
                f'set(DXX_DEPENDENCY_MANIFEST "{manifest.as_posix()}")\n'
                f'set(DXX_DEPENDENCY_CACHE_DIR "{cache.as_posix()}")\n'
                f'include("{ROOT.as_posix()}/cmake/dxx-verified-dependencies.cmake")\n'
                "dxx_verified_cached_file(FIXTURE source result)\n"
            )

            def run():
                return subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True, timeout=30)

            result = run()
            assert (result.returncode == 0) == success, result.stdout + result.stderr
            assert COUNTS[f"/{route}"] == attempts, COUNTS
            assert not list(cache.glob("*.tmp")), list(cache.iterdir())
            entries = list(cache.glob("*.source"))
            assert bool(entries) == success
            if success:
                assert entries[0].read_bytes() == PAYLOAD
                assert run().returncode == 0
                assert COUNTS[f"/{route}"] == attempts
                entries[0].write_bytes(b"corrupt cache")
                assert run().returncode != 0
                assert COUNTS[f"/{route}"] == attempts
            print(f"PASS: {route}, {attempts} requests, cache publication and cleanup")
        for route, success in [("busy", True), ("shell-missing", False)]:
            target = directory / "shell-download"
            result = subprocess.run(
                [
                    "bash",
                    "-c",
                    'source "$1"; download_file "$2" "$3"',
                    "fixture",
                    str(ROOT / "android/get_deps/helpers/platform.sh"),
                    str(target),
                    f"http://127.0.0.1:{server.server_port}/{route}",
                ],
                capture_output=True,
                text=True,
                timeout=30,
            )
            assert (result.returncode == 0) == success, result.stderr
            assert COUNTS[f"/{route}"] == (2 if success else 1)
            if success:
                assert target.read_bytes() == PAYLOAD
            print(f"PASS: bootstrap {route}, bounded retry and error propagation")
    finally:
        server.shutdown()
        server.server_close()

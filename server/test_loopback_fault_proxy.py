"""Bounded owned-socket controls for the emulator fault bridge."""

import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest


class ProbeFilterTest(unittest.TestCase):
    def test_owned_filter_scope_rollback_and_input_guards(self):
        script = Path(__file__).parents[1] / "android/helpers/online_probe_fault.sh"
        with tempfile.TemporaryDirectory() as root:
            root = Path(root)
            log = root / "calls"
            command = root / "iptables"
            command.write_text(
                '#!/bin/sh\nprintf \'%s\\n\' "$*" >> "$TEST_CALLS"\n'
                'if [ -n "$TEST_FAIL" ] && [ "$1" = -A ]; then exit 1; fi\n'
            )
            command.chmod(0o755)
            env = dict(
                os.environ,
                PATH=str(root) + os.pathsep + os.environ["PATH"],
                TEST_CALLS=str(log),
                TEST_FAIL="",
            )

            def run(operation, uid="10123", chain="DXX_T_1234abcd"):
                return subprocess.run(
                    ["sh", str(script), operation, uid, chain],
                    env=env,
                    capture_output=True,
                    timeout=5,
                )

            self.assertEqual(run("install").returncode, 0)
            calls = log.read_text().splitlines()
            self.assertEqual(
                calls,
                [
                    "-N DXX_T_1234abcd",
                    "-A DXX_T_1234abcd -d 127.0.0.0/8 -j RETURN",
                    "-A DXX_T_1234abcd -d 10.0.2.2 -p udp -m multiport --dports 3478,3479,9001 -j RETURN",
                    "-A DXX_T_1234abcd -j DROP",
                    "-I OUTPUT -p udp -m owner --uid-owner 10123 -j DXX_T_1234abcd",
                ],
            )
            for uid, chain in [
                ("0", "DXX_T_1234abcd"),
                ("x", "DXX_T_1234abcd"),
                ("10123", "OUTPUT"),
            ]:
                self.assertNotEqual(run("install", uid, chain).returncode, 0)
            self.assertEqual(log.read_text().splitlines(), calls)
            env["TEST_FAIL"] = "1"
            self.assertNotEqual(run("install").returncode, 0)
            self.assertEqual(
                log.read_text().splitlines()[-3:],
                [
                    "-D OUTPUT -p udp -m owner --uid-owner 10123 -j DXX_T_1234abcd",
                    "-F DXX_T_1234abcd",
                    "-X DXX_T_1234abcd",
                ],
            )


class BridgeTest(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.TemporaryDirectory()
        self.addCleanup(self.root.cleanup)
        self.control = Path(self.root.name) / "control.json"
        self.mode("pass", "pass")
        self.stop = threading.Event()
        self.sources = set()
        self.sockets = []
        self.threads = []
        self.addCleanup(self.close_backends)
        self.tcp = socket.socket()
        self.tcp.bind(("127.0.0.1", 0))
        self.tcp.listen()
        self.tcp.settimeout(0.1)
        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp.bind(("127.0.0.1", 0))
        self.udp.settimeout(0.1)
        self.sockets.extend([self.tcp, self.udp])

        def tcp_echo():
            while not self.stop.is_set():
                try:
                    stream, _ = self.tcp.accept()
                except socket.timeout:
                    continue
                except OSError:
                    break
                with stream:
                    stream.settimeout(0.1)
                    while not self.stop.is_set():
                        try:
                            data = stream.recv(4096)
                            if not data:
                                break
                            stream.sendall(data)
                        except socket.timeout:
                            continue
                        except OSError:
                            break

        def udp_echo():
            while not self.stop.is_set():
                try:
                    data, address = self.udp.recvfrom(4096)
                    self.sources.add(address)
                    self.udp.sendto(data, address)
                except socket.timeout:
                    continue
                except OSError:
                    break

        for target in (tcp_echo, udp_echo):
            thread = threading.Thread(target=target, daemon=True)
            thread.start()
            self.threads.append(thread)
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            self.ws_port = reservation.getsockname()[1]
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
            reservation.bind(("127.0.0.1", 0))
            self.udp_port = reservation.getsockname()[1]
        self.process = subprocess.Popen(
            [
                sys.executable,
                str(Path(__file__).with_name("loopback_fault_proxy.py")),
                "--control",
                str(self.control),
                "--ws-port",
                str(self.ws_port),
                "--ws-upstream",
                str(self.tcp.getsockname()[1]),
                "--udp-port",
                str(self.udp_port),
                "--udp-upstream",
                str(self.udp.getsockname()[1]),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        self.addCleanup(self.close_bridge)
        deadline = time.monotonic() + 3
        while True:
            self.assertIsNone(self.process.poll())
            try:
                self.client = socket.create_connection(("127.0.0.1", self.ws_port), timeout=0.5)
                break
            except OSError:
                self.assertLess(time.monotonic(), deadline)
                time.sleep(0.02)
        self.addCleanup(self.client.close)
        self.datagrams = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.datagrams.connect(("127.0.0.1", self.udp_port))
        self.datagrams.settimeout(0.3)
        self.addCleanup(self.datagrams.close)

    def mode(self, ws, udp):
        temporary = self.control.with_suffix(".tmp")
        temporary.write_text(json.dumps({"ws": ws, "udp": udp}))
        temporary.replace(self.control)
        time.sleep(0.06)

    def close_bridge(self):
        self.process.terminate()
        self.process.wait(timeout=3)

    def close_backends(self):
        self.stop.set()
        for stream in self.sockets:
            stream.close()
        for thread in self.threads:
            thread.join(timeout=1)
            self.assertFalse(thread.is_alive())

    def test_udp_only_loss_preserves_control_and_recovers(self):
        self.datagrams.send(b"before")
        self.assertEqual(b"before", self.datagrams.recv(4096))
        self.mode("pass", "drop")
        self.client.sendall(b"control alive")
        self.assertEqual(b"control alive", self.client.recv(4096))
        self.datagrams.send(b"dropped")
        with self.assertRaises(socket.timeout):
            self.datagrams.recv(4096)
        self.mode("pass", "pass")
        self.datagrams.send(b"after")
        self.assertEqual(b"after", self.datagrams.recv(4096))

    def test_rebinding_changes_only_upstream_udp_endpoint(self):
        self.datagrams.send(b"before")
        self.assertEqual(b"before", self.datagrams.recv(4096))
        self.assertEqual(1, len(self.sources))
        self.mode("pass", "rebind")
        self.datagrams.send(b"after")
        self.assertEqual(b"after", self.datagrams.recv(4096))
        self.assertEqual(2, len(self.sources))
        self.mode("pass", "readdress")
        self.datagrams.send(b"new address")
        self.assertEqual(b"new address", self.datagrams.recv(4096))
        self.assertEqual({"127.0.0.1", "127.0.0.2"}, {address[0] for address in self.sources})
        self.client.sendall(b"control alive")
        self.assertEqual(b"control alive", self.client.recv(4096))

    def test_control_only_loss_preserves_udp(self):
        self.mode("drop", "pass")
        self.assertEqual(b"", self.client.recv(4096))
        self.datagrams.send(b"udp alive")
        self.assertEqual(b"udp alive", self.datagrams.recv(4096))

    def test_impairment_really_drops_duplicates_and_reorders(self):
        self.mode("pass", "impair")
        for value in range(100):
            self.datagrams.send(value.to_bytes(2, "big"))
        received = []
        while True:
            try:
                received.append(int.from_bytes(self.datagrams.recv(4096), "big"))
            except socket.timeout:
                break
        self.assertGreater(len(received), 0)
        self.assertLess(len(set(received)), 100)
        self.assertGreater(len(received), len(set(received)))
        self.assertNotEqual(sorted(received), received)


if __name__ == "__main__":
    unittest.main()

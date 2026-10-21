#!/usr/bin/env python3
"""Rehearse supplied server artifacts on owned loopback ports and synthetic state."""

import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import shutil
import socket
import ssl
import subprocess
import tempfile
import time


def digest(binary):
    with binary.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def reserve_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def ready(port):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=0.5)
    try:
        connection.request("GET", "/api/v1/ready")
        response = connection.getresponse()
        assert response.status == 200, "Listener readiness failed"
        return json.loads(response.read())
    finally:
        connection.close()


def wait_ready(process, port):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        assert process.poll() is None, "Owned server exited before readiness"
        try:
            state = ready(port)
            assert state["ready"] is True
            assert all(state[key] == 0 for key in ("rooms", "relay_sessions", "pending_and_authenticated_sockets")), (
                "Resources did not return to baseline"
            )
            return state
        except (OSError, AssertionError):
            time.sleep(0.05)
    raise AssertionError("Readiness/baseline deadline exceeded")


def check_tls(port, cert):
    # A normal trust store must reject this disposable self-signed test identity
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=2) as raw:
            with ssl.create_default_context().wrap_socket(raw, server_hostname="localhost"):
                pass
    except ssl.SSLCertVerificationError:
        pass
    else:
        raise AssertionError("Untrusted synthetic certificate was accepted")
    context = ssl.create_default_context(cafile=str(cert))
    with socket.create_connection(("127.0.0.1", port), timeout=2) as raw:
        with context.wrap_socket(raw, server_hostname="localhost") as stream:
            stream.sendall(
                b"GET /ws HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                b"Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n"
            )
            response = b""
            while b"\r\n\r\n" not in response:
                chunk = stream.recv(4096)
                assert chunk, "TLS stream closed before WebSocket upgrade"
                response += chunk
                assert len(response) <= 16384, "Oversized upgrade response"
            assert response.startswith(b"HTTP/1.1 101 "), "WSS upgrade failed"


def exercise(binary, config, root, label):
    with subprocess.Popen(
        [str(binary)],
        cwd=root,
        env={"CONFIG_FILE": str(config)},
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    ) as process:
        try:
            settings = json.loads(config.read_text())
            http_port = int(settings["http_listen_addr"].rsplit(":", 1)[1])
            ws_port = int(settings["ws_listen_addr"].rsplit(":", 1)[1])
            baseline = wait_ready(process, http_port)
            check_tls(ws_port, root / "test-cert.pem")
            wait_ready(process, http_port)
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
    try:
        ready(http_port)
    except OSError:
        pass
    else:
        raise AssertionError("Owned HTTP endpoint remained available after stop")
    return {
        "phase": label,
        "sha256": digest(binary),
        "protocol_version": baseline["protocol_version"],
        "result": "pass",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--previous-binary", type=Path)
    args = parser.parse_args()
    if not __debug__:
        parser.error("Rehearsal assertions require Python without optimization")
    binary = args.binary.resolve(strict=True)
    previous = args.previous_binary.resolve(strict=True) if args.previous_binary else None
    openssl = shutil.which("openssl")
    assert openssl, "OpenSSL is required for the disposable localhost test certificate"
    with tempfile.TemporaryDirectory(prefix="dxx-preview-") as temporary:
        root = Path(temporary)
        subprocess.run(
            [
                openssl,
                "req",
                "-x509",
                "-newkey",
                "rsa:2048",
                "-nodes",
                "-days",
                "1",
                "-subj",
                "/CN=localhost",
                "-addext",
                "subjectAltName=DNS:localhost",
                "-keyout",
                str(root / "test-key.pem"),
                "-out",
                str(root / "test-cert.pem"),
            ],
            check=True,
            timeout=15,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        os.chmod(root / "test-key.pem", 0o600)
        settings = {
            name: "127.0.0.1:0"
            for name in ("relay_listen_addr", "stun_listen_addr", "stun_listen_addr_alt", "admin_http_listen_addr")
        }
        settings.update(
            ws_listen_addr=f"127.0.0.1:{reserve_port()}",
            http_listen_addr=f"127.0.0.1:{reserve_port()}",
            stun_public_addrs="127.0.0.1:3478,127.0.0.1:3479",
            db_path=str(root / "synthetic.db"),
            log_dir="",
            skip_gpgs_verify=False,
            tls_cert_path=str(root / "test-cert.pem"),
            tls_key_path=str(root / "test-key.pem"),
        )
        config = root / "config.json"
        config.write_text(json.dumps(settings))
        phases = []
        if previous:
            assert digest(previous) != digest(binary), "Rollback requires distinct artifacts"
            phases.append(exercise(previous, config, root, "previous baseline"))
        phases.append(exercise(binary, config, root, "candidate"))
        phases.append(exercise(previous or binary, config, root, "rollback" if previous else "restart"))
        assert len({phase["protocol_version"] for phase in phases}) == 1, "Matched protocol required"
        print(
            json.dumps(
                {
                    "scope": "owned loopback TLS/process only; no gameplay or deployment proof",
                    "rollback_exercised": previous is not None,
                    "phases": phases,
                },
                indent=2,
            )
        )


if __name__ == "__main__":
    main()

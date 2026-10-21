#!/usr/bin/env python3
"""Owned emulator test bridge: loopback only, no payload logging or system network changes."""

import argparse
import heapq
import json
from pathlib import Path
import selectors
import socket
import time


def run(control, ws_port, ws_upstream, udp_port, udp_upstream):
    selector = selectors.DefaultSelector()
    tcp = socket.socket()
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sockets = {tcp, udp}
    pairs = {}
    mappings = {}
    pending = []
    serial = 0
    count = 0
    mode = {"ws": "pass", "udp": "pass"}
    modified = None

    def close(stream):
        peer = pairs.pop(stream, None)
        if peer is not None:
            pairs.pop(peer, None)
        for item in (stream, peer):
            if item is None:
                continue
            try:
                selector.unregister(item)
            except (KeyError, ValueError):
                pass
            sockets.discard(item)
            item.close()

    try:
        tcp.bind(("127.0.0.1", ws_port))
        tcp.listen(32)
        udp.bind(("127.0.0.1", udp_port))
        tcp.setblocking(False)
        udp.setblocking(False)
        selector.register(tcp, selectors.EVENT_READ, "accept")
        selector.register(udp, selectors.EVENT_READ, "udp_front")
        print("LOOPBACK_FAULT_BRIDGE ready", flush=True)
        while True:
            stamp = control.stat().st_mtime_ns
            if stamp != modified:
                requested = json.loads(control.read_text())
                assert requested.get("ws") in ("pass", "drop")
                assert requested.get("udp") in ("pass", "drop", "impair", "rebind")
                mode = requested
                modified = stamp
                if mode["ws"] == "drop":
                    for stream in list(pairs):
                        if stream in pairs:
                            close(stream)
                if mode["udp"] in ("drop", "rebind"):
                    pending.clear()
                if mode["udp"] == "rebind":
                    for client, old in list(mappings.items()):
                        upstream = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                        upstream.bind(("127.0.0.1", 0))
                        upstream.connect(("127.0.0.1", udp_upstream))
                        upstream.setblocking(False)
                        sockets.add(upstream)
                        selector.register(upstream, selectors.EVENT_READ, client)
                        mappings[client] = upstream
                        close(old)
                print(f"LOOPBACK_FAULT_BRIDGE ws={mode['ws']} udp={mode['udp']}", flush=True)
            now = time.monotonic()
            while pending and pending[0][0] <= now:
                _, _, stream, data, address = heapq.heappop(pending)
                if stream in sockets:
                    try:
                        if address is None:
                            stream.send(data)
                        else:
                            stream.sendto(data, address)
                    except OSError:
                        pass
            for key, _ in selector.select(0.02):
                stream = key.fileobj
                kind = key.data
                if kind == "accept":
                    downstream, _ = tcp.accept()
                    if mode["ws"] == "drop" or len(pairs) >= 64:
                        downstream.close()
                        continue
                    try:
                        upstream = socket.create_connection(("127.0.0.1", ws_upstream), timeout=0.5)
                    except OSError:
                        downstream.close()
                        continue
                    for endpoint, peer in ((downstream, upstream), (upstream, downstream)):
                        endpoint.settimeout(0.5)
                        sockets.add(endpoint)
                        pairs[endpoint] = peer
                        selector.register(endpoint, selectors.EVENT_READ, "tcp")
                elif kind == "tcp":
                    try:
                        data = stream.recv(65536)
                        if not data:
                            close(stream)
                        else:
                            pairs[stream].sendall(data)
                    except (OSError, KeyError):
                        close(stream)
                else:
                    try:
                        if kind == "udp_front":
                            data, client = udp.recvfrom(65536)
                            if client not in mappings:
                                if len(mappings) >= 64:
                                    continue
                                upstream = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                                upstream.bind(("127.0.0.1", 0))
                                upstream.connect(("127.0.0.1", udp_upstream))
                                upstream.setblocking(False)
                                mappings[client] = upstream
                                sockets.add(upstream)
                                selector.register(upstream, selectors.EVENT_READ, client)
                            destination, address = mappings[client], None
                        else:
                            data = stream.recv(65536)
                            destination, address = udp, kind
                    except OSError:
                        continue
                    count += 1
                    if mode["udp"] == "drop" or (mode["udp"] == "impair" and count % 11 == 0):
                        continue
                    copies = 2 if mode["udp"] == "impair" and count % 7 == 0 else 1
                    for copy in range(copies):
                        if len(pending) >= 1024:
                            break
                        serial += 1
                        delay = ((count % 4) * 0.02 + copy * 0.01) if mode["udp"] == "impair" else 0
                        heapq.heappush(pending, (time.monotonic() + delay, serial, destination, data, address))
    finally:
        selector.close()
        for stream in sockets:
            stream.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--control", required=True, type=Path)
    parser.add_argument("--ws-port", type=int, default=9000)
    parser.add_argument("--ws-upstream", type=int, default=9002)
    parser.add_argument("--udp-port", type=int, default=9001)
    parser.add_argument("--udp-upstream", type=int, default=9003)
    args = parser.parse_args()
    if not __debug__:
        parser.error("Test bridge requires assertions enabled")
    run(args.control, args.ws_port, args.ws_upstream, args.udp_port, args.udp_upstream)


if __name__ == "__main__":
    main()

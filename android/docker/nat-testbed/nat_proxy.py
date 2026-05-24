#!/usr/bin/env python3
"""Transparent UDP NAT proxy for STUN traffic testing.

Listens on two UDP ports (3478, 3479 -- standard STUN ports) and forwards
traffic to an upstream STUN server on the host, applying NAT-like port
mapping behavior so the server sees different source ports depending on
the configured NAT type.

Environment variables:
  NAT_TYPE       -- full-cone (default), port-restricted, symmetric, symmetric-seq
  UPSTREAM_HOST  -- IP/hostname of the STUN server (default: host.docker.internal)
  UPSTREAM_PORT1 -- upstream STUN port 1 (default: 3478)
  UPSTREAM_PORT2 -- upstream STUN port 2 (default: 3479)
  LISTEN_PORT1   -- local listen port 1 (default: 3478)
  LISTEN_PORT2   -- local listen port 2 (default: 3479)
  SEQ_BASE_PORT  -- base port for symmetric-seq allocations (default: 50000)
"""

import os
import socket
import select
import sys
import time

NAT_TYPE = os.environ.get("NAT_TYPE", "full-cone")
UPSTREAM_HOST = os.environ.get("UPSTREAM_HOST", "host.docker.internal")
UPSTREAM_PORT1 = int(os.environ.get("UPSTREAM_PORT1", "3478"))
UPSTREAM_PORT2 = int(os.environ.get("UPSTREAM_PORT2", "3479"))
LISTEN_PORT1 = int(os.environ.get("LISTEN_PORT1", "3478"))
LISTEN_PORT2 = int(os.environ.get("LISTEN_PORT2", "3479"))
SEQ_BASE_PORT = int(os.environ.get("SEQ_BASE_PORT", "50000"))

# Mapping tables
# key -> (ext_socket, allowed_remotes set)
# key format depends on NAT type:
#   full-cone / port-restricted: (client_addr,)
#   symmetric / symmetric-seq:   (client_addr, upstream_dest)
mappings = {}
# ext_socket -> (listen_socket, client_addr)   -- reverse lookup for responses
reverse_map = {}
next_seq_port = SEQ_BASE_PORT


def mapping_key(client_addr, upstream_dest):
    if NAT_TYPE in ("full-cone", "port-restricted"):
        return (client_addr,)
    return (client_addr, upstream_dest)


def get_or_create_mapping(client_addr, upstream_dest):
    """Get or create an external socket for this mapping."""
    global next_seq_port
    key = mapping_key(client_addr, upstream_dest)
    if key in mappings:
        ext_sock, allowed = mappings[key]
        allowed.add(upstream_dest)
        return ext_sock
    # Create new external socket
    ext_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    if NAT_TYPE == "symmetric-seq":
        for _ in range(100):
            try:
                ext_sock.bind(("0.0.0.0", next_seq_port))
                next_seq_port += 1
                break
            except OSError:
                next_seq_port += 1
        else:
            ext_sock.bind(("0.0.0.0", 0))
    else:
        ext_sock.bind(("0.0.0.0", 0))
    ext_sock.setblocking(False)
    allowed = {upstream_dest}
    mappings[key] = (ext_sock, allowed)
    ext_port = ext_sock.getsockname()[1]
    print(f"NAT [{NAT_TYPE}] new mapping: {client_addr} -> :{ext_port} (dest {upstream_dest})")
    return ext_sock


def is_inbound_allowed(ext_sock, sender_addr):
    """Check if inbound packet from sender is allowed through the NAT."""
    if NAT_TYPE == "full-cone":
        return True
    # For port-restricted, symmetric, symmetric-seq: sender must be in allowed set
    for key, (sock, allowed) in mappings.items():
        if sock is ext_sock:
            return sender_addr in allowed
    return False


def find_client_for_ext(ext_sock):
    """Find the client address and listen socket for a given external socket."""
    return reverse_map.get(id(ext_sock))


def main():
    print(f"NAT proxy starting: type={NAT_TYPE} upstream={UPSTREAM_HOST}:{UPSTREAM_PORT1}/{UPSTREAM_PORT2}")
    print(f"Listening on :{LISTEN_PORT1} and :{LISTEN_PORT2}")

    listen1 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listen1.bind(("0.0.0.0", LISTEN_PORT1))
    listen1.setblocking(False)

    listen2 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listen2.bind(("0.0.0.0", LISTEN_PORT2))
    listen2.setblocking(False)

    # Map listen port -> upstream port
    upstream_for_listen = {
        id(listen1): (UPSTREAM_HOST, UPSTREAM_PORT1),
        id(listen2): (UPSTREAM_HOST, UPSTREAM_PORT2),
    }
    listen_by_id = {id(listen1): listen1, id(listen2): listen2}

    pkt_count = 0
    while True:
        # Build list of all sockets to poll
        readable = [listen1, listen2]
        for key, (ext_sock, _) in list(mappings.items()):
            readable.append(ext_sock)

        try:
            ready, _, _ = select.select(readable, [], [], 5.0)
        except (ValueError, OSError):
            # Socket was closed, rebuild
            continue

        for sock in ready:
            try:
                data, addr = sock.recvfrom(4096)
            except (BlockingIOError, OSError):
                continue

            if id(sock) in upstream_for_listen:
                # Inbound from client -> forward to upstream through NAT
                listen_sock = sock
                upstream_dest = upstream_for_listen[id(listen_sock)]
                ext_sock = get_or_create_mapping(addr, upstream_dest)
                # Store reverse mapping
                reverse_map[id(ext_sock)] = (listen_sock, addr)
                try:
                    ext_sock.sendto(data, upstream_dest)
                except OSError as e:
                    print(f"  send error to {upstream_dest}: {e}")
                pkt_count += 1
                if pkt_count <= 10 or pkt_count % 50 == 0:
                    ext_port = ext_sock.getsockname()[1]
                    print(f"  [{pkt_count}] client {addr} -> :{ext_port} -> {upstream_dest} ({len(data)}B)")
            else:
                # Inbound from upstream -> forward back to client through NAT
                if not is_inbound_allowed(sock, addr):
                    print(f"  DROPPED inbound from {addr} (NAT filter)")
                    continue
                info = find_client_for_ext(sock)
                if info:
                    listen_sock, client_addr = info
                    try:
                        listen_sock.sendto(data, client_addr)
                    except OSError as e:
                        print(f"  send error to client {client_addr}: {e}")
                    pkt_count += 1
                    if pkt_count <= 10 or pkt_count % 50 == 0:
                        print(f"  [{pkt_count}] upstream {addr} -> client {client_addr} ({len(data)}B)")


if __name__ == "__main__":
    main()

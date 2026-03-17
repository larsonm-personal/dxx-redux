"""
UDP relay to bridge two Android emulators for multiplayer game testing.

Each emulator has its own NAT. This relay listens on a single port and
forwards packets between the two emulators.

Setup:
  EMU1 redir: host:42500 -> EMU1:42424  (host game, inbound only)
  EMU2: no redir needed (redir blocks outbound from game port 42424)

Joiner (EMU2) connects to 10.0.2.2:42600 (this relay).
Relay forwards to EMU1 via redir (127.0.0.1:42500).
EMU1 replies come back through the redir session to this relay.
Relay forwards replies to EMU2 via its learned NAT address.
"""
import socket
import select
import sys

RELAY_PORT = 42600
EMU1_REDIR = ("127.0.0.1", 42500)  # host game (via redir, inbound only)

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", RELAY_PORT))
    sock.setblocking(False)

    print(f"UDP relay listening on :{RELAY_PORT}")
    print(f"  EMU1 (host)  redir: {EMU1_REDIR}")
    print(f"  EMU2 (join)  via NAT (no redir)")

    # Track source addresses.
    # EMU2 (joiner) sends first. We remember its NAT address to send replies.
    # EMU1 (host) replies come through the redir session.
    emu2_nat_addr = None  # learned from first packet (joiner via SLIRP NAT)
    emu1_redir_addr = None  # learned from first reply (host via redir session)
    pkt_count = 0

    while True:
        readable, _, _ = select.select([sock], [], [], 1.0)
        if not readable:
            continue

        data, addr = sock.recvfrom(65535)
        pkt_count += 1

        # First packet from a new source: joiner sends game_info request
        if emu2_nat_addr is None:
            emu2_nat_addr = addr
            print(f"  [{pkt_count}] EMU2 joiner identified: {addr}")

        if addr == emu2_nat_addr:
            # From joiner -> forward to host via redir
            sock.sendto(data, EMU1_REDIR)
            if pkt_count <= 5 or pkt_count % 100 == 0:
                print(f"  [{pkt_count}] EMU2->EMU1: {len(data)} bytes")
        else:
            # From host (via redir session) -> forward to joiner via NAT
            if emu1_redir_addr is None:
                emu1_redir_addr = addr
                print(f"  [{pkt_count}] EMU1 host identified: {addr}")
            if emu2_nat_addr:
                sock.sendto(data, emu2_nat_addr)
            if pkt_count <= 5 or pkt_count % 100 == 0:
                print(f"  [{pkt_count}] EMU1->EMU2: {len(data)} bytes")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nRelay stopped.")
        sys.exit(0)

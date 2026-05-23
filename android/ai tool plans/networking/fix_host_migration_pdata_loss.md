# Fix: Host Migration PDATA Loss After 2+ Swaps

## Problem
After 2-3 host swap cycles in LAN coop, the rejoining player receives object
sync and UPID_SYNC successfully but then receives ZERO packets (PDATA, PING,
anything) from the host for 15 seconds, causing timeout. Both players end up
assuming host roles.

## Evidence from logs (Player68, build 11170)
- Round 1 (join Player32): works fine
- Round 1 host migration (Player68 becomes host, Player32 rejoins): works fine
- Round 2 (Player68 joins Player32): object sync OK (102 objects) then 15s silence then timeout
- Round 3: same pattern, 99 objects synced then 15s silence then timeout

## Key findings
1. The proxy works for object sync + SYNC (many packets flow successfully)
2. After SYNC, absolutely no packets arrive for 15 seconds (not PDATA, not PING)
3. The issue is NOT token mismatch (valid_token would log drops via drop_rx_packet)
4. The issue is NOT valid_sender role confusion (UPID_PDATA is not in the switch)
5. The non-RetroProtocol PDATA address check silently drops packets on memcmp fail

## Analysis of process_pdata address check (prime suspect)
```c
// For non-master client:
if (memcmp(&sender_addr, &Netgame.players[0].protocol.udp.addr, sizeof(struct _sockaddr)))
    return;  // SILENT drop!
```
- Netgame.players[0].protocol.udp.addr is set from SYNC sender (recvfrom)
- PDATA sender_addr is also from recvfrom
- Both should come from proxy 127.0.0.1:42430
- BUT: the full memcmp on struct _sockaddr includes all padding bytes

## Alternative hypothesis: proxy stops after SYNC
- The Kotlin proxy's forwardRealToLocal coroutine might crash, exit, or stop
  receiving packets after the SYNC is forwarded
- No timeout/inactivity logic in the proxy, but an exception could kill it

## Alternative hypothesis: host not sending
- Player32 (host in round 2) might not be calling net_udp_send_pdata at all
- Some connected status or Game_mode issue on the host side
- We only have Player68's logs so can't verify this

## Phase 1: Diagnostic logging [DONE]
Add MPDIAG logging to both d1/ and d2/:
1. process_pdata: log all three silent drop reasons (Network_status, size, address)
2. send_pdata: log first few sends with target address to confirm sending
3. read_sync_packet: log stored master address after SYNC
4. main listen loop: periodic heartbeat log showing packet receipt status

## Phase 2: Root cause fix [DONE]
Replaced full struct _sockaddr memcmp with sockaddr_equal() (IP+port only) on
Android in the PDATA sender validation and process_dump. After socket rebind
cycles during host migration, getaddrinfo vs recvfrom can produce struct
_sockaddr values with different sin_zero padding, causing silent memcmp failures.

Changes:
- d2/main/net_udp.c: sockaddr_equal() function + forward decl, used in
  process_pdata and process_dump
- d1/main/net_udp.c: same sockaddr_equal() function + forward decl, used in
  process_pdata

Other memcmp locations (ping/pong validation) are secondary and can be fixed
if diagnostics show they're also failing.

## Phase 3: Build and verify [DONE]
Build successful (49 tasks). Ready for two-device testing.

## Phase 4: Test and iterate
Deploy to both devices, reproduce 2-3+ host swap cycles, collect MPDIAG logs
from BOTH devices. If still failing, the new diagnostic logs will show the
exact failure point.

# Fix: PDATA loss after host migration rejoin

## Problem
After a host migration cycle (host->client->re-host), the rejoining client receives
zero PDATA from the host while MDATA (93 packets) works. After 15s with no PDATA,
the client triggers another timeout/migration, cascading into a desync.

## Root cause analysis
The LocalhostProxy (Java) is created once during `SetupActivity.launchMP()` and never
recreated during in-game host migration. When a player does `rebind_for_hosting` (becoming
host), the game socket moves from loopback to INADDR_ANY, but the proxy keeps running with
stale state. When the SAME player later needs to rejoin as a client (auto_join), the old
proxy is reused. After multiple migration cycles, the proxy's loopback<->LAN relay state
degrades, causing selective packet loss (specifically PDATA drops while MDATA survives
through retransmission/retry paths).

Evidence from debug logs (build 11181, 2026-04-09):
- Player68 Phase 5 sync: `read_sync: PLAYING Player_num=1 master=0 N_players=2`
- Player68 listen: `rx_total=125 pdata=0 mdata=93` -- zero PDATA for 15 seconds
- Player32 (host): `process_pdata: RX #300 from P1` -- host receives client's PDATA fine
- Player32: `send_pdata: RETRO TX #900` -- host IS sending PDATA
- C code paths for PDATA and MDATA are identical (same socket, same address, same validation)
- The only non-C difference is the Java proxy forwarding layer

CROSS-PROCESS NOTE: MainActivity runs in `:game` process, SetupActivity + MatchmakingService
+ LocalhostProxy run in the default process. Proxy lifecycle MUST be managed from the default
process side, using broadcasts or MatchmakingService directly.

## Completed work (Session 1)

### Phase A: Proxy shutdown on host migration [DONE]
When the HOST_MIGRATION broadcast arrives in SetupActivity, the old proxy must be shut down
BEFORE the game restarts. This ensures port 42430 is released and a fresh proxy can bind.

- Added `MatchmakingService.shutdownLanProxy()` static method
- Called `shutdownLanProxy()` in `hostMigrationReceiver.onReceive()` before LAN broadcast
- This ensures the proxy is torn down when the game transitions through host migration

### Phase B: Diagnostic logging in C [DONE]
- `net_udp_send_pdata()`: Enhanced RETRO TX log to include destination address and conntype
  per connected player (d1 and d2)
- `net_udp_listen()`: Added WARNING when pdata=0 but mdata>0 while NETSTAT_PLAYING as
  client, logging host address and bind_loopback state (d1 and d2)

### Phase C: UPID-aware proxy logging [DONE]
- `LocalhostProxy.kt` PeerProxy: Added pdataForwarded/mdataForwarded volatile counters
- `forwardRealToLocal()`: UPID tracking (0x22=PDATA, 0x25/0x26=MDATA), periodic log
- `deliverIncoming()`: Same UPID tracking for shared-socket mode
- Both include pdata/mdata counts in periodic log messages

### Phase D: Build and lint [DONE]
- assembleDebug BUILD SUCCESSFUL (both d1 and d2 compile)
- ktlint passes on all modified Kotlin files
- clang-format: only pre-existing warnings in upstream code

## Remaining work (future sessions)

### Phase E: JNI bridge for proxy lifecycle
The current fix (Phase A) shuts down the proxy when the HOST_MIGRATION broadcast fires.
However, this is a one-shot mechanism -- the proxy isn't recreated until the game fully
restarts through SetupActivity. If a more dynamic proxy lifecycle is needed (e.g. for
in-game rejoin without full restart), JNI methods would be needed:

1. `jni_main.c`: Add `android_recreate_lan_proxy(host_addr, host_port)` and
   `android_shutdown_lan_proxy()` JNI callbacks
2. Call from `rebind_for_hosting()` and `net_udp_auto_join()` respectively

### Phase F: Integration test
- Test the full cycle: Player1 hosts -> Player2 joins -> Player1 leaves ->
  Player2 becomes host -> Player1 rejoins -> Player2 leaves -> Player1 becomes host
- Verify pdata>0 in listen logs after each transition
- Use the existing introspection API and run_test.ps1 infrastructure

## Key files modified
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt` -- shutdownLanProxy()
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` -- hostMigrationReceiver proxy shutdown
- `android/app/src/main/java/com/dxxredux/app/multiplayer/LocalhostProxy.kt` -- UPID counters/logging
- `d2/main/net_udp.c` -- send_pdata dest logging, listen pdata=0 warning
- `d1/main/net_udp.c` -- mirror of d2 diagnostic logging

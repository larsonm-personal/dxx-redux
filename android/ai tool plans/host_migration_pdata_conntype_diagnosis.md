# Host Migration PDATA Loss - CONNT_PROXY Root Cause

## Status: FIXED -- root cause found and patched, build successful

## Key Findings (build 11190 logs)

### The Symptom
After the second host migration (Phase 3), the CLIENT (Player68) receives:
- pdata=0 (ZERO position updates from host)
- mdata=89 (game events from host DO arrive)

This leads to a 15s timeout and cascading migration failure.

### Root Cause Chain
1. The HOST (Player32) sends pdata with `conntype=2` (CONNT_PROXY) instead of `conntype=1` (CONNT_DIRECT)
2. `net_udp_send_to_player()` at line 8212 routes CONNT_PROXY through `net_udp_send_to_player_proxy()`
3. `net_udp_send_to_player_proxy()` at line 8232 has: `if(connection_statuses[through_player].type != CONNT_DIRECT) { return; }`
4. In a 2-player game, `proxy_through=0` (self), which has no CONNT_DIRECT status -- **packets are silently dropped**

### Why mdata Still Arrives
- For RetroProtocol + needack=1: mdata uses `dxx_sendto()` directly (line ~7125), bypassing `net_udp_send_to_player()` routing
- For RetroProtocol + needack=0: mdata uses `net_udp_send_to_player()` (line 7111) -- these packets would ALSO be dropped
- Many critical game events are needack=1, so they bypass the broken routing

### Mystery: How Does conntype Become CONNT_PROXY?
**SOLVED**: When a player RECONNECTS (not a new join), `net_udp_welcome_player()` finds the
existing player slot via `find_player_by_identity()` and enters the "Player is reconnecting"
branch (line ~2235). This branch sets `Network_player_added = 0`.

Later, `net_udp_send_rejoin_sync()` checks `if (Network_player_added)` before calling
`net_udp_new_player()`. Since `Network_player_added == 0`, `net_udp_new_player()` is NEVER
called. That function is the ONLY place where `connection_statuses[pnum].type = CONNT_DIRECT`
gets set during the welcome flow.

The connection_statuses for the returning player retains whatever value it had before --
which is `CONNT_PROXY` from the Phase 2 timeout handler at line 6500 (set 30+ seconds earlier).

### Fix Applied
In `net_udp_send_rejoin_sync()`, added `connection_statuses[player_num].type = CONNT_DIRECT`
in the non-observer branch when `multi_i_am_master()` is true. This ensures reconnecting
players always get CONNT_DIRECT on the master, regardless of whether `net_udp_new_player()`
is called.

Files modified:
- `d2/main/net_udp.c` - fix + conntype transition logging at all 9 mutation sites
- `d1/main/net_udp.c` - same changes mirrored

## Timeline From Logs
```
07:09:33  Phase 1 starts (Player32=host, Player68=client)
07:09:56  Phase 2 migration (Player68 becomes host)  
07:10:05  Phase 2 gameplay: pdata=62 mdata=9 -- WORKING
07:10:13  Phase 2 ends (Player68 leaves to find Phase 3)
07:10:28  Player32 times out Player68 (15s), sets CONNT_PROXY
07:10:43  Player32 times out Player68 (30s), disconnects, rebind_for_hosting
07:10:51  Player68 auto_joins Phase 3 as CLIENT
07:10:52  Object sync, welcome flow (CONNT_DIRECT set)
07:10:53  First logged pdata #600: conntype=2 (CONNT_PROXY) *** BUG ***
07:10:53  Player68 listen: pdata=0 mdata=89 -- BROKEN
07:11:07  Player68 times out Player32 (15s), cascading failure
```

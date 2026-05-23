# Rejoin Sync Fixes - Build 1115

## Status: IMPLEMENTED, BUILT, NEEDS TESTING

## Problems Fixed

### 1. SIGABRT crash in multi_reset_player_object (ASSERT failure) -- DIAGNOSTICS ADDED

**Symptom**: Fatal assert `(objp->type == OBJ_PLAYER) || (objp->type == OBJ_GHOST)` at
multi.c in `multi_reset_player_object`. Crash seen in build 1112 logs. Breadcrumbs
showed `nm_draw_bg1: (null)` (null-background menu, consistent with the "waiting for
sync" dialog used during client-side level sync).

**Root cause analysis**: Investigated all 7 call sites. Every site except `multi_prep_level`
explicitly sets the type before calling. `multi_prep_level` relies on
`gameseq_init_network_players` having set types to OBJ_PLAYER, but on the CLIENT side,
`init_objects()` wipes the Objects array during object sync AFTER
`gameseq_init_network_players` sets `Players[k].objnum`. The objnum values become stale
if the host's objects don't land at exactly the same indices as the level file had.
Could not definitively confirm this is the cause without the original crash packet data.

**Fix**: Assert kept intact (game ran 20 years with it). Added pre-assert diagnostic
logging that fires ONLY when the type is wrong:
- `obj_type_name()` helper uses case statement to print human-readable type names
- `multi_reset_player_object`: logs obj index, type name, segment, id before the assert
- `multi_prep_level`: logs player index, objnum, type name for any player whose object
  has an unexpected type; also catches out-of-range objnum values

**Files**: d2/main/multi.c, d1/main/multi.c

### 2. Reconnecting player gets wrong slot (new ephemeral port breaks matching)

**Symptom**: Player17 was originally slot 1 but gets slot 2 on rejoin. Host treats them
as a new player instead of a returning one.

**Root cause**: Player reconnection matching uses `memcmp` on the full `struct _sockaddr`
(which includes both IP address and port). When a client disconnects and reconnects,
it creates a new UDP socket with a different ephemeral port. The full sockaddr comparison
fails, so the player is treated as new and assigned a fresh slot.

**Fix**:
- Added `sockaddr_ip_equal()` helper function that compares only the IP address portion
  of sockaddrs, ignoring the port. Handles both IPv4 (sin_addr) and IPv6 (sin6_addr).
- Replaced all 4 reconnection matching locations with IP-only comparison:
  - `net_udp_welcome_player`: 1 location (player lookup loop)
  - `net_udp_do_refuse_stuff`: 3 locations (existing player checks)
- Added `update_address_for_player()` call in the reconnection path of
  `net_udp_welcome_player` so the stored address is updated to the client's new port.
  Without this, subsequent game traffic would be sent to the old (dead) port.

**Files**: d2/main/net_udp.c, d1/main/net_udp.c

## Previous Fix Confirmed Working (Build 1114)

The WiFi MTU truncation fix from build 1114 is confirmed working in the logs:
- Client correctly detects `TRUNCATED at body 6/8` on every packet
- Clean `verify_objects: FAIL packet_loss` dialog shown (no crash)
- 15 of 110 objects lost (every 7th, consistent with 1500-byte MTU truncation)

## Testing Plan

1. Deploy build 1115 to both devices
2. Host coop game on device A, join from device B
3. Play briefly, then force-disconnect device B (airplane mode or kill app)
4. Rejoin from device B -- verify:
   - Player gets their original slot (not a new one)
   - No crash on either device
   - Game state syncs correctly
5. Check logcat for any "BAD TYPE" diagnostic messages

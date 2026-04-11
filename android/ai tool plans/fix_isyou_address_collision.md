# Fix: isyou address collision after host migration

## Status: COMPLETE

## Problem
After double host migration (A hosts -> B hosts -> A hosts again), the
rejoining client sends pdata with `conntype=0` (CONNT_NONE) because
`connection_statuses[host_slot]` is never set.

## Root cause
`isyou` in SYNC packets is determined by address comparison:
`memcmp(&sender_addr, &Netgame.players[i].protocol.udp.addr)` in
`net_udp_send_game_info()`. After migration, the HOST's own address in
slot 0 is stale from the previous game (127.0.0.1:42430) -- the same
address the host proxy assigns to the incoming client. Both slots match
the recipient's address, so `isyou=1` for ALL slots.

The client's `read_sync_packet` uses `!isyou` to gate the CONNTYPE fix
loop. Since `isyou=1` for all slots, the loop never fires, leaving
`connection_statuses[0] = CONNT_NONE`. Pdata falls through to
`net_udp_send_to_player_proxy(data, len, 0, 0)` which drops the packet
because `connection_statuses[0].type != CONNT_DIRECT`.

## Why isyou exists (and why the original authors used it)
`isyou` is a network-level identity: the host tells the recipient "you
are at this address, which is slot X." In the original desktop game,
every player has a unique IP:port, so this is reliable and provides a
cross-check against callsign-based identity.

The Android proxy architecture creates address collisions because all
peers map through 127.0.0.1:4243x ports. After migration, the new host's
stale proxy address can match an incoming client's proxy address.

## Why zeroing the host address doesn't work
The host's own address in the Netgame struct is read by:
- is_master_ip() for packet validation (clients reject all host data)
- send_sync() self-send (packet goes to 0.0.0.0:0)
- find_player_by_identity() (reconnection matching fails)
Zeroing it would break client-side packet validation.

## Fix applied (ifdef'd, Android-only)
On Android, replace the `!isyou` guard with `i != Player_num` in
`read_sync_packet()`. `Player_num` is set by callsign match (gated by
isyou) in the same loop iteration ABOVE the CONNTYPE block.

For iterations before self-discovery, `Player_num` is -1, so `i != -1`
is TRUE for all valid i -- correctly treating them as other players.

Desktop code retains the original `!isyou` check unchanged.

## Files changed
- d2/main/net_udp.c: read_sync_packet CONNTYPE guard (#ifdef __ANDROID__)
- d1/main/net_udp.c: read_sync_packet CONNTYPE guard (#ifdef __ANDROID__)

## Build verification
- `gradlew.bat bundleDebug`: BUILD SUCCESSFUL

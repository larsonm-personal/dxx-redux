# Coop Peer Status Broadcast

## Problem
The CoopStatsOverlay reads from `Players[i].shields/energy` and `Coop_kill_stats[i]`,
but these are never updated for remote players in coop mode. The existing
MULTI_DAMAGE/MULTI_REPAIR/MULTI_SHIP_STATUS packets are gated behind
`is_observer()` and only sent to the host.

## Solution (revised)
Reuse existing MULTI_DAMAGE/MULTI_REPAIR/MULTI_SHIP_STATUS infrastructure. These
packets are already sent on-change from 20+ call sites across collide.c, powerup.c,
laser.c, weapon.c, controls.c, etc. Two problems were blocking them from working
in coop:
1. Send functions bailed early unless observers configured
2. Sent only to master via multi_send_data_direct(), not to all peers
3. Receive handlers only processed data under is_observer() gate

Fix: Under #ifdef __ANDROID__, in coop mode:
- Send functions skip the observer-only gate and use multi_send_data() (broadcast)
- Receive handlers update Players[] fields for remote peers

MULTI_COOP_PEER_STATUS trimmed to kills-only (8 bytes) since shields/energy/weapons
are now covered by the existing packets.

## MULTI_COOP_PEER_STATUS packet layout (8 bytes, kills only)
- [0]    = MULTI_COOP_PEER_STATUS
- [1]    = Player_num (sender)
- [2..3] = robots_killed (short)
- [4..7] = score_earned (int)

## Changes (all complete)

### d2/main/multi.h
- [x] MULTI_COOP_PEER_STATUS resized to 8 bytes
- [x] Declare `coop_send_peer_status()` and `coop_do_peer_status()`

### d2/main/multi.c
- [x] multi_send_damage: skip observer gate in coop, broadcast to all peers
- [x] multi_do_damage: add coop peer path to update remote shields
- [x] multi_send_repair: skip observer gate in coop, broadcast to all peers
- [x] multi_do_repair: add coop peer path to update remote shields
- [x] multi_send_ship_status: skip observer gate in coop
- [x] multi_send_ship_status_for_frame: broadcast in coop
- [x] multi_do_ship_status: add coop peer path for weapons/energy
- [x] coop_send_peer_status: trimmed to kills only (8 bytes)
- [x] coop_do_peer_status: trimmed to kills only

### d1/main/multi.h and d1/main/multi.c
- [x] Same changes duplicated for d1 (5 secondary weapons vs 10)

### No JNI or Kotlin changes needed
- The JNI already reads from Players[] and Coop_kill_stats[]
- The overlay already polls at 1Hz
- [x] Same changes duplicated for d1

### No JNI or Kotlin changes needed
- The JNI already reads from Players[] and Coop_kill_stats[]
- The overlay already polls at 1Hz

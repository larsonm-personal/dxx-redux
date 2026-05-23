# Rejoin + Exit Button + Guidebot Fixes -- Phase 2

## Root causes found

### 1. Rejoin after host migration (FIXED)
Player32's engine socket is bound to `127.0.0.1:42424` (loopback) from the initial auto_join
(`bind_loopback=1` because host_addr="127.0.0.1" through the proxy). After host migration,
Player68's proxy sends GAME_INFO_REQ to `192.168.88.49:42424` (Player32's wlan0 interface).
The OS drops the packet because Player32's socket only accepts loopback traffic.

Fix: `net_udp_rebind_for_hosting()` in net_udp.c -- closes the loopback socket and reopens
on 0.0.0.0 (INADDR_ANY). Called from `multi_disconnect_player()` in multi.c when this player
becomes the new master.

### 2. Exit button during auto_join (FIXED in prior commit, verified)
`auto_join()` 30-second blocking loop never polled SDL events or checked `Quitting`.
Fix: check `android_force_quit` flag each iteration.

### 3. Guidebot ownership deadlock (FIXED)
Chicken-and-egg: AI guard `Escort_owner_player == Player_num` blocked `do_escort_frame()`,
but `Escort_owner_player` is only set INSIDE `do_escort_frame()` via `ok_for_buddy_to_talk()`.
Result: `Escort_owner_player` stayed at -1 forever, nobody ran guidebot AI in coop.

Fix: changed guard in ai.c to `Escort_owner_player == -1 || Escort_owner_player == Player_num`.
When unowned, all players run do_escort_frame; first to call ok_for_buddy_to_talk() when
buddy is free claims ownership and broadcasts via multi_send_escort_owner.

## Files changed

### d2/main/net_udp.c
- [x] Added `net_udp_rebind_for_hosting()` after `udp_open_socket()`
- [x] Added `android_force_quit` check in auto_join loop (prior commit)
- [x] Added MPDIAG logging: auto_join req count, GAME_INFO_REQ host-side recv/send

### d2/main/net_udp.h
- [x] Declared `net_udp_rebind_for_hosting()`

### d2/main/multi.c
- [x] Call `net_udp_rebind_for_hosting()` in host migration when new_master == Player_num

### d2/main/ai.c
- [x] Changed escort AI guard to allow unowned (-1) guidebot to run

### d2/main/escort.c
- [x] Added ESCORT_DIAG logging at ownership claim, receive, and transfer points

### d1/main/net_udp.c, net_udp.h, multi.c
- [x] Same socket rebind fix and auto_join exit button fix
- [x] Same MPDIAG logging for GAME_INFO_REQ
- (d1 has no guidebot)

## Build status
- [x] compileDebugSources: PASS, no warnings
- [x] clang-format: PASS
- [ ] User test pending

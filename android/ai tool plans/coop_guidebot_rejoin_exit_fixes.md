# Coop Guidebot Freeze, Rejoin Failure, and Exit Button Fixes

## Problem Summary

Three issues from LAN coop testing (2026-04-09):

### 1. Guidebot frozen in coop after release
- Player releases guidebot, it sits frozen on releasing player's screen
- On other player's screen, it wiggles and makes noises  
- Root cause: `Escort_owner_player` is set but `REMOTE_OWNER` on the buddy object is never updated, so `multi_send_robot_position()` bails out and no position updates are sent

### 2. Rejoin fails after host migration
- Host leaves coop game, client becomes new host via migration
- Ex-host can see new host's LAN broadcast
- Ex-host tries to join but is stuck at "joining" screen forever
- Root cause: `net_udp.c` hardcodes `players[0]` as master in 13+ places, unaware of `Multi_master_playernum`. After migration, `GAME_INFO` packets corrupt the player array and the ex-host ends up thinking it's also master

### 3. Exit button doesn't work on join screen
- While stuck at the joining screen, the exit button does nothing
- Root cause: `net_udp_sync_poll` and `net_udp_request_poll` return 0 for all non-DRAW events, including KEY_ESC. The SDL_QUIT cascade may not reliably unwind blocking menus

## Fix Plan

### Phase 1: Guidebot REMOTE_OWNER fix (d2/ and d1/)
- [x] In `escort_release_control()`: set `REMOTE_OWNER` on buddy object when changing owner
- [x] In `multi_do_escort_owner()`: set `REMOTE_OWNER` when receiving ownership message
- [x] In `escort_transfer_ownership_on_disconnect()`: set `REMOTE_OWNER` on transfer
- [x] In `ok_for_buddy_to_talk()`: set `REMOTE_OWNER` at initial claim
- [x] Apply same changes to d1/ (if escort code exists there)

### Phase 2: Exit button fix (d2/ and d1/)
- [x] Add KEY_ESC handling to `net_udp_sync_poll` callback
- [x] Add KEY_ESC handling to `net_udp_request_poll` callback  
- [x] Add Quitting check at top of both poll functions
- [x] Apply same changes to d1/main/net_udp.c

### Phase 3: Host migration rejoin fix (d2/ only, android-only)
- [x] Add `Multi_master_playernum` to GAME_INFO and SYNC packets
- [x] Replace `players[0]` with `players[multi_who_is_master()]` in key net_udp.c locations
- [x] Add logging around rejoin flow for diagnosis
- [x] Defer address overwrite in `process_game_info` until master slot is parsed
- [x] Fix send_endlevel, process_endlevel, send_sync (self), process_ping, auto_join restore
- [ ] Fix silent ignore of UPID_REQUEST during `Network_sending_extras`

### Phase 4: Build and test
- [x] Build d2 Android debug (compiles clean, no warnings)
- [ ] Run existing test suite
- [ ] Test rejoin after host migration on LAN

# Coop next-level host start failure

## Goal

Find and durably fix the cooperative transition failure where the host does not
start the next level, using the July 24 desync log, the implemented transition
state machine, and the recent attempted fixes as independent evidence.

## Plan

- [x] Reconstruct the host and client transition timelines from the supplied log
- [x] Trace all next-level start gates, packets, retained-checkpoint hooks, and
      recent fixes in D1 and D2
- [x] Identify the violated distributed-state invariant and choose a systemic fix
- [x] Implement matched D1/D2 changes with diagnostic coverage
- [x] Run focused tests, scoped code quality, and D1/D2 build verification
- [x] Record the root cause, change, and verification results here

## Root cause

The level loaded successfully. The failure was the host rejecting its own local
sync immediately afterward:

- At 21:45:47 the restored save contained host callsign `coopsave`, while the
  live host callsign was `touch`
- `coop_remap_restored_players()` matched the player correctly through metadata,
  then copied the entire saved `player` structure into the live slot
- That copied the storage-only callsign `coopsave` over the live callsign, while
  `Netgame.players[0].callsign` correctly remained `touch`
- At 21:46:47 the host loaded level 10, then `net_udp_read_sync_packet()` cleared
  `Player_num` and tried to rediscover the local player using `isyou` plus the
  now-stale callsign
- No roster entry matched `coopsave`, so `Player_num` remained `-1`,
  `Network_status` became `NETSTAT_MENU`, and level sync returned `-1`, matching
  the final log entry exactly

The recent request-wait reset and sync-return validation changes improved the
handshake and made this failure explicit, but neither change protected player
identity across save restoration.

## Fix

- Added one shared player-session rule used by D1 and D2: a coop restore owns
  gameplay state, but the live session owns callsign, network address,
  connection state, object slot, and packet counters
- Changed restored-player remapping to apply that rule instead of copying the
  whole saved player structure as live state
- Changed Android level-sync self-discovery to prefer the stable client ID, with
  the existing `isyou` plus callsign rule retained as a compatibility fallback
- Improved restore and rejection diagnostics so future logs show both the live
  and saved callsigns and a client-ID prefix
- Added a D1/D2 native regression that recreates `touch` versus `coopsave`,
  verifies restored gameplay values, verifies preserved live session fields,
  and verifies stable-ID matching across a callsign mismatch

## Verification

- Scoped `android/run-code-quality.ps1 -Fix`: passed
- Focused `test_coop_player_session` compiled and passed against both D1 and D2
  player layouts
- `:app:externalNativeBuildDebug`: passed for arm64-v8a, armeabi-v7a, and x86_64,
  compiling both D1 and D2
- `git diff --check`: passed
- The standard Windows wrapper was attempted, but vcpkg stalled in its
  pre-build compiler-hash step and was stopped before compilation. The focused
  MSVC tests and all-ABI Android CMake build cover the changed code.

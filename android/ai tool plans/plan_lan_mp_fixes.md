# Plan: LAN Multiplayer Fixes, Level Picker, Logging, and Test Automation

**STATUS: COMPLETE**

Five issues across LAN multiplayer: D1 mission selection bug (empty-string filename),
missing level/difficulty picker for game start, silent join failures, cross-device
discovery problems, and no automated LAN test.

## Issues

1. When hosting a LAN game for D1, selecting "First Strike" leaves the mission box blank
2. No way to choose level and difficulty when starting a LAN game (level names desired)
3. Clicking "Join" in LAN game list does nothing visible
4. After joining, the joiner's phone can't discover hosted games from the other phone
5. No automated test for the LAN game creation and joining flow

## Phase 1: Fix D1 "First Strike" Mission Selection Bug

**Root cause:** D1's builtin mission filename is "" (empty string, per d1/main/mission.h).
MissionPickerField line ~148 short-circuits on `selectedFilename.isEmpty()` returning
empty display text. After selecting "First Strike", the filename "" is treated as
"no selection" and the field appears blank.

**Fix:** Change `selectedFilename` from `String` to `String?` (nullable). Use `null`
for "no selection" instead of empty-string. This lets "" flow through to the
`missions.find{}` lookup which correctly matches the D1 builtin.

Files:
- android/app/src/main/java/com/dxxredux/app/multiplayer/MissionPicker.kt
- android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt

## Phase 2: Level Picker with Level Names

**Current state:** StartLanGameDialog has difficulty buttons + a bare text field
for level number (hardcoded max 30). No level names shown.

**Approach:** Read level count from the mission file (.msn/.mn2) on the Kotlin side.
The `num_levels` line is already parsed by MissionScanner but discarded. Keep it
in MissionInfo so the host/start dialogs can cap the level range properly.

For level display names: these live inside .rdl/.rl2 level files within .hog archives
and require the game's HOG I/O to extract. Defer names and just show numbered levels
with correct count for now. This is still a big improvement over hardcoded 1-30.

Steps:
1. Add `levelCount: Int` to MissionScanner.MissionInfo, parse from `num_levels=` line
2. For builtins, hardcode: D2 Counterstrike=30, D1 First Strike=27, D2 First Strike=27
3. Pass level count from HostLanGameDialog through to StartLanGameDialog
4. Replace level text field with a dropdown/list capped to actual level count
5. Validate level in 1..levelCount instead of hardcoded 1..30

Files:
- android/app/src/main/java/com/dxxredux/app/multiplayer/MissionPicker.kt
- android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt

## Phase 3: LAN Join/Discovery Diagnostic Logging

**Goal:** Make it easy to diagnose "join does nothing" and "host not discoverable"
from adb logcat output.

Steps:
1. LobbyService.kt: add detailed Log.i/Log.d in:
   - joinLobby(): log target address, lobby ID, socket state, packet bytes
   - sendTo() / sendBroadcast(): log byte count or failure with exception
   - openSocket(): log bound port, multicast lock status, local addresses
   - receive loop: log parsed packet type + sender (DEBUG level)
   - handleAnnounce(): log lobby ID, new vs update vs filtered-own
   - handleJoin(): log whether hosting, lobby ID match, acceptance/rejection
2. Add JOIN retry: send JOIN up to 3 times at 1s intervals (current is fire-and-forget)
3. Add visual "Joining..." state with timeout on the discovery screen
4. Add a status line on the LAN screen showing socket status and packet counts

Files:
- android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt
- android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt

## Phase 4: Direct Connect ("Join by IP") and LAN Test Commands

**Why:** Broadcast discovery fails between emulators (separate SLIRP NATs) and can
fail on real networks with AP isolation or multicast filtering.

Steps:
1. Add "lan_launch" command to the existing MP_COMMAND broadcast receiver:
   - Accepts: game, mp_mode (host/join), mission, mode, max_players,
     level_num, difficulty, host_addr, host_port, callsign
   - Constructs GameLaunchInfo and calls launchMultiplayerGame() directly
   - Bypasses the lobby UI entirely for testing
2. Add a "Join by IP" button to LanDiscoveryView with a dialog for entering
   host IP and port, then launching directly as joiner

Files:
- android/app/src/main/java/com/dxxredux/app/SetupActivity.kt
- android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt

## Phase 5: Two-Emulator LAN Integration Test

**Approach:** Use the lan_launch MP_COMMAND to bypass launcher UI. Reuse the
UDP relay pattern from run_mp_test.ps1 for cross-emulator engine traffic.

Steps:
1. Create android/game_scripts/test_lan_mp.json5:
   - wait_for screen_mode == "game" (with timeout)
   - assert is_network == true
   - assert multiplayer.num_connected >= 2
2. Create android/run_lan_test.ps1:
   - Verify both emulators online
   - Set up UDP relay between emulators
   - Send lan_launch host on EMU1
   - Send lan_launch join on EMU2
   - Wait for both in-game via introspection polling
   - Push and trigger test_lan_mp.json5 on both
   - Read automation_result.json from both

Files:
- android/game_scripts/test_lan_mp.json5
- android/run_lan_test.ps1

## Verification

- Phase 1: Select D1 -> pick "First Strike" -> field shows "Descent: First Strike"
- Phase 2: Level picker shows correct count (27 for D1, 30 for D2 Counterstrike)
- Phase 3: adb logcat -s LobbyService shows packet traces for all LAN operations
- Phase 4: lan_launch command launches game directly from adb
- Phase 5: run_lan_test.ps1 passes with both emulators in-game

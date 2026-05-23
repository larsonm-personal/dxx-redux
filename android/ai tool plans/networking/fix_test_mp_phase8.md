# Fix test_mp Phase 8: Game Never Launched

## Problem
Phase 8 of test_mp failed with "Player 2 never entered game" -- neither player's
game engine process actually launched successfully.

## Root Cause
The host emulator (EMU1) had its active file set pointing to `regression_test`
(an empty directory) instead of `default` (which contains descent2.hog and other
game data files). The game engine started, couldn't find descent2.hog, logged
"Could not find a valid hog file", and crashed with SIGABRT within 1 second.

When the host's game process crashed:
1. SetupActivity.onResume() fired
2. isGameProcessAlive() returned false  
3. endGame() sent END_GAME to the matchmaking server
4. Server reset the lobby to Waiting state

The joiner (EMU2) had a correct file set and launched successfully, but its
auto-join to 127.0.0.1:42430 timed out because the host's game was dead.

A secondary issue was stale introspect.json files from previous test sessions
giving false positives -- EMU1 appeared "in game" at the first poll because
the old introspect data had in_game=True from a D1 single-player session.

## Changes Made

### android/tests/test_mp.ps1 (DONE)
- Added file set reset: writes `{"migration_version":1,"active":"default"}`
  to both emulators' file_sets.json before Phase 1, ensuring the game finds
  hog files regardless of what previous tests left behind
- Added introspect.json cleanup: deletes stale introspect.json on both
  emulators before Phase 7/8, so the test doesn't read old game state

### Previous session changes (already committed)
- Server DirectLan downgrade in ws_handler.rs
- Relay config in test_helpers.ps1
- Custom relay removal from test_mp.ps1
- Various test_mp.ps1 structural improvements (logcat capture, phase ordering)

## Test Results
- test_mp.ps1 passes all 9 phases with exit code 0
- Both players enter network game (game_mode=4 = GM_NETWORK)  
- Both see 2 players with correct callsigns (HostPlt, JoinPlt)
- Code quality checks pass (ktlint, PSScriptAnalyzer, shellcheck, shfmt)

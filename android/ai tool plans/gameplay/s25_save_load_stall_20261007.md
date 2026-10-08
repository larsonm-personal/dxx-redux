# S25 save-load stall

## Plan

1. [x] Preserve device logs and inspect the installed build and available diagnostics
2. [x] Reproduce leaving D2 for the game menu and loading "auto best"; identify the simulation/input blocker using targeted diagnostics
3. [x] Fix the confirmed cause, checking the corresponding D1 path
4. [x] Run scoped formatting, relevant builds, and a regression reproduction

## Initial evidence

- Connected S25: RFCY703C48J, SM-S931U
- Installed package is com.dxxrevival.app and is not debuggable; run-as and the debug introspection receiver are unavailable
- Original logcat saved under temp/s25_stall_20261007 before changing game state
- User reports music, Kotlin controls, and graphics setting redraws continue while gameplay is stalled
- Existing unrelated working-tree changes are present and will be preserved

## Confirmed cause and fix

- Normal main-menu launch, abort, and restore passes on the emulator
- Launcher "Load Last Save", abort, and restore reproduces `time_paused=true` with the gameplay window in front
- Temporary Android pause logging shows the closing window receives DEACTIVATED at depth 0, CLOSE at depth 1, another DEACTIVATED at depth 1, and CLOSED at depth 2
- The next gameplay window's activation releases one pause, leaving the simulation paused
- Startup resume has no hidden main menu, so `restore_game_menus()` creates a new menu inside EVENT_WINDOW_CLOSE while the old game window is still in the window list
- Move Android menu restoration to EVENT_WINDOW_CLOSED, after the window has been unlinked, in both engines; desktop behavior is unchanged
- Diagnostic logging was removed after preserving the trace in temp/s25_stall_20261007/diagnostic_logcat.txt
- Extend the existing registered shared save/load integration test with normal and startup-resume abort/restore cycles, a repeated reload, and firing that must consume energy

## Validation

- Unfixed D2 and D1 both fail the startup-resume abort/restore assertion with `time_paused=true`
- Fixed D2 emulator run passes all 86 steps, including repeated reload and firing
- Fixed D1 emulator run passes all 85 steps, including repeated reload and firing
- Scoped mixed-language formatting passed; final rerun made no changes
- Automation catalog validation and catalog integration passed
- Debug x86_64 and arm64-v8a CMake/Gradle builds passed
- S25 test runner initially stopped at the secure lock screen; after normal unlock, the isolated com.dxxredux.app.nsdtest build passed all 86 D2 regression steps, including repeated reload and firing
- S25 result and final state are saved in temp/s25_stall_20261007/d2_fixed_s25_result.json and d2_fixed_s25_state.json
- The installed com.dxxrevival.app distribution and its saves were not replaced or cleared

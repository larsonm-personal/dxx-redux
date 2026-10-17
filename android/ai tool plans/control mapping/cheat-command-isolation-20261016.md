# Cheat menu command isolation

- [x] Extract each engine's recognized-cheat effect handler without changing physical keyboard recognition
- [x] Submit complete menu codes through the existing session-owned pause command queue; remove delayed key injection and Kotlin enablement cache
- [x] Cover actual D1/D2 effects, keyboard entry, input/view preservation, recording policy and stale/blocked command requests
- [x] Run scoped formatting, relevant native/Android builds and regression checks

The menu must never emit keyboard events. D1 menu requests enable cheats using native state. Keyboard typing retains its historical behavior. The existing pause queue owns UI revision/session cancellation and game-thread execution; requests execute only in eligible foreground single-player gameplay after the tray closes. Preserve semantic weapon-grant recording and cancellation for unsupported cheats.

## Implementation and verification

- D1/D2 `FinalCheats` retain their keyboard matching and return behavior and share `execute_cheat` with complete-code `game_execute_cheat`
- MainActivity closes the tray and submits its UI revision and selected code atomically through the existing pause mailbox. Native execution requires allowed gameplay input; stale owners/sessions/revisions, other modals, death, multiplayer, endlevel and demo playback cannot execute the menu command
- Removed the 260 ms delayed keystrokes, concatenated D1 enable prefix and cached Kotlin enablement state. The engine handles enablement and validates exact names
- Android x86_64 Debug APK builds both engines; 34 Kotlin cheat/tray tests passed
- Windows D1/D2 builds passed. Real-engine public all-weapons fixtures passed native keyboard entry, direct menu commands, repeated grants, conflicting bindings, held controls/modifiers, forward/rear cockpit preservation, invalid/blocked commands and semantic recording/capture cancellation
- `test_android_pause_state.ps1 -Serial emulator-5554 -Install` passed both games, including four menu grants per game with rear view off/on, pause reconciliation, save/load, stale cheat admission and level transitions
- Device evidence: `android/temp/pause-state-20261016-090658/results.json` and paired logcat files. Native evidence: `temp/cheat-host-d1.log`, `temp/cheat-host-d2.log`. Build logs: `temp/cheat-android-build-final.log`, `temp/cheat-host-build-final.log`
- Automation catalog and master-suite catalog checks passed. Scoped formatting passed. No physical-device or ARM runtime claim

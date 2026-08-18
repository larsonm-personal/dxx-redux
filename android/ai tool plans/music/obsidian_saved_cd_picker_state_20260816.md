# Obsidian saved CD picker state

## Goal

Make the in-game Android music picker reflect the music source restored by a save, and ensure track navigation and volume operate on that active source. Reassess whether the launcher-level prefer-bundled-music setting should remain authoritative.

## Plan

- [x] Trace saved music restoration, bundled-music selection, picker state, navigation, and volume in D1 and D2
- [x] Identify the source-of-truth mismatch and define the policy decision needed for saved games and newly opened mods
- [x] Implement option 2: pilot-authoritative music with bundled music selected on new mod activation
- [x] Add or extend regression coverage for pilot-authoritative save loading and unified track controls
- [x] Run scoped formatting, relevant tests, and the required CMake build/test verification

## Notes

- `android/outstanding_bugs.md` has pre-existing user changes and must not be overwritten.
- Resume launch writes the save metadata's `music_type` into `descent.cfg`, and level loading starts that source correctly.
- D1 and D2 later call `set_highest_level()`, which calls `read_player_file()` and reparses the Android `[music]` block from the pilot `.plx`.
- The pilot read assigns `GameCfg.MusicType` and the mission preference but does not restart music. This can leave Redbook playback active while configuration and picker state say bundled MIDI.
- `songs_next_track()`, `songs_prev_track()`, `songs_get_track_list()`, and `songs_set_volume()` all dispatch from `GameCfg.MusicType`, so the single mismatch explains all reported symptoms.
- The existing `Use mission soundtrack when available` preference is a blanket launch override, not a one-time default for a newly opened mod. It conflicts with the proposed simpler policy of persisting the latest explicit picker source in the pilot while choosing bundled music only when a new soundtrack-bearing mod is activated.

## Result

- Save metadata still records the music type for informational compatibility, but neither launcher resume nor D1/D2 state restore applies it to playback.
- The pilot `source` is authoritative for ordinary and resumed launches.
- Importing or re-enabling a mission ZIP with a soundtrack writes `mission` as the pilot source while preserving volume and play order.
- Selecting a source in the in-game picker writes that exact source back to the pilot and launcher fallback preference.
- The blanket launcher preference, UI, export entry, lifecycle application, and JNI command were removed.

## Validation

- Scoped code quality passed for all changed implementation, test, and plan files.
- Focused `MusicLaunchPolicyTest`: passed.
- `python android/tests/test_android_audio_lifecycle.py`: 9 tests passed.
- Android native build passed for arm64-v8a, armeabi-v7a, and x86_64.
- `:app:assembleDebug`: passed.
- Windows D1 and D2 build: passed.
- CTest: D1 33/33 and D2 40/40 passed.
- `test_music_track_controls_unified.json5`: passed on the emulator for D2 and D1.
- Full Android unit suite: 841/842 passed; the unrelated RAR import test could not initialize the SevenZip native library in the local JVM.

# Unbound actions and multi-BIN track investigation plan - 2026-05-22

## Goals
- Make the controller-only unbound-actions menu treat Energy->Shield as a held action while A is held on that row
- Show clear in-game feedback when Headlight is triggered without owning the headlight powerup
- Investigate why imported multi-BIN CD sources show zero in-game tracks even though launcher preview, playback, hashing, and chromaprint work

## Current local hypotheses
- The controller-only unbound-actions menu in `TouchOverlayView.kt` currently synthesizes every remaining action as a short press by pairing `pressLayoutButtonBinding(...)` with a delayed release, so Energy->Shield cannot stay active while A is held
- The D2 headlight toggle path in `d2/main/lighting.c` is a silent no-op when `PLAYER_FLAGS_HEADLIGHT` is absent, so the menu has no way to surface useful feedback today
- The multi-BIN runtime failure is likely below launcher import and preview, in the game-process playlist or native runtime parser path (`AudioSourceManager.writePlaylist()` -> `rbaudio_bin.c`), because preview already uses the same multi-file CUE model successfully

## Plan
1. [completed] Patched the Android controller-menu remaining-action path so Energy->Shield uses press on A-down and release on A-up while other actions keep the current tap behavior
2. [completed] Added D2-native feedback when Headlight is triggered without possession and revalidated the same menu path
3. [completed] Instrumented the game-process multi-BIN runtime path with a Redbook init-status string plus audio-track count surfaced through game introspection so on-device checks can tell whether failure happens in playlist loading, cue parsing, or BIN opening
4. [completed] Ran focused validation for the touched slices and updated this plan with results

## Notes
- `TouchOverlayView.kt` now treats `Energy->Shield` as the only held remaining action in the controller-only extra menu. A-down presses the binding, A-up releases it, and the menu stays open instead of collapsing immediately
- `d2/main/lighting.c` now shows `No headlight boost` when the headlight toggle is triggered without owning the headlight pickup
- `game_introspect.cpp` now emits `redbook.init_status` and `redbook.num_audio_tracks`, backed by new status reporting in `rbaudio_bin.c`
- A local attempt to bring up the emulator through `android\Run-Emulator.ps1` did not yield a usable device in this turn because the helper hit a separate Kotlin daemon temp-backup failure during its APK build path

## Validation targets
- Completed: `cd android; .\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ControllerMenuCycleTest --console=plain`
- Completed: `cd android; .\gradlew.bat :app:externalNativeBuildDebug --console=plain`
- Remaining on-device follow-up: launch the game with an imported multi-BIN source, then inspect `redbook.init_status`, `redbook.num_tracks`, and `redbook.num_audio_tracks` through the existing introspection path
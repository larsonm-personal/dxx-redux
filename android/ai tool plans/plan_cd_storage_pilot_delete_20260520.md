# CD Audio Storage And Pilot Delete Plan - 2026-05-20

## Goals
- Research in-place multi-BIN CD audio handling without merging all BIN files into one app-private image
- Fix reconstructed CD output so it honors the overridden app storage folder and audit similar write paths
- Add Android-accessible pilot deletion through touch long press and controller long hold / A hold

## Current Status
- Research complete for the first planning pass
- Implemented the first storage relocation tranche for reconstructed / merged CD audio artifacts
- Implemented and validated Android pilot long-hold deletion for D1 and D2
- Completed the dedup follow-up so the Android-only pilot hold logic now lives in shared Android-owned code and the D1/D2 diff is smaller
- Completed the pilot-delete follow-up so the confirmation prompt opens as soon as the Android A hold crosses the threshold and Shield/TV DPAD center is routed through the same controller-A hold path
- Full in-place multi-BIN playback remains a planned follow-up tranche

## Findings
- In-place multi-BIN CD audio is feasible, but it is a medium-sized cross-layer change. The launcher CUE parser already tracks `fileIndex`, but the persisted source model, playlist writer, in-game Redbook player, and preview player are still shaped around one live BIN handle.
- `SetupActivity.kt` deliberately normalizes multi-BIN SAF disc audio by concatenating all selected BINs into one generated BIN and writing a regenerated CUE. That helper currently writes both generated files directly under `filesDir`, so it ignores the imported-files override volume.
- `AudioSourceManager.kt` has cleanup and playlist helpers that assume generated local artifacts live under `filesDir`. Moving generated audio artifacts to the import root needs matching changes in deletion, orphan pruning, source path resolution, and storage-inspector labeling.
- `rbaudio_bin.c` parses the playlist `bins` array but only opens the first BIN for a source. Its source CUE parser computes track lengths from one BIN size. True in-place multi-BIN playback needs per-track file indexes and multiple open handles per source.
- `cd_preview.c`, `CdPreviewBridge.kt`, and `MusicPickerPage.kt` are also single-BIN shaped. Preview support must be extended along with runtime playback or multi-BIN sources will import but not preview correctly.
- Other large internal-storage candidates exist near the same code: `filesDir/tmp` is used for archive, GOG, SOW, and CUE staging; `mods/` and `custom_music/` also live under `filesDir`. Some of these are temporary or existing design choices, but the GOG/SOW temporary staging paths are worth moving to the import root in a later audit because they can be large.
- File-set game data already honors the override through `ImportLocationManager` and `FileSetManager`. The active set path is written back to small metadata files under `filesDir/d1x-redux` and `filesDir/d2x-redux`, which is appropriate.
- Pilot deletion is already centralized in `d1/main/menu.c` and `d2/main/menu.c`: `player_menu_keycommand()` handles `KEY_CTRLED + KEY_D`, prompts via `nm_messagebox`, deletes `.plr`, `.eff`, `.ngp`, and saves, then calls `listbox_delete_item()`.
- The pilot list title is `TXT_SELECT_PILOT` in both `d1/main/text.h` and `d2/main/text.h`, currently `Select pilot\n<Ctrl-D> deletes`. This should become Android-specific text while preserving the desktop text.
- Android native listboxes already route controller A to Enter, B to Esc, and D-pad buttons to arrows in `newmenu.c`. Touch is routed through `nativeTouchEvent` to SDL mouse down/up. There is no long-hold path yet.
- The safest pilot UX implementation is native and duplicated in D1/D2: add Android-only hold tracking around the pilot listbox so a short tap/A press still selects, while a held tap/A press fires the existing Ctrl-D delete path. The confirmation dialog can keep using existing menu input: tap/A confirms the selected button and B/Esc backs out.

## Plan
1. Completed: survey launcher/native CD audio import, preview, and runtime playback paths
2. Completed: identify where reconstructed CD images and related artifacts choose their output directory
3. Completed: audit nearby import/extraction/reconstruction paths for direct use of internal data storage instead of the configured app storage root
4. Completed: survey D1 and D2 pilot select/change menus, existing Ctrl-D delete flow, and Android input plumbing for long press / long hold
5. Completed: fix generated merged CD audio storage location
	- Added a launcher helper for generated CD audio artifacts under `ImportLocationManager(filesDir).getActiveRoot()/cd_audio/`
	- Changed `stageMergedSafDiscAudioSource()` to write generated BIN/CUE files there instead of `filesDir`
	- Stored generated local paths in `AudioSourceManager.AudioSource` as absolute local paths while keeping existing relative `filesDir` paths supported
	- Updated `AudioSourceManager` cleanup, orphan pruning, missing-source pruning, and playlist CUE resolution for generated artifacts outside `filesDir`
	- Updated music picker removal/info/preview path handling for generated and local absolute CD audio paths
	- Added JVM tests for generated merged artifact path reporting and absolute generated CUE resolution
6. Follow-up audit tranche: move other large temporary or import-like files off internal app storage where practical
	- Prioritize `filesDir/tmp` for GOG installer fallback staging, SOW staging, archive expansion, and CUE copies
	- Consider whether `mods/` and `custom_music/` should be moved under the same import root or left for a later migration because those affect active mod/music paths and UI expectations
	- Keep small metadata, configs, manifests, saves, and debug logs in `filesDir`
7. Deferred feature tranche: true in-place multi-BIN CD audio
	- Extend `AudioSource` to preserve one URI/path per BIN instead of one `binContentUri`
	- Write playlist entries with all local paths or all `/proc/self/fd/<n>` paths and keep all SAF descriptors alive for the game session
	- Replace or extend the runtime CUE parsing in `rbaudio_bin.c` so each track records a file index and sector offset within that file
	- Open multiple BIN handles per source and read audio from the correct handle for each track
	- Extend `cd_preview.c`, `CdPreviewBridge.kt`, and `MusicPickerPage.kt` to preview from multiple paths/fds
	- Add tests using a tiny synthetic multi-BIN cue with known track boundaries
8. Pilot deletion tranche
	- Completed on 2026-05-20
	- Added Android-only pilot list text in `d1/main/text.h` and `d2/main/text.h`, preserving `<Ctrl-D> deletes` on desktop
	- Added Android-only hold tracking in both `d1/main/newmenu.c` and `d2/main/newmenu.c`, gated so it applies only to the pilot listbox/title and only to existing pilots, not the create-new row
	- For touch, record the pressed row/time on left mouse down and on mouse up choose delete if the press exceeded the hold threshold, otherwise keep the current select behavior
	- For controller A, handle button down/up in the listbox path so a short A selects and a held A calls the existing `KEY_CTRLED + KEY_D` callback path
	- Kept B/Esc behavior as the back/cancel path and changed the Android confirmation's positive button to `Ok`
	- Added `android/game_scripts/test_pilot_long_hold_delete_unified.json5` to navigate to pilot selection, trigger the delete confirmation via A hold, cancel with B, then confirm with `Ok` on a disposable pilot
9. Dedup follow-up tranche
	- Completed on 2026-05-20
	- Extracted the Android-only pilot listbox hold state and timing logic into `android/app/src/main/cpp/shared/android_pilot_listbox_hold.c`
	- Kept only thin Android hook calls in `d1/main/newmenu.c` and `d2/main/newmenu.c`
	- Revalidated the native Android build and the unified D1/D2 pilot delete automation after the extraction
10. Pilot hold follow-up tranche
	- Completed on 2026-05-20
	- Changed the shared Android pilot hold helper so the delete confirmation triggers from polling once the hold threshold elapses, instead of waiting for button release
	- Suppressed the later A-release select action after a hold-triggered delete prompt so the held confirm input does not immediately activate the underlying pilot row
	- Routed `AKEYCODE_DPAD_CENTER` through the native virtual joystick A button path in `android_input.c` so Shield/TV remote center follows the same Android hold logic as controller A
	- Narrowed `android/game_scripts/test_pilot_long_hold_delete_unified.json5` to a focused threshold regression because the automation harness does not reliably interact with the nested confirmation prompt buttons

## Validation Targets
- Completed: `android\stop-stale-formatters.ps1` reported no stale formatter tasks
- Completed: scoped `android\run-code-quality.ps1 -Fix -Paths ...` passed on modified main Kotlin files; the repo ktlint wrapper does not scan `app/src/test`
- Completed: focused JVM tests for `AudioSourceManagerArtifactPathsTest` and `CdAudioSourceVisibilityTest` passed
- Completed: `cd android; .\gradlew.bat :app:testDebugUnitTest --console=plain` passed
- Completed: `cd android; .\gradlew.bat :app:externalNativeBuildDebug --console=plain` passed
- Note: an attempted full unscoped `android\run-code-quality.ps1 --fix` was parsed as a path by PowerShell and only linted. It reported a pre-existing clang-format issue in `android\app\src\main\cpp\shared\net\net_udp_android_autonet_shared.h`; the scoped pass avoided unrelated formatter churn.
- Completed: `cd android; .\gradlew.bat :app:assembleDebug --console=plain` passed after pilot delete changes
- Completed: `android\run_test.ps1 test_pilot_long_hold_delete_unified.json5 -Game d2 -Install -TimeoutSeconds 240` passed
- Completed: `android\run_test.ps1 test_pilot_long_hold_delete_unified.json5 -Game d1 -TimeoutSeconds 240` passed
- Completed: final scoped `android\run-code-quality.ps1 -Fix -Paths ...` passed on pilot delete files
- Completed: final `git diff --check` passed with only line-ending warnings
- Completed: post-extraction `cd android; .\gradlew.bat :app:externalNativeBuildDebug --console=plain` passed before and after the scoped formatter pass
- Completed: post-extraction `android\run_test.ps1 test_pilot_long_hold_delete_unified.json5 -Game d2 -Install -TimeoutSeconds 240` passed
- Completed: post-extraction `android\run_test.ps1 test_pilot_long_hold_delete_unified.json5 -Game d1 -TimeoutSeconds 240` passed
- Completed: post-extraction scoped `android\run-code-quality.ps1 -Fix -Paths ...` passed
- Completed: follow-up `cd android; .\gradlew.bat :app:assembleDebug --console=plain` passed after the threshold/DPAD-center changes
- Completed: focused threshold regression `android\run_test.ps1 -ScriptName test_pilot_long_hold_delete_unified.json5 -Game d2 -TimeoutSeconds 240` passed
- Completed: focused threshold regression `android\run_test.ps1 -ScriptName test_pilot_long_hold_delete_unified.json5 -Game d1 -TimeoutSeconds 240` passed
- Note: the Android automation harness currently proves the prompt opens before release, but it does not reliably dismiss or confirm the nested `nm_messagebox` prompt via scripted menu interactions, so those prompt-button paths still need by-hand validation if we want automated coverage later
- If true multi-BIN playback is implemented, add a synthetic multi-BIN runtime/preview test before relying on device-only audio checks

# Input Demo Device Recording And Regression Plan

## Goal

Study the remaining work needed to record input demos on real devices, manage
new recordings in the Android launcher, and replay recorded demos through
Windows and headless regression runners

## Study Checklist

- [x] Review existing input-demo recording and replay architecture
- [x] Review Android launcher advanced/export patterns
- [x] Review existing key binding and extended-button option paths
- [x] Review desktop replay smoke runner and headless test options
- [x] Draft implementation phases with validation gates

## Current Findings

### Native Recorder And Replay

- D1 and D2 both already start input-demo recording from `newdemo_start_recording()` and flush from `newdemo_stop_recording()`.
- The current recorder writes one `.dximdemo` file with a header record, one interleaved input/RNG record per frame, and an embedded result trailer.
- Recording is currently tied to classic demo recording. This is useful because natural stop paths already call `newdemo_stop_recording()` when the player dies, exits the mine, finishes the game, quits, or leaves a state that cannot keep recording.
- Manual keyboard F5 still prompts for a classic demo filename. A real-device binding should not depend on that prompt. It needs an Android quick-record path that reuses the recording state but silently names and flushes the input-demo artifact.
- Live input-demo recording is single-player only and requires `D_RAND_REPLAY_MODE_LCG_STATE`. The current guard already reports why recording did not start.
- Replay currently starts from the command line with `-inputdemo-replay <demo-file>`, loads the mission and level, skips intro screens, runs the recorded frames, writes `<demo-file>.actual.json`, compares it to the embedded result trailer, and exits the replay path.

### Android Binding Path

- Touch and controller "extra" actions are `TouchBindings.META_*` IDs in Kotlin, mostly mirrored by `android_meta_actions.h`. The current Kotlin-only gyro toggle uses ID 1033 and is handled before JNI dispatch.
- Touch editor and radial/segment pickers already pull from `TouchBindings.META_BUTTON_LABELS` when the user taps "extra buttons".
- Controller config also pulls from `META_BUTTON_LABELS` and writes meta action bindings into `controller_config.json` as `meta_bindings`.
- `MainActivity.dispatchMetaAction()` sends extra actions to `NativeMetaActions.nativeMetaAction()`, except for Kotlin-only actions such as gyro toggle.
- `android_meta_actions.c` already has special pending-flag actions for cases that cannot be expressed as a simple key injection, such as return-to-launcher and guide-bot release. Demo recording should follow this model instead of injecting F5, because F5 would still use the classic prompt path.

### Launcher File Management

- `AdvancedSettingsPage.kt` already has row patterns for listing log/crash files, saving to Downloads, sharing through a cache copy, and deleting files.
- Existing `saveToDownloads()` already handles a single file but uses `text/plain`. Input-demo export should copy the `.dximdemo` file with `application/octet-stream` or a dedicated MIME type once chosen.
- `FileSetManager` keeps game data in active sets. Existing standalone `.dem` imports go to `<active set>/demos/`.
- `SetupActivity` has an existing `DemosSection` that lists classic `.dem` files from `<active set>/demos/`, but it does not know about `.dximdemo` files.

### Desktop And Headless Runners

- `android/tests/test_input_demo_runtime_smoke.ps1` is the known-good Windows runtime path. It creates a sandbox, writes a local windowed `descent.cfg`, launches with `ProcessStartInfo`, waits for `<demo-file>.actual.json`, and kills the process after result creation.
- Direct PowerShell execution of the game binary is not the supported replay path. Keep using `ProcessStartInfo` and a sandboxed working directory.
- The codebase does not currently have true headless/no-window replay. The existing plan calls for staged headless work: first windowed replay with no sound/music, then no-render, then true no-window/no-GPU support.

## Proposed Artifacts And Paths

- Canonical artifact: `.dximdemo` is the demo file itself, not an archive or directory.
- Newly recorded staging area: `input_demo_recordings/new/<auto-name>.dximdemo` under the engine write directory for the active game.
- Installed in-game area: `<active file set>/demos/<user-name>.dximdemo`, matching the existing place where imported classic `.dem` files live.
- Exported file: `Downloads/<auto-name>.dximdemo`, copied as a single file.
- Initial retention limit: keep the newest 10 entries in `input_demo_recordings/new/` per game. The native game path should trim after a successful flush so the limit is enforced even if the launcher is not opened.

## Naming Rules

Use C/C++ game state to build the default name, not Kotlin guesses:

`<game>_<mission-slug>_level<level-number>_<yyyyMMdd_HHmmss>.dximdemo`

- `<game>` is `d1` or `d2`.
- `<mission-slug>` comes from current mission metadata in the engine. Use `Current_mission_filename` when present, normalize the D1 builtin empty filename to `descent`, and normalize D2 builtin `d2` to `descent2` if we want the user-facing filename to match the HOG name.
- Strip a trailing `.hog` if any path-like mission value includes it.
- Sanitize to printable ASCII `[A-Za-z0-9_.-]`, folding other runs to `_`.
- `level-number` should preserve signs for secret levels, for example `level-1`.
- Add a numeric suffix if the exact timestamp collides.

The C/C++ and Kotlin sides will both know the `.dximdemo` extension and the staging/install directory names. Document these duplicated constants beside `TouchBindings` and the native header when they are added.

## Implementation Phases

### Phase 1: Native Quick Recording Toggle

Goal: record a playable input-demo fixture from a real device without showing the classic demo filename prompt.

Tasks:

- [x] Add `META_DEMO_RECORD_TOGGLE` to `TouchBindings.kt` and `android_meta_actions.h`, using ID 1034 unless the native header is first updated to explicitly reserve the Kotlin-only 1033 gyro toggle.
- [x] In `android_meta_actions.c`, handle the new action as a special case that sets a volatile pending flag on press.
- [x] Consume the pending flag on the game thread in both D1 and D2, near the existing in-game command handling. This should toggle recording only when `Newdemo_state` is `ND_STATE_NORMAL` or `ND_STATE_RECORDING`, not during playback or multiplayer.
- [x] Add a small D1/D2 helper around existing `newdemo_start_recording()` and `newdemo_stop_recording()` for Android quick recording. The helper should mark the recording as auto-named and suppress the classic prompt on stop.
- [x] Keep the classic recording state for this tranche so natural stop behavior stays intact.
- [x] On stop, flush the input-demo file to `input_demo_recordings/new/<auto-name>.dximdemo` and delete or ignore the companion classic temp `.dem` if this was an Android quick input-demo recording.
- [x] Preserve existing keyboard F5 behavior outside the new Android quick path.
- [x] Trim `input_demo_recordings/new/` to the newest 10 entries after a successful flush.
- [x] Stop Android quick recording on normal and secret level exits instead of pausing into the next level.

Validation:

- [x] `run-windows-build.ps1 -Target d1`
- [x] `run-windows-build.ps1 -Target d2`
- [x] From `android/`, `./gradlew.bat :app:externalNativeBuildDebug --no-daemon`
- [ ] Manual device check: bind the new action, start a level, tap once to start recording, then either tap again or finish the level and verify the single `.dximdemo` file exists, stops before the next level, and ends with a result trailer.

### Phase 2: Touch And Controller UI Binding

Goal: make start/stop recording selectable in the existing extra button UI for touch and physical controllers.

Tasks:

- [x] Add the label `Demo Recording` to `TouchBindings.META_BUTTON_LABELS`.
- [x] Keep it out of `D2_ONLY_META_ACTIONS` so it is available for both games.
- Confirm touch button, long-press, radial segment, and controller picker paths display it through their existing `META_BUTTON_LABELS` plumbing.
- Confirm `controller_config.json` persists the action in `meta_bindings` and `MainActivity.loadMetaBindings()` dispatches it for physical controllers.

Validation:

- Add or extend a small launcher unit test around controller config meta binding serialization if the existing test structure has a nearby fit.
- Manual launcher check: action appears in touch extra buttons and controller extra functions for D1 and D2.
- Manual device check: touch and controller bindings both toggle recording.

### Phase 3: Launcher Recorded Demo Manager

Goal: show newly recorded demos on the Advanced page and provide Save and Add to Game actions.

Tasks:

- [x] Add an `InputDemoManager.kt` helper in the launcher for listing staged `.dximdemo` files across `d1x-redux` and `d2x-redux`.
- [x] Parse only the first header line for display: game, mission, level, frame count, modified time, and total size. Do not duplicate gameplay validation in Kotlin.
- [x] Add a new Advanced page section for `Newly Recorded Demos` using the log/crash row style.
- [x] Add `Save` to copy the `.dximdemo` file through cache, then write it to Downloads. Use `application/octet-stream` rather than `text/plain`.
- [x] Add `Add to Game` to prompt for a clean user filename, then copy the file to `<active file set>/demos/<name>.dximdemo` for the demo's game. After a successful copy, remove the staged copy or mark it no longer new.
- [x] Add `Delete` or `Delete All` for staged recordings so the limit is not the only cleanup path.
- [x] Reuse `LauncherFileCopy` for progress where possible. Keep file copy helpers narrow rather than widening unrelated import code.

Validation:

- [x] Launcher unit test for listing, trim/list ordering, Save, and Add to Game copy using temp directories.
- Manual Advanced page check with one D1 and one D2 staged demo file.
- Save a `.dximdemo` file to Downloads and verify the host can read its header and trailer lines.
- Add a demo to the active file set and verify it appears under that set's `demos/` directory.

### Phase 4: Host Replay Wrapper For Real Recordings

Goal: point a script at a recorded `.dximdemo` file and get a pass/fail result from the Windows build.

Tasks:

- Add a reusable PowerShell runner, likely `android/tests/run_input_demo_replay.ps1`, with parameters for `-DemoPath`, `-Game auto|d1|d2`, `-DataDir`, `-TimeoutSeconds`, and `-KeepSandbox`.
- Accept a `.dximdemo` file path. Directory and zip compatibility are intentionally out of scope for new recordings.
- Read the first header line to auto-select D1 or D2, then reuse the smoke runner's sandbox, local windowed config, `-hogdir`, `-nomusic`, `-nosound`, and `ProcessStartInfo` pattern.
- Wait for `<demo-file>.actual.json`, kill the process after result creation, and rely on the engine-side comparison against the embedded trailer. The wrapper can also parse the trailer for clearer host-side messages.
- Refactor `test_input_demo_runtime_smoke.ps1` to call the wrapper or share a small helper module so the launch behavior stays consistent.

Validation:

- Existing smoke fixture passes through the wrapper for both games.
- A device-recorded exported `.dximdemo` replays through the Windows build.
- Failure output includes the sandbox path and repro command.

### Phase 5: Staged Headless Runner

Goal: provide a script with the same interface as the Windows replay wrapper, then grow engine support from windowed replay to no-render and finally true headless.

Tasks:

- Add `android/tests/run_input_demo_headless.ps1` with the same input handling as the Windows wrapper.
- Initially route to the current fast windowed/no-sound replay path unless a native no-render flag is available. The script should clearly report when true headless is not available.
- Add a native replay flag such as `-inputdemo-norender` in a later tranche. Keep this minimal: still initialize SDL/OpenGL if required, but skip expensive per-frame rendering and flips while running game simulation and writing `result.actual.json`.
- Only after no-render is stable, investigate true no-window/no-GPU support. This likely needs deeper arch/SDL and OpenGL initialization changes and should not block real-device demo collection.

Validation:

- Wrapper accepts the same `.dximdemo` input as the Windows replay wrapper.
- No-render mode, when added, passes the same D1/D2 smoke fixtures and at least one real device recording.
- Timeouts fail with a clear message and clean up any child process.

### Phase 6: In-Game Input Demo Library

Goal: make `Add to Game` meaningful from inside the game, not just from host scripts.

Tasks:

- Extend D1/D2 demo listing minimally to recognize `.dximdemo` files under `demos/` alongside classic `.dem` files, or add a separate `Input Demos` list if mixing formats creates too much UI risk.
- Selecting an input demo should load the single demo file, use the existing replay setup path, skip intros/briefings, write `<demo-file>.actual.json`, and compare against the embedded trailer when playback finishes.
- Keep classic `.dem` playback untouched. Input-demo playback should be clearly separated in code paths even if it appears near the existing demo menu.
- Avoid parsing fixture details in Kotlin. The launcher only installs the artifact; C/C++ owns replay validation and mission loading.

Validation:

- Add-to-game recording appears in the in-game input-demo list for its own game only.
- Selecting it plays back without requiring keyboard input.
- Existing classic `.dem` listing and playback still work.

### Phase 7: Regression Test Body Workflow

Goal: turn real-device recordings into repeatable tests.

Tasks:

- Add a documented workflow for recording on device, saving/exporting, copying to host, and running the wrapper.
- Add a `temp` based import script for local experiments and a committed fixture location only after recordings are stable enough to keep.
- Extend result JSON in focused increments as the first real regressions demand more assertions.
- Keep long recordings out of git until they are known to be stable and useful.

Validation:

- Record one short D1 and one short D2 demo on real hardware.
- Export each through Advanced.
- Replay both through the Windows wrapper.
- Re-run both after a clean Windows build.

## Risks And Constraints

- The recording toggle should not inject F5 because that would inherit classic demo prompt behavior.
- The input-demo artifact should remain a plain single file. Archive compatibility should not be added unless a future concrete need appears.
- Trimming must not delete installed demos under `<active file set>/demos/`; only staged new recordings are capped at 10.
- Full headless mode is a larger graphics/bootstrap project. The first useful runner can be fast windowed replay with reliable result capture.
- Any D1/D2 source edits should be mirrored carefully and kept narrow. New reusable code should live under `android/app/src/main/cpp/shared` or launcher Kotlin where possible.

## Final Validation Pass For The Full Tranche

- `android/stop-stale-formatters.ps1`
- `android/run-code-quality.ps1 -Fix`
- `run-windows-build.ps1 -Target d1`
- `run-windows-build.ps1 -Target d2`
- From `android/`, `./gradlew.bat :app:externalNativeBuildDebug --no-daemon`
- `android/tests/test_input_demo_runtime_smoke.ps1 -Game both`
- New replay wrapper against at least one exported `.dximdemo`
- Manual real-device recording and Advanced page export/add-to-game check

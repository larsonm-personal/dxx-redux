# Touch More Gyro Light Follow-up - 2026-05-12

## Goal
Refine the Android touch overlay so the More menu shows better state for bomb switching, excludes launcher exit, hides gyro toggle when gyro is not configured, fixes gyro toggle persistence semantics, and highlights the light control while the headlight is active.

## Plan
1. [x] Create this plan file
2. [x] Inspect current More-menu label generation, gyro toggle handling, and light button drawing after recent edits
3. [x] Remove launcher exit from More and show current bomb type in the Toggle Bomb label
4. [x] Hide gyro on/off unless gyro is configured and fix the in-game toggle semantics so it does not persist incorrectly
5. [x] Highlight the light control while the headlight is currently on, including auto-off cases
6. [x] Update focused tests and run validation

## Notes
- Keep this scoped to Android Kotlin unless the investigation shows a missing native state hook.
- Preserve unrelated current worktree edits.
- More now omits `META_RETURN_TO_LAUNCHER`, only adds `META_GYRO_TOGGLE` when `layout.gyro.enabled`, and shows `Toggle Bomb [current: ...]` from the actual native `which_bomb()` state.
- Shared weapon polling was extended in `jni_main.c` and `WeaponState.kt` to include `currentBomb`; the existing `playerFlags` field also drives headlight-on highlighting via `PLAYER_FLAGS_HEADLIGHT_ON`.
- Gyro toggle semantics now keep the persisted touch-layout config as the base "configured yes/no" state and track the temporary in-game on/off state separately in `MainActivity`.
- Touch buttons bound to headlight now use the existing green latched fill while the headlight is actually on, so low-energy auto-off is reflected automatically on the next weapon-state poll.
- Validation: `:app:testDebugUnitTest --tests com.dxxredux.app.RemainingKeyTouchActionsTest --tests com.dxxredux.app.GyroToggleConfigTest` passed.
- Validation: `:app:buildCMakeDebug[arm64-v8a]` and `:app:buildCMakeDebug[arm64-v8a]-2` passed, compiling the shared JNI change for both games.
- Validation: scoped ktlint passed for `TouchOverlayView.kt`, `MainActivity.kt`, and `WeaponState.kt`.
- Validation: `git diff --check` passed for the follow-up files, and VS Code diagnostics report no errors in the changed Kotlin/test files.
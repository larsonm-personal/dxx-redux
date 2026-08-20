# Guide-Bot Destroyed Redeploy Plan

## Goal
Return the Guide-Bot touch control to its undeployed state when the active Guide-Bot is destroyed, allow a replacement to be deployed, and consistently label the recall command `Warp to Me`.

## Tasks
- [x] Read repository instructions and locate the Guide-Bot control, deployment, and recall label paths.
- [x] Trace destruction state from the engine through JNI to the touch overlay.
- [x] Update the control state and any remaining `Warp Me` labels.
- [x] Add or extend focused regression coverage.
- [x] Run scoped formatting, tests, build verification, and `git diff --check`.
- [x] Record implementation and validation results here.

## Implementation
- Added a live Guide-Bot check that rejects destroyed, exploding, and pending-deletion companion objects.
- Changed the JNI deployment signal to require both the existing released flag and a live Guide-Bot. The touch Guide control therefore returns to its deploy ring after destruction, while an intact caged Guide-Bot remains undeployed.
- Updated the Advanced and Claw preset labels to `Warp to Me`.
- Bumped the touch layout version to 11 and added a migration that renames the recall slice in existing saved layouts.
- Added a focused migration regression test for the old `Warp Me` label.

## Validation
- Scoped `android/run-code-quality.ps1 -Fix` passed.
- `GuidebotLockedWheelTest` passed 11 tests and `GyroToggleConfigTest` passed 9 tests with zero failures.
- Android `assembleDebug` produced `app-debug.apk`.
- `run-windows-build.ps1 -Target d2` completed successfully.
- All 40 tests in the D2 Windows CTest suite passed.
- `git diff --check` passed.

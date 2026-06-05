# Launcher Autoselect Newest Pilot Mismatch Plan - 2026-06-04

## Goal
When the launcher weapon autoselect editor opens:

- Show the ordering from the most recently modified pilot file for the selected game.
- Enable Save immediately if valid pilot files for that game disagree on ordering.
- Show a launcher note when pilot configs are mismatched.
- Keep Save inactive on first open when all pilot files match.

## Key Files

- `android/app/src/main/cpp/android_autoselect.cpp`
  - Currently reads the first matching pilot file.
  - Already writes the shown ordering to every pilot file for the game.
  - Best home for newest-pilot selection and mismatch detection so Kotlin does not learn pilot formats.

- `android/app/src/main/java/com/dxxredux/app/NativeAutoselectPatcher.kt`
  - JNI declarations and dispatcher wrappers.
  - Add a summary return type for launcher use.

- `android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt`
  - Load newest summary instead of first-pilot data.
  - Display mismatch note and keep Save active when mismatch is detected.

- `android/tests/test_autoselect_plx.ps1`
  - Existing integration coverage for autoselect pilot files.
  - Extend if feasible after native API lands.

## Work Items

- [x] Add this plan.
- [x] Add native summary read for D1 and D2.
- [x] Add Kotlin summary wrapper.
- [x] Update launcher editor load/save state and mismatch note.
- [x] Add pure Kotlin coverage for mismatch-driven Save enablement.
- [x] Run focused quality/build/tests and update this status.

## Verification

- [x] `android/run-code-quality.ps1 -Fix -Paths @(...)`
- [x] `android/gradlew.bat :app:assembleDebug`
- [x] `git diff --check`
- [ ] `android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.AutoselectReorderScrollTest`
  - Blocked before running this test by unrelated compile errors in `ResumeSavePanelTest.kt` for missing `difficultyChanged`, `difficultyMin`, and `difficultyMax` parameters.

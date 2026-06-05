# Launcher Autoselect Save Only Mismatch Plan - 2026-06-04

## Goal
Keep the launcher autoselect Save button inactive unless the native scan reports at least one pilot file ordering mismatch for the active game.

Switching between Descent 1 and Descent 2, resetting visible list state, or other page-local twiddling should not activate Save by itself.

## Work Items

- [x] Add this plan.
- [x] Change Save enablement to depend only on detected pilot mismatch.
- [x] Clear stale mismatch/save state immediately when switching games.
- [x] Update focused helper tests.
- [x] Run focused quality/build checks.
- [x] Update status.

## Verification

- [x] `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt','android/app/src/test/java/com/dxxredux/app/AutoselectReorderScrollTest.kt','android/ai tool plans/launcher automation, editor, scripting/plan_autoselect_save_only_mismatch_20260604.md')`
- [x] `android/gradlew.bat :app:assembleDebug`
- [ ] `android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.AutoselectReorderScrollTest`
  - Blocked before running this test by unrelated compile errors in `ResumeSavePanelTest.kt` for missing `difficultyChanged`, `difficultyMin`, and `difficultyMax` parameters.

# Launcher Autoselect Save Condition From Shown Order - 2026-06-04

## Goal
Start over on the launcher autoselect Save condition:

- The launcher has a shown ordering for Descent 1 and Descent 2.
- Save is active iff at least one pilot file in either game differs from that game's shown launcher ordering.
- Reordering visible items should activate Save when pilot files no longer match the shown ordering.
- Switching between games should not activate Save by itself.

## Work Items

- [x] Add this plan.
- [x] Add native mismatch-count helpers that compare pilot files against supplied order arrays.
- [x] Track shown ordering and mismatch counts for both games in the launcher page.
- [x] Make Save write every game whose pilot files differ from its shown ordering.
- [x] Update focused helper tests.
- [x] Run focused quality/build checks and update status.

## Status

- Save state is now derived only from D1/D2 mismatch counts against the orderings currently shown or retained by the launcher.
- Visible reorders update the active game's shown ordering and immediately recount pilot mismatches against that exact ordering.
- Game switching first preserves the active shown ordering, then loads the target game's retained shown ordering; switching by itself does not create a mismatch.
- Save writes every game whose pilot files differ from its shown ordering, then clears that game's mismatch count.

## Verification

- [x] `android/run-code-quality.ps1 -Fix` on the touched launcher, JNI, test, and plan files.
- [x] `./gradlew.bat :app:assembleDebug`
- [ ] `./gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.AutoselectReorderScrollTest`
  - Blocked before running the focused test by existing compile errors in `ResumeSavePanelTest.kt` for missing `difficultyChanged`, `difficultyMin`, and `difficultyMax` parameters.

# Merged wall experiment test compile plan

## Goal
- Fix the unit-test compilation failure where `SettingsChildOverlayControllerTest.kt` references unresolved `MERGED_WALL_EXPERIMENT`.
- Preserve any unrelated dirty workspace changes.

## Steps
- [x] Locate the unresolved test reference and intended graphics preference constant.
- [x] Patch the missing or stale symbol using the existing settings naming pattern.
- [x] Run scoped formatting for touched files.
- [x] Run the focused unit test that was previously blocked.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths ...` passed for the touched test and plan.
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ResumeSavePanelTest` passed.
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.SettingsChildOverlayControllerTest` passed.

## Notes
- The current `VideoInfoControllerAction` list has `MERGED_WALL` and `MERGED_WALL_TAP`, but no `MERGED_WALL_EXPERIMENT`.
- Native automation/introspection no longer exposes `merged_wall_experiment` or `merged_wall_force_two_pass`; stale script references remain outside this compile fix.

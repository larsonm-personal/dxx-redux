# Test suite cleanup 2026-06-20

## Goal
- Implement the accepted cleanup items from the runtime survey.
- Collapse the duplicate autosave resume coverage.
- Remove or merge debug-era graphics probe coverage.
- Resolve the stage-script standalone metadata issue without dropping useful coverage by accident.

## Plan
- [x] Re-read project instructions and inspect the autosave scripts.
- [x] Update autosave coverage so one script covers ordinary resume and missing-pilot resume.
- [x] Remove redundant/debug-only top-level scripts.
- [x] Audit stage-script standalone metadata and choose explicit suite behavior.
- [x] Run targeted validation or static checks and record results.

## Result
- Folded `test_autosave_resume_unified.json5` into `test_autosave_resume_missing_pilot_unified.json5`.
- Removed debug-only or superseded scripts:
  - `test_autosave_resume_unified.json5`
  - `test_door45_pose_repro.json5`
  - `test_controls_bottom_stage_d2.json5`
  - `test_kconfig_keyboard_stage_d2.json5`
- Added `test_controls_readability_d2.json5` as an explicit top-level owner for useful controls and kconfig readability coverage that had been sitting in stage-style files.
- Removed the stale timeout override for the deleted autosave test.

## Validation
- `test_controls_readability_d2.json5` passed on emulator with D2.
- `test_autosave_resume_missing_pilot_unified.json5` passed on emulator with D2.
- `test_autosave_resume_missing_pilot_unified.json5` passed on emulator with D1.
- Parsed `android/run_all_tests.ps1` with the PowerShell AST parser.
- Confirmed no stale references remain for the deleted script names under `android/run_all_tests.ps1`, `android/tests`, or `android/game_scripts`.
- Ran scoped code quality over the touched paths; checks passed.

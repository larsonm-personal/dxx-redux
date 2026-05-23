# Plan: Test Report 2026-05-19 23:25:20

## Scope

- `test_abort_game_to_main_menu_d2`
- `test_autosave_resume_missing_pilot_unified`
- `test_autosave_resume_unified`
- `test_quick_record_classic_sidecar_install`

## Initial Hypotheses

- The three resume-related failures share a launcher automation handoff problem after `enter_launcher`, not a missing resume candidate. The common failure point is the resumed `enter_launcher` yield/continue path.
- The sidecar install failure is likely a test-order dependency because the install test ran before `test_quick_record_classic_sidecar_stage`, which is the test that stages the demo.

## Work Plan

### Phase 1: Trace launcher handoff path

- Inspect `run_test.ps1` result-file monitoring around `LAUNCHER_CONTINUE`
- Inspect `SetupActivity` and `LauncherScriptExecutor` resume flow after returning from game
- Confirm whether the runner incorrectly restarts or fails while launcher automation is already continuing in-process

### Phase 2: Trace staged demo install path

- Inspect the quick-record stage/install tests and their ordering assumptions
- Inspect launcher `install_staged_demo` handling and staged-demo discovery
- Decide whether to make install self-contained or encode explicit suite ordering

### Phase 3: Implement minimal fixes

- Apply the smallest runner or launcher fix that resolves the shared resume failures
- Apply the smallest test/harness fix that removes the staged-demo order dependency

### Phase 4: Validate

- Run the narrowest available checks for the touched slices first
- Re-run the four failing tests if targeted checks pass
- Mark completed phases here before finishing

## Status

- [x] Captured report details and narrowed the failure clusters
- [x] Trace launcher handoff path
- [x] Trace staged demo install path
- [x] Implement fixes
- [x] Run targeted validations

## Completed Work

- Fixed the shared launcher resume regression in `SetupActivity.kt` so the resume offer survives return-to-launcher flow and resumes the correct game/save.
- Fixed the remaining D1-only autosave resume failure in `d1/main/state.c` by restoring the pre-object single-player block in the same order it was saved and by initializing `ObjectStartLocation` before object loading.
- Removed the temporary D1 restore breadcrumbs after validation and revalidated the cleaned D1 path.
- Fixed `android/run_all_tests.ps1` ordering so `test_quick_record_classic_sidecar_stage` runs before `test_quick_record_classic_sidecar_install`.

## Validation Results

- `android/run_test.ps1 -ScriptName test_autosave_resume_unified.json5 -Game d1 -Install` passed after the D1 restore fix and again after removing the temporary breadcrumbs.
- `android/run_all_tests.ps1 -Filter 'test_quick_record_classic_sidecar*' -TestTimeoutSeconds 300` passed with `stage` before `install`.
- `android/run_test.ps1 -ScriptName test_autosave_resume_unified.json5` passed.
- `android/run_test.ps1 -ScriptName test_autosave_resume_missing_pilot_unified.json5` passed.
- `android/run_test.ps1 -ScriptName test_abort_game_to_main_menu_d2.json5 -Game d2` passed.
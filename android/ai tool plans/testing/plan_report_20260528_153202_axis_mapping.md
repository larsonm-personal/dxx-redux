# Plan: report 20260528 153202 axis mapping

## Goal
Fix or harden the remaining `test_axis_mapping` failure from `temp/test_reports/report_20260528_153202.md` without broad launcher or engine churn.

## Steps
- [x] Read the report, captured output, and axis mapping test script.
- [x] Identify whether the failure is controller config state, launcher automation timing, emulator input behavior, or app code.
- [x] Patch the narrowest script or Android-side fix.
- [x] Run focused validation for `test_axis_mapping`.
- [x] Run formatting/build checks needed for touched files.
- [x] Record final results and remaining risk here.

## Notes
- Keep existing controller focus policy intact unless the failure shows a real launcher navigation regression.
- Prefer harness readiness fixes if the test is asserting before controller mappings are persisted or visible.
- D1 failed after the help-menu path with `axis_bind_bank == 6` but `bank_time == 0`, while D2 passed the same axis phase.
- Patched Android joystick axis handling in both engine copies so repeated nonzero held-axis events are not dropped before `kconfig.c` can rescale them with the current `FrameTime`.
- Validation passed: `android/run-code-quality.ps1 -Fix`, `android/gradlew.bat -p android :app:assembleDebug`, `android/helpers/run_test.ps1 -ScriptName test_axis_mapping.json5 -Install`, and `run-windows-build.ps1 -Target both`.
- Focused automation result: D1 and D2 both passed `test_axis_mapping`; D1 Phase 5a reported `bank_time != 0` after axis 6.
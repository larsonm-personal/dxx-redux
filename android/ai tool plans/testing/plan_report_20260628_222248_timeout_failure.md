# Investigate report 20260628_222248 timeout and failure

## Goal
- Identify the timed-out and failed tests in `report_20260628_222248.md`
- Determine whether the failures point to real product regressions or harness fragility
- Apply robust fixes when the cause is in the test harness or reporting layer

## Plan
- [completed] Read project instructions and inspect the report plus referenced logs
- [completed] Trace the timeout and failure to their test scripts or helpers
- [completed] Patch robust fixes where appropriate
- [completed] Run focused validation and scoped code quality
- [completed] Record final verification notes here

## Findings
- `test_autosave_resume_missing_pilot_unified` timed out in the top-level runner at 300s while its child launcher test runner had selected a 600s per-game monitor timeout
- The script runs both D1 and D2, so the top-level timeout must cover both child runs plus setup overhead
- The report's `Failed: 1` and `Timeouts: 1` referred to the same timed-out row, not a separate failing test row

## Fix
- JSON5 tests now pass an explicit child timeout to `run_test.ps1`
- The top-level JSON5 process timeout is now child timeout plus setup overhead, multiplied by the script game count
- Timeout rows no longer increment the separate failed-test counter, but still make the suite exit nonzero

## Verification
- Parser check passed for `android/run_all_tests.ps1`
- `android/run_all_tests.ps1 -Filter "__NO_MATCH__"` completed test discovery and report generation
- Timeout calculation probe for `test_autosave_resume_missing_pilot_unified`: child=600, gameCount=2, parent=1410
- Focused top-level run passed: `android/run_all_tests.ps1 -Filter "test_autosave_resume_missing_pilot_unified" -ReportDir .\temp\test_reports_timeout_focus`
- Focused report: `temp\test_reports_timeout_focus\report_20260628_233054.md`
- Scoped `android/run-code-quality.ps1 -Fix -Paths ...` passed

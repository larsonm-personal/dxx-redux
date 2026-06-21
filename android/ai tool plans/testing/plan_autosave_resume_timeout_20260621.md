# Autosave resume timeout 2026-06-21

## Goal
- Investigate `test_autosave_resume_missing_pilot_unified` from `report_20260621_122914.md`.
- Find a durable fix for the suite failure without timeout churn.

## Plan
- [x] Read project instructions and load the report.
- [x] Inspect the full per-test log and owning automation script.
- [x] Identify whether the failure is app behavior, launcher resume detection, or test script state.
- [x] Apply a focused fix if the root cause is clear.
- [x] Run targeted validation and record results.

## Notes
- The D1 leg reached `resume_offer_enabled = true` and validated the expected resume candidate.
- Failure happened later in `assert_button` because `Load Last Save` was not visible to the accessibility scan.
- Button search only scrolled downward, so a launcher left below the resume panel could keep moving away from the target.
- Added shared upward scrolling plus a top reset before downward button scans.
- Validation passed:
  - `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\java\com\dxxredux\app\SetupAutomationApi.kt','android\app\src\main\java\com\dxxredux\app\LauncherScriptExecutor.kt','android\app\src\main\java\com\dxxredux\app\SetupActivity.kt')`
  - `.\gradlew.bat :app:assembleDebug`
  - `.\android\helpers\run_test.ps1 test_autosave_resume_missing_pilot_unified.json5 -Game d1 -TimeoutSeconds 420`
  - `.\android\helpers\run_test.ps1 test_autosave_resume_missing_pilot_unified.json5 -Game d2 -TimeoutSeconds 420`

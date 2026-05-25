# SetupActivity resume panel split - 2026-05-24

## Goal
Continue reducing `SetupActivity.kt` by moving resume-save thumbnail decoding, display formatting, launch path/callsign helpers, and the resume panel composable into a focused `SetupResumePanel.kt` module.

## Plan
- [x] Confirm resume helper call sites and dependencies.
- [x] Create `SetupResumePanel.kt` in package `com.dxxredux.app`.
- [x] Remove moved declarations from `SetupActivity.kt` and clean imports if safe.
- [x] Run focused diagnostics, JVM tests, and scoped code-quality checks.
- [x] Update this plan and the survey plan with results.

## Notes
- Keep the activity's resume candidate state and launch orchestration in `SetupActivity.kt`.
- Move only the pure formatting/path helpers, thumbnail decode, and `ResumeSavePanel` UI.

## Results
- Added `SetupResumePanel.kt` with resume thumbnail decoding, display formatting, launch path/callsign resolution, resume log summary, and `ResumeSavePanel`.
- Kept resume candidate state, refresh triggers, and launch orchestration in `SetupActivity.kt`.
- Validation passed with focused diagnostics, `:app:testDebugUnitTest`, and scoped `android\run-code-quality.ps1 -Fix -Paths ...`.

# SetupActivity file-import split - 2026-05-24

## Goal
Continue reducing `SetupActivity.kt` by moving SAF scanning, generic file import, hashing, temp cleanup, and archive extraction helpers into a focused `SetupFileImport.kt` module.

## Plan
- [x] Add a short Java/JAVA_HOME validation note to `.github/copilot-instructions.md`.
- [x] Inspect current import/helper dependencies in `SetupActivity.kt`.
- [x] Create `SetupFileImport.kt` for import models and helper functions.
- [x] Remove moved declarations from `SetupActivity.kt` and adjust imports/visibility.
- [x] Run focused diagnostics, Kotlin/JVM tests, and scoped code-quality checks.
- [x] Update this plan with results.

## Notes
- Keep declarations in package `com.dxxredux.app`.
- Avoid moving import dialogs, launcher UI state, setup receivers, or disc/config helper blocks in this tranche.
- Prefer behavior-preserving movement over renaming or restructuring.

## Results
- Added `SetupFileImport.kt` with SAF scanning, generic file import, archive extraction, hashing, temp cleanup, and copy-progress helpers.
- Reduced `SetupActivity.kt` from 9398 lines after the first split to 8816 lines before the next tranche.
- Validation passed with focused diagnostics, `:app:testDebugUnitTest`, and scoped `android\run-code-quality.ps1 -Fix -Paths ...`.

# SetupActivity config and disc helper split - 2026-05-24

## Goal
Continue reducing `SetupActivity.kt` by moving config file helpers and disc/GOG/SOW import helper functions into focused modules without changing launcher UI behavior.

## Plan
- [x] Inspect config and disc helper dependencies.
- [x] Create `SetupConfigFiles.kt` for Descent config read/write helpers.
- [x] Create `SetupDiscImport.kt` for GOG/SOW/CUE/ISO/disc helper functions.
- [x] Remove moved declarations from `SetupActivity.kt` and clean imports/visibility.
- [x] Run focused diagnostics, JVM tests, and scoped code-quality checks.
- [x] Update this plan with results.

## Notes
- Keep declarations in package `com.dxxredux.app`.
- Avoid moving the Compose import dialogs in this tranche; only move their backing helper functions.

## Results
- Added `SetupConfigFiles.kt` with Descent config read/write helpers, redbook config setup, and launch music config writing.
- Added `SetupDiscImport.kt` with GOG pair detection/registration, SOW extraction, nested file hoisting, CUE image ordering, ISO/CUE import, and merged SAF audio staging helpers.
- Renamed the moved `leafName` helper to `discLeafName` to avoid colliding with `LauncherFileLabels.kt`.
- Reduced `SetupActivity.kt` to 8144 lines. Current extracted modules are `SetupGameFiles.kt` at 293 lines, `SetupFileImport.kt` at 561 lines, `SetupConfigFiles.kt` at 144 lines, and `SetupDiscImport.kt` at 512 lines.
- Validation passed with focused diagnostics, `:app:testDebugUnitTest`, and scoped `android\run-code-quality.ps1 -Fix -Paths ...`.

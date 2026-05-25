# SetupActivity game-file split - 2026-05-24

## Goal
Begin the Kotlin refactor work by moving the game-file catalog and readiness helpers out of `SetupActivity.kt` into a focused module, without changing launcher behavior.

## Plan
- [x] Inspect current `SetupActivity.kt` game-file and readiness helper dependencies.
- [x] Create `SetupGameFiles.kt` with catalog data, status models, and readiness helpers.
- [x] Remove the moved declarations from `SetupActivity.kt` and adjust visibility/imports as needed.
- [x] Run focused Kotlin/JVM validation.
- [x] Run formatter/code-quality if needed and update this plan with results.

## Notes
- This tranche should stay in package `com.dxxredux.app` to avoid package churn.
- Do not move activity receivers, import dialogs, or JNI-facing behavior in this tranche.
- Keep behavior-preserving moves separate from later import/disc/config splits.

## Results
- Added `SetupGameFiles.kt` for game-file catalog data, file status models, readiness checks, file lookup, setup file-dump helpers, recommended mod metadata, and demo download metadata.
- Removed the moved declarations from `SetupActivity.kt`, reducing it by 337 lines before formatting.
- Kept SAF directory scanning, URI import, archive extraction, dialogs, and activity receiver wiring in `SetupActivity.kt` for later tranches.

## Validation
- `get_errors` on `SetupActivity.kt` and `SetupGameFiles.kt`: no errors.
- `Push-Location android; .\gradlew.bat :app:testDebugUnitTest; Pop-Location`: passed after setting `JAVA_HOME=C:\local\jdk-21` in the terminal session.
- `.\android\run-code-quality.ps1 -Fix -Paths 'android\app\src\main\java\com\dxxredux\app\SetupActivity.kt'`: passed.
- `.\android\run-code-quality.ps1 -Fix -Paths 'android\app\src\main\java\com\dxxredux\app\SetupGameFiles.kt'`: passed.
- Full `.\android\run-code-quality.ps1 --fix` was attempted first but this script expects `-Fix`; that full run also reported an unrelated UTF-8 BOM in `android\tests\extract_regression_spec_helpers.ps1`, so this tranche used scoped checks to avoid mutating that unrelated file.

## Next tranche suggestion
Move SAF scanning, generic file import, hashing, temp cleanup, and archive extraction helpers from `SetupActivity.kt` into a focused `SetupFileImport.kt` module.

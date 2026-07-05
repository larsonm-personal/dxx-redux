# Metadata Resume Invalid Station Type Plan

## Goal
Find and fix the launcher metadata-browser crash: `Error: Invalid station type 48 in fuelcen.c` after analyzing `descent2.hog`, minimizing, and resuming.

## Tasks
- [x] Read project instructions and inspect the fuel center error path.
- [x] Trace metadata analysis lifecycle and resume/re-entry behavior.
- [x] Identify whether fuel-center state, level globals, or PhysFS/runtime init are reused incorrectly.
- [x] Patch the narrowest reset/lifecycle/error-detail path that explains the crash.
- [x] Run focused host/native tests or add a regression if feasible.
- [x] Update this plan with findings and validation.

## Notes
- User saw the first analysis succeed, then resume produced the crash.
- Suspect stale global level/fuel-center state across repeated metadata analysis or app lifecycle re-entry.
- `LevelMetadataDialog` was keyed on `refreshTrigger`, so SetupActivity resume could relaunch the same metadata scan just because the launcher refreshed.
- The D1/D2 metadata service starts a new thread for every request. Native scanner state is process-global, so overlapping D2 scans can corrupt level globals and plausibly produce bogus `Segment2s[].special` values like station type `48`.
- Fixed by keying the dialog only on the selected target and adding a process-local single-flight guard around native metadata analysis.
- Validation: `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\java\com\dxxredux\app\LevelMetadata.kt','android\app\src\main\java\com\dxxredux\app\SetupSections.kt','android\app\src\test\java\com\dxxredux\app\LevelMetadataAnalysisSingleFlightTest.kt')` passed.
- Validation: `.\android\gradlew.bat -p android testDebugUnitTest --tests com.dxxredux.app.LevelMetadataAnalysisSingleFlightTest` passed.

# Metadata Crash Context Plan

## Goal
Add enough crash-report context to metadata analysis failures to identify which file or mission was being read when native analysis crashes.

## Tasks
- [x] Read project instructions and inspect existing Android crash/error reporting hooks.
- [x] Trace metadata request fields available at the Kotlin/native boundary.
- [x] Add narrow crash context with source name, source path, mission, level file, and game where available.
- [x] Validate with formatter and a focused build/test if feasible.
- [x] Update this plan with findings and validation.

## Notes
- Android native crash reports already include `crash_breadcrumb` output.
- Added `source_path` and `archive_path` to the metadata request JSON so direct files like `descent2.hog` can be named even when analyzed as a built-in mission.
- Added native breadcrumbs for the overall metadata request and for each level file before `load_level()`.
- Expected breadcrumbs look like `levelmeta begin game=d2 type=mission file=descent2.hog` and `levelmeta level n=7 file=level07.rl2 source=descent2.hog`.
- Validation: `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\java\com\dxxredux\app\LevelMetadata.kt','android\app\src\main\cpp\jni_level_metadata.cpp','android\app\src\test\java\com\dxxredux\app\LevelMetadataAnalysisSingleFlightTest.kt','android\ai tool plans\metadata_crash_context_20260705.md')` passed.
- Validation: `.\android\gradlew.bat -p android :app:externalNativeBuildDebug` passed.
- Validation: `.\android\gradlew.bat -p android testDebugUnitTest --tests com.dxxredux.app.LevelMetadataAnalysisSingleFlightTest` passed.

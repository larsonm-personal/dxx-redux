# Level metadata implementation

## Goal
- Add the Android launcher foundation for level metadata viewing
- Keep risky native level analysis out of the launcher process
- Start with safe request/result plumbing and UI, then attach the native analyzer

## Plan
- [x] Add launcher data model and safe result parsing for level metadata
- [x] Add isolated worker service and manifest entry
- [x] Add native bridge entry point with structured JSON result and debug crash/hang hooks
- [x] Add dialog/table UI and buttons in direct-file and mission ZIP preview paths
- [x] Add first-pass file eligibility and preflight/staging helpers
- [x] Run scoped formatting/build validation
- [x] Add HOG-only custom mission fallback by enumerating level entries when no descriptor is available
- [x] Re-run scoped formatting/build validation after fallback work

## Completed
- Added `LevelMetadata.kt` with:
  - request/target/result models
  - ZIP staging with file count and size caps
  - `LevelMetadataAnalysisService` in a separate `:levelmeta` process
  - polling, timeout handling, worker-process kill, checkpoint/crash diagnostics
  - native library dispatch for D1 or D2
- Added `jni_level_metadata.cpp` to both Android game libraries
  - initializes a headless engine runtime in the worker process
  - mounts the base data directory and optional staged mission directory
  - loads engine-known missions or single level files
  - returns JSON rows with level number, name, robots, hostages, secrets, matcens, and energy centers
  - writes checkpoint JSON before risky stages
  - includes debug crash/hang source types for later containment tests
- Wired the unified `LevelMetadataDialog` into:
  - direct file preview for HOG/MSN/MN2/RDL/RL2/SDL/SL2
  - top-level mission ZIP mod details
  - mission ZIP constituent details
- Updated HOG content summaries so known base HOGs show:
  - D1: 27 normal, 3 secret, 30 total
  - D2: 24 normal, 6 secret, 30 total
- Added HOG-only fallback for direct and ZIP-constituent HOGs:
  - Kotlin passes normal and secret level filenames from parsed HOG contents
  - Native mounts the exact staged/direct HOG in the isolated worker process
  - Results preserve HOG entry order, number normal levels from 1, and number secret levels from S1
- Updated top-level mission ZIP metadata analysis to scan parsed descriptor level files directly from staged files/HOGs.
- Added a generic top-level ZIP probe so descriptorless ZIP packages with level files or HOGs still get the metadata button.

## Validation
- `.\android\run-code-quality.ps1 -Fix -Paths <changed files>`
- `.\gradlew.bat :app:compileDebugKotlin`
- `.\gradlew.bat :app:externalNativeBuildDebug`
- Re-ran scoped formatting after HOG fallback edits.
- Re-ran `.\gradlew.bat :app:compileDebugKotlin` and `.\gradlew.bat :app:externalNativeBuildDebug` from `android\`.

## Follow-Up
- Add an emulator integration test for opening the dialog on base HOGs.
- Add debug-only crash and hang containment tests against `:levelmeta`.

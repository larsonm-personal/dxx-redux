# Fix BR-0429 Bounded Configuration Import

## Goal

Bound configuration document reads before materialization and keep provider I/O plus JSON parsing off the Android main thread across every supported configuration-import entry point.

## Plan

- [x] Inspect BR-0429, all import entry points, lifecycle ownership, provider-read helpers, and existing tests
- [x] Design one reusable bounded import loader with explicit oversize and malformed-input results
- [x] Add focused tests for exact limits, oversize streams, short and zero-length reads, provider failures, malformed JSON, and worker-thread execution
- [x] Integrate every configuration-import entry point without changing successful confirmation and publication behavior
- [x] Run scoped formatting, focused and full tests, Android builds, native CMake validation, and whitespace checks
- [x] Record the tested resolution, move BR-0429 to the done ledger, and verify ledger integrity

## Validation

- Focused `ConfigImportLoaderTest`: 6 tests passed
- Full `:app:testDebugUnitTest`: 540 tests, 0 failures, 0 errors, 1 skipped
- `:app:assembleDebug`: passed
- Native CMake debug builds: passed for arm64-v8a, armeabi-v7a, and x86_64
- Scoped code quality: passed for all six changed production Kotlin files
- Direct ktlint: passed for `ConfigImportLoaderTest.kt`
- Entry-point audit: Advanced Settings, Touch Editor, Controller Editor, and SetupActivity all launch lifecycle-owned coroutines before importing
- SetupActivity audit: selection stores `PreparedConfigImport`; confirmation does not reopen or reparse the provider
- BR-0429 finding count: active 0, done 1; disposition-log count: 1
- Duplicate finding IDs across both ledgers: 0
- Pending resolutions in the done ledger: 0
- `git diff --check`: passed

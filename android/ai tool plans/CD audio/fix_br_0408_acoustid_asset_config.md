# Fix BR-0408 AcoustID Android Asset Configuration

## Goal

Connect the documented local AcoustID configuration to the Android asset consumed by `AcoustIdClient`, without committing credentials or silently packaging a placeholder.

## Plan

- [x] Inspect BR-0408, documented setup, Gradle asset packaging, and runtime key loading
- [x] Define one local configuration contract with explicit missing and malformed behavior
- [x] Add focused packaging and runtime regressions
- [x] Implement the configuration-to-asset connection
- [x] Run scoped quality checks, focused and full tests, Android builds, native CMake validation, and whitespace checks
- [x] Record the tested resolution, move BR-0408 to the done ledger, and verify ledger integrity

## Validation

- Valid, missing, malformed, placeholder, and stale-output Gradle packaging checks passed
- Runtime JSON5 parsing and key validation tests passed
- Full Android JVM suite passed: 546 tests, 0 failures, 1 skipped
- Configured and unconfigured debug APKs and the configured internal APK built successfully
- Configured debug and internal APK assets exactly matched the maintained key without printing it
- Release asset merging passed and exactly matched the maintained key
- Full release APK assembly was blocked by the unrelated existing `d1/3d/interp.c` release-only `nest_count` compile error
- All three debug native ABIs, scoped code quality, direct test-source ktlint, PowerShell parser checks, and `git diff --check` passed

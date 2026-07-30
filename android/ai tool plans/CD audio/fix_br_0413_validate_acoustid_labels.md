# Fix BR-0413 AcoustID Label Validation

## Goal

Accept AcoustID-derived album and track labels only when they agree with maintained source metadata, preserving deterministic local labels when remote metadata is incomplete or mismatched.

## Plan

- [x] Inspect BR-0413, the AcoustID response path, maintained disc metadata, persistence consumers, and existing tests
- [x] Define a small validation policy tied to the maintained source metadata
- [x] Add focused regressions for matching, incomplete, reordered, and mismatched remote labels
- [x] Apply validation before any Android AcoustID label is displayed or persisted
- [x] Run scoped formatting, focused and full tests, Android builds, native CMake validation, and whitespace checks
- [x] Record the tested resolution, move BR-0413 to the done ledger, and verify ledger integrity

## Validation

- Focused AcoustID selection and cache persistence tests passed
- Full Android JVM suite passed: 544 tests, 0 failures, 1 skipped
- Debug APK and CMake builds passed for arm64-v8a, armeabi-v7a, and x86_64
- Scoped code quality, direct test-source ktlint, PowerShell parser checks, and `git diff --check` passed
- The known false Torche label is absent from both maintained Vampyro metadata files

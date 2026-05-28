# Android Test Gitignore Audit Plan

## Goal
Determine whether Android JVM test files are hidden by ignore rules, make any needed gitignore changes, and identify other test files that should become trackable.

## Steps
- [x] Check whether `ResumeSavePanelTest.kt` is ignored
- [x] Inspect relevant ignore files and ignored test listings
- [x] Update gitignore rules only if an ignore rule is hiding source tests
- [x] List other untracked or ignored Android test files that may need to be added
- [x] Run focused checks for any edited ignore files

## Result
No gitignore edit is needed. `ResumeSavePanelTest.kt` is addable as `??`, not ignored. The same pattern applies to two other Android JVM tests: `LauncherFocusPolicyTest.kt` and `LobbyDiagnosticsTest.kt`. Ignored test-like files found by broad scans were generated build/dependency outputs under `android/tests/build` or `android/app/.cxx`; after excluding generated roots, there were no ignored source test candidates.

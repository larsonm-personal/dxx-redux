# BR-0043 fingerprint enumeration failure

## Goal

Make audio fingerprinting fail closed when directory enumeration is incomplete,
so partial scans cannot produce apparently valid database updates.

## Plan

- [x] Create the required implementation plan
- [x] Read the complete finding and trace enumeration and publication paths
- [x] Implement checked, fail-closed directory traversal
- [x] Add focused incomplete-enumeration and valid-scan coverage
- [x] Run scoped quality, focused tests, native suites, and platform builds
- [x] Record the resolution and move BR-0043 to the done ledger

## Verification

- Scoped C, CMake, and Android PowerShell quality passed; all changed PowerShell files parsed successfully
- `android/tests/test_fingerprint_audio_enumeration.ps1` passed readable-empty, missing, non-directory, Unicode, injected mid-read, exact-limit, one-over, and exact-result-set cases
- `android/tests/test_fingerprint_mission_zip_budgets.ps1` passed
- All 19 tests registered by `android/tests/test_cue_iso.ps1` passed
- `run-windows-build.ps1` completed the D1 and D2 host builds
- JDK 21 `:app:assembleDebug` built arm64-v8a, armeabi-v7a, and x86_64 successfully
- POSIX runtime execution was unavailable in the configured environment

# BR-0038 canonical fingerprint threshold

## Goal

Make every duplicate-matching surface consume the validated finite threshold
from the canonical fingerprint configuration and fail closed when that value is
missing or malformed.

## Plan

- [x] Read repository instructions and the complete finding
- [x] Trace threshold ownership through host tools, scripts, JNI, and Kotlin
- [x] Remove independent fallback values and enforce strict finite parsing
- [x] Add focused threshold boundary and configuration-failure coverage
- [x] Run scoped quality, focused tests, native suites, and Android builds
- [x] Record the resolution and move BR-0038 to the done ledger

## Verification

- Scoped code quality passed for the changed C, CMake, Kotlin, and PowerShell files
- `android/tests/test_fingerprint_threshold.ps1` passed the canonical, 0.40, 0.65, omitted, missing, malformed, trailing-junk, infinity, and NaN matrix
- `FingerprintMatchingConfigTest` and the full `:app:testDebugUnitTest` suite passed
- `chromaprint_db_config_tests` passed and all 18 tests registered by `android/tests/test_cue_iso.ps1` passed
- `run-windows-build.ps1` completed the D1 and D2 host builds
- JDK 21 `:app:assembleDebug` built arm64-v8a, armeabi-v7a, and x86_64 successfully

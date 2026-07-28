# BR-0041 extension policy ownership

## Goal

Establish one authoritative extension policy shared by native and Kotlin
consumers, with tests that detect drift and preserve the intended import and
classification behavior.

## Plan

- [x] Create the required implementation plan
- [x] Read the complete finding and trace every extension-policy consumer
- [x] Select and implement the smallest single-owner contract
- [x] Add focused policy-parity and behavior coverage
- [x] Run scoped quality, focused tests, native suites, and platform builds
- [x] Record the resolution and move BR-0041 to the done ledger

## Verification

- Scoped C, CMake, and Kotlin quality passed
- The focused parity and behavior suite passed against the final policy
- Removing one native game extension made the parity test fail as intended
- The full `:app:testDebugUnitTest` suite passed
- All 19 tests registered by `android/tests/test_cue_iso.ps1` passed
- `run-windows-build.ps1` completed the D1 and D2 host builds
- JDK 21 `:app:assembleDebug` built arm64-v8a, armeabi-v7a, and x86_64 successfully

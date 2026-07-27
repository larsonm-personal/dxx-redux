# BR-0198 bound every HMP track event read before MIDI conversion

## Goal

Reject truncated or malformed HMP track events before any event byte, variable
length quantity, payload, or running-status state is read during MIDI
conversion.

## Plan

- [x] Read repository instructions and the complete BR-0198 finding
- [x] Compare the frozen and live HMP track parser, conversion callers, and
      malformed-input coverage
- [x] Define bounded event-cursor and fail-closed conversion semantics
- [x] Implement the smallest parser fix and add truncated-event regressions
- [x] Run scoped code quality, focused HMP tests, native suites, and Android
      ABI builds
- [x] Finalize BR-0198 and move its complete finding and disposition entry to
      the done ledger

## Verification

- Scoped code quality passed for all touched C, header, CMake, ledger, and plan
  paths
- Focused warning-enabled `hmp_android_shared_tests` passed
- All 15 tests in `android/tests/test_cue_iso.ps1` passed
- `run-windows-build.ps1 -Target both` built D1 and D2 successfully
- `:app:assembleDebug` with JDK 21 built arm64-v8a, armeabi-v7a, and x86_64
- AddressSanitizer could not link because the installed MSVC toolchain lacks
  its runtime library; WSL had no runnable compiler for sanitizer fallback
- No retail HMP asset was present in the repository for a retail hash oracle

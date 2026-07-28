# BR-0065 JNI string boundary conversion

## Goal

Convert Java and native strings correctly across the affected JNI boundaries,
including malformed input, supplementary Unicode, embedded nulls, ownership,
and pending exceptions.

## Plan

- [x] Read repository instructions, review process, and the complete finding
- [x] Trace every affected Java-to-native and native-to-Java string boundary
- [x] Implement one bounded conversion policy and regression coverage
- [x] Run scoped code quality, focused tests, native suites, and Android builds
- [x] Record the resolution and move BR-0065 to the done ledger

## Verification

- Strict codec corpus passed for ASCII, BMP, non-BMP, embedded nulls, malformed
  surrogates, overlong and truncated UTF-8, isolated continuation bytes,
  out-of-range scalars, round trips, and capacity failures
- Scoped C, C++, CMake, and BOM quality passed
- `android/tests/test_cue_iso.ps1`: all 16 native suites passed
- JDK 21 `:app:assembleDebug`: passed for arm64-v8a, armeabi-v7a, and x86_64
- Final APK installed successfully on the configured emulator
- `test_music_track_controls_unified.json5 -Game d2`: all 42 steps passed
  with no CheckJNI error

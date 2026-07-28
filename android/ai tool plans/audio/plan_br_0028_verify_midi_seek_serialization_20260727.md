# BR-0028 verify MIDI seek serialization

## Goal

Independently verify that MIDI seek, rendering, state snapshots, ring reset,
and teardown are serialized without introducing lifecycle deadlocks.

## Plan

- [x] Read repository instructions, the complete BR-0028 finding, and P1
      verification requirements
- [x] Trace every live MIDI preview state access and its lock ordering
- [x] Review and extend synchronization regression coverage if gaps remain
- [x] Run scoped code quality, focused tests, native suites, and Android ABI
      builds
- [x] Record the independent verification and move BR-0028 to the done ledger

## Verification

- Frozen-source tracing confirmed that rendering raced slider seek and the
  100 ms state poll
- Live-source tracing confirmed one acyclic control, playback, ring-reset,
  callback, and teardown lock graph
- `python android/tests/test_midi_preview_sync.py`: 7 passed, before and after
  scoped formatting
- Scoped `android/run-code-quality.ps1 -Fix`: passed
- `android/tests/test_cue_iso.ps1`: all 15 native suites passed
- JDK 21 `:app:assembleDebug`: rebuilt the APK with arm64-v8a, armeabi-v7a,
  and x86_64 D1 and D2 libraries
- ThreadSanitizer and a maintained on-device rapid-seek stress run were not
  available

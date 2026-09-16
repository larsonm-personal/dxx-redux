# Phone sound provenance logging

## Request

Prepare targeted logging for a reproducible unexpected D2 Counterstrike robot
sound on the user's phone, in fresh and restored levels

## Plan

- [x] Capture sound-bank source, sample offsets/names, read results, and loaded hashes
- [x] Log playback fingerprints and mixer-cache fingerprints with mission/robot mapping context
- [x] Bound logging and reuse launcher-exported Game logs; preserve playback behavior
- [x] Check corresponding D1 hooks and keep desktop builds unaffected
- [x] Format scoped changes, compile Android libraries, and verify diagnostics with focused tests
- [x] Document phone reproduction/export instructions and remaining limitations

## Implementation

Shared Android diagnostics capture bank-load and mixer-conversion snapshots,
then compare them on playback when Game Logs is enabled. D2 also reports opened
HAM, sound-header, HXM, extra-HAM, and HAM-patch origins. D1 has equivalent mixer
hooks and PIG-backed load snapshots; external/custom replacement provenance is
explicitly incomplete. No resource reload behavior was changed

## Verification

- Scoped code quality checks passed
- Android assembleDebug succeeded for ARM64, ARMv7, and x86_64
- Windows D1 and D2 builds succeeded; existing weapon.c return-path warnings
  remain outside this change
- test_sound_trace_fingerprint passed in both native host CTest configurations
- Stock Counterstrike emulator trace captured bank/load/cache matches and IT
  Droid mapping (robot 37, see 59:53 and attack 60:54)
- Corrected the smoke script's input key to the supported lctrl spelling
- Final smoke run passed all 23 steps and the log assertions; the actual
  launcher-shared debug file also contains bank, robot mapping, and cache evidence
- Stopped the test emulator after verification
- Phone symptom reproduction remains the user's next step

Collection instructions: [SOUND_TRACE.md](../SOUND_TRACE.md)

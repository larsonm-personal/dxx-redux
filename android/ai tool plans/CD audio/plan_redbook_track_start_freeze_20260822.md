# Redbook track start freeze

## Goal

Prevent Android gameplay stalls when a new CD audio track from an extracted
disc image starts, while preserving Redbook playback, looping, seeking, and
diagnostics behavior for D1 and D2.

## Plan

- [x] Trace and instrument the synchronous track-start path to identify the
  expensive work and its thread ownership
- [x] Design and implement a bounded background preparation path with safe
  cancellation and lifecycle handling
- [x] Add focused regression coverage for track transitions and background
  preparation state
- [x] Run scoped formatting, native tests, Windows CMake builds, and the
  relevant Android build or integration test
- [x] Record results without editing the protected outstanding bug list

## Constraints

- Keep the implementation in shared Android code where possible
- Do not change desktop Redbook behavior
- Do not modify or overwrite unrelated working-tree changes

## Results

- The producer thread uses Android nice level 5 and yields for 1 ms after each
  2048-frame render chunk, preventing the initial 2.7-second ring-buffer fill
  from monopolizing CPU and storage bandwidth
- Sector refill checks producer cancellation before each read, bounding the
  synchronous wait when a playing track is replaced or stopped
- Introspection reports producer priority, track-start request time, producer
  stop wait time, and time to the first buffered audio; debug game logs report
  the same track-start and first-buffer timings
- `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
- `test_saf_redbook.ps1` passed 62 of 62 game-side steps after its pilot cleanup
  was made deterministic; it covered playback, multi-track transitions,
  pause/resume, stop/restart, data-track skipping, and I/O failure behavior
- Emulator diagnostics reported producer nice 5, 2 ms to first buffer, 4 ms
  for the latest start request, and 0 ms waiting for the previous producer
- Scoped code quality passed; D1 and D2 Windows builds passed; all 43 D2 CTest
  tests passed and the D1 configuration contained no CTest tests
- `android/outstanding_bugs.md` retains its original unchecked entry because
  the file explicitly directs Codex not to edit it

# Input Demo Phase 4 Replay Session Tranche

## Goal

Add the first shared replay-session layer that can read a recorded single-player
demo file and expand its sparse frame records back into exact per-frame replay
data.

This tranche should:

- read one `.dximdemo` file from disk
- parse header, interleaved input/RNG frame records, and result trailer
- expand sparse input updates into exact per-frame replay state
- expose a simple current-frame plus advance API for later engine hooks

This tranche does not yet add D1/D2 runtime replay CLI wiring, level restore,
or final result comparison.

## Constraints

- Keep the loader and sparse-stream policy in `android/app/src/main/cpp/shared`
- Reuse the existing shared controls and fixture helpers rather than re-parsing
  the file formats in D1/D2 code
- Keep scope to single-player stream 0 for now
- Validate with focused host tests before any engine hook work

## Planned Steps

- [x] Add single-file read/parse support to the shared fixture helper
- [x] Add a shared replay-session helper that loads and expands demo frames
- [x] Add a focused host test that round-trips a recorded fixture through the replay loader
- [x] Wire the new replay host test into the D1 and D2 host graphs
- [x] Run focused host validation

## Progress on 2026-04-25

- Added single-file read/parse support in `input_demo_fixture.h/cpp` so shared
  code can now read `.dximdemo` files back into `input_demo_file`
- Added `input_demo_replay.h/cpp` under the shared helper tree as a singleton
  single-player replay session that loads one demo file, expands sparse input
  updates and RNG records into exact per-frame replay data, and exposes current-frame plus
  advance accessors for later engine hooks
- Added `android/tests/test_input_demo_replay.cpp` to round-trip a freshly
  recorded sparse fixture through the replay loader and verify exact frame
  timing, held input state, pulse input, RNG state, and call-count restoration
- Wired the new replay host test into `d1/maths/CMakeLists.txt` and
  `d2/maths/CMakeLists.txt`
- Validation completed after formatting:
  - `test_input_demo_replay` passed in both `buildd1` and `buildd2`
  - Android `:app:externalNativeBuildDebug` passed with `JAVA_HOME` pinned to
    `c:\local\jdk-21`

## Exit Criteria

- A recorded single-player demo can be loaded from one `.dximdemo` file
- The replay helper exposes exact per-frame `FrameTime`, controls, and RNG state
- The host test proves sparse input and RNG streams expand back into the expected frames
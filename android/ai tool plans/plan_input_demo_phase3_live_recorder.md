# Input Demo Phase 3 Live Recorder Tranche

## Goal

Hook the first live single-player recorder path into the engine using the new
shared helpers.

This tranche should:

- start a recorder session when classic demo recording starts
- capture frame-start controls and RNG data once per game frame
- flush `demo.json5`, `inputs.p0.jsonl`, and `rng.p0.jsonl` to a temp unpacked
  fixture directory when recording stops

This tranche does not add replay, CLI launch modes, or final `result.json`
serialization yet.

## Constraints

- Mirror engine hook edits in both `d1/` and `d2/`
- Keep most logic in `android/app/src/main/cpp/shared`
- Reuse the existing `input_demo_controls` and `input_demo_fixture` helpers
- Use `newdemo_start_recording()` / `newdemo_stop_recording()` as the initial
  lifecycle surface instead of inventing new UI or commands first
- Validate with focused host tests before wider builds

## Planned Steps

- [x] Add a shared recorder session helper that buffers control and RNG frames
- [x] Add a focused host test for start/capture/flush behavior
- [x] Link the new helper into D1/D2 desktop and Android targets
- [x] Hook recorder start/stop into `newdemo.c` in D1 and D2
- [x] Hook per-frame capture into `GameProcessFrame()` in D1 and D2
- [x] Run focused host validation and compile validation for D1/D2 plus Android

## Progress on 2026-04-26

- Added `input_demo_recorder.h/cpp` under the shared Android-side helper tree
  to buffer frame-start control and RNG snapshots, coalesce them with the
  existing JSON helpers, and flush `demo.json5`, `inputs.p0.jsonl`,
  `rng.p0.jsonl`, and a minimal `result.json`
- Added `android/tests/test_input_demo_recorder.cpp` and wired it into the D1
  and D2 host test graphs
- Wired the new shared recorder helper into the desktop D1/D2 main targets and
  Android native targets
- Hooked `newdemo_start_recording()` / `newdemo_stop_recording()` in D1 and D2
  to start and flush the recorder session
- Hooked the top of `GameProcessFrame()` in D1 and D2 to capture the mapped
  `Controls` snapshot plus frame-start RNG state once per gameplay frame
- Gated live recording to single-player fresh-level starts only for now, so the
  emitted metadata can truthfully stay on `start_mode: "new_level"`
- Validation completed after formatting:
  - `test_input_demo_recorder` passed in both `buildd1` and `buildd2`
  - desktop `dxx-redux-d1` and `dxx-redux-d2` builds passed
  - Android `:app:externalNativeBuildDebug` passed with `JAVA_HOME` pinned to
    `c:\local\jdk-21`

## Exit Criteria

- Starting classic demo recording also starts an input-demo recorder session
- Each game frame captures frame-start control and RNG snapshots exactly once
- Stopping classic demo recording flushes a temp unpacked fixture directory with
  `demo.json5`, `inputs.p0.jsonl`, and `rng.p0.jsonl`
- D1/D2 host tests pass and desktop plus Android builds still pass

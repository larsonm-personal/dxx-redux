# Input Demo Phase 5 Result Serializer Tranche

## Goal

Add the first shared final-result serializer for deterministic input demos and
use it from the D2 replay runtime.

This tranche should:

- add a shared `input_demo_result` helper under `android/app/src/main/cpp/shared`
- keep the helper pure-data so host probes can validate JSON layout without engine globals
- reuse the helper for recorder-written embedded result trailers
- write a richer actual replay result file from the runtime when replay completes

This tranche does not yet add a fixture diff script or full D1 runtime result
capture.

## Constraints

- keep JSON format and ordering in the shared helper, not in D2 gameplay code
- keep D2 engine edits local to collecting existing gameplay state into the
  shared result structure
- start with fields that already have clean source-of-truth access: top-level
  replay metadata, player summary, position, and a small level summary
- validate with a focused host probe before wider D2/Android builds

## Planned Steps

- [x] Add shared result serializer helper and focused host test
- [x] Reuse the helper from recorder flush
- [x] Expose replay result output paths from the shared replay session
- [x] Write D2 actual replay result on replay completion
- [x] Run focused D2 and Android validation

## Exit Criteria

- Shared code can write stable result JSON text from a pure-data summary
- Recorder still writes a valid embedded result trailer
- D2 replay writes a richer actual result file when a replay finishes

## Progress on 2026-04-25

- added `input_demo_result.h/cpp` under the shared helper tree with a C-safe pure-data summary struct and stable JSON writer for the first result slice
- added `android/tests/test_input_demo_result.cpp` and wired it into the D1 and D2 host graphs to lock the stable key order, sparse ammo maps, and pretty-print layout
- updated `input_demo_recorder.cpp` to route its existing minimal baseline through the shared result helper so recorder trailers use the same source of truth
- extended `input_demo_replay.h/cpp` to retain the embedded baseline result and an actual output path in the replay session, with `<demo-file>.actual.json` written beside the demo
- updated D2 `game.c` so replay completion captures current player stats, position, `GameTime64`, and a small level summary into the shared result struct and writes the actual result before the replay window closes
- desktop validation needed one local repair after the first build because the new result struct reused `level` for both the numeric level id and the nested level summary; renaming the nested field to `level_summary` fixed the collision
- desktop validation also needed a clean rebuild after a stale `buildd2` cache lost `PHYSFS_LIBRARY`; `run-windows-build.ps1 -Target d2 -Clean` restored the build graph
- validation passed after formatting with `run-windows-build.ps1 -Target d2`, `buildd2\maths\test_input_demo_result.exe`, `buildd2\maths\test_input_demo_recorder.exe`, `buildd2\maths\test_input_demo_replay.exe`, and `android\gradlew.bat :app:externalNativeBuildDebug --no-daemon`

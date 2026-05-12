# Demo 134049 RNG regression 2026-05-11

## Goal
- use the new failing D2 level 9 demo to find the first determinism break
- strongly test for an overzealous `_fx()` transition before widening scope
- check whether Phoenix-cannon-related RNG or call ordering is involved
- keep replay timing state centralized instead of adding replay-only branches to AI/gameplay logic

## Steps
- [completed] locate the new demo artifact and compare its replay traces to find the first meaningful divergence
- [completed] map the first divergence to the owning gameplay gate and confirm it is AI gun-point timeslicing keyed off `d_tick_count`
- [completed] use a temporary AI timeslice change to prove `d_tick_count` controlled the failure window
- [completed] step back from the temporary AI change and analyze whether `d_tick_count` can remain deterministic from initial tick state plus recorded `FrameTime`
- [completed] fix the central paused automap path that advances simulation d-tick while gameplay is not advancing, then remove the AI-local replay helper
- [completed] validate with D2 and D1 Windows builds plus the D2 and D1 input-demo runtime smoke tests
- [completed] rerun focused validation: D2 and D1 Windows builds passed, and replaying the old demo confirmed the expected stale-baseline mismatch shape

## Notes
- user guidance: prioritize determinism repair over retrospective narration
- likely suspicion: an overzealous `_fx()` transition, potentially on a Phoenix-adjacent path
- confirmed root cause candidate: `obj_ref = objnum ^ d_tick_count` flips the `!(obj_ref & 3)` gun-point visibility gate differently between recording and replay
- `d_tick_count` is not a fixed 25fps frame counter; it is a 20Hz accumulator driven by `FrameTime`
- replay supplies recorded `FrameTime`, so d-tick should be deterministic from the restored `game_d_tick_state` when only simulation frames advance it
- first tick mismatch in the old trace is frame 409: recorded automap advanced paused d-tick by 164 counts while replay suppressed the modal automap and kept normal gameplay state aligned
- preferred fix is to stop paused automap from mutating simulation d-tick, then restore the AI code to raw `d_tick_count`
- implemented in both D1 and D2: paused automap still assigns global `FrameTime` for automap input scaling, but no longer calls `calc_d_tick()`, and AI timeslice code is back to raw `d_tick_count`
- validation passed: `run-windows-build.ps1 -Target d2`, `run-windows-build.ps1 -Target d1`, `android/tests/test_input_demo_runtime_smoke.ps1 -Game d2`, and `android/tests/test_input_demo_runtime_smoke.ps1 -Game d1`
- because the original `.dximdemo` embedded expectations were recorded before this timing fix, a freshly recorded demo is now the meaningful determinism oracle

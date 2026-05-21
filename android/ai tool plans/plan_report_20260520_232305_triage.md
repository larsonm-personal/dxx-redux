# Plan: Test Report 2026-05-20 23:23:05 Triage

## Scope

- `test_input_demo_determinism_matrix`
- `test_input_demo_regressions`
- `test_input_demo_regressions_graphics`
- `test_input_demo_runtime_smoke`
- `test_death`

## Initial Hypotheses

- The four input-demo failures are one shared host-build regression, not four separate demo regressions. Both D1 and D2 Windows host builds stop in `arch/ogl/ogl.c` because Android-only cache profiling identifiers are referenced from code that still compiles on Windows.
- `test_death` is a separate D2-only timeout after the level has already loaded and the script is waiting on `player_dead`. Do not change the test to a substitute level. First distinguish between a stale/flaky run, a D2 `Player_is_dead` reporting issue, and a real gameplay/input-state regression on the existing level.

## Work Plan

### Phase 1: Fix the shared host-build break

- Inspect the exact `ogl.c` blocks using `stage_start`, `stage_end`, and cache profiling counters in both `d1` and `d2`
- Apply the smallest D1/D2-matched fix so Windows host builds no longer reference Android-only symbols outside the guarded path
- Run the narrowest host-build validation that falsifies the build hypothesis

### Phase 2: Triage `test_death` on the existing D2 path

- Inspect the D2 automation script and the `player_dead` wait path
- Check prior notes to see whether this failure is known to be stale/non-repro on fresh reruns
- Re-run the narrow D2 death test if needed to distinguish stale flake from current regression
- If it is current, inspect the D2 death/introspection path without changing the level selection

### Phase 3: Summarize fixes

- Map each failing test to the owning root cause
- Note the concrete code or harness fix needed for each one
- Mark completed phases here

## Status

- [x] Capture report details and separate failure clusters
- [x] Fix shared host-build break
- [x] Triage D2 `test_death` on existing level
- [x] Summarize per-test fixes

## Completed Work

- Fixed the shared Windows host-build regression in `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` by guarding Android-only cache profiling calls inside `ogl_loadbmtexture_f()`. The report's four input-demo failures all stopped at this same compile break before replay logic ran.
- Validated the host-build fix with `run-windows-build.ps1 -Target both`, which rebuilt D1 and D2 successfully.
- Re-ran `android/tests/test_input_demo_runtime_smoke.ps1 -Game both`; it passed for both D1 and D2 on the current build.
- Re-ran `android/tests/test_input_demo_regressions.ps1`; it passed all 11 committed regression demos.
- Re-ran `android/tests/test_input_demo_regressions_graphics.ps1`; it passed all 11 committed regression demos.
- Re-ran `android/tests/test_input_demo_determinism_matrix.ps1`; it completed with `MATRIX_EXIT: 0` and wrote results under `temp/input_demo_determinism_matrix/20260521_073017`.
- Re-ran `android/run_test.ps1 -ScriptName test_death.json5 -Game d2` without changing the script or level. It passed on the existing D2 level 24 and reached `player_dead=true` in about 18 seconds, so the report failure is stale or environment-sensitive rather than a reason to substitute a different level.

## Per-Test Fix Mapping

- `test_input_demo_determinism_matrix`: fixed by restoring successful Windows host builds; the direct rerun passed.
- `test_input_demo_regressions`: fixed by restoring successful Windows host builds; the direct rerun passed.
- `test_input_demo_regressions_graphics`: fixed by restoring successful Windows host builds; the direct rerun passed.
- `test_input_demo_runtime_smoke`: fixed by restoring successful Windows host builds; the direct rerun passed.
- `test_death`: no code or script change justified from this report. Keep the existing level and treat the suite result as stale or flaky until it reproduces again with current artifacts.
# D1 level 16 and 18 demo replay plan

Goal: verify the new D1 level 16 and level 18 input demos in plain D1, then run the same recordings as d1-in-d2 and fix reproducible desyncs where practical.

Demo files:

- `android/regression_demos/d1_descent_level16_20260618_201843.dem`
- `android/regression_demos/d1_descent_level16_20260618_201843.dximdemo`
- `android/regression_demos/d1_descent_level16_20260618_201843.dximdemo.rngtrace.jsonl`
- `android/regression_demos/d1_descent_level18_20260618_202117.dem`
- `android/regression_demos/d1_descent_level18_20260618_202117.dximdemo`
- `android/regression_demos/d1_descent_level18_20260618_202117.dximdemo.rngtrace.jsonl`

Plan:

1. [done] Verify all listed demo files exist and have sidecar RNG traces.
2. [done] Run level 16 and level 18 in plain D1 with strict result checks.
3. [done] If plain D1 fails, use state trace comparison to identify the first split and decide whether a focused engine fix is justified.
4. [done] Run level 16 and level 18 as d1-in-d2.
5. [done] If d1-in-d2 fails, separate D1 replay issues from d1-in-d2 translation/semantic mismatches.
6. [done] Re-run focused build, replay, and code-quality validation for any edits.

Notes:

- Fresh recordings should include the newer per-robot AI-static diagnostic arrays, so they should be more useful for `danger_laser` and AI-static splits than the earlier level 16 demo.
- All six listed files are present.
- Plain D1 strict replay passes for both fresh recordings.
  - Level 16 `d1_descent_level16_20260618_201843.dximdemo`: PASS.
  - Level 18 `d1_descent_level18_20260618_202117.dximdemo`: PASS.
- A stale D2 replay window was left alive from an earlier run under
  `temp/input_demo_runtime_wrapper/d2/d1_descent_level16_20260618_201843`.
  The replay runner now terminates replay processes in a `finally` block and
  clears sandbox-local stale D1/D2 processes before reusing a sandbox.
- D1-in-D2 semantic fixes landed in D2 only:
  - D1 homing missile turn scaling and homing lifetime timing.
  - D1 robot firing readiness, firing-state unflinch behavior, and gun wrapping.
  - D1 direct-fire and homing-branch weapon selection.
  - D1 Trainee damage and energy/shield pickup scaling.
  - D1 AI path-to-player targeting against the actual player segment.
- D1-in-D2 final replay checks now pass for both fresh recordings.
  - Level 16 `d1_descent_level16_20260618_201843.dximdemo`: PASS.
  - Level 18 `d1_descent_level18_20260618_202117.dximdemo`: PASS.
- A full state-trace comparison still reports frame-0 diagnostic-only noise in
  some expanded hash fields. The final-result checks are passing, so the next
  cleanup should distinguish meaningful per-frame divergence from these
  initialization diagnostics.

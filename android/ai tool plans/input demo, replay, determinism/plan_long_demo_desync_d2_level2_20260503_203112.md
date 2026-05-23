# Plan: Long Demo Desync Investigation D2 Level 2 2026-05-03 203112

## Goal
- Drive `android\temp_game_logs\newest_failing\d2_descent2_level2_20260503_203112.dximdemo` to its first replay desync
- Identify the first mismatching frame and the nearest controlling gameplay path for the first simulation fork

## Local Hypothesis
- The first replay fork is in player weapon fire at frame `1266`, not in guidebot, thief, matcen, or rendering
- The current best hypothesis is that replay reaches a different weapon-fire state by frame `1266`, so the same recorded input produces a spreadfire-linked `create_awareness_event()` burst on replay that the recording only accounts for with two RNG draws across that frame
- The cheapest discriminating check is to inspect the replay weapon-fire path at frame `1266` and confirm whether replay has diverged in selected weapon or in the local fire-timing state that controls `do_laser_firing_player()`

## Execution Plan
- Phase 1
  - run the long replay with `-TraceState -CompareStateTrace` to capture the first mismatch frame and mismatch type
- Phase 2
  - inspect the compare output around the first mismatch and correlate nearby frames with the nearest controlling gameplay path
- Phase 3
  - rerun with `-ReplayDebugLog` and targeted output capture to determine which subsystem diverges first
- Phase 4
  - step to the nearest controlling code path and decide whether the root cause is a replay control-application issue, weapon-selection drift, or weapon-fire timing drift

## Status (2026-05-04)
- Phase 1 completed
  - the first mismatch is reproducible at `frame=1267` with replay-side RNG mismatch `expected=2017763195 actual=2985398680`
- Phase 2 completed
  - same-run RNG tracing and awareness probes localize the first replay-only extra RNG draw to `d2/main/ai.c:create_awareness_event()` during frame `1266`
  - the replay-side source is `laser_player_fire` with `aux_obj=12`, which maps to `SPREADFIRE_ID`
- Phase 3 completed
  - the recorded `.dximdemo` uses per-frame JSONL rows with frame field `f`, not `frame`
  - the recorded frame rows around the fork are:
    - `f=1265`: `rng.c=47636`
    - `f=1266`: `input.s.f1s=4`, `input.p.f1=1`, `rng.c=47636`
    - `f=1267`: `rng.c=47638`
  - this means the recorded run consumed two RNG draws during frame `1266`, while replay consumes three
- Phase 4 completed
  - replay-side weapon probes confirm replay is still on spreadfire at frame `1266`, so the recording rather than replay application is where the live selected-weapon state is lost
  - the live Android debug log contains `Laser Cannon Level 4 selected!` near the fork, but the recorded demo contains no `p.sw` rows anywhere
  - root cause: `ReadControls()` calls `kconfig_read_controls()` and then immediately consumes pulse-style weapon and item controls through `do_weapon_n_item_stuff()`, so `Controls.select_weapon_count` can be spent before `GameProcessFrame()` reaches `input_demo_record_game_frame()`
  - fix implemented: stage pulse-style controls into the shared recorder from `d1/main/gamecntl.c` and `d2/main/gamecntl.c` right after `kconfig_read_controls()`, then merge the staged pulse into the next `input_demo_recorder_capture_frame()`
  - validation completed:
    - `buildd2\\maths\\test_input_demo_recorder.exe` now passes with a new staged-pulse regression check
    - `cmake --build buildd2 --target dxx-redux-d2-headless` succeeds
    - `cmake --build buildd1 --target dxx-redux-d1` succeeds
  - limitation: this explains the current long-demo desync, but it does not repair the already-recorded `.dximdemo`; the missing switch pulse has to be captured in a fresh recording
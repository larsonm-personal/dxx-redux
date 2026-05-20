## Goal

- refresh the tracked `d1/` and `d2/` branch delta against `main` on the `cmake` branch
- choose the highest-yield dedup or shared-helper extraction target from the current top churn files
- land one validated shrink slice from that top target instead of continuing ad hoc local trims

## Baseline

- local `main` is the merge base for `cmake`; `upstream/main` is ahead, but the current branch diverges from `main` at `fb555eec75e1ed12c8348805ab335afb4c721b06`
- current top tracked `d1/` and `d2/` deltas vs `main...HEAD` are led by `d2/main/input_demo_hooks.c`, `d1/d2 arch/ogl/ogl.c`, `d1/d2 main/state.c`, and `d1/d2 main/net_udp.c`
- `input_demo_hooks.c` is the biggest remaining paired sink file, and prior shrink work already established `android/app/src/main/cpp/shared/input_demo_hooks_shared.c` as the right home for shared replay and diagnostic helpers

## Plan

- [completed] extract the next shared `input_demo_hooks.c` replay or recording scaffolding block into `android/app/src/main/cpp/shared/input_demo_hooks_shared.c`
	- moved the identical `input_demo_record_game_frame` and `input_demo_update_rng_trace_context` bodies out of both sink files and into the existing shared implementation
	- moved the duplicated replay mismatch helper bodies into shared helper entry points and left only thin local glue where D2 still appends extra debug logging callbacks
- [completed] validate with Android native build and Windows host build
	- `android\gradlew.bat :app:externalNativeBuildDebug --console=plain` passed after a one-line follow-up include fix for `input_demo_rng_trace.h`
	- `run-windows-build.ps1 -Target both` passed
	- `android\run_quick_tests.ps1` passed with `Passed: 15  Failed: 0  Timeouts: 0  Skipped: 0`
	- `android\run-code-quality.ps1 -Fix -Paths @(...)` passed for the four touched files, followed by a final `:app:externalNativeBuildDebug` rerun
- [completed] update this plan with the exact shrink slice and resulting next-ranked follow-up candidate
	- second adjacent slice completed: moved the exact `input_demo_delay_replay_frame` body into a shared helper keyed by `input_demo_replay_last_timer_value`, and moved the exact `input_demo_step_replay_frame` wrapper into a shared helper that still delegates local `prepare` and `sync` callbacks
	- validation for the second slice passed on Android native build, Windows host build, scoped code quality, and the replay-targeted quick-suite coverage (`input_demo_rng_trace_compare`, `input_demo_state_trace_compare`, and both quick regression demo tests all passed)
	- third adjacent slice completed: moved the shared core of `input_demo_prepare_replay_frame` and `input_demo_advance_replay_frame` into `input_demo_hooks_shared.c`, leaving only tiny D2 hooks for kill-baseline refresh and post-final-frame debug logging
	- validation for the third slice passed on Android native build, Windows host build, scoped code quality, and a full quick-suite rerun with `Passed: 15  Failed: 0  Timeouts: 0  Skipped: 0`

## Next Candidate

- stay in `input_demo_hooks.c` for the next pass instead of re-opening a fresh diff survey
- the next adjacent shared slice is now the core of `input_demo_sync_replay_rng_to_current_frame` and possibly `input_demo_stop_replay`
- likely shape: keep the D2-only RNG mismatch and stop-result debug logging as tiny local callbacks, then move the common replay stop and RNG restore bodies into `input_demo_hooks_shared.c`
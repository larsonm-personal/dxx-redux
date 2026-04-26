# Input Demo Phase 2 Coalescer Tranche

## Goal

Add the recorder-side coalescer that turns per-frame portable control snapshots
into sparse `input_demo_control_record` runs.

This stays Android-first and buildable from host tests:

- no live recording hook yet
- no replay startup changes yet
- no Kotlin changes in this slice

## Constraints

- Reuse the existing shared `input_demo_controls` schema helper
- Use `nlohmann::json`, not hand-rolled JSON
- Keep D1/D2 edits minimal or avoid them entirely if possible
- Match the sparse control semantics already documented in `input-demo-schema.md`
- Keep validation cheap with host probes before any engine integration

## Planned Steps

- [x] Add a per-frame portable snapshot type and a shared coalescer API
- [x] Emit sparse records with stable `f/n/ft/s/p` behavior
- [x] Reject D2-only fields on D1 coalescing paths
- [x] Extend the existing host probe with coalescing-focused tests
- [x] Run Windows host validation and Android native validation

## Completed Notes

- Added `input_demo_control_frame` plus `input_demo_control_records_coalesce_frames()`
	to the shared helper so recorder-side code can build sparse records from
	contiguous per-frame snapshots.
- The coalescer now collapses unchanged runs into `n`, emits `s` only for actual
	held-state transitions, preserves explicit zero releases, and keeps one-frame
	pulse updates isolated so they do not smear across later frames.
- D1 coalescing paths now reject D2-only state/pulse fields early instead of
	silently dropping them.
- Extended `android/tests/test_input_demo_controls.cpp` with coalescing-focused
	cases for constant runs, pulse splits, explicit releases, frame-time changes,
	and D1/D2 policy behavior.

## Validation

- `android\stop-stale-formatters.ps1` reported no stale formatter tasks.
- `android\run-code-quality.ps1 -Fix` passed.
- `cmake --build buildd1 --target test_input_demo_controls` passed.
- `cmake --build buildd2 --target test_input_demo_controls` passed.
- `buildd1\maths\test_input_demo_controls.exe` passed.
- `buildd2\maths\test_input_demo_controls.exe` passed.
- `android\gradlew.bat :app:externalNativeBuildDebug --no-daemon` passed with
	the coalescer integrated into the Android native build.

## Exit Criteria

- Consecutive unchanged frames compress into `n` runs
- Held-state changes emit sparse `s` updates only when values change
- Pulse fields emit one-frame `p` updates and do not smear across runs
- D1 rejects D2-only inputs during coalescing
- Host tests and Android native build pass

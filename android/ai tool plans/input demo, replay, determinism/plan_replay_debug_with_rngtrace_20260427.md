# Replay Debug With RNG Trace 2026-04-27

## Goal

Reproduce the new D2 level 1 recorded demo on host, compare replay behavior against the recorded `.rngtrace.jsonl`, and narrow the first concrete mismatch.

## Planned Steps

- [x] Confirm the host replay wrapper path and current replay invocation surface.
- [x] Inspect the replay-side logging and RNG trace hooks to identify the cheapest discriminating comparison.
- [x] Run the new demo through the host replay wrapper and capture focused output.
- [x] Compare the recorded RNG trace against replay logs around the first mismatch frame.
- [x] Make the smallest rooted probe needed and rerun focused validation.
- [x] Broaden the AI and path probes so the next Android recording logs the owning robot and local decision state in addition to the RNG mismatch.

## Notes

- Demo: `android\temp_game_logs\d2_descent2_level1_20260427_231540.dximdemo`
- Trace: `android\temp_game_logs\d2_descent2_level1_20260427_231540.dximdemo.rngtrace.jsonl`
- Host replay still fails with `p0.s` expected 160 vs actual 159.
- First host boundary RNG mismatch is at frame 15, not frame 74.
- Recorded frame 14 consumes six `aipath.c:create_random_xlate` RNG calls after `cntrlcen.c:do_controlcen_frame`; host replay reaches frame 15 without any `create_path_points` call in that window.
- Replay-only probes show the companion is still following path point `1/3` at frames 14-15 and `do_escort_frame()` reports `visit=0`, so the missing frame-14 path burst is not the simple guidebot follow-path wrap case.
- Replay later prints `Finding REACTOR` at frame 26 while the recorded trace around frame 26 has no `aipath` activity, so escort or AI path-creation timing is still out of sync and needs another pass.
- New demo: `android\temp_game_logs\d2_descent2_level1_20260428_074105.dximdemo`
- New trace: `android\temp_game_logs\d2_descent2_level1_20260428_074105.dximdemo.rngtrace.jsonl`
- The new demo diverges earlier: recorded frame 9 consumes `cntrlcen.c:do_controlcen_frame` followed by six `aipath.c:create_random_xlate` calls and replay reaches frame 10 still at the post-reactor RNG state.
- Widening `aipath.c` probes to all objects on replay confirms the missing burst is not the buddy in host replay; the next device recording needs the same broader logging surface so the owning robot is visible directly in Android logs.

## Mine Exit Follow-up

- [x] Confirm the first mine-exit validation never reached replay because the host build failed with `LNK1168` on a stale `dxx-redux-d2.exe`.
- [x] Re-run `android\temp_game_logs\d2_descent2_level1_20260428_101150.dximdemo` after clearing the lock and confirm the current failure is a 300 second replay timeout with no `.actual.json`.
- [x] Change the replay mine-exit hook to terminate the replay session cleanly after sampling the pre-death result state.
- [x] Re-run the `101150` replay wrapper and resume the RNG and fire-state desync investigation once the runner exits cleanly again.

## Fire-State Follow-up

- [x] Verify the input-demo schema uses `s.f1s` and `p.f1` for primary fire rather than `s.f`.
- [x] Find the first held primary-fire segment in `101150` at frame 87 and the explicit release at frame 174.
- [x] Widen replay fire probes from the old 60-82 guess to the actual 86-174 spreadfire window.
- [x] Add a focused spreadfire emission probe in `do_laser_firing()` and rerun the host replay once.
- [x] Compare host spreadfire cadence against the recorded fire-state window before returning to the downstream frame-271 awareness mismatch.

## Late Weapon Lifetime Follow-up

- [x] Add a replay-only awareness-context probe for frames 260-272 so every surviving `create_awareness_event()` caller logs object ownership and type.
- [x] Add a replay-only player-owned weapon lifecycle probe for frames 260-272 and rerun the host replay once.
- [x] Use the late weapon and awareness logs to decide whether the frame-271 recording-only awareness event is downstream of player weapon lifetime or a different caller family.

## Expanded Evidence Pass

- [x] Switch user-visible host replay reruns for this investigation from accelerated mode to realtime mode.
- [x] Broaden the replay probes across spreadfire, awareness, player-owned weapon creation, and escort/path generation so one rerun captures all active suspects.
- [x] Re-run `android\tests\run_input_demo_replay.ps1` on `android\temp_game_logs\d2_descent2_level1_20260428_101150.dximdemo` in realtime with `-KeepSandbox` and compare the expanded logs against the recording trace.

## Demo-Agnostic Probe Cleanup

- [x] Remove committed frame-specific replay gates and one-off frame probes from the active D2 replay-debug files.
- [x] Keep only replay-wide or event-driven fire, awareness, path, collision, and motion instrumentation.
- [x] Rebuild D2 on Windows after the cleanup pass.
- [x] Re-run the host replay wrapper on `android\temp_game_logs\d2_descent2_level1_20260428_101150.dximdemo` in realtime and confirm the wrapper still passes.
- [x] Run `android\run-code-quality.ps1 -Fix` scoped to the touched D2 files and re-run the D2 Windows build.

## Updated Notes

- The latest host rerun proved replay has no `create_awareness_event()` calls and no player-owned weapon objects at all in frames 260-272.
- Replay spreadfire cadence exists through frame 172, but the next rerun needs direct player-weapon creation evidence and live weapon counts so the firing path is visible even when the shot is hard to see on screen.
- The tightest current RNG owner is the escort path rebuild at frame 265: replay enters from the same RNG state as recording, overshoots the recording burst by six calls, and lands at the state recording only reaches later in frame 266.
- The realtime rerun now logs replay-side spreadfire object creation directly: frame 87 creates three player-owned `SPREADFIRE_ID` weapon objects and the fire probe reports `live_spreadfire=3` immediately after `do_laser_firing_player()`.
- The frame-265 escort path rebuild now logs `seed_xlate=1`, `refresh_rolls=42`, and `refresh_xlate=10`, which accounts for 108 RNG calls total and matches the observed six-call overshoot if recording only hit the refresh shuffle branch nine times.
- The cleanup pass removed committed replay assumptions tied to specific frame numbers and left the durable replay logs keyed to replay presence, actual fire events, and actor/path activity instead.
- Post-cleanup validation succeeded: `run-windows-build.ps1 -Target d2` passes, the realtime host replay wrapper still reports `RESULT: PASS`, and the frame-265 escort/path overshoot remains the leading mismatch owner.
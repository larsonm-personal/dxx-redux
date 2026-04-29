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
- [ ] Change the replay mine-exit hook to terminate the replay session cleanly after sampling the pre-death result state.
- [ ] Re-run the `101150` replay wrapper and resume the RNG and fire-state desync investigation once the runner exits cleanly again.
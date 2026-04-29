# Replay Probe D2 20260428 191632

## Plan

- [x] Inspect the provided demo and RNG trace artifacts to identify their scope and format
- [x] Rebuild D2 host and replay the provided demo with the newest code
- [x] If replay diverges, compare host replay output against the provided RNG trace and isolate the first desync window
- [x] Summarize whether the newest build reproduces the issue and what the earliest concrete mismatch is

## Inputs

- `android/temp_game_logs/d2_descent2_level1_20260428_191632.dximdemo`
- `android/temp_game_logs/d2_descent2_level1_20260428_191632.dximdemo.rngtrace.jsonl`

## Notes

- Host replay reproduces on both accelerated and realtime runs with the same first boundary mismatch: frame 49, `gt=128451`, expected RNG state `2717118966`, actual `1381680294`
- The earliest host-only RNG owner is the guidebot (`obj=28`) in frame 48. Replay-only AI RNG probes show its `do_ai_frame()` moves RNG call count from `3` to `19`
- Of those 16 guidebot-owned calls in frame 48, 15 are the `create_path_to_player()` burst triggered by `time_to_visit_player()`, and the remaining 1 call is downstream fallout in the same buddy AI pass after entering `AIM_GOTO_PLAYER`
- The causal branch actually begins one frame earlier. In frame 47 the host guidebot advances from path `2/6` to `3/6` because `dist=415298` falls just under `threshold=426478`, which makes frame 48 satisfy the `time_to_visit_player()` half-path gate
- The recorded device RNG trace for frame 48 shows no matching buddy path burst, so the strongest remaining root cause is pre-RNG guidebot path-follow drift before frame 47 rather than a later RNG stream classification problem
- A fixed-point replacement for the companion goal-velocity nudge was tested and reverted because it produced no observable change on this artifact
- Useful durable probes were kept in `d2/main/object.c` and `d2/main/aipath.c` so future recorder/replay runs show per-robot AI RNG ownership and companion path-advance thresholds directly

## Spreadfire Render Follow-up

- [x] Confirm whether player-owned spreadfire objects reach `draw_object_blob()` during replay
- [x] If they do, compare their draw parameters against creation state to narrow bitmap, size, or blend issues
- [ ] Step outward from the draw call into the OGL upload path for `sprdblob`

## Spreadfire Render Findings

- Host replay reaches `draw_object_blob()` for player-owned spreadfire every frame with `bm=262` / `name=sprdblob`, valid object size, and on-screen projected coordinates
- The stock `sprdblob` bitmap is not empty on the CPU side: `64x64`, `opaque=2925`, bounding box `(2,1)-(62,61)`, brightest palette index `1` with RGB `(61,61,61)`
- Replay-only screenshots taken immediately after the logged draw calls still show no visible spreadfire projectiles in the engine window
- Replay-only render overrides that changed tint, blend mode, and depth testing did not change the screenshots, which strongly suggests the `sprdblob` GPU texture sample is effectively black or otherwise not contributing pixels despite the CPU bitmap being valid
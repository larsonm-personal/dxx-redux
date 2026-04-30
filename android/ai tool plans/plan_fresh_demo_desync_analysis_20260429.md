# Plan: fresh D2 demo desync analysis

Date: 2026-04-29

Goal: analyze the fresh non-`old/` demo artifacts in `android/temp_game_logs`, using the newly recorded classic-demo control trace where present, and narrow any record/replay desyncs before asking for more instrumentation.

## Phases

| phase | task | status |
|---|---|---|
| 1 | Inventory the fresh D2 L1/L2 artifacts and identify the matching `.dem`, `.dximdemo`, rng trace, and game logs. | completed |
| 2 | Dump each `.dem` to JSONL with `-classicdemo-dump-json` and verify whether `player.control.valid` is present. | completed |
| 3 | Correlate classic dump, input demo, rng trace, and replay log into TSV for both runs. | completed |
| 4 | Rerun the fresh input demos through the available replay path and capture new logs. | completed |
| 5 | Compare frame-by-frame onset windows, classify desyncs, and identify the narrowest plausible root cause. | completed |
| 6 | Define the next fix or instrumentation needed for a follow-up build. | completed |
| 7 | Record and restore guidebot escort runtime timestamps in input demo checkpoint/replay, then rerun L2. | in_progress |

## Notes

- Ignore `android/temp_game_logs/old/`.
- Do not interpolate or synthesize classic positions. Use only directly observed classic dump frames and directly observed replay states.
- Fresh version 16 `.dem` files expose record-side control data as `player.control` in the JSON dump and `classic_same_control_*` fields in the TSV after the dumper preserve-pending-trace fix.
- Dump results: `temp/l1_233432_classic.jsonl` has 293 frames and `temp/l2_233507_classic.jsonl` has 570 frames. Both dumps are version 16 and not truncated.
- Correlation outputs: `temp/l1_233432_compare.tsv` and `temp/l2_233507_compare.tsv`.
- Same-frame comparison appears to diverge early if the 20 Hz classic stream is treated as 25 Hz. Direct cadence-aware sampled scans show no sampled flight desync: L1 `sampled=291 bad=0 max_pos=1023 max_vel=0`, L2 `sampled=568 bad=0 max_pos=1023 max_vel=0`.
- L1 emulator input replay passed against the embedded terminal result.
- L2 emulator input replay completed but failed the terminal result: actual shields 174 vs expected 169, score 33600 vs 33800, segment 108 vs 110, position `-19509055,-3584333,-3444163` vs `-18108068,-2827687,-4193631`, robots_alive 62 vs 61, powerups_remaining 66 vs 64.
- L2 rerun root cause is narrowed to guidebot escort runtime state, not player flight physics. At frame 0, the recorded L2 log has `last_seen=1077412 last_player_path=899154 escort_last_path=1077412`, while the emulator replay has `last_seen=18350 last_player_path=144179 escort_last_path=1077412`.
- The bad initial guidebot timestamps matter at frame 213. The recording has `recent_path_gate=1 visit=0` and no guidebot path request. The replay has `recent_path_gate=0 visit=1`, runs `create_path_to_player` for obj 55, consumes RNG calls `0->14` from `1541453555` to `679045789`, and reports the first RNG state mismatch at frame 214.
- After accounting for the one-frame logging offset in motion rows, the first observed player-motion divergence between the original L2 log and the emulator rerun is original frame 372 vs rerun frame 371. That later divergence appears to be a consequence of the earlier guidebot/RNG branch, not the first cause.
- The checkpoint schema now records and restores guidebot escort state through the shared input-demo path. The recorded block currently includes `Buddy_allowed_to_talk`, `Buddy_last_seen_player`, `Buddy_last_player_path_created`, `Escort_last_path_created`, `Last_come_back_message_time`, and `Buddy_last_missile_time`.
- Host validation now covers the new checkpoint fields: `maths\test_input_demo_replay.exe` passes after a clean rebuild and verifies recorder -> file -> replay-loader round-trip for the escort checkpoint state.
- The old failing artifact `android/temp_game_logs/d2_descent2_level2_20260429_233507.dximdemo` predates this schema change, so replaying it cannot validate the new record-side escort checkpoint fields. A fresh post-change L2 recording and replay is still required to complete phase 7.

## Fresh artifacts

- L1: `d2_descent2_level1_20260429_233432.dem`, `.dximdemo`, `.dximdemo.rngtrace.jsonl`
- L2: `d2_descent2_level2_20260429_233507.dem`, `.dximdemo`, `.dximdemo.rngtrace.jsonl`
- Shared Android debug log: `debuglog_20260429_233409.txt`
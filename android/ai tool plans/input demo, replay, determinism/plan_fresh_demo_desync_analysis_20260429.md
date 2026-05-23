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
| 7 | Record and restore guidebot escort runtime timestamps in input demo checkpoint/replay, then rerun L2. | completed |
| 8 | Trace the fresh April 30 L2 replay-only frame-302 awareness event and verify checkpoint/runtime restore boundaries. | completed |
| 9 | Repair any remaining checkpoint save/load timestamp rebasing bug in the object restore path and rerun L2. | completed |
| 10 | Trace the earlier side spreadfire pellet that hits robot 100 before frame 302 and compare its replay lifetime against the classic dump. | completed |
| 11 | Instrument robot 100 health/state in the replay probes and classic dump, then compare the replay frame-302 kill against the classic run. | completed |
| 12 | Compare record-side versus replay robot 100 motion at the first drift frame, extend hidden-state probes, and decide whether the next step needs a fresh Android recording. | in_progress |

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
- Fresh April 30 L1 analysis showed the engine already matched the embedded terminal-exit subset. The remaining L1 failure was only a host wrapper comparison bug against raw `endlevel_completed`; after fixing the wrapper, the fresh L1 replay passes.
- Fresh April 30 L2 analysis narrowed the first real divergence to a replay-only frame-302 spreadfire-on-robot awareness event. The replay log shows `impact probe: frame=302 kind=robot weapon_obj=167 weapon_id=12 ... robot_obj=100`, then `awareness probe: frame=302 ... obj_id=12`, while the recorded `.rngtrace.jsonl` has no frame-302 awareness RNG call and does not advance from RNG state `472276992` until frame 304.
- The checkpoint runtime restore probe shows the replay is not losing its saved runtime slice after restore. At load, it logs `Input demo replay checkpoint runtime restore: gt=296222 ... spreadfire_toggle=1 ... rng=3662108683 has_rng=1`, and the same run still reaches the first RNG mismatch at frame 303 immediately after the extra frame-302 awareness event.
- A new local root-cause candidate is in `d2/main/state.c`: object save writes weapon and powerup `creation_time` fields as deltas from `GameTime64`, but object restore assigned those deltas back as absolute times instead of rebasing them onto the restored `GameTime64`. That can skew pre-existing checkpoint weapon age and collision timing without consuming RNG before the first mismatch.
- The earlier side spreadfire pellet is no longer the leading suspect. Under the cadence-aware 20 Hz to 25 Hz join, replay `obj=162 sig=4106` tracks classic `sig=4107` closely and reaches the same robot-100 contact window.
- Robot 100 lines up better at `replay_frame - 54` than `replay_frame - 53`, which is good enough to compare motion drift even though the projectile anchor used a different equivalent-frame join.
- Replay `obj=167` matches classic `sig=4114`, not classic `sig=4113`. The replay/classic center-pellet path stays aligned through replay frame 301 vs classic frame 242, then separates only at replay frame 302 when the replay pellet collides with robot 100.
- Replay robot 100 is already slightly displaced relative to the classic run before frame 302, and the replay AI trace drops robot 100 entirely at frame 302 while the classic dump still shows robot 100 alive through at least frame 244. The next check is whether the replay robot's shields or related state already differ before the center-pellet hit.
- The Android record-side debug log is authoritative for robot 100 motion and visible AI scalars in the fresh April 30 run even though the line label says `Input demo replay AI robot`, because the same trace is active while recording.
- Record-side versus host replay robot 100 first diverges at frame 276. Frames 268 through 275 still match in position, then frame 276 starts a steadily growing drift: `dpos=(2638,-513,919)` at frame 276, reaching roughly `(14607,-36365,75406)` by frame 301.
- Visible AI fields are not the source of that drift. Across frames 268 through 302, record-side and replay robot 100 match on `mode`, `seg`, `goal_seg`, `prev_vis`, `aware`, `aware_time`, `seen`, `next_action`, `next_fire`, `path_index`, `path_length`, `hide`, and `skip`.
- Host replay hidden-state logging now shows that frame 276 is also a hidden AI-state transition point for robot 100: frame 275 has `cur_state=6 goal_state=5 gun=1 danger_obj=167 danger_sig=4113`, while frame 276 has `cur_state=5 goal_state=4 gun=1 danger_obj=167 danger_sig=4113`.
- The classic `.dem` playback path is not authoritative for hidden AI state. Dumping `Ai_local_info` fields yielded zeros, and extending the classic dump to include `obj->ctype.ai_info` also still yielded zeros for robot 100 around classic frames 220 through 225 even though the object positions remained useful.
- The fresh `.dximdemo` header metadata issue does not currently reproduce in the checked-in Android build configuration. The Android `compile_commands.json` and `build.ninja` already compile `input_demo_recorder.cpp` with `DXX_INPUT_DEMO_BUILD_NUMBERi=12640` and `DXX_INPUT_DEMO_GIT_VERSION="e8f330a"`, so the observed `build_number: 0` and `git_version: "unknown"` headers most likely came from a stale or older installed APK rather than the current source tree.

## Fresh artifacts

- L1: `d2_descent2_level1_20260429_233432.dem`, `.dximdemo`, `.dximdemo.rngtrace.jsonl`
- L2: `d2_descent2_level2_20260429_233507.dem`, `.dximdemo`, `.dximdemo.rngtrace.jsonl`
- Shared Android debug log: `debuglog_20260429_233409.txt`
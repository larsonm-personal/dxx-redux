# Plan: temp game logs replay analysis

Date: 2026-05-01

Goal: inventory the non-`old/` demo artifacts in `android/temp_game_logs`, replay each input demo that can be replayed on host, generate fresh replay state and RNG trace outputs, compare them to the recorded artifacts, and analyze the first desync with the nearby game logs.

## Phases

| phase | task | status |
|---|---|---|
| 1 | Inventory the non-`old/` artifacts and identify which files are replayable input demos versus supporting logs. | completed |
| 2 | Run the host replay wrapper on each replayable `.dximdemo` and capture fresh replay result, state trace, and RNG trace outputs. | completed |
| 3 | Compare the replay state and RNG traces against the recorded demo artifacts and note the first mismatch for each demo. | completed |
| 4 | Read the nearby game log artifacts and analyze the first desync with the trace results. | completed |
| 5 | Summarize the outputs, mismatch points, and the most likely desync explanation. | completed |

## Notes

- Ignore files under `android/temp_game_logs/old/`.
- Use the input demo replay wrapper for `.dximdemo` files. Treat classic `.dem` files as supporting reference only.
- Prefer the fresh replay-generated `.actual_state.jsonl` and `.actual_rngtrace.jsonl` outputs when analyzing the first divergence.

## Findings

- The non-`old/` root bundle contains one replayable input demo, one classic `.dem`, one recorded `.rngtrace.jsonl`, and one debug log: `d2_descent2_level2_20260501_085718.*` plus `debuglog_20260501_085631.txt`.
- Host replay wrote fresh outputs to `temp/input_demo_state_traces/d2_descent2_level2_20260501_085718.actual_state.jsonl`, `temp/input_demo_state_traces/d2_descent2_level2_20260501_085718.actual_rngtrace.jsonl`, and `android/temp_game_logs/d2_descent2_level2_20260501_085718.dximdemo.actual.json`.
- Final replay result diverged: recorded trailer expects `player0.shields=165`, replay ended with `player0.shields=150`.
- First RNG trace mismatch is line 9 / seq 7. The recorded side has a `create_random_xlate` event at frame 28, call count 82, file `d2/main/aipath.c`, while replay does not consume that batch until frame 43, call count 108.
- First visible state mismatch is frame 88 in `position.x`. Replay is already offset before segment drift becomes visible at frame 91 and before shield drift begins at frame 95.
- The kept replay sandbox log shows the desync starts at frame 28. Replay restores the guidebot checkpoint state as `mode=9 cur_path=0/53 hide_index=38 last_seen=0 last_player_path=0 escort_last_path=393740 seen=-332399`, then logs `visit=0` and never calls `create_path_to_player` for the buddy at frame 28.
- The recorded debug log for the same frame shows `visit=1` at frame 28 and immediately enters `create_path_to_player` for buddy object 55, consuming RNG calls 81->107 and changing the escort path from `0/53` to `0/4`.
- The best local root-cause hypothesis is the replay-only checkpoint suppression branch in `d2/main/escort.c:time_to_visit_player()`. For checkpoint demos it explicitly suppresses the visit-player path rebuild when `ailp->time_player_seen < checkpoint_start_gt`, `ailp->mode == AIM_GOTO_OBJECT`, and `cur_path_index < path_length/2`. The frame-28 replay values satisfy that branch exactly, so replay skips the same buddy path rebuild that the live recording performed.
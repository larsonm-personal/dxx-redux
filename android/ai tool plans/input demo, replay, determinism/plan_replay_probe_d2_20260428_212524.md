# Replay Probe D2 20260428 212524

## Plan

- [x] Inspect the provided demo and RNG trace artifacts to identify their scope and early RNG activity
- [x] Replay the provided demo with the current D2 host binary and capture the first mismatch window
- [x] Compare the host replay window against the recorded RNG sidecar and corrected D2 AI/path/escort trace logs
- [x] Fix the smallest local cause of the earliest mismatch and rerun the focused replay validation
- [x] Summarize the remaining desync state, if any, and mark completed work here

## Inputs

- `android/temp_game_logs/d2_descent2_level1_20260428_212524.dximdemo`
- `android/temp_game_logs/d2_descent2_level1_20260428_212524.dximdemo.rngtrace.jsonl`

## Early Notes

- Header: D2, level 1, difficulty 1, checkpoint start, `frame_count=238`, `rng_mode=lcg_state`, `start_gt=44564`
- Recorded sidecar begins with a `do_controlcen_frame` draw at frame 0, then the first dense simulation bursts appear in `d2/main/aipath.c`
- Early recorded RNG is already path-heavy: frame 6 ends at state `2002524901`, frame 7 at `1603507876`, frame 8 at `3889491909`, and frame 9 at `3852055717`
- The cheapest discriminator is still the first host replay mismatch window because the corrected recorder-active AI/path/escort probes are now available for live captures if replay and recording diverge before the next RNG event

## Results

- Initial host replay started checkpoint demos from `GameTime64 = 0`, so the first mismatch hit at frame 7 while replay `gt` was still `18350` instead of the recorded `62914`
- Fix 1: `d1/main/state.c` and `d2/main/state.c` now add `input_demo_replay_checkpoint_start_gt()` back to the save's zeroed `GameTime64` during checkpoint restore
- That moved the earliest mismatch to frame 1 and exposed a second restore bug: AI and escort checkpoint timestamps were still being restored as raw deltas instead of absolute times
- Fix 2: `d1/main/ai.c` and `d2/main/ai.c` now reapply `GameTime64` to restored AI delta timestamps (`time_player_seen`, `time_player_sound_attacked`, `next_misc_sound_time`, cloak timers, boss timers, and `Escort_last_path_created` where present)
- Focused validation passed for both code changes with `run-windows-build.ps1 -Target d2`, reran the 212524 replay after each fix, and also built `run-windows-build.ps1 -Target d1` after mirroring the shared restore change
- After both fixes, replay matches the recorded sidecar through frame 31 and the first mismatch moves to frame 32 (`gt=128450`)
- Remaining earliest desync: replay takes guidebot `time_to_visit_player()` at frame 31, consuming the exact RNG sequence that the recorded sidecar does not consume until frame 35 (`gt=138936`), so the remaining gap is a non-RNG live-vs-replay state drift before that escort branch rather than another missing RNG/base-time restore
- Added the requested state-based D2 escort probes in `d2/main/escort.c`: a one-shot restore-normalization log plus transition-only `escort segment change` and `escort visit change` logs
- Focused validation for the probe tranche passed with `run-windows-build.ps1 -Target d2`, and a host replay spot-check of `212524` showed the new `escort visit change` and `escort segment change` lines firing at the expected branch transitions without repeating on every visible frame
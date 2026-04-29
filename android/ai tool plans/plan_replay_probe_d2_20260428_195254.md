# Replay Probe D2 20260428 195254

## Plan

- [x] Inspect the provided demo and RNG trace artifacts to identify their scope and early RNG activity
- [x] Rebuild or confirm the current D2 host binary, then replay the provided demo with the newest code
- [x] If replay diverges, compare host replay output against the provided RNG trace and isolate the first mismatch window
- [ ] Fix the smallest local cause of the earliest mismatch and rerun the focused replay validation
- [x] Summarize the remaining desync state, if any, and mark completed work here

## Inputs

- `android/temp_game_logs/d2_descent2_level1_20260428_195254.dximdemo`
- `android/temp_game_logs/d2_descent2_level1_20260428_195254.dximdemo.rngtrace.jsonl`

## Early Notes

- Header: D2, level 1, difficulty 1, checkpoint start, `frame_count=267`, `rng_mode=lcg_state`
- Sidecar begins at replay frame 1 / `gt=128451` / call count `398`
- Early recorded RNG is dominated by `create_path_points` / `create_random_xlate` bursts in `d2/main/aipath.c` at frames 5, 13, 14, 20, 21, and 22
- Recent nearby replay failures in this code path have come from AI path-follow drift before the first visible RNG mismatch, so that is the first local hypothesis to test here as well

## Findings

- Host replay with the current probes matches the recorded sidecar through the end of frame 46, including the frame 45 `create_path_to_player` burst ending at `542948482` and the frame 46 `create_path_to_segment` burst ending at `2763380606`
- The recorded sidecar has no RNG events at frames 47, 48, or 49 and resumes at frame 50 with `state_before=2763380606`
- Host replay advances the buddy path from `0/3` to `1/3` during frame 48, then frame 49 satisfies `time_to_visit_player()` and consumes a `create_path_to_player` burst from `2763380606` to `4001946725`
- The first replay mismatch stays at frame 50 with `expected=2763380606 actual=4001946725`
- An extra fixed-point companion goal-nudge probe was tested in `d2/main/aipath.c`, did not move the mismatch, and was reverted
- Android replay on the emulator reproduces the same replay-side branch as host: frame 48 already lands on `path=1/3`, frame 49 consumes the same `create_path_to_player` burst, and frame 50 hits the same RNG mismatch
- That means this artifact is currently narrowed to live-recording versus replay-state drift before frame 49, not a Windows-versus-Android replay math difference
- Recorder-active D2 probe logs in `d2/main/aipath.c`, `d2/main/escort.c`, and `d2/main/ai.c` were using `input_demo_replay_next_frame_index()` even during live recording; they now use a shared current-frame helper that falls back to `input_demo_recorder_frame_count() - 1` during recording
- `GameProcessFrame()` records the current input-demo frame before AI, escort, and path logic runs, so `frame_count - 1` is the correct live frame label for these probes
- `run-windows-build.ps1 -Target d2` passes after the logging-only patch, so the next discriminator is a fresh live-recording log from the original recording environment, not another replay-only run
- A post-patch host replay of artifact 195254 still takes the same frame 48 follow advance and frame 49 `create_path_to_player` branch in `gamelog.txt`, so this change stayed instrumentation-only
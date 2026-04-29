# Replay Probe D2 20260428 220300

## Plan

- [x] Inspect the provided demo and RNG trace artifacts to identify their early RNG window
- [x] Replay the provided demo with the current D2 host binary and preserve the sandbox gamelog
- [x] Compare the first replay mismatch window against the recorded RNG sidecar and escort probe logs
- [x] Compare recorder-side escort probe logs from the same Android run against the host replay window
- [x] Use the expanded motion/orientation probe to identify whether frame 11 creates the drift inside `object_move_all()` or later in `GameProcessFrame()`
- [x] Extend the motion probe with hidden player runtime state, rerun the replay, and use the first frame-11 movement window to disconfirm the afterburner-state hypothesis before widening scope

## Inputs

- `android/temp_game_logs/d2_descent2_level1_20260428_220300.dximdemo`
- `android/temp_game_logs/d2_descent2_level1_20260428_220300.dximdemo.rngtrace.jsonl`

## Early Notes

- Header: D2, level 1, difficulty 1, checkpoint start, `events=1216`, recorded RNG begins at frame 3 with `state_before=2695732133`
- Host replay sandbox log: `temp/input_demo_runtime_wrapper/d2/d2_descent2_level1_20260428_220300/gamelog.txt`
- Replay restore normalization now looks sane for the buddy on this artifact: `gt=301466`, `raw_seen=301466`, `raw_escort_last_path=83887`, `final_last_player_path=301466`

## Results

- Replay matches the recorded sidecar through frame 66
- First mismatch is at frame 68 (`gt=479724`): replay expects next RNG state `4156555451`, but the host process is already at `1133281152`
- The divergence is caused one frame earlier at frame 67, before the mismatch line prints
- Recorded sidecar frame 67 has only seven `aipath.c` RNG calls (`seq 107-113`), ending at state `4156555451`
- Recorder-side Android Game Logs confirm the recorded run does not take the replay-only buddy branch at frame 67:
  - Recorder frame 67 keeps `mode=8`, updates `believed_seg=142->160`, reports `dist=2800649`, and only runs the `obj=93` seven-call path burst (`2359694016 -> 4156555451`)
  - Recorder frame 68 still stays above `MIN_ESCORT_DISTANCE` at `dist=2751373`, with no buddy goal-path request
- Host replay frame 67 performs an extra buddy `escort_create_path_to_goal` burst first:
  - `mode=8`, `dist=2611738`, `buddy_seg=142`, `player_seg=160`, `cur_path=2/3`, `last_player_path=390595`, `escort_last_path=414188`
  - `MIN_ESCORT_DISTANCE` is `F1_0*40` (`2621440`), so replay crosses the `dist_to_player < MIN_ESCORT_DISTANCE` gate by about `9702`
  - Buddy goal-path creation consumes `57` RNG calls (`2359694016 -> 3019889537`) at frame 67
  - Robot `obj=93` then consumes the recorded seven-call path burst (`3019889537 -> 1133281152`)
- The earliest remaining desync on 220300 is therefore a non-RNG state drift that reaches the escort gate by frame 67: replay leaves `AIM_GOTO_PLAYER` and recreates a goal path there, while the recorded run does not
- Existing paired logs show that the drift starts earlier than the escort branch itself:
  - Frame 0 escort state already differs in `last_player_path` (`301466` on host replay vs `277873` on the recorder)
  - By frame 12 the paired `AI state` lines already show a small player-position split before any RNG mismatch is reported
  - Buddy follow state remains nearly identical through that window, so the next local slice is player motion and replay entry timing rather than another escort-only RNG owner
- A control-side search through `kconfig_read_controls()` did not surface a convincing missing gameplay input field: the recorded demo schema already carries the final per-frame movement times, and the remaining hidden control variables appear either UI-only or already baked into those times
- The motion probe now logs orientation plus `entry`, `after_move`, and `exit` stages in both D1 and D2, and both Windows targets build successfully with that instrumentation
- Android replay automation is now working for this artifact, and fresh Android replay logs match the host replay bit-for-bit through frames 10-13 and at the later frame-68 RNG mismatch window
- The replay schema stores the full per-frame control state, including `afterburner_state`, so the missing afterburner-field hypothesis is now disfavored; the next cheap check is hidden player runtime state such as `afterburner_charge`
- The motion probe now logs replay-time player physics scalars plus loaded ship tuning in both D1 and D2: `phys=(mass,drag,brakes)` and `ship=(mass,drag,brakes,max_thrust,max_rotthrust,wiggle)`
- A fresh host replay still reproduces the same early motion window and the same frame-82 RNG mismatch, while the new fields stay constant through frames 0-13 and later mismatch windows: `phys=(262144,2162,0)` and `ship=(262144,2162,0,511180,9175,32768)`
- Replay startup now emits a single path-independent config line that captures the same setup state without depending on exact frame numbers: `callsign=<empty> result=0 auto_level=0 player_flags=0x1184 phys=(262144,2162,0,0x4b) ship=(262144,2162,0,511180,9175,32768)`
- That startup line keeps the empty-callsign sandbox path in focus: the current host replay is still loading player config successfully under `callsign=<empty>`, which means the next recorder-side comparison can test pilot/data-environment drift without requiring the same exact path or frame alignment

## Next Step

- Use the new startup config line as the next recorder-vs-replay comparison point; if a future recorder rerun reports different `callsign`, `auto_level`, `player_flags`, `phys`, or `ship` values there, treat that as the root-cause candidate before reopening frame-by-frame motion analysis
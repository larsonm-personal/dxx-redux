# Replay Probe D2 20260429 074558

## Plan

- [x] Inspect the provided demo, recorder debug log, and RNG trace to anchor the early replay window
- [x] Replay the provided demo with the current D2 host wrapper and preserve the sandbox gamelog
- [x] Compare the host replay startup config line against the recorder-side motion/log context before widening scope
- [x] Compare the first host mismatch window against the recorder motion probe and RNG sidecar
- [x] Preserve checkpoint-restored auto-leveling across replay startup player-config reload and rerun the exact host replay

## Inputs

- `android/temp_game_logs/d2_descent2_level1_20260429_074558.dximdemo`
- `android/temp_game_logs/d2_descent2_level1_20260429_074558.dximdemo.rngtrace.jsonl`
- `android/temp_game_logs/debuglog_20260429_074534.txt`

## Early Notes

- Header: D2, level 1, difficulty 1, checkpoint start, `frame_count=226`, recorded RNG begins at frame 2 with `call_count=234`
- Recorder-side debug log now includes the expanded motion probe for this run, starting at `frame=0 stage=entry gt=133693`
- The first recorded RNG bursts still come from `d2/main/aipath.c` on frames 2 and 15, so the prior escort-path investigation remains the nearest known comparison point
- Initial host replay showed `callsign=<empty> result=0 auto_level=0` while the recorder motion probe showed `al=1` from frame 0 onward
- That host startup drift cleared `PF_LEVELLING` by the early motion window and led to the first RNG mismatch at frame 76, with later cascades through the 210 to 223 AI/control-center window
- Root cause was replay startup restoring the checkpoint, then calling `new_player_config(); read_player_file();`, which overwrote `PlayerCfg.AutoLeveling` from the empty host pilot/defaults instead of preserving the restored player physics state
- Preserving `PlayerCfg.AutoLeveling` from the restored `ConsoleObject->mtype.phys_info.flags & PF_LEVELLING` fixed the startup line (`auto_level=1`) and kept the early motion probe aligned with the recorder

## Result

- Rebuilt both desktop targets with `run-windows-build.ps1 -Target both`
- Reran `android/tests/run_input_demo_replay.ps1` on `d2_descent2_level1_20260429_074558.dximdemo`
- Host replay now passes with no `Input demo replay rng state mismatch` lines, writes the expected actual result JSON, and reports `RESULT: PASS`
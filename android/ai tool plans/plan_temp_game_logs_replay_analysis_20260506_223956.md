# Plan: input demo replay analysis 20260506_223956

## Goal
- explain why replay writes `level_summary.endlevel_completed = true` for `android/temp_game_logs/d2_descent2_level3_20260506_223956.dximdemo` while the embedded trailer records `false`

## Steps
- [completed] verify whether the replay harness detects the mine-exit terminal path for this demo when sandbox output is preserved
- [completed] confirm whether the mismatch is caused by replay result capture timing or by wrapper compare logic
- [completed] implement the smallest local fix in engine or wrapper, whichever matches the verified control path
- [completed] rerun the failing replay with focused validation and update this plan with the outcome

## Notes
- embedded demo trailer already records `control_center_destroyed = true` and `endlevel_completed = false`
- replay-side result capture uses `Endlevel_sequence` directly unless a terminal exit override is applied
- quiet headless-console replays do not write `gamelog.txt`, so wrapper terminal-exit detection could not rely on the old marker scan alone
- the existing terminal-exit subset helper also omitted `player0` and `position`, which made subset compares fail on extra-key diffs until those sections were pinned to the actual result
- validated fix: `android/tests/run_input_demo_replay.ps1 -DemoPath android/temp_game_logs/d2_descent2_level3_20260506_223956.dximdemo -Game d2 -Runner headless-console -PreferHeadlessConsole -KeepSandbox` now returns `RESULT: PASS`
# Plan: terminal exit marker in recorded replay results

## Goal
- add an explicit terminal-exit marker to input demo result records so replays can identify level-exit or mine-exit terminal comparisons without heuristics

## Steps
- [completed] extend the shared input demo result schema with an optional terminal-exit field
- [completed] wire the D2 quick-record level-exit stop path to set the recorded terminal-exit marker
- [completed] wire D2 replay terminal result writes to emit the same marker in actual results
- [completed] update the replay wrapper to prefer the explicit marker and keep the old fallback for older demos
- [completed] run a focused build and replay validation, then update this plan with the outcome

## Notes
- the current wrapper pass relies on a fallback for quiet headless replays because older demos do not carry an explicit terminal-exit marker
- the requested change is to make new recordings mark that terminal condition directly in the embedded result record
- validated replay output now writes `"terminal_exit": "level_exit"` for `android/temp_game_logs/d2_descent2_level3_20260506_223956.dximdemo`
- validated command: `android/tests/run_input_demo_replay.ps1 -DemoPath android/temp_game_logs/d2_descent2_level3_20260506_223956.dximdemo -Game d2 -Runner headless-console -PreferHeadlessConsole -KeepSandbox`
- recording-side marker is wired through D2 quick recording when `start_endlevel_sequence()` stops recording via `newdemo_stop_quick_recording_for_level_exit()`; I did not generate a fresh new demo in this session
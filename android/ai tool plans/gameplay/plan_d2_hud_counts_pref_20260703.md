# D2 HUD Counts Preference Investigation - 2026-07-03

## Goal
Find why a Descent 2 player can have the robot and secret counts preference checked in-game but not see those counts below the score line, especially for a player created through co-op.

## Plan
- [x] Locate the D2 preference flag, save/load path, and HUD rendering checks.
- [x] Trace normal player creation versus co-op-created player initialization.
- [x] Identify the most likely failure path and decide whether a small source fix is appropriate.
- [x] If code changes are made, run focused validation and mark this plan complete.

## Findings
- The D2 runtime flag is `PlayerCfg.ShowRobotHostageCounts`; it is saved in `.plx` as `robothostagecounts`.
- The launcher writes this setting only to existing pilot-backed files. New regular pilots start from `new_player_config()`, which defaults the flag to `0`.
- Android auto-created co-op pilots try to load recent/fallback pilot prefs first, but if there is no suitable pilot yet they also fall back to `new_player_config()`, so the flag starts off.
- The launcher preferences page reads one preferred/first pilot, so it can show the checkbox checked even when another newly-created or co-op callsign is the active player with the flag off.
- Even when the flag is on, the HUD draws the block only outside status bar/rear view and when `HUD_toolong` is not set.

No source fix was made in this investigation pass.

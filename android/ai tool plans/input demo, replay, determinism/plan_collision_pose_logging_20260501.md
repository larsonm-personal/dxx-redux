# Plan: Collision Pose Logging 20260501

## Scope

Add symmetric event-local logs for player, weapon, and robot position and velocity around the player-weapon-vs-robot collision path that appears to kill robot `68` too early in the `20260501_133242` replay.

## Hypothesis

The next useful signal is not another frame-targeted probe. It is a symmetric collision snapshot keyed by event label and object identity so recording and replay can be compared even when the next hand-recorded demo lands the same sequence on different frame numbers.

## Plan

1. Add a shared collision logging helper in `d2/main/collide.c` for player, weapon, and robot pose and velocity snapshots
2. Emit the snapshot on both recording and replay around player-owned weapon vs robot impacts and the corresponding robot-damage transition
3. Keep the logs keyed by step label, object indices, signatures, ids, segment, position, and velocity so they do not rely on exact frame alignment
4. Build the touched host target and rerun the failing replay bundle to confirm the new logs appear at the early-kill point
5. Record the validation result and intended usage for the next recorded demo

## Findings

- Added a shared collision-pose helper in `d2/main/collide.c` that emits one event-local snapshot line containing:
	- player object identity, segment, position, and velocity
	- weapon object identity, parent identity, segment, position, and velocity
	- robot identity, segment, shields before and after, dead flag, position, and velocity
	- collision point
- The helper is active for both recorder and replay runs, not replay only
- The logs are keyed by `step` plus object ids and signatures, so they can be matched even when a new hand-recorded demo lands the same sequence on different frame numbers
- The local player weapon-vs-robot collision path now emits:
	- `step=weapon_robot pre_damage`
	- `step=weapon_robot post_damage`

## Validation

- `run-windows-build.ps1 -Target d2`
- `android/run-code-quality.ps1 -Fix -Paths @('.\d2\main\collide.c')`
	- completed successfully but reported `No files found to format` for the scoped C path
- Reran replay for `android/temp_game_logs/d2_descent2_level2_20260501_133242.dximdemo`
- Confirmed the new logs appear in the replay sandbox at the first divergent kill window, including robot `68`, sig `3779`:
	- frame `783` pre and post damage for the non-lethal hit
	- frame `784` pre and post damage for the lethal hit that advances the `+400` score event

## Intended Use

- Compare recorded and replay logs by `step`, robot identity, weapon identity, player pose, robot pose, and collision point rather than by exact frame number
- For the next recorded demo, the main anchor is the first `weapon_robot pre_damage` or `weapon_robot post_damage` line for the suspect robot rather than the first score mismatch frame
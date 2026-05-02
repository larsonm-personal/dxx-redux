# Plan: Player Motion Probe 20260501 141150

## Scope

Add a focused host replay probe for the first translational desync window in:

- `android/temp_game_logs/d2_descent2_level2_20260501_141150.dximdemo`

Keep the probe narrow to the local player movement path in `d2/main/physics.c` and validate it by rerunning the same bundle.

## Local Hypothesis

The first replay drift is introduced inside the player translation step in `do_physics_sim()` for the local player, not by later AI or collision side effects. The cheapest discriminating check is to log the immediate translational intermediates around frames `546` through `552`: velocity before drag, velocity after drag, the scaled `frame_vec`, and the `new_pos` target before FVI/collision handling.

## Plan

1. Add a targeted player-only replay probe in `d2/main/physics.c` for frames `546` through `552`
2. Rebuild the D2 host target
3. Rerun the `141150` replay bundle with sandbox retention
4. Inspect the new physics detail logs around frames `546` and `547`
5. Summarize whether the first drift appears in drag integration, frame scaling, or later collision handling

## Findings

- Updated `d2/main/game.c` so replay-side wrapper motion logs are no longer gated to the stale `816..825` window
- Added a focused `pre_fvi` motion-detail probe in `d2/main/physics.c` and retargeted the existing drag probe to frames `546` through `552`
- `run-windows-build.ps1 -Target d2` completed successfully
- The replay rerun still fails with the same final mismatch: `result.position.x expected -9075092 actual -9075077`
- Canonical RNG compare still passes completely
- The new probe shows the first visible position drift is already present before collision handling:
	- Replay `frame=546 step=pre_fvi` starts from `pos=(-14724001,-2805196,-5158424)` and computes `target=(-14668449,-2814913,-5148038)`
	- The same step reports `fate=none`, so FVI does not modify the target
	- The compare output for the next serialized state is `expected (-14668450,-2814913,-5148040)` vs `actual (-14668449,-2814913,-5148038)`
- Comparing the replay sandbox log to the original Android debug log by `gt` tightens the root cause one step further:
	- At `gt=1520435`, recorder and replay `after_move` player motion still match
	- At `gt=1523057`, position still matches but velocity is already split
		- Recorder: `vel=(1323974,-261507,245636)`
		- Replay: `vel=(1323989,-261493,245692)`
	- At `gt=1525678`, the replay `pre_fvi` target differs from the recorder's implied translation by exactly the later visible position error

## Conclusion

The 141150 desync is not collision-driven and not RNG-driven. The first hidden divergence is a small local-player velocity mismatch at `gt=1523057`, while serialized position still matches. One move later, the replay computes a different `frame_vec` and `new_pos` before FVI, which produces the first visible position drift.

## Status

1. [x] Add a targeted player-only replay probe in `d2/main/physics.c` for frames `546` through `552`
2. [x] Rebuild the D2 host target
3. [x] Rerun the `141150` replay bundle with sandbox retention
4. [x] Inspect the new physics detail logs around frames `546` and `547`
5. [x] Summarize whether the first drift appears in drag integration, frame scaling, or later collision handling
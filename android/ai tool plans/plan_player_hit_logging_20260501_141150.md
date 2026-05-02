# Plan: Player Hit Logging 20260501 141150

## Scope

Prepare instrumentation for the next recording of:

- `android/temp_game_logs/d2_descent2_level2_20260501_141150.dximdemo`

Focus on the player-hit path that occurs on the first hidden-drift step, while removing demo-specific frame gates so the probes remain useful on a fresh recording.

## Local Hypothesis

The remaining hidden split is more likely on the weapon-hit side than in pure player drag math. The cheapest discriminating change is to add recorder+replay logging around weapon lifetime and player-hit collision inputs, while removing frame-number gating from the current player-motion probes in `d2/main/physics.c` and related helpers.

## Plan

1. Find remaining frame-specific input-demo probe gates in the D2 logging path
2. Add symmetric recorder/replay weapon-state logging for the projectile that later hits the player
3. Remove frame-specific probe gates so the next recording does not require retuning
4. Rebuild the D2 host target
5. Summarize the new logging surface and next recording expectations

## Findings

- Completed steps 1 through 4
- The active player-hit logging path had three relevant demo-specific gates:
	- `d2/main/physics.c` player drag and motion-detail probes were pinned to frames `545..552`
	- `d2/main/controls.c` control and wiggle probes were pinned to frames `18..24`
	- `d2/main/laser.c` robot-weapon creation logging still had a hardcoded replay-only frame/parent gate
- `d2/main/physics.c` now uses an event-driven threat window instead of frame numbers:
	- it activates when the local player shares a segment with a live robot-owned weapon
	- it stays active for the immediate trailing frame so the next-step consequence of the hit is still logged
- `d2/main/controls.c` now uses the same threat-window idea for control and wiggle logging, so the next recording captures pre-hit control construction without retuning frame constants
- `d2/main/collide.c` player-hit logging is now symmetric for recorder and replay:
	- player damage log now works in both modes
	- player weapon hit log now works in both modes and includes weapon life, flags, creation time, position, velocity, player position, player velocity, and hit point
	- bump probe now reports `mode=record|replay` instead of being replay-labeled only
- `d2/main/laser.c` weapon creation logging no longer depends on the old hardcoded replay frame window for robot-owned weapons
- D2 host build succeeded after the changes

## Conclusion

The next recording no longer depends on the current `141150` frame numbers to emit the detailed player-hit diagnostics. The active logging path is now keyed to the presence of a nearby robot-owned weapon and to the actual player-hit collision itself, which makes it portable to a fresh demo. Recorder-side Android logs and replay-side host logs should now both expose the player-hit inputs needed to compare pre-hit control/drag state and the weapon state at impact.

## Next Step

Record a fresh demo with the new instrumentation, then compare the recorder-side `player control probe`, `player wiggle probe`, `player drag probe`, `player motion detail`, `player weapon hit`, `player damage`, `bump probe`, and robot-weapon `weapon probe` lines by `gt`.
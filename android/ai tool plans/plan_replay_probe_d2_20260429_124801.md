# D2 Fresh Replay Probe Plan 2026 04 29 124801

## Goal

Turn the fresh D2 level 1 and level 2 input demos in
`android/temp_game_logs/` into stable regression coverage with no replay
divergence.

## Current Hypothesis

- the earlier escort replay divergences at frames 27, 327, and 334 were real
	and are now fixed in `d2/main/escort.c`
- the remaining level 2 host divergence is no longer in escort, player motion,
	or the weapon's own frame-615 sweep
- the current host-only branch is a near-threshold robot-side collision:
	`obj 98` sweeps into stationary spreadfire `obj 166` at frame 615
- host probe details now show `obj 98` starts about 9.8k units outside the
	combined robot plus weapon radius and ends about 0.9k units inside it, so a
	very small position or collision difference can flip the hit

## Cheap Check

- compare a fresh Android device run with the new position and physics probes,
	or if that is unavailable, keep probing the host around the frame-615 robot
	sweep until the controlling upstream state is exposed

## Status

- [completed] Reproduce the fresh level 1 replay on host and validate the
	shared replay `GameTime64` fix there
- [completed] Reproduce the fresh level 2 replay on host and isolate the early
	escort divergences at frames 27, 327, and 334
- [completed] Patch the escort replay state gaps in `d2/main/escort.c` and
	revalidate that the earliest meaningful host mismatch moved to frame 615
- [completed] Add narrow replay probes in `d2/main/laser.c`, `d2/main/ai.c`,
	and `d2/main/physics.c`, then rerun host and Android emulator probes
- [completed] Disprove the replay-only spreadfire muzzle-ray object-hit
	hypothesis by patching it and showing frame-615 behavior stayed unchanged
- [completed] Localize the current host-only branch to robot 98 colliding with
	stationary spreadfire object 166 during the robot's own frame-615 physics
	sweep
- [in progress] Determine whether the remaining branch is caused by tiny
	upstream robot or weapon position drift or by platform-sensitive collision
	math in the frame-615 robot sweep
- [not started] Convert the fresh demos into regression coverage once the
	remaining frame-615 branch is resolved

## Validation Target

- `android/tests/run_input_demo_replay.ps1 -DemoPath .\android\temp_game_logs\d2_descent2_level1_20260429_124828.dximdemo -Game d2 -Mode accelerated`
- `android/tests/run_input_demo_replay.ps1 -DemoPath .\android\temp_game_logs\d2_descent2_level2_20260429_124911.dximdemo -Game d2 -Mode accelerated`

## Latest Findings

- the original Android device artifact still matches host player motion and AI
	state fields for `obj 98` and `obj 99` through frame 614
- the fresh Android emulator run is not a reliable authority for this slice
	because it already diverges much earlier in player motion and weapon creation
- host frame-615 collision details from the latest probe:
	`obj 166` does not hit anything during its own frame-615 weapon sweep
	`obj 98` frame-615 physics is `fate=object` with `hit_object=166`
	`obj 98 size=373267`, `obj 166 size=32768`
	weapon center at impact: `(-12967760,-3290494,-2602198)`
	robot 98 frame-615 sweep start: `(-12827082,-3660603,-2729073)`
	robot 98 frame-615 sweep target: `(-12833722,-3659714,-2701577)`
	probe distances: `attempted=28300 actual=25818 sim=(2393,229)`

## Next Steps

- if a fresh physical-device probe is available, rerun it with the new AI and
	physics probes to see whether Android robot 98 position or sweep differs by
	only a tiny amount in this slice
- otherwise step one hop deeper into `d2/main/fvi.c` and the frame-615
	robot-vs-weapon sweep math to see whether a replay-only deterministic host
	guard can be justified without guessing

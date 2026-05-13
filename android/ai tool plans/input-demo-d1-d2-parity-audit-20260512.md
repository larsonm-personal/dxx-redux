# Input demo D1/D2 parity audit

## Goal
- Check whether the D2 missile-camera wake engine fix has a D1 equivalent
- Audit recent D2-only input-demo determinism fixes for likely D1 parity gaps
- Patch clear shared-root D1 gaps with minimal source churn
- Validate with focused quality checks, host builds, and available shared tests

## Steps
1. Compare D1 and D2 camera/render-side simulation hooks
2. Compare D1 and D2 input-demo replay timing and direct-command paths
3. Identify D2-only fixes that are not applicable to D1 because the feature does not exist
4. Patch any clear D1 parity gaps
5. Run scoped quality checks, Windows host build, and shared replay tests

## Status
- [x] Plan created
- [x] D1/D2 wake paths compared
- [x] Missed parity fixes identified
- [x] Clear D1 gaps patched
- [x] Validation run

## Notes
- User concern: avoid rediscovering D2-only determinism fixes later when D1 demos are created
- Keep D1/D2 source edits narrow and style-consistent
- D1 does not have the D2 missile-camera wake feature surface: no `Missile_viewer`, no `Guided_missile`, no missile/guided cockpit-window users, and no `wake_up_rendered_objects` path. The D2 camera wake engine fix is D2-only
- D1 already has the shared held-secondary replay timing fix through `ReadControlsReplayPostFrame()` after `GameProcessFrame()`
- D1 and D2 both have the render-to-simulation danger-laser warning moved into the gameplay frame. D1 calls `update_all_robot_location_info()`, while D2 calls `render_warn_robots_about_player_fire()` plus the D2-only missile-camera wake pass
- Clear D1 gap found and patched: D1 homing missile initial target selection still used the render-produced `Ordered_rendered_object_list` during replay. D1 now mirrors D2 by using the deterministic full object scan for input-demo replay
- D2 direct-command replay support is mostly D2 feature surface: guidebot, marker input, weapon and flag drop helpers, and escort release do not have D1 equivalents in the current D1 tree. D1 death-abort support is already present
- D2 save/checkpoint transient weapon state has D1 equivalents already in place for D1-supported fields. D1 stores spreadfire, missile gun, and proximity drop state; D2 additionally stores helix, smartmines, guided missile rebind, omega, and afterburner timing because those systems are D2-only
- Broader D2 investigation-probe cleanup remains: several old debug-only probes still name specific objects, segments, or frame windows outside the D1 parity issue. I left them unchanged in this parity patch because current RNG sidecars compare source line locations
- Validation passed: scoped `android/run-code-quality.ps1 -Fix -Paths @('d1\main\laser.c')`, `run-windows-build.ps1 -Target both`, D1/D2 `test_input_demo_replay.exe`, and both supplied D2 level 9 demos with state and RNG trace compare
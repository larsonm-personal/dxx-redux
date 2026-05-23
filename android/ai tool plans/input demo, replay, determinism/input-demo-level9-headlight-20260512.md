# Input demo level 9 fresh replay failures

## Goal
- Use the fresh level 9 demos as determinism repros, not as narrative artifacts
- Find the earliest state or RNG split in both demos
- Patch the narrowest shared game-engine nondeterminism or missing replay capture path
- Validate with focused tests/builds and the affected demos, or record exact next diagnostics

## Demos
- `android/regression_demos/d2_descent2_level9_20260512_115624.dximdemo`
- `android/regression_demos/d2_descent2_level9_20260512_115227.dximdemo`

## Steps
1. Check current worktree and relevant input-demo notes before editing
2. Re-run both demos with state and RNG traces, saving outputs under `temp`
3. Compare expected/actual traces to locate the first shared divergence
4. Inspect the implicated game path, especially headlight, reactor, robot attacks, phoenix cannon, and endlevel if relevant
5. Patch the root determinism/capture gap with minimal D1/D2 source churn
6. Validate focused tests/builds and rerun the affected demos

## Status
- [x] Worktree and notes checked
- [x] Fresh repro traces generated
- [x] First divergence localized to robot wake/awareness diagnostics
- [x] Fix implemented
- [x] Validation run

## Notes
- 2026-05-12: Both fresh demos reproduce with `headless-console`, accelerated mode, default render profile, state trace, and RNG trace
- `d2_descent2_level9_20260512_115624`: first state split at frame 530, `robot_wake_transition`, expected `awareness_events=1`, actual `0`; actual also has one fewer player/weapon object and did not spend one homing missile by that frame
- `d2_descent2_level9_20260512_115227`: first state split at frames 1253-1255, `robot_wake_transition`, expected `camera_awake_robots=1`, actual `0`; runtime/object/weapon hashes initially match, then robot state hash splits
- Removed the temporary input-demo frame event for D2 missile-camera AI wake transitions after user review rejected replay special handling
- Replaced render-driven missile-camera AI wake mutation with a D2 simulation-side pass over the active missile camera and visible nearby robots
- Implemented a D1/D2 replay post-frame weapon/item pass so held secondary fire is rechecked after `GameTime64` advances, matching live inter-frame input handling around missile cooldowns
- After the post-frame pass, `d2_descent2_level9_20260512_115624` advances past the original frame 530 missing missile/awareness split and now reaches a later camera-wake split at frame 636
- Final camera-wake predicate uses close line-of-sight wake, with a forward cone required for farther robots to avoid broad render-list over-wakes
- Validation: scoped `android/run-code-quality.ps1 -Fix` passed, `run-windows-build.ps1 -Target both` passed, D1/D2 `test_input_demo_replay.exe` passed, and both supplied level 9 demos pass with the engine-side camera wake fix
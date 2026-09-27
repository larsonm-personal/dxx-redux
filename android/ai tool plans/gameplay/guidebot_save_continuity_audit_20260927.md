# Same-mode Guidebot save continuity audit

- Audit normal save restore separately from replay checkpoint restoration
- Add an opt-in native continuity probe to the existing Guidebot navigation harness
- Compare unchanged-mode save state and a fixed-step continuation against uninterrupted navigation in both modes
- Normalize absolute timer origins, but retain goal, path, pose, velocity and simulation RNG differences
- Run on real D2 level geometry, report observed losses and distinguish this focused probe from full-world demo equivalence

## Findings

- The new same-mode `guidebot_routing_start_session` call returns without resetting routing
- Existing `guidebot_routing_restore_mode` unconditionally resets Enhanced navigation
- Existing `escort_rebuild_runtime_state_after_restore` reconstructs timers and clears ordinary-save goals, marker/key state, and route goals
- Replay checkpoints have separate escort header fields restored through an early-return path; this protection does not apply to ordinary saves
- An ordinary Scram save in both modes lost special goal 13 (became -1), last key 8 (became -1), and the saved player-path timer offset
- Saving alone preserved the inspected state; the first navigation/physics frame after load changed position, velocity, path, and actual simulation RNG state
- Levels 1 and 11 reproduced in two independent processes each, with byte-identical JSON reports across repeats
- Reports are in `temp/guidebot-save-continuity/level-{1,11}-{1,2}.json`
- The opt-in probe returns failure for these confirmed losses; the normal routing/reference checks still pass on levels 1 and 11
- The probe preserves raw RNG call counts as diagnostics but excludes their intentional load reset from equality

## Limits and required follow-up

This audit does not certify full-world simulation or fix save persistence. The
probe directly advances live escort navigation and physics at a fixed rate with
a stationary player and other robots removed. It is sufficient to disprove
same-mode continuity, but a full uninterrupted-versus-midpoint-save comparison
still needs the complete frame loop and world/RNG traces

A fix must persist missing escort globals and Enhanced planner/navigation state
in ordinary saves, restore timers relative to the saved clock without behavioral
normalization, and reserve destructive resets for actual routing-mode changes
Older saves need an explicit fallback; demo-only header restoration is not a
substitute for ordinary-save persistence

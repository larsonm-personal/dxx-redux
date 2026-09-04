# Counterstrike level 17 reactor-to-exit door

## Goal

Fix the deterministic GuideBot route simulation stall at the unopened door on the level 17 reactor-to-exit path without weakening key, wall, or trigger traversal rules for other levels.

## Phases

- [x] Reproduce the focused level 17 post-reactor stall and capture the frontier wall, trigger, doorway state, and interaction decision
- [x] Trace the mismatch between planned route semantics and in-engine door activation
- [x] Implement the narrow shared route-confirmation fix
- [x] Add or extend an integration regression that requires level 17 to reach the exit
- [x] Run focused repeated simulations, relevant campaign coverage, Windows builds, CTest, and scoped code quality

## Findings

- The actor stalls in segment 149 beside locked red door wall 0 because exit wall 70 remains closed
- The reactor route includes directional fly-through trigger 5 on segment 149 side 4, which opens wall 70
- The route confirmation controller only executed the currently named trigger objective, so incidental triggers crossed along longer route legs never changed the live world
- Incidental trigger crossings must be source-directional; crossing the reverse portal must not activate the source wall's trigger
- The final rule is limited to needed, persistent `OPEN_WALL` effects crossed on a reactor leg; applying every incidental trigger regressed otherwise working campaign routes through toggle and unrelated effects
- Level 17 now confirms all eight objectives through the exit in 5,169 frames on both repeated runs with identical ending RNG
- A repeated two-run sweep of all 30 Counterstrike levels had no nondeterministic pairs and changed only level 17 from timeout to ok
- The Windows D2 build, all 45 D2 CTest tests, scoped code quality, PSScriptAnalyzer, and the focused level 17 integration test passed

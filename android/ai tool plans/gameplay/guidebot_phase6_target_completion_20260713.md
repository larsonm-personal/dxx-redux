# Guide-Bot Phase 6 Target Completion

## Goal

Close the boss, reactor, and exit portion of Phase 6 passive player guidance before adding blastable-wall semantics and proceeding to invalidation, multiplayer, and legacy-planner removal.

## Plan

- [x] Add a test-only player completion action that damages the active boss or reactor through the ordinary engine damage functions.
- [x] Treat an active end-level sequence as passive completion of the selected exit step.
- [x] Extend the existing KCXF2 boss, Obsidian reactor, and KCXF2 exit fixtures through completion and the next expected route state.
- [x] Verify Guide-Bot remains a passive observer with no route-specific aiming, firing, movement, or RNG changes.
- [x] Run scoped quality checks, D1/D2 builds and native tests, Android tests/build, and focused emulator fixtures.
- [x] Update the main unification plan and identify blastable-wall semantics as the final Phase 6 implementation tranche.

## Boundary

The automation helper is debug-only and acts through player-sourced engine damage. Production Guide-Bot code only observes the resulting world state at the existing four-Hz cadence.

## Results

- Boss and reactor completion use ordinary player-sourced damage in the debug-only automation adapter. Production routing only observes object and control-center state.
- The passive completion monitor runs independently of the companion AI frame, so a dead or temporarily absent Guide-Bot cannot strand a completed high-level objective. It performs no path construction, visibility query, aiming, firing, or RNG work.
- End-level planning preserves the selected exit side and separates the exit face aim marker from an activation point four fixed-point game units inside the exit segment. The legacy shadow planner uses the same temporary projection until Phase 8 removes it.
- KCXF2 level 4 advances from a player-destroyed boss to `exit_pending`; Obsidian level 7 advances from a player-destroyed reactor to `exit_pending`; KCXF2 level 5 reaches `exit_pending` after its directed trigger sequence.
- KCXF2 custom exits immediately replace the level when no rendered end-level sequence exists, so that fixture cannot observe `Endlevel_sequence`. Native coverage verifies the exact exit side and activation pose, while live coverage verifies the terminal pending objective. Rendered exits are passively satisfied when `Endlevel_sequence` becomes active.

## Validation

- [x] D1 and D2 route snapshot and metadata scan native tests pass with zero complete-route shadow mismatches.
- [x] Android debug APK assembled and installed for emulator validation.
- [x] `test_kcxf2_guidebot_route_next.json5` passes.
- [x] `test_kcxf2_guidebot_hidden_door_next.json5` passes.
- [x] `test_obsidian_guidebot_route_next.json5` passes.
- [x] Scoped quality, full D1/D2 builds, all 43 native test executables, Android JVM tests/all-ABI APK build, and focused emulator fixtures pass.

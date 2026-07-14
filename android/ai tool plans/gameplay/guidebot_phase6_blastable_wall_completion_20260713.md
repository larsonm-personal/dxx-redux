# Guide-Bot Phase 6 Blastable Wall Completion

## Goal

Finish Phase 6 by making blastable walls explicit shared semantic route steps that metadata and live Guide-Bot guidance consume identically, while preserving classic Guide-Bot movement and requiring ordinary player damage for completion.

## Plan

- [x] Add a `blastable_wall` semantic kind and `destroy_blastable_wall` activation to the shared route ABI and presentation names.
- [x] Track simulated blastable-wall completion in shared progression state so a selected path emits each required wall once and then continues through it.
- [x] Insert blastable-wall steps from the shared selected segment chain without changing edge ranking, target selection, or classic Guide-Bot path construction.
- [x] Keep the temporary legacy native shadow projection behaviorally aligned until Phase 8 removes it.
- [x] Add passive live satisfaction based only on the selected wall's blasted/open state, plus concise player guidance and automap positions.
- [x] Add native planner tests and a player-assisted emulator fixture that proves the step remains pending until ordinary player wall damage destroys it.
- [x] Run the corpus/status gates, D1/D2 builds and native suites, Android JVM/all-ABI build, focused emulator fixture, and scoped quality checks.
- [x] Mark Phase 6 complete; Phase 7 invalidation, caching, and multiplayer work is next.

## Boundary

The shared planner may choose and describe the high-level wall waypoint. It must not aim, orient, fire, alter collision, construct a route-specific physical path, or consume simulation RNG. The player remains the only actor that damages the wall.

## Results

- Shared and temporary legacy-shadow planners emit the same blastable-wall step from the already-selected segment chain and simulate the paired wall as destroyed before continuing to the endpoint.
- Live Guide-Bot guidance observes only the selected wall's actual type and blasted state. It does not add a route-specific action, aim, firing behavior, physical path, or RNG call.
- The debug-only completion adapter uses an ordinary player-owned weapon impact. The KCXF2 level 1 fixture proves the wall remains pending while the player waits, confirms classic path and RNG parity, then advances to the exit only after wall damage.
- Strict host regeneration passed built-in Counterstrike and 109 mission archives with one expected descriptor-less skip and zero failures. Across all 1,274 reviewed levels, route status and problem text are unchanged; 74 explicit blastable-wall steps were added.

## Validation

- [x] Scoped code quality passes.
- [x] Full D1/D2 Windows builds and all 43 native test executables pass.
- [x] Android JVM tests and all three configured ABI builds pass.
- [x] `test_kcxf2_guidebot_blastable_wall_next.json5` passes end to end.
- [x] The regenerated 1,274-level corpus fingerprint and strict base-campaign status gates pass.

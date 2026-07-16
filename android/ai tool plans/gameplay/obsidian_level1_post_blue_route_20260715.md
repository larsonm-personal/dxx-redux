# Obsidian Level 1 Post-Blue Route

## Goals

- Match in-game objective numbers to metadata numbering, including Start as objective 1.
- Reproduce Obsidian level 1 immediately after collecting the blue key without forcing trigger state.
- Remove invalid pre-red-door switch guidance from both shared metadata and GuideBot routing.

## Plan

- [x] Capture the live post-blue route, keys, trigger states, door states, and GuideBot goal.
- [x] Reproduce the pre-red trigger 5 ray without a distance cap and record its start point, claimed segment, FVI fate, and reported traversal.
- [x] Correct long disconnected FVI visibility across keyed regions in the shared game adapter without mission-specific exceptions.
- [x] Remove the 200-unit route shot limit and reject disconnected rays that geometrically cross a closed keyed-door face.
- [x] Make automap and upper-left objective numbers use canonical metadata indices including Start.
- [x] Make shootable semantic steps retain the selected firing point instead of the switch wall center.
- [x] Extend focused metadata, automap, and GuideBot integration coverage for the post-blue ordering and selected firing endpoints.
- [x] Run scoped quality, route tests, metadata regressions, D1/D2 builds, Android tests, and focused emulator validation.

## Boundaries

- Do not add Obsidian-specific trigger, wall, segment, or coordinate exceptions.
- Keep GuideBot path generation, polishing, following, firing, flares, and RNG behavior unchanged.
- Keep metadata and GuideBot route ordering sourced from the same shared C++ planner.

## Findings

- The rejected trigger 5 candidate was 559.2 units long from segment 167 to wall 89 in segment 286.
- FVI returned `HIT_NONE` with `seglist=167,286`, although segment 167 has no child link to segment 286. The ray therefore never evaluated the intervening red door.
- The 1996 FVI source explicitly notes that its segment list can be incorrect, so unconditional segment-list rejection caused broad metadata regressions.
- FVI repeats the bad jump even without `FQ_TRANSWALL`, so a second recursive FVI call cannot independently validate the first traversal.
- The final fallback reuses FVI's existing face-polygon intersection math without its recursive segment traversal and tests the alleged ray against each closed keyed-door face. There is no distance threshold.
- The full host corpus completed with 109 archives passed, one descriptor-less archive skipped, and no processing failures. Relative to unconditional disconnected-FVI acceptance, four formerly `ok` routes become `partial` because their old solution crosses a closed keyed door: `eq-set` level 8, `lagrange` level 1, `Obsidian` level 10, and `outerrch11` secret level 1. They need play-level review; six routes degraded by the discarded 550-unit workaround return to `ok`.
- Final validation passed D1 and D2 Windows builds, both route snapshot tests, both metadata scan tests, Android unit tests and APK assembly, and all 46 steps of `test_obsidian_level1_objective_markers.json5` on the emulator.

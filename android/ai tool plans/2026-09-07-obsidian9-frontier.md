# Obsidian level 9 trigger-0 frontier

1. Reproduce the post-blue segment-2 frontier stall and inspect its physical blockers
2. Fix the shared live/simulation navigation or interaction logic using the observed cause
3. Verify full-level completion with repeat determinism and a reusable regression
4. Check core routing, relevant builds/tests, and refresh affected regression data

Earlier steering work fixed reaching blue but explicitly left the later trigger-0 frontier unresolved. Latest desktop run 20260907_142751 still stops at frame 1988 after eight frontier retries.

## Completed

The trigger-0 region has a closed entrance opened by trigger 0 itself and an alternate entrance through blastable wall 95 (segment 416 side 2). Trigger firing-path selection crossed the blastable wall without scheduling its destruction. Physical navigation then waited at the closed entrance near segment 2.

The shared dependency planner now clears blastable prerequisites on selected trigger approach paths and recomputes the trigger approach afterward. Ordinary movement uses the same prerequisite helper. No mission-specific behavior or steering changes were needed.

Added a synthetic fly-through prerequisite case to test_route_snapshot and a reusable test_obsidian_level9_frontier_route.ps1 integration test. Refreshed Obsidian metadata and simulation JSON. Metadata changes affect levels 3 and 9; level 3 also needed its existing blastable prerequisite moved before the dependent trigger.

## Validation

- Windows D1 and D2 builds pass
- All 49 D2 CTest tests pass
- Obsidian 9 completes twice with identical results: 9557 frames, 159.283325 simulated seconds, metadata agrees
- Route order: blue key, wall 95, trigger 0, trigger 7, switch 8, gold key, red key, trigger 1, boss, exit
- Core 88-level simulation completes with no previously successful level regressing; Obsidian 9 changes from timeout to ok
- Castaway secret level -1 changes from timeout to route_mismatch in the broader run; its separate metadata discrepancy remains outside this level fix
- Scoped code quality passes
- Android device behavior and the full mission corpus were not tested

Evidence: android/temp/test_obsidian_level9_frontier/run_20260907_143951_059, android/temp/obs9_frontier_core, android/temp/obs9_frontier_metadata, and temp/obs9_frontier_ctest.log

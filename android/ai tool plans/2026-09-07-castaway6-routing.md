# Castaway level 6 trigger dependencies

1. Reproduce the stop after blue and trace the planner's blocked paths and switch dependencies
2. Fix the general dependency/trigger handling that prevents a physically possible route
3. Add a reusable Castaway L6 integration test and verify a complete physical simulation, repeated deterministically
4. Run core routing coverage, refresh affected metadata/simulation records, and apply scoped quality checks

Initial evidence: desktop and headless stop at frame 1059 after switch 3 and blue. Static and live plans report red key unreachable. The user has physically completed the level.

## Root cause and fix

The planner correctly discovers that fly-through trigger 1 restores the switch for trigger 2, but rejects the subsequent firing pose. The actual collision ray reaches the switch face in segment 350 and finishes across that shared face in segment 351. The credibility check insists on ending in segment 350, incorrectly rejecting the connected traversal. The planner then treats trigger 2 as a circular dependency and ultimately reports red unreachable.

The shared visibility check now accepts that boundary endpoint only when the entire segment chain is connected, its penultimate segment is the target wall's segment, and its final segment is the child across the specific target face. Other disconnected rays remain rejected. No level-specific trigger indices, movement shortcuts, or simulation timeout overrides were added. Temporary planner traces were removed.

Regenerated metadata expands L6 from a two-objective partial route to the complete route. Its distance-based time allowance now reflects the actual route rather than the obsolete 79-second limit.

## Verification

- Both Windows engine builds passed; existing weapon ordering return-path warnings are unrelated
- D2 CTest: 49/49 passed
- New integration test: `android/tests/test_castaway_level6_trigger_route.ps1 -NoBuild`
- Two final runs completed all 16 objectives, including both switch restoration sequences, all keys, boss, and exit, identically at 10381 frames (173.016663 simulated seconds), with compact status ok and exact metadata agreement
- Core routing: all 88 levels processed, no infrastructure failures, and no previously successful routes became unsuccessful
- Refreshed Castaway metadata and simulation JSON. The same visibility correction extends L8's partial route; L8 remains unresolved and its live simulation reports an unsupported activation/projection mismatch, not success
- Scoped code quality and diff whitespace checks passed
- Artifacts: `android/temp/castaway6_core_validation`, `android/temp/test_castaway_level6_trigger`, and `android/temp/castaway6_metadata`
- No full mission corpus or Android device test was run

# Counterstrike level 2 switch completion

## Plan

- [x] Reproduce the level 2 unresolved first objective and inspect its trigger links and live completion state.
- [x] Identify the general completion or route-refresh defect without adding Counterstrike-specific handling.
- [x] Implement the smallest shared fix and add focused native and emulator regression coverage.
- [x] Verify D1/D2 native tests and builds, Android assembly, focused mission metadata, and code quality.
- [x] Verify that static metadata is unaffected and record the final behavior.

## Findings

- Counterstrike level 2 objective 1 is trigger 17, an `unlock_door` trigger linked to walls 116 and 117. Firing it clears `WALL_DOOR_LOCKED`; it does not open either door.
- Direct trigger notifications already advanced both trigger 17 and the later unresolved trigger 21 in the device harness. The stale objective was reproducible when trigger 17 fired before an active Guidebot goal, forcing the canonical route to recover from current world state.
- Before the fix, that recovery waited 60 seconds and retained `shoot_switch`. The generic completion predicate recognized only open, opened, or flyable linked walls, so it could not recognize a completed unlock action.
- The shared world-state predicate now treats an `unlock_door` route step as complete when any linked door is no longer locked. Other trigger types retain their existing open/flyable rules.
- The focused device regression now fires trigger 17 before requesting Guidebot guidance, verifies that Guidebot selects the blue key instead of the already-fired switch, then fires unresolved trigger 21 and verifies the red key becomes next. The same script failed before the fix and passes after it.
- Historical metadata confirms trigger 21 became unresolved when routing changed from permissive wall visibility to projectile-valid shootability. Its earlier `shoot_switch` route was therefore based on the old line-of-sight rule, not the current collision-valid proof. This fix does not restore that unproven firing pose.
- D1 and D2 metadata-scan and route-cache tests, D1/D2 Windows builds, Android all-ABI debug assembly, scoped code quality, and `git diff --check` passed.
- Focused Counterstrike metadata remains `ok`; trigger 21 remains explicitly not calculated and no generated metadata changed.

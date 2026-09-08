# Obsidian level 11 post-gold door stall

1. Inspect the repeated wall-80 closed-trigger-door replans after gold key and switch 16
2. Correct the shared door interaction decision using live wall state
3. Verify full-level repeatability and add a regression test
4. Build both engines, run native tests and core routing, and refresh affected simulation data

## Findings and implementation

The baseline reaches gold at frame 2735 and switch 16 at frame 3145, then repeatedly replans at wall 80 until its 286-second budget expires. Wall 80 is a locked door with pass-through trigger 14. Switch 17 unlocks it; trigger 18 locks it. The single controlling_trigger field points to 18, which is not an opening prerequisite.

The shared planner prepared non-shootable trigger surfaces only when their wall kind was closed. A closed locked door retained kind door, so its unlock prerequisite was omitted. Preparation also recognized only changes in wall kind, overlooking successful unlock/open effects that preserve that kind.

The planner now prepares closed locked door sources and recognizes changes in lock/open state. Open locked doors remain crossable. No simulation-only door override is needed. Temporary diagnostic logging was removed.

Added focused unlock/open prerequisite cases to test_route_snapshot and a reusable test_obsidian_level11_door_route.ps1 requiring deterministic full completion, switch 17 before crossing trigger 14, and agreement with regenerated metadata. Only level 11 metadata changes.

The first physical completion is 7034 frames (117.233322 seconds), including red key, reactor, and exit. Evidence: android/temp/obs11_door_diagnostic, android/temp/obs11_door_fix, and android/temp/obs11_door_metadata

## Final validation

- Both Windows engines build; no new warnings in changed code (existing D1 weapon.c return-path warnings remain)
- All 49 D2 CTest tests pass
- Level 11 integration test passes with two identical 7034-frame results and matching metadata
- Core 88-level run completes with no previously successful routes regressing; level 11 is the only status change, timeout to ok
- Refreshed Obsidian simulation JSON from the core run
- Scoped code quality and git diff checks pass
- Android device behavior and the full mission corpus were not tested

Final evidence: android/temp/test_obsidian_level11_door/run_20260907_160907_455, android/temp/obs11_door_core, temp/obs11_final_build.log, and temp/obs11_ctest.log

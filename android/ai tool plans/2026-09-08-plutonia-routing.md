# Plutonia routing development

Add Plutonia to the shared targeted metadata/simulation selection and verify all 32 levels are included (six missions, 152 levels). Refresh its simulations with the current engine, select a reproducible failure, diagnose before changing behavior, and retain general fixes with focused deterministic coverage and the expanded corpus. TEW 9 and Counterstrike secret -5 remain deferred. Do not edit outstanding_bugs.md or place implementation in headers.

## Findings and implementation

- Added `plutonia.json` / `plutonia.zip` to the shared metadata and simulation mission selection, with selection/count assertions
- Chose Plutonia 11, Stygian Meander. Baseline stopped after blue and red keys. Switches 2 and 13 sit behind doors that open only after the boss dies; attempting to shoot through those permanently locked hidden doors produced a false dependency
- Conditional switch-shot visibility now requires an opener for a locked hidden door
- The boss in segment 532 is separated from the player's route by impassable grates. Its arena doors open through trigger 7, whose switch room first requires crossing trigger 6. A flyable path to the boss cannot discover this dependency
- Added a native visibility query that treats one nominated wall and its opposite face as open without mutating the level. The planner tests reachable firing positions, including offsets by the navigator radius, then plans the actual opener and its dependencies. An unlock-only action does not prove an open firing line. This fallback uses topology and engine ray tests, with no mission or segment constants in implementation
- Simulation boss/reactor visibility now uses `FQ_TRANSPOINT`, matching weapon rays through transparent openings instead of treating every grate as solid
- Plutonia 11 now completes blue key, red key, pass-through 6, switch 7, boss, exit in 4786 frames. Repeated runs match exactly
- The general fix also finds a blue-key-only route for TEW 15 through triggers 2, 6 and 16. Refreshed metadata and the existing budget regression test; it still records the optional optimization budget note and completes deterministically in 5659 frames
- Plutonia 16 now has a complete planned route but its physical simulation still times out. It remains a failure in simulation JSON

## Validation

- Windows builds for D1 and D2 pass; final build log `temp/plutonia_final_build.log`
- All 49 native tests pass, including new gated-boss and unlock-only cases in `test_route_snapshot.cpp`; `temp/plutonia_final_ctest.log`
- Scoped formatting/lint passes; `temp/plutonia_quality.log`, `temp/plutonia_tew_quality.log`
- Mission selection and regeneration stage tests pass; `temp/plutonia_final_selection_test.log`, `temp/plutonia_final_stages_test.log`
- New `android/tests/test_plutonia_level11_boss_grate.ps1` passes twice with identical raw results and agreement with metadata; `temp/plutonia11_final_integration.log`
- Existing TEW 15 deterministic integration passes; `temp/plutonia_tew15_integration.log`
- Full six-mission run: 141/152 pass, versus baseline 140/152. Castaway 10/10, Counterstrike 29/30, First Strike 30/30, Obsidian 18/18, TEW 31/32, Plutonia 23/32. No previously passing level becomes a failure; `android/temp/core_six_plutonia_verified`
- Refreshed Plutonia/TEW metadata and changed normalized simulation results from the generators. Counterstrike -5 is still a route mismatch and TEW 9 still times out; neither was investigated

Remaining Plutonia simulation failures: 1, 4, 5, 7, 10, 16, 22, 29, 30. The new visibility fallback handles a door bordering the primary target's segment; chains of multiple sight-blocking doors are not solved by this change.

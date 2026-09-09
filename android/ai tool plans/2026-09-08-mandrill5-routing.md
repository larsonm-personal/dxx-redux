# Mandrill level 5 routing

Reproduce the fresh frame-zero failure in Mandrill Battleship, compare live and static reachability, and identify the blocked dependency using engine diagnostics. Implement a general correction in source files if supported by the evidence, then verify repeated focused completion and the small regression set. Preserve the two deferred failures and leave outstanding_bugs.md untouched

Baseline: android/temp/mandrill5_baseline fails at frame zero with blue key unreachable in the live plan. Saved metadata instead reaches the blue key and reports red key unreachable

## Confirmed clearance correction

Mandrill's companion radius is 331815, versus player radius 310312. The topology cache recorded all ordinary clearances as 310312 while the live query compared them with 331815. This incorrectly forced normal rooms into transit-only states, excluding the blue-key dead end at 185. Rebuild clearance and transit masks for the maximum of the player and queried actor radius, and invalidate the geometry cache when that radius changes

With the correction, L5 physically collects blue at frame 647. The 262-level corpus at android/temp/mandrill5_radius_corpus preserves every previous pass and adds Mandrill L2, completing at 4271 frames. Mandrill L1 also advances through fly-through 0 and the red key before its remaining failure, changing timeout to failed

## Remaining L5 dependency

Gold key 256 requires trigger 5, wall 10 in segment 76. Red key 250 then requires the gold door at 207:2. The switch cannot be approached directly while those keys are pending. Shot traces from reachable segments 66, 67, and 68 hit buddy-proof closed grates 117, 81, and 119, with textures 267, 264, and 267. The planner falls back to walking to source segment 76 and cannot resolve that dependency

Artifacts: mandrill5_walls records engine topology and triggers; mandrill5_move records dependency targets; mandrill5_shots records actual ray blockers. All are under android/temp. An experiment adding more interior firing samples did not improve completion and was reverted, along with every temporary diagnostic. No mission-specific engine behavior was added

The retained integration test repeats L2 to full completion and L5 through blue-key pickup, checking deterministic results and that the fixture actually has a companion larger than the player. It deliberately does not claim L5 completion

## Validation

Final Windows D1 and D2 builds pass. All 49 D2 CTest cases pass. Scoped formatting/lint passes. The new integration test passes with identical repeated raw results: L2 completes at 4271 frames, and L5 reaches blue at 647 frames before its separate unresolved switch dependency. Logs are temp/mandrill5_final_build.log, temp/mandrill5_ctest.log, temp/mandrill5_quality.log, and temp/mandrill5_integration.log

The small-set result is 238 ok, 15 failed, 8 timeout, and 1 unsupported, preserving all 237 prior passes. Refreshed Mandrill.simulation.json from the corpus output to record L2 completion and the additional progress in L1/L5. Static metadata remains unchanged

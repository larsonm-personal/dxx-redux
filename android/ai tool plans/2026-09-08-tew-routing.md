# Five-mission routing development set

Add The Enemy Within (TEW.json / TEW.zip) to the shared metadata and simulation selection and update selection coverage. Counterstrike secret -5 stays deferred.

Review the checked-in TEW failures, rerun all 32 TEW levels with the current engine, and distinguish static planning failures from physical navigation and interaction failures. Pursue a representative reproducible failure using geometry and engine state; avoid mission-specific identifiers or forced completion. Keep unsuccessful cases visible in regression JSON.

Validate retained changes with native tests, focused deterministic simulation, both Windows builds when native code changes, scoped quality checks, and the expanded 120-level corpus. Do not edit outstanding_bugs.md.

## Initial evidence

Current-engine TEW baseline android/temp/tew_initial reproduces all eight checked-in failures: level 1 after blue key; level 9 at frontier 117 toward gold key 656; level 13 after opening the blue-key frontier at 106; level 15 exhausts shared planning work after blue key; level 20 cannot extend frontier 372 toward boss 48; level 23 cannot extend frontier 77 toward red key 80; level 26 repeatedly returns from switch 10 to fly-through trigger 11; secret -3 stalls during pass-through trigger 5. Levels 9 and 15 also have incomplete static metadata. These observations classify symptoms, not proven causes for the uninvestigated levels.

## TEW level 1

The actor stops in segment 85 near the planned firing pose in adjacent segment 86, targeting switch wall 34. Native level_metadata_wall_shootable_from_position confirms the shot from the actor's actual position. Simulation required exact segment membership even though the task was shooting, not crossing into the recess. Retain normal same-segment behavior; additionally accept a nearby adjoining-room position for switch actions only, within the existing arrival tolerance and only with the native shot test. Key pickups, fly-through triggers, and physical crossings retain their separate requirements. No TEW identifiers or geometry constants appear in engine behavior.

Two focused runs complete all seven objectives and exit at frame 3631, including verified adjoining-room shots at both switches. Added test_tew_level1_adjacent_switch.ps1 for repeatability, objective order, native-shot evidence, and metadata agreement. The selection test now checks five missions, all 32 TEW levels, and 120 total levels.

This is an example of the generalization issue raised by the user: segment identity was acting as a substitute for action reachability. Future failures should similarly be resolved through the relevant engine capability (shooting, contact, traversal, or trigger state), rather than broad tolerances or per-mission exceptions.

## Validation

The shared mission list and regression menu now include The Enemy Within; metadata resolves TEW.zip and simulation resolves TEW.json. Discovery/sampling and regression-stage tests pass. Both Windows engines build (temp/tew_verified_build.log), all 49 native tests pass (temp/tew_ctest.log), scoped quality checks pass (temp/tew_quality.log), and the focused level test passes (temp/tew1_integration.log). The 120-level core_five_tew_verified run has 112 ok, no formerly passing regressions, and only TEW level 1 changes status. Final corpus verification uses the rebuilt executable before refreshing checked-in simulation JSON. Static metadata is unchanged by this simulation-only correction. Android device behavior was not tested.

Final rebuilt-engine corpus android/temp/core_five_tew_final confirms 112/120 ok with zero previously passing regressions. TEW is 25/32; remaining TEW failures are 9, 13, 15, 20, 23, 26 and secret -3. Counterstrike secret -5 remains deferred. Updated the changed simulation JSON files after comparing every level against the checked-in baseline. The sole status improvement is TEW level 1; other changes reflect earlier valid switch activation timing. No static mission JSON changes were needed.

## TEW level 26 follow-up

Trace why the route repeatedly reactivates fly-through trigger 11 but reaches its controlled door closed before shooting switch 10. Compare authored door timing, current state, and the installed physical path. Retain a general correction with deterministic level and five-mission validation; Counterstrike secret -5 remains deferred.

The timed doors are behaving as authored. Trigger 11 opens door 26, but the physical waypoint route goes 161 -> 160 -> 162 -> ... -> 156 -> 157 and arrives after the door closes. Native visibility proves switch wall 34 is shootable from 161/160 shortly after opening, through the authored line of fire. The mirrored trigger 12 / switch 9 arrangement behaves the same way. Simulation was treating the preferred firing waypoint as mandatory instead of taking a verified shot from the current position. Expand the previous nearby-shot rule to accept any engine-verified shot for the currently selected switch objective, preserving physical frontier prerequisites and existing action types. No door timer, movement speed, budget, or mission-specific behavior changes.

Initial repeat-2 completion: 6277 frames, all 11 objectives and exit; each timed switch is hit 20 frames after its fly-through opener. Added test_tew_level26_timed_switches.ps1 to check both corridor shots, prompt activation, exact objective order, deterministic results, and metadata agreement. Temporary geometry/timing probes removed. Full five-mission validation follows.

Final TEW 26 validation: both Windows engines build (temp/tew26_verified_build.log), all 49 native tests pass (temp/tew26_ctest.log), and scoped quality checks pass. Dedicated repeat-2 tests confirm TEW 26 at 6277 frames and TEW 1 at 3562 frames, with deterministic JSON and metadata agreement. Full 120-level run android/temp/core_five_tew26_probe has 113 ok, no previously passing regressions, and only TEW 26 changes status. Refreshed changed simulation JSON; no static metadata change. TEW now passes 26/32. Remaining TEW: 9, 13, 15, 20, 23, secret -3. Counterstrike secret -5 remains deferred. Android device behavior was not tested.

## TEW level 23 follow-up

Investigate the unopened frontier at segment 77 after the blue key. Inspect wall state and visibility before changing shared interaction rules, then run deterministic level coverage and the five-mission corpus. Counterstrike secret -5 remains deferred.


Wall 0 at 77:0 is an unlocked blue-key door with animation flags 12 (including WCF_HIDDEN). The actor owns the key and is at its frontier, but both the live certifier and flare eligibility required exploration beyond the hidden wall. Static planning correctly treated it as a keyed door. Allow owned-key doors with that animation flag to remain player-reachable and identify them as player-assisted frontiers; permit physical flares with the same key/lock validation. Keyless hidden-door discovery remains unchanged. No mission identifiers occur in engine behavior.

The focused integration test completes all four objectives at frame 3819 in two identical runs and verifies a physical flare at the blue-key door. Native coverage includes missing-key and locked-door rejection for the same hidden animation. Both Windows engines build (temp/tew23_build.log); all 49 native tests pass (temp/tew23_ctest.log). Scoped mixed-language quality passes. The D1 full rebuild emits existing POrderList/SOrderList return-path warnings in untouched weapon.c; changed code adds no warnings. Android device behavior was not tested. Full corpus comparison follows.

Final corpus android/temp/core_five_tew23_verified passes 114/120 with no previously passing regressions. TEW is 27/32; only TEW level 23 changes status (timeout to ok), and all other mission simulation files are byte-identical. Updated TEW.simulation.json; static mission metadata remains unchanged. Remaining TEW failures: 9, 13, 15, 20, secret -3. Counterstrike secret -5 remains deferred.

## TEW level 13 follow-up

Inspect waypoint advancement and swept clearance after the blue-key door opens. Diagnose the stalled approach before altering shared navigation; validate any correction with deterministic integration and all five missions.

The blue-key door opens normally. The actor consumes the adjacent approach waypoint, then stops at the portal rim while steering toward the following segment. Reusing the prior point only while the next leg is blocked still oscillates at the clearance boundary. The retained recovery activates after sustained lack of actual movement, verifies a full-radius ray to the preceding approach, and finishes that approach before retrying the turn. It preserves the path cursor, objectives, collision radius, and normal door rules. The reusable engine helper lives in guidebot_path_recovery.h with a small D2 companion hook; D1 has no companion equivalent.

TEW 13 completes all five objectives at frame 4026 in two identical runs. Added test_tew_level13_corner_recovery.ps1, including recovery-log evidence and metadata agreement. Both Windows engines build and all 49 native tests pass. Scoped quality covered the shared header and PowerShell test; upstream-style d2/main/aipath.c is excluded by the formatter configuration. Temporary probes and unsuccessful alternatives removed. Android device behavior was not tested.

Final validation: android/temp/core_five_tew13_verified passes 115/120 with no previously passing regressions. TEW is 28/32; level 13 is the only status improvement. Other differences are physical recovery timing, including faster Counterstrike 9 and 18 and some slightly longer successful routes. Refreshed all five changed simulation JSON files. Static mission metadata is unchanged. TEW 26 timed-switch repeat-2 integration also passes at frame 6272. Build evidence: temp/tew13_verified_build.log; native tests: temp/tew13_ctest.log; integration: temp/tew13_integration.log. Remaining TEW failures: 9, 15, 20, secret -3; Counterstrike secret -5 stays deferred.

## Recovery implementation organization

Move the stateful recovery implementation from the header to a compiled C source, leaving a declaration-only header. Register it in the shared D2 source list used by host and Android builds. Review recent related routing headers for similar implementations, then verify builds and unchanged deterministic TEW 13 results.

Moved recovery into guidebot_path_recovery.c and the related metadata progress policy into android_route_metadata_progress_policy.c; headers now contain declarations and types only. Updated host/Android CMake and the existing progress-policy test target. Other recent routing and texture-diagnostic headers contain no similar function implementations. Both Windows engines build; the final D2 rebuild compiles and links the extracted progress policy, and all 49 native tests pass. TEW 13 repeat-2 integration remains deterministic at 4026 frames. Scoped quality passes. This organization-only follow-up does not regenerate simulation JSON or claim Android device validation.

## TEW level 20 follow-up

Inspect the physical frontier at segment 372 after all keys, toward the boss. Diagnose authored wall properties and live interaction before changing shared routing. Validate retained correction with deterministic level coverage and the five-mission corpus. Keep implementations in source files; Counterstrike secret -5 stays deferred.

TEW 20's goal is the boss. Wall 130 at 722:3 is an ordinary unlocked auto-closing door; its reverse face (wall 131 at 723:1) is a countdown link. The live certifier blocked both directions until the countdown, losing the strategic chain and choosing nearby segment 372 with nothing to interact with. Treat countdown opening as an additional access mechanism, then apply normal approached-face door checks before countdown. The earlier Buddy-proof hypothesis was based on misreading flag 16 and was discarded; no corresponding change remains. All temporary probes removed.

Initial completion is 8752 frames. Added a repeat-2 TEW 20 integration test requiring the five-objective sequence, physical player door interaction, deterministic JSON, and metadata agreement. Native coverage checks directional countdown-door access and preserves locked-door and missing-key rejection.

Focused repeat-2 integration passes at 8752 frames (temp/tew20_integration.log). Both Windows engines build (temp/tew20_verified_build.log); all 49 native tests pass (temp/tew20_ctest.log), including ordinary access opposite a locked countdown face, locked countdown-only frontier behavior, and key requirements. Scoped quality passes. The full D1 rebuild still emits the existing untouched weapon.c return-path warnings. Android device behavior was not tested. Full five-mission comparison follows.

Final corpus android/temp/core_five_tew20_verified passes 116/120 with no previously passing regressions. TEW is 29/32; only level 20 changes status. Refreshed the two changed simulation files (TEW and Castaway Redux); Counterstrike, First Strike, and Obsidian are byte-identical. No static metadata change. Remaining TEW failures: 9, 15, secret -3. Counterstrike secret -5 remains deferred.

## TEW level 9 follow-up

Trace the planned gold-key route against the live physical frontier, then inspect the incomplete reactor route. Retain general fixes only and verify deterministic simulation plus the five-mission set. Preserve prior uncommitted TEW 20 work and defer Counterstrike secret -5.

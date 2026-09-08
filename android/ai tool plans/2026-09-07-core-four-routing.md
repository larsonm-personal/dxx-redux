# Core four routing and physical confirmation

Goal: every level in First Strike, Counterstrike, Castaway Redux, and Obsidian has ok routing metadata and ok Guide-Bot simulation, verified with the full 88-level development set

1. Audit current metadata and simulation failures against native logs
2. Fix shared planner and physical interaction defects, retaining authored locks and triggers
3. Add focused regression coverage and verify affected levels through their exits
4. Refresh affected metadata and simulation data and rerun all 88 levels

Initial audit: First Strike all ok. Counterstrike secret -5 routing/sim failed. Castaway 5, 7, 8 routing partial; simulations 1, 8, 9 mismatch, 5 unsupported, 7 failed. Obsidian 10 routing partial and simulation unsupported; 12 and 13 simulation timeout. Earlier Vertigo checkpoint edits remain in the worktree and must be preserved.

## Door interaction progress

Obsidian 13 required opening hidden door 55 from segment 342 through a grate, then shooting switch 13 through the open door. The flare visibility check rejected the intervening transparent surface, and the simulator incorrectly required crossing the remotely opened door. Direct flare hits retain their existing behavior; a second visibility check permits transparent pixels in intervening walls. Remote hidden-door objectives complete when the door opens instead of demanding a crossing.

Obsidian 12's post-gold stall was a missed nearby doorway: the AI cursor had advanced beyond the actor's actual segment. Flare target search now falls back to the actor's location in the existing path when its usual lookahead finds no door. It now completes trigger 18, switch 12, and switch 7. A later path stall toward switch 6 remains: actor segment 438, semantic target 0, path ends at segment 438. Evidence: android/temp/core_four_door_final/logs/Obsidian.json_0_12_lususfls.rl2_run_1.log

## Verified current state

- Obsidian 13: two identical full completions, 6955 frames, metadata matches; new test_obsidian_level13_remote_door_route.ps1 passes
- Both Windows engines build; final D2 rebuild and all 49 native tests pass
- Full 88-level simulation: no previously successful level regressed, Obsidian 13 changes timeout to ok
- Simulation totals: First Strike 30/30, Counterstrike 29/30, Castaway 5/10, Obsidian 16/18; total 80/88 ok
- Refreshed all four simulation JSON files from android/temp/core_four_door_final
- Scoped quality and diff checks pass

The goal remains active. Remaining simulations: Counterstrike -5; Castaway 1, 5, 7, 8, 9; Obsidian 10, 12. Route metadata still needs Counterstrike -5, Castaway 5/7/8, and Obsidian 10. Castaway mismatch statuses can conceal real unsupported/time-out results; inspect the native per-run JSON as well as the compact status. Android device behavior was not tested.

## Obsidian 12 switch approach

The native path safety check collided with switch wall 76 at the final segment center, excluded the preceding segment 84, then accepted a truncated replacement ending at 438. Keep the final approach when that collision is against the active shooting objective itself. Normal physical collision and shooting remain authoritative; other walls and intermediate blocked waypoints retain their checks.

Targeted repeat test completes all 16 objectives and the exit identically in 7899 frames, with metadata agreement. Added test_obsidian_level12_switch_approach.ps1. Removed temporary PATH diagnostics. Full core simulation and relevant build validation follow.

Validation: full 88-level run at android/temp/core_four_switch_approach has 81 ok, with only Obsidian 12 changing timeout to ok and no regressions. Obsidian now has 17/18 ok. Updated Obsidian.simulation.json from that run. D2 Windows build, all 49 native tests, scoped quality, and diff checks pass. Remaining seven: Counterstrike -5; Castaway 1, 5, 7, 8, 9; Obsidian 10. Remaining route metadata failures unchanged.

## Castaway 1 recovery result identity

The live certifier correctly substitutes trigger 20 to restore the vanished switch 2 surface at wall 81. It uses the original route index for that temporary objective, so strict activation-kind comparison incorrectly rejected a confirmed physical run. The certifier now marks recovery and identifies its restored wall. Physical results publish restores_switch_wall for that action. Projection validation accepts only a recovery for the next planned shooting objective's exact wall and still requires the original objective. Objective deduplication distinguishes recovery from the planned step, including shootable restorers.

The tightened Castaway 1 integration test requires ok plus two identical exit completions and explicit restoration of wall 81. Schema tests reject an unrelated wall, omitted original objective, and out-of-order recovery. Native certifier coverage checks recovery provenance, including valid wall zero.

Final validation: both Windows builds and all 49 native tests pass. Full 88-level run at android/temp/core_four_restorer_final has 82 ok and no regressions. Updated Castaway simulation JSON; level 1 now ok, level 8 now correctly unsupported and level 9 timeout instead of masking those real outcomes as route mismatches. Schema tests, targeted deterministic Castaway 1 integration, scoped quality, and diff checks pass. Remaining six simulations: Counterstrike -5; Castaway 5, 7, 8, 9; Obsidian 10. Static route failures remain Counterstrike -5; Castaway 5, 7, 8; Obsidian 10. Counterstrike -5 has only Start at segment 75 and route target unreachable before simulation begins; inspect planner reachability next.

## Counterstrike secret -5 investigation

Verified initial route failure from segment 75. Reactor at segment 6 is enclosed by walls 46-53, opened by trigger 11 (shootable wall 152, segment 101 side 4). The planner successfully plans switch 11, then unlock trigger 2 (position segment 117), then needs trigger 5 (wall 139, segment 178 side 4). A passive crossing closes gate 31:4 (wall 155) via trigger 13 at 32:5. Its opener is trigger 14 at 74:5. Rearming that ordinary opener advances the dependency search, but then it cannot return from segment 27 to opener segment 74. This is a one-way progression/order issue, not reactor visibility.

Evidence: android/temp/cs5_sequence_diagnostic/logs/Counterstrike.json_0_-5_d2leve-s.rl2_run_1.log (dependency sequence); android/temp/cs5_enclosure_diagnostic (all native wall/trigger definitions); android/temp/cs5_state_diagnostic (boundary after ordinary opener rearming).

Rejected experiments: detailed reactor visibility samples did not help; trying each opening trigger as the first action did not help; broadly rearming ordinary-wall trigger effects did not complete -5 and regressed several corpus levels (including Castaway secret timeout). All experimental code and temporary logging withdrawn. Keep the 82/88 baseline and investigate preservation of future switch access across one-way transitions. Do not repeat the visibility-only or broad-rearming approach.

Restoration verified: both Windows engines rebuilt successfully (temp/core_four_restored_build.log); all 49 native tests pass (temp/core_four_restored_ctest.log). Planner and snapshot test files have no diff from the 82/88 baseline. Simulation corpus files were not replaced with experimental results.

## Castaway 9 invalid reactor objective

A 420-second diagnostic run confirms this is not a short time limit. The bot reaches segment 275 but target_pos_valid=0 and target_objnum=-1. Native object 3 there is OBJ_GHOST (12), not OBJ_CNTRLCEN; d2/main/cntrlcen.c:init_controlcen_for_level ghosts the ordinary reactor when a boss exists. The planner's fallback from an unreachable boss to the reactor therefore produces a false completing route. Removed that fallback and corrected the existing high-level metadata scan test: an inaccessible boss plus reachable reactor must remain incomplete.

Actual boss: object 2 at segment 43, behind closed wall 6 on 41:5. Boss simulation AI is frozen by remove_ordinary_robots, which preserves bosses but sets CT_NONE/MT_NONE. D2 ai2.c:init_boss_segments supports engine teleport destinations beyond one wall when its initial room has no useful destinations. Further work must establish real boss reachability/emergence and route to a legitimate encounter; do not substitute the ghost reactor or fabricate completion. Evidence: android/temp/cast9_actor_diagnostic (actor/object state), temp/cast9_extended.log, android/temp/cast9_primary_diagnostic (boss and optimistic boundary), android/temp/cast9_boss_diagnostic (honest failure after red key).

Boss-objective correction validated: both engines build, all 49 native tests pass, scoped quality and diff checks pass. Castaway host metadata regenerated successfully; only L9 metadata changes, now partial with route target unreachable. Full core run android/temp/core_four_boss_objective retains all 82 previously ok simulations; L9 now fails honestly after red key instead of waiting at a ghost reactor. Updated Castaway metadata and simulation JSON. Remaining six simulations unchanged; six static routes now honestly incomplete (-5 Counterstrike; 5/7/8/9 Castaway; 10 Obsidian). Goal not complete. Next boss work should read actual ai2.c teleport-segment construction and awareness activation, rather than treating any sealed room as an automatic valid encounter.

# Guide-Bot destination stop analysis

## Scope

Read-only investigation of Guide-Bot stopping near a requested destination, especially the yellow key in Counterstrike level 24. Do not run an emulator or change game code in this tranche.

## Plan

- [x] Research original Descent II behavior from primary source code and supporting contemporary material
- [x] Trace current escort goal selection, path creation, path following, arrival, retry, and return-to-player behavior
- [x] Compare this tree with relevant original and upstream implementations and identify behavioral deltas
- [x] Enumerate plausible stopped-in-place states and rank them against the reported symptom
- [x] Design focused automated and emulator tests that protect currently working Guide-Bot behavior
- [x] Record conclusions and low-risk next-step recommendations

## Completion notes

The remembered original behavior is supported by the 1996 source. Normal path endpoints reverse or circle the path, and a one-point circle falls back to a short path toward the player. `do_escort_frame` also replaces paths shorter than three points with a five-segment wander path. A four-second unseen-player timeout and a path-midpoint check can independently send the Guide-Bot back to the player. The explicit stationary case is the SCRAM goal after the player is no longer visible.

The Android semantic routing layer deliberately accepts one- and two-point paths and suppresses the classic midpoint return while a semantic route is active. This preserves exact/frontier guidance, but removes two original liveness safeguards. The current stall monitor also does not count a stalled target when the target path point is in the Guide-Bot's current segment.

The same-session cooperative level 24 log confirms the reported state. The selected gold-key goal is segment 281. After the Guide-Bot reaches segment 281, each periodic refresh creates `len=1 segments=[281]`. For more than 25 seconds it remains in `AIM_GOTO_OBJECT` near that point, with approximately 0.13 down to 0.008 fixed-point units of sampled movement per second, while the log emits `suspected_spin`. This is a same-segment one-point endpoint livelock. Earlier in the run, the physical routing frontier also produced short partial paths such as `[315,160]` for semantic target 281, which can make the bot patrol a frontier rather than visibly progress. The trace does not expose the cooperative owner, so the next test must distinguish an owner-side endpoint transition failure from a non-owner pose-replica/ownership scheduling failure.

The intended architecture is a strict two-layer contract. Semantic routing selects the objective, a reachable general-area segment waypoint, and the instruction shown to the player. The original escort and path-following code owns physical movement, short-path wandering, endpoint patrol, return-to-player behavior, and recovery. Semantic routing must not substitute an exact activation or firing coordinate into the lower-level path. Route compilation may still use activation geometry internally to choose a useful guidance segment, but Guide-Bot should navigate to that segment using an ordinary segment-center path and tell the player that a switch must be found and shot.

There will be no short-path exception for firing positions. Losing exact pose placement is intentional because it provides more assistance than desired. The upgraded functionality to preserve is correct objective ordering, reachable waypoint selection, frontier selection, and useful textual hints.

## Revised implementation tranche

- [x] Add Guide-Bot position, velocity, target distance, liveness age, and cooperative owner/replica state to introspection
- [x] Restore the original direct short-path fallback for ordinary and semantic paths, with endpoint liveness covered by automation scenarios
- [x] Stop applying semantic `target_pos` as the final physical path point; retain only the selected guidance segment and player-facing switch hint
- [x] Remove semantic suppression of the original path-midpoint return-to-player decision while preserving the semantic objective across the return trip
- [x] Ensure returning to the player does not clear or downgrade the compiled route and that waypoint guidance resumes afterward
- [x] Add a Counterstrike level 24 gold-key liveness test that rejects sustained one-point `AIM_GOTO_OBJECT` parking in segment 281
- [x] Change switch-route tests to assert arrival in the useful general-area segment plus the correct shoot-switch instruction, not an exact firing pose
- [x] Preserve coverage for nearby objectives, long routes, unreachable frontiers, single-player, cooperative owner simulation, and cooperative replica synchronization
- [ ] Run focused host tests first, then the designed emulator scenarios when the emulator is available

Expected behavior change: near a destination or frontier, Guide-Bot can twiddle, wander, or return to the player just as in the original game. It may no longer place itself at a specially calculated switch-firing coordinate. Objective selection, general-area guidance, route continuation after world-state changes, and hints remain upgraded.

## Cleanup and simplification opportunities

Treat route-planning geometry and live escort movement as separate boundaries. The planner may retain activation positions, visibility tests, and shot-quality calculations when they are needed to select a useful general-area segment. Do not copy that exact geometry into the live `escort_route_goal` or physical AI path.

### Definite removals

- [x] Remove `target_pos_valid` and `target_pos` from `escort_route_goal`
- [x] Remove `path_endpoint_pos_valid` and `path_endpoint_pos` from `escort_route_goal`
- [x] Delete `escort_route_apply_target_pos` and its call from `escort_create_path_to_goal`
- [x] Delete exact target/path-endpoint position getters and the corresponding introspection fields and equality assertion
- [x] Remove exact-position comparisons from passive route-adoption logging and decisions
- [x] Replace `escort_path_needs_fallback(path_length, semantic_route_active)` with the original direct `path_length < 3` rule, then remove the semantic-specific helper and test cases
- [x] Delete `escort_semantic_route_suppresses_midpoint_visit` and its branch in `time_to_visit_player`

### Runtime-state consolidation

- [x] Remove stored path-endpoint segment bookkeeping; derive the current endpoint from the ordinary AI path when diagnostics need it
- [x] Replace route-specific `route_goal_path_pending` plumbing with the already exposed ordinary path length, index, and direction state
- [x] Reduce the live route goal to semantic objective identity plus one current segment waypoint; avoid parallel `target`, `guidance`, and `path endpoint` representations of the same movement destination
- [x] Consolidate duplicate wall, trigger, and side fields where `target_*` and `objective_*` always identify the same semantic step
- [x] Replace `REACH_FIRING_POSITION` as a movement mode with a general switch-area instruction derived from `activation_kind`
- [x] Use a single generic find-and-shoot-switch hint at runtime; remove live route-goal shot-quality and incidence fields if they no longer affect that hint or adoption identity
- [x] Keep an explicit nearest-progress/frontier marker only if needed for the "as close as possible" hint; do not retain a broad guidance-mode enum solely for introspection

### Keep deliberately

- [x] Keep planner/certifier activation geometry and line-of-sight analysis when it is required to choose the correct guidance segment
- [x] Keep semantic objective ordering, world-state completion monitoring, event invalidation, cache/audit governance, and cooperative authority handling
- [x] Keep physical-frontier selection for currently unreachable objectives
- [x] Keep different-segment stalled-edge avoidance unless classic movement and new liveness tests prove it redundant
- [x] Keep shot-quality data in planner diagnostics and certification tests if it remains useful for route validity, even though it is removed from live movement behavior

### Test cleanup

- [x] Remove emulator assertions for exact target position, exact path-endpoint position, and endpoint/target coordinate equality
- [x] Replace stored route-endpoint assertions with generic AI path and Guide-Bot movement assertions
- [x] Rename firing-position expectations to switch-area guidance and assert the selected segment plus the generic find-and-shoot hint
- [x] Retain planner-level geometry tests separately from Guide-Bot movement tests so internal route certification is not confused with player-facing precision guidance
- [x] Run cleanup after behavior tests are in place, in small compilable steps: remove movement override, restore classic policies, migrate tests/introspection, then collapse redundant state

## Implementation notes

Implemented without running an emulator. The Windows D2 build, all 44 D2 host tests, scoped code-quality checks, and the Android debug build for all configured ABIs pass. The Counterstrike level 24 liveness scenario and migrated switch/cooperative scenarios are ready for the next emulator pass.

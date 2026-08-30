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

- [ ] Add Guide-Bot position, velocity, target distance, liveness age, and cooperative owner/replica state to introspection
- [ ] Add policy tests establishing that every ordinary and semantic path shorter than three points receives the original five-segment fallback
- [ ] Stop applying semantic `target_pos` as the final physical path point; retain only the selected guidance segment and player-facing switch hint
- [ ] Remove semantic suppression of the original path-midpoint return-to-player decision while preserving the semantic objective across the return trip
- [ ] Ensure returning to the player does not clear or downgrade the compiled route and that waypoint guidance resumes afterward
- [ ] Add a Counterstrike level 24 gold-key liveness test that rejects sustained one-point `AIM_GOTO_OBJECT` parking in segment 281
- [ ] Change switch-route tests to assert arrival in the useful general-area segment plus the correct shoot-switch instruction, not an exact firing pose
- [ ] Cover nearby objectives, long routes, unreachable frontiers, single-player, cooperative owner simulation, and cooperative replica synchronization
- [ ] Run focused host tests first, then the designed emulator scenarios when the emulator is available

Expected behavior change: near a destination or frontier, Guide-Bot can twiddle, wander, or return to the player just as in the original game. It may no longer place itself at a specially calculated switch-firing coordinate. Objective selection, general-area guidance, route continuation after world-state changes, and hints remain upgraded.

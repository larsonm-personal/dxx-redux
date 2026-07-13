# Guide-Bot and Metadata Pathing Unification Plan

Date: 2026-07-11
Status: Implementation started

## Executive Decision

Move toward one engine-neutral C++ route planner, but do it as a staged replacement rather than a rewrite. The first goal is observable behavioral parity. The second is to make metadata generation and live Guide-Bot navigation consume the same route result. The final cleanup can then remove the duplicate searches and interpretations that currently allow the two systems to drift.

The core invariant will be:

> Given the same topology snapshot, dynamic state, endpoint policy, and actor profile, every consumer receives the same semantic steps, activation pose, player action, completion predicate, and partial-route frontier. The segment chain remains planner evidence and diagnostics; live Guide-Bot movement consumes only the selected goal segment through the unmodified classic pathfinder.

The Kotlin layer is not currently a second pathfinder. It deserializes and displays native metadata results. The routing split is inside native code: the metadata analyzer plans in `level_metadata_scan.c`, `escort.c` reinterprets those steps, and `aipath.c` independently constructs the physical path.

## Objective

Bring live Guide-Bot navigation and metadata route analysis to behavioral parity, then converge duplicated routing decisions into shared C++ code while preserving game-engine integration, deterministic simulation, multiplayer ownership, save compatibility, and the existing mission regression corpus.

Parity has two separate meanings and both must pass:

1. **Planner parity:** the same state produces the same progression, dependencies, target, and fallback.
2. **Guidance parity:** Guide-Bot can identify and classically navigate toward the selected goal segment, tell the player where and what to do, detect the resulting world-state completion, and continue without changing classic movement or flare behavior.

## Planning Checklist

- [x] Map both routing pipelines from goal selection through path execution.
- [x] Inventory duplicated traversal, blocker, dependency, visibility, satisfaction, and fallback decisions.
- [x] Classify known and likely parity failures.
- [x] Audit dynamic state, cache invalidation, multiplayer ownership, determinism, and performance.
- [x] Define a shared route-planning API and engine adapter boundary.
- [x] Design an incremental migration with shadow comparisons and rollback points.
- [x] Define corpus, unit, integration, multiplayer, performance, and determinism gates.
- [x] Identify obsolete code to delete only after replacement coverage exists.

## Implementation Progress

- [ ] Phase 0 in progress: freeze corpus comparisons and characterize current live behavior.
  - Added a checked-in fingerprint baseline for 1,274 route-bearing mission levels and a comparison script that detects added, removed, or changed route records.
  - Confirmed the existing unexplored-goal emulator fixture is stale at command selection: its hardcoded wheel action IDs select `end_of_level`, so it does not currently exercise unexplored routing.
  - Added semantic radial-menu automation backed by the Kotlin overlay's visible wheel labels and migrated the unexplored fixture away from its numeric goal action ID.
  - The repaired emulator fixture verifies that `Guide` -> `Unexplored` is visible, dispatches through the production radial binding path, selects unexplored route mode, and retains that mode after the former refresh interval.
  - Phase 6 now has a player-assisted switch fixture; additional hidden-door,
    directed-crossing, blastable-wall, boss, reactor, and exit fixtures remain.
- [ ] Phase 1 in progress: add the engine-neutral topology/state snapshot in shadow-only form.
  - Added deterministic C++ topology and mutable-state snapshots, hashes, validation, C ABI summary, unit tests, and canonical level-load capture for D1 and D2.
  - Expanded the shadow snapshot with normalized D1/D2 trigger kinds, trigger effect links, per-side opener source walls, mutable trigger flags/disabled state, and live progression-object identity, containment, position, boss, and companion state.
  - Added a separate live Guide-Bot snapshot summary for route-only replans; canonical level-load diagnostics are no longer the only observable snapshot and are cleared independently on level changes.
  - Exposed the canonical snapshot summary through introspection without changing route selection or simulation RNG.
  - Added normalized wall kinds, key requirements, lock/open/hidden state, endpoint kinds, progression and navigator profiles, and the explicit route-query domain type.
  - Added engine-derived side centers and stable wall target points to shared topology; the legacy metadata analyzer now consumes the same side-center callback with its vertex average retained as a headless fallback.
  - Added independent mutable-state fingerprints and live generations for start, progression, navigation, trigger, object, and automap domains, exposed for canonical/live introspection and future cache keys.
  - Preserved each semantic step's exact analyzer-selected activation position and distinct wall/object aim point through the shared route-step ABI and live introspection; off-center visible firing positions are no longer discarded.
  - Player-radius occupancy validation for guidance poses, action-specific player hit validation, and event-driven generation hooks remain pending.
- [x] Phase 2 complete: centralize rich edge evaluation after snapshot parity is established.
  - Added a shared C++ edge evaluator with rich blocker/action results and a legacy three-state projection.
  - Added canonical per-side shadow comparison against the active legacy evaluator, mismatch introspection, and progression/navigator capability unit coverage.
  - Confirmed zero edge-cost mismatches in synthetic D1/D2 fixtures and the Counterstrike level 1 emulator route scan while preserving all 1,274 reviewed corpus route projections.
  - Added an opt-in strict headless shadow gate and a no-copy host corpus mode so archive regeneration executes old/new edge parity across every loaded level without rewriting reviewed JSON.
  - Confirmed zero edge-cost mismatches across all base Descent and Counterstrike levels plus 1,244 levels from 109 mission archives; the remaining archive contains no mission descriptor and was skipped by existing policy.
  - The legacy native planner remains authoritative; porting semantic dependency planning is the next migration boundary.
- [x] Phase 3 complete: port semantic dependency planning behind corpus shadow comparisons.
  - Ported deterministic weighted segment search, parent-chain retention, progress weighting, and first-obstruction capture into the shared C++ planner.
  - Added exact pessimistic/optimistic per-segment shadow comparison for reachability, distance, progress weight, parent segment, and parent side, with strict headless and introspection diagnostics.
  - Confirmed zero search mismatches in D1/D2 unit fixtures, all base Descent and Counterstrike levels, and 1,244 levels from 109 mission archives.
  - Normalized object roles, direct and contained keys, dead-object state, and control-center segments without exposing D1/D2 constants to the planner.
  - Ported ordered key, boss, reactor, control-center fallback, and normal-exit target discovery, including secret-exit filtering and stable activation positions.
  - Confirmed zero target inventory mismatches in D1/D2 unit fixtures, all base Descent and Counterstrike levels, and 1,244 levels from 109 mission archives.
  - Added explicit simulated progression state for acquired or avoided keys, fired or avoided triggers, opened hidden walls, current pose, and reactor/boss completion.
  - Added shared state transitions for key acquisition, trigger activation, and paired hidden-wall opening; searches now accept an immutable progression state instead of assuming level-start state.
  - Confirmed zero search mismatches across initial, post-key/reactor, fully opened, and dependency-avoidance states in D1/D2 unit fixtures, all base campaigns, and 1,244 levels from 109 mission archives.
  - Ported deterministic exit target selection using progression weight, endpoint-adjusted geometric distance, stable source-order ties, and the preserved selected segment chain.
  - Confirmed zero target-selection mismatches across all four progression states in D1/D2 unit fixtures, all base campaigns, and 1,244 levels from 109 mission archives.
  - Added explicit distance-first and forbidden-missing-key search policy, then ported direct or contained key-recovery target choice with selected path retention.
  - Confirmed zero key-selection mismatches for blue, red, and gold across all four progression states in D1/D2 unit fixtures, all base campaigns, and 1,244 levels from 109 mission archives.
  - Fixed MSVC packing for all new snapshot, edge, and planner C ABI summaries after expanded diagnostics exposed engine-header packing leakage.
  - Ported ordered trigger-source discovery with direct-side-before-reverse-side precedence, stable source-wall ordering, source activation positions, and fired, disabled, non-progress, and in-progress trigger filtering.
  - Added per-side trigger-source shadow comparison in all four progression states, strict headless diagnostics, unit coverage, and live introspection; confirmed zero mismatches in D1/D2 fixtures, all base campaigns, 1,244 levels from 109 mission archives, and the Counterstrike unexplored-goal emulator fixture.
  - Ported trigger firing-path selection through an engine-neutral visibility callback, retaining deterministic search visit order and the exact preferred, center, side, vertex, and edge sample sequence used by metadata.
  - Reused one pessimistic search tree across candidate trigger sources, preserving direct-path and visibility-fallback ranking while avoiding the legacy planner's repeated full-mine search per source.
  - Added direct and visibility-fallback unit fixtures plus bounded direct/visible shadow comparisons for distinct progression states; base campaigns, 106 fully strict archive scans, the remaining available archive comparisons, and the Counterstrike unexplored-goal emulator fixture report zero firing-path mismatches.
  - Existing unrelated strict diagnostics remain in K_SOS and Levigen edge/search parity, while KCXF2 level 4 still reports shadow data unavailable; none reports a firing-path mismatch.
  - Completed the exact FVI visibility cache shared by metadata recursion, C/C++ shadow comparison, and live Guide-Bot fallback. Wall and object-target rays are keyed by the exact source pose and target, so reachability-only recalculations reuse them while door doorway state, textures, or level geometry changes invalidate the cache through a direct world fingerprint.
  - Added live cache counters and assertions. Counterstrike level 1 reused 18,420 rays after 12,642 evaluations with zero bypasses and zero planner shadow mismatches; strict D2 base-campaign analysis dropped from 18.1 seconds to 3.2 seconds while all 1,274 reviewed route fingerprints and base campaign statuses remained unchanged.
  - Completed recursive trigger dependency resolution in the shared planner. Trigger sources can now transactionally resolve prerequisite keys, hidden doors, and other triggers; failed branches restore semantic steps and progress state, dependency loops retain legacy diagnostics, and every emitted step preserves its activation pose, aim target, opened links, distance, and selected segment chain.
  - Added a legacy-C shadow entry point and exact result comparison for success, diagnostics, final progress, step order and fields, activation and aim poses, opened links, and distances. Direct, visible, nested-trigger, and loop fixtures pass in D1 and D2.
  - Strict base campaigns and all 109 processable mission archives pass with zero recursive-dependency mismatches; one descriptor-less archive remains an expected skip. The Counterstrike unexplored live fixture compared four dependencies with zero mismatches while preserving all 1,274 reviewed route fingerprints.
  - Completed the generic end-of-level and specified-segment route driver in shared C++. It now owns top-level key recovery, boss/reactor and exit actions, partial status, unresolved trigger or hidden-door frontier steps, exact terminal positions, and complete route-signature comparison against the legacy planner retained during migration.
  - Expanded differential coverage from level-start synthetic states to every emitted key, trigger, and hidden-door checkpoint, plus independent all-keys-avoided and all-triggers-avoided states. The strict headless gate now includes recursive dependency and complete-route mismatches instead of silently omitting them.
  - Fixed keyed doors that also have trigger openers: avoiding a failed trigger now falls through to the independent key requirement instead of blocking the edge. Counterstrike level 17 and its TRINITY level 44 copy remain `ok` while dropping the same unnecessary trigger 5 detour; these are the only reviewed route-signature changes across all 1,274 corpus records.
  - Fixed D1 snapshot normalization for ordinary doors whose unset raw key field is zero even though `KEY_NONE` is nonzero. D1 and D2 unit tests and strict base campaigns pass with zero edge, search, dependency, or complete-route mismatches.
  - Re-ran built-in Counterstrike and all 110 mission archives with the expanded strict gate: 109 archives passed, one descriptor-less archive was skipped by existing policy, and none failed. The reviewed 1,274-record corpus and base-campaign status tests pass.
  - Ported unexplored-component discovery and ranking into shared C++. End-of-level, unexplored, and explicit-segment queries now use the same dependency solver; unexplored differs only by component connectivity and endpoint ranking.
  - Added exact unexplored-route shadow comparison for component size, target segment, first semantic waypoint, direct reachability, status, diagnostics, and every semantic step. D1/D2 unit fixtures, both strict base campaigns, built-in Counterstrike, and all 109 processable mission archives pass with zero unexplored-route mismatches; one descriptor-less archive remains an expected skip.
  - Updated the live Counterstrike unexplored-wheel fixture for data-dependent checkpoint counts. It selects `Guide` -> `Unexplored` by visible text, confirms Guide-Bot retains unexplored mode, and reports zero unexplored-route, complete-route, dependency, firing-path, source, target, search, and edge mismatches with the visibility cache active.
  - Phase 4 is next: separate canonical and live shared plans, then make Guide-Bot consume the first pending shared waypoint without rebuilding route meaning in `escort.c`.
  - Live behavior remains on the legacy planner until the Phase 4 consumer bridge is green across the same gates.
- [x] Phase 4 complete: separate canonical and live plans and bridge Guide-Bot to shared C++ output.
  - Added a C ABI projection for end-of-level, unexplored, and explicit-segment shared plans, including the first pending waypoint and its preserved diagnostic-only path summary.
  - Canonical metadata and live Guide-Bot route state now have independent storage and getters. Canonical serializers and overlays no longer observe route-only Guide-Bot replans.
  - Route-only live scans now use the shared C++ planner. Guide-Bot selects its first pending shared waypoint directly; the legacy all-step selector remains only as a temporary parity diagnostic and stale-plan detector.
  - Fixed the first live selector mismatch exposed by the new gate: key steps were omitted from the common targetability helper because the legacy selector handled them in a separate branch.
  - Nonowner co-op peers return before live route planning. Introspection reports shared planner provenance, first-waypoint path data, current selector parity, and the exact pair from any mismatch.
  - D1/D2 projection tests cover all three endpoint policies. Windows D1/D2 builds, both strict base-campaign scans, corpus status/fingerprint tests, all Android ABIs, and the live Counterstrike unexplored-wheel fixture pass with zero selector or planner-shadow mismatches.
  - [x] Phase 5 complete: restored and enforced the classic movement boundary while retaining the shared planner only for semantic objective and goal-segment selection.
  - [x] Restored the original `aipath.c` doorway/openable-door predicate and removed the metadata-aware Guide-Bot path wrapper.
  - [x] The shared planner's first pending path terminal is now the high-level Guide-Bot goal; the duplicate live firing-position and reachability searches were deleted.
  - [x] Restored classic command scheduling and return-to-player timing by removing command-time path creation, duplicate polishing, and pending-route suppression.
  - [x] Partial-route closest-frontier selection now belongs to the shared planner, and shared route goals issue one ordinary classic path request.
  - [x] Added path, scheduling, and RNG parity coverage before closing Phase 5.
  - D1 and D2 Windows builds and native suites pass (20 and 23 tests), including the partial-frontier planner test. Android debug builds pass for all three ABIs.
  - Full host metadata regeneration completed 109 mission archives with one skip and zero failures; all 1,274 reviewed route fingerprints and base-campaign status gates remain unchanged.
  - The KCXF2 hidden-door fixture and the unexplored wheel fixture pass. The KCXF2 fixture now follows semantic objective identity instead of assuming the total live route-step count remains fixed while Guide-Bot moves.
  - Added a debug-only ordinary-versus-route path comparator. It snapshots Guide-Bot AI, all robot path lengths, route intent, and simulation RNG; runs classic path construction twice from the same object, segment, target, and RNG state; compares every point, AI bookkeeping, return value, RNG calls, and ending RNG state; then proves the original state was restored.
  - [x] Phase 6 initial slice complete: added passive player guidance and switch completion coverage without adding a Guide-Bot action executor.
  - [x] Route instructions now tell the player to follow Guide-Bot and perform the marked switch or hidden-door action. Introspection exposes the active instruction, activation/aim positions, completion reason, linked-wall passability, and first blocking wall.
  - [x] The Android automap marks the active activation position and aim point. These positions are presentation data only and are never passed to Guide-Bot movement, orientation, or firing.
  - [x] Numbered automap cheat objectives now render distinct shoot-switch activation and aim positions as a same-number pair with an inset, same-color 3D connector. Connector candidate/drawn counts are introspectable; D2 Counterstrike level 2 and the D1 no-pair case pass the shared automap fixture.
  - [x] Added a test-only player pose action and extended the KCXF2 level 5 live fixture. It proves trigger 6 remains blocked while Guide-Bot waits, then advances from the blue-key stage to the red key only after ordinary player fire from the reported guidance pose.
  - [x] Rejected an experimental 200-unit firing-ray limit after the corpus exposed 55 route-status regressions. Also rejected stricter finite `FQ_TRANSPOINT` sampling for semantic dependency discovery after six valid routes regressed. Route discovery retains its established transparent-wall existence approximation; exact player execution is tested live.
  - Windows D1/D2 builds and route snapshot tests pass. The full host corpus remains 109 passed, one descriptor-less skip, zero failed, with no route-status regressions. The KCXF2 player-assisted switch fixture passes all 45 steps.
  - The live KCXF2 level 4 fixture compared two identical 30-point paths toward hidden-door segment 221. Both consumed 458 simulation RNG calls and ended at RNG state 2654953511, with no point or AI mismatch and exact post-probe restoration.
  - Route commands once again use only the original Guide-Bot frame scheduling, return-to-player interruption, `create_path_to_segment()`, and polish lifecycle. The fixture uses the existing warp command only to establish a deterministic nearby starting state before NEXT, then confirms the route remains active at wall 61 after classic scheduling continues.
  - The comparator intentionally exercises core classic path construction without invoking `polish_path()` twice in one tick. The original companion polish guard makes a same-tick second call differ by design; source structure and the live fixture instead verify that ordinary and route goals share the one classic polish lifecycle.
- [x] Canonical metadata route cut over to the shared C++ planner.
  - Keep non-route metadata aggregation in `level_metadata_scan_level_summary()` while producing route and travel fields with the same shared `route_planner_plan_view()` result consumed by live Guide-Bot routing.
  - Retain the legacy native planner only behind strict differential comparison until the full mission corpus proves the authoritative cutover is behaviorally stable.
  - Expose canonical planner provenance through introspection so integration tests can distinguish shared authority from shadow availability.
  - Split summary-only metadata aggregation from legacy route construction, so normal canonical generation no longer computes and discards a legacy route before calling the shared planner.
  - Bounded malformed trigger-link counts to the engine array and normalized a 255-wall third-party level to the 254-wall engine limit without out-of-bounds metadata reads. Orion, KCXF2, and Plutonia retained their prior valid routes.
  - Regenerated built-in Counterstrike and all 110 mission archives: 109 passed, one descriptor-less archive was skipped, and none failed.
  - Reviewed the only three checked route changes across the corpus. `eq-set` now exposes hidden doors crossed by two routes, `vignett2` uses a verified nearby switch firing position, and `af_d1_beta` preserves its semantic steps while avoiding a shorter traversal that treated a hidden wall as freely passable.
  - Windows D1/D2 builds, all 20 D1 and 23 D2 native tests, all Android ABIs, and the 37-step live KCXF2 fixture pass. Live introspection reports `shared_cpp` for both canonical metadata and Guide-Bot routing.

## Non-Goals

- No route behavior changes during this planning pass.
- No mission-specific exceptions.
- No immediate conversion of upstream D1 or D2 AI source files to C++.
- No replacement of classic Guide-Bot path construction, path polishing, path refresh timing, steering, collision behavior, AI modes, or flare behavior for any goal.
- No route-specific Guide-Bot aiming, firing, directed trigger crossing, exact-pose movement, or world-state action execution.
- No synchronization of full route plans over multiplayer. Route intent remains synchronized and the active owner computes the transient plan.

## Constraints

- Metadata generation must remain headless and independent of a rendered game loop.
- Live guidance and passive completion must respond to doors, triggers, keys, destroyed objects, automap exploration, save restore, and cooperative ownership changes.
- The route planner must not read D1 or D2 globals directly.
- D1 and D2 must use the same planner through a C ABI and game-specific adapters.
- Input-demo recording and replay must remain deterministic after route behavior changes and new fixtures are recorded.
- The checked-in mission JSON corpus is an acceptance oracle, not proof of live executability.
- Planner ranking must not depend on platform floating-point tie behavior, unordered iteration, or simulation RNG.
- Guide-Bot and player capabilities are not identical. Legitimate differences must be explicit actor-profile data, not separate algorithms.
- Shared planning must consume no simulation RNG. Given the same Guide-Bot state, goal segment, and RNG state, live path points, RNG calls, and subsequent movement must match the classic implementation.
- Exact activation and aim positions are player-guidance data for messages, introspection, and automap markers. They must never be fed into Guide-Bot movement, orientation, or firing.

## Current Pipeline Map

### Metadata Route Analysis

1. `secret_area_game_adapter.c` exposes segment, wall, trigger, object, visibility, key, reactor, and automap state through `level_metadata_scan_view` callbacks.
2. `level_metadata_scan.c` discovers targets and opener relationships.
3. It classifies edges as passable, progress-bearing, or blocked.
4. It runs deterministic-looking but floating-point Dijkstra searches for endpoints and firing visibility.
5. It recursively resolves key, trigger, hidden-door, boss/reactor, exit, or unexplored dependencies.
6. It emits `level_metadata_route_step` records and aggregate travel metrics.
7. JNI and the headless metadata dumper serialize those records; Kotlin only consumes them.

### Live Guide-Bot Route Selection

1. `escort.c` asks the same analyzer to rescan from the Guide-Bot object.
2. The analyzer's route replaces the route fields inside the process-global `Level_metadata_state`.
3. `escort.c` independently scans steps and decides which are satisfied.
4. It independently maps the selected step to legacy `ESCORT_GOAL_*` and guidance modes.
5. For switches, reactors, and bosses, it independently searches for a visible segment.
6. If the goal is unreachable, it independently computes a nearest reachable segment through two BFS passes.

### Physical Navigation and Activation

1. `escort.c` passes only a target segment to `create_path_to_segment_metadata_route()`.
2. `aipath.c` reruns a randomized, unweighted BFS and may return a partial path when it cannot reach the requested segment.
3. The metadata wrapper shares only edge classification through `Metadata_route_path_depth`; it does not share the metadata segment chain or exact endpoint.
4. Generic companion behavior fires a flare straight ahead when an ordinary unlocked, non-hidden door is nearby.
5. No route-specific action executor consumes `objective_wall`, an aim point, a fly-through side, or a completion predicate.

Items 1 and 3 cross the newly fixed architecture boundary and must be rolled back in Phase 5. Item 2 is intentional classic behavior and must remain. Item 4 is the only normal Guide-Bot world-state action and must remain unmodified. Item 5 is now a requirement rather than a missing feature: exact positions and actions guide the player, while completion predicates observe what the player changes.

## Parity Matrix

| Decision | Metadata today | Guide-Bot today | Unified target |
| --- | --- | --- | --- |
| End-of-level dependencies | Native analyzer | Same emitted steps | One `RoutePlan` |
| Unexplored dependencies | Same analyzer with another endpoint | Same emitted steps plus local target copy | Same planner, endpoint policy only |
| Dynamic start | Player start for canonical metadata | Guide-Bot object for live route | Explicit query start |
| Actor restrictions | Usually player-like | Companion buddy-proof restriction | Explicit progression and navigator profiles |
| Edge semantics | Rich internal blocker, coarse public cost | Coarse public cost only | Shared rich `EdgeDecision` |
| Path ranking | Weighted Dijkstra | BFS by segment count | Shared deterministic ranking |
| Segment chain | Computed then discarded | Recomputed by classic randomized pathfinder | Preserved only as planner evidence and diagnostics; never consumed by movement |
| Firing position | Exact sampled position computed then discarded | Visible segment recomputed; position discarded | Exact player pose and aim target exposed as guidance; Guide-Bot receives only the goal segment |
| Trigger traversal | Source side known internally | Routes to source segment only | Tell the player which side to cross; observe completion without directing Guide-Bot traversal |
| Hidden door | Semantic step emitted | Routes near wall; generic classic flare behavior remains incidental | Tell the player where to stand and what to open; never aim or fire Guide-Bot |
| Blastable wall | Treated immediately passable | Treated passable although buddy flare cannot damage it | Explicit player-required obstruction action |
| Step completion | Analyzer simulation | Reimplemented from live globals | Shared completion evaluator |
| Nearest fallback | Partial semantic result | Separate optimistic BFS heuristic | Frontier from the same failed plan proof |
| State invalidation | Fresh headless scan | Mostly keys, commands, restore, handoff, target visited | Generation-keyed snapshots and coalesced invalidation |
| Multiplayer | Not applicable | Owner-local plan, mode synchronized | Preserve owner-local planning |
| Determinism | No RNG, but double ties | Classic physical BFS consumes simulation RNG | RNG-free shared planning plus byte-for-byte classic movement RNG behavior for the same goal segment |

## Audit Findings

### P0: Semantic Steps Need Complete Player-Guidance Contracts

`level_metadata_route_step` stores a segment, side, wall, trigger, and label, but historically omitted the exact player activation pose, aim point, directed transition, action owner, and completion predicate. The analyzer can prove that a switch is visible from a sampled point and then discard that point. Guide-Bot may still navigate toward the segment through classic movement, while the exact pose and target must be retained so the player can be told where and what to do.

This is the clearest explanation for a route that is correct in metadata but stalls in-game.

### P0: Physical Path Selection Must Remain Intentionally Independent

Metadata uses weighted Dijkstra over segment-center distances and progress cost. `aipath.c` uses randomized BFS, a maximum depth, legacy safety polishing, and partial-path fallback. Sharing edge classification does not make these searches equivalent. The physical endpoint can differ from the planner's endpoint even when both use the same passable-edge rule.

This divergence is not something to eliminate. Metadata chooses the semantic objective and a goal segment for the player-guidance task. The classic Guide-Bot pathfinder independently decides how the companion moves toward that segment. The metadata edge override, route-specific path wrapper, exact-pose materialization, and shared-chain consumption are therefore architecture violations.

### P0: Route Actions Belong to the Player

The companion's generic flare code does not target the selected route wall. `openable_doors_in_segment()` explicitly excludes hidden doors and recognizes only ordinary unlocked doors. This is correct classic behavior and must not be expanded for route goals.

Shoot-switch, hidden-door, fly-through, and pass-through steps must identify a player action. Guide-Bot may travel toward the selected source or firing segment using classic movement, then tell the player the exact activation position, aim point, or side. It must not orient, fire, cross a directed side, or otherwise execute the action on the player's behalf.

### P0: Blastable Walls Are Misclassified for Live Navigation

The shared edge classifier treats every blastable wall as passable before it is blasted. Companion flares cannot damage blastable walls because `wall_hit_process()` only applies blastable-wall damage for player-parented weapons. A Guide-Bot path can therefore be planned through an obstruction it cannot remove.

The unified planner should model this as a progression action assigned to the player, with Guide-Bot navigating to a safe waypoint on the near side and waiting for the wall state to change.

### P1: Completion Logic Is Duplicated and Lossy

`escort.c` independently determines whether keys, triggers, hidden doors, bosses, reactors, exits, and unexplored targets are complete. Notable edge cases include:

- An invalid trigger is silently considered satisfied instead of forcing a replan or reporting an invalid plan.
- A missing unowned key is observed but `key_exists` does not affect satisfaction or selection.
- Trigger completion requires every recorded link to be passable, even when only a subset was required by the selected route.
- Trigger disabled state is recorded but does not define completion. A disabled trigger with its required edge still blocked needs an explicit impossible state, not a generic blocked state.
- The `HOSTAGE` route kind is understood by the live bridge but is not emitted by the route analyzer.

The planner must emit the exact required effects for a step, and the shared evaluator must return `pending`, `complete`, `invalidated`, or `impossible`.

### P1: Visibility Work Is Duplicated

Metadata and `escort.c` both sample the center, face-biased points, vertex-biased points, and edge-biased points. The live version can cast up to 115 rays per segment during a full search. The copies can drift and both currently use zero-radius visibility even though the companion has physical size.

The shared planner should return a player-occupiable pose and an action-specific line trace that confirms the intended wall or object can be hit. This validation must not be reused as Guide-Bot movement or aiming input.

### P1: Nearest-Progress Fallback Is a Different Planner

The fallback in `escort.c` computes current reachability and an optimistic reverse BFS, then ranks candidates primarily by segment count. It does not reuse the analyzer's weighted route, dependency proof, avoided-trigger state, or exact blocker. It can choose a point that is close in a graph that requires an impossible sequence of actions.

The route search should retain its best reachable frontier and the next unresolved action. "Get as close as possible" can then follow the same selected route rather than inventing another one.

### P1: Dynamic Invalidation Is Incomplete

Full metadata rescans are now dirty-driven, which avoids earlier CPU spikes, but the invalidation model is implicit. Keys, commands, save restore, owner handoff, and an explored target cause replanning. Arbitrary wall/trigger changes, control-center links, object removal, boss movement, and changes elsewhere in an unexplored component do not have a unified generation signal.

The five-second Guide-Bot refresh currently resets the legacy goal and physical path, but does not mark metadata dirty. The automation script `test_guidebot_unexplored_goal.json5` still expects a `periodic_refresh` replan reason that no longer exists in the code. That stale expectation must be resolved before the script can be treated as a migration gate.

### P1: Canonical and Live Route State Are Conflated

A live route-only rescan copies its route fields into global `Level_metadata_state`, replacing the canonical route generated at level load. Static metadata, live owner state, route introspection, and Guide-Bot execution should have separate result objects built by the same planner.

### P1: Player Progression and Guide-Bot Movement Need Separate Profiles

Metadata describes how the player can complete a level. Live routing asks which goal segment can usefully guide that progression while classic code independently moves the Guide-Bot. Buddy-proof walls and navigator radius can inform high-level endpoint suitability, while every route progression action belongs to the owning player. Classic incidental Guide-Bot door and flare interactions are not route capabilities.

A single "actor" flag is insufficient. A query needs both:

- A **progression profile** for owner keys and actions that make the level advance.
- A **navigator profile** used only to judge whether a high-level Guide-Bot goal segment is sensible under classic traversal constraints. It must not replace the classic pathfinder or grant route actions.

Semantic steps should remain the same. The live plan selects a goal segment or a well-defined near-side frontier, then classic movement independently attempts that segment. All progression actions remain assigned to the player.

### P1: Existing Live Tests Stop Before Execution

Current KCXF2 and Obsidian scripts validate step selection and then directly grant keys or fire triggers through debug controls. The hidden-door script checks that a path remains pending. These are useful planner tests but need player-assisted guidance and passive-completion assertions.

### P2: Planner State Is Global and Non-Reentrant

`level_metadata_scan.c` owns large static scratch arrays for segments, targets, heap state, unexplored components, route snapshots, and visibility search. This prevents concurrent planner instances and makes canonical and live state easier to mix accidentally. A reusable C++ workspace object should own this memory per planner instance.

### P2: Limits and Result Loss Need Explicit Handling

The C ABI caps routes at 96 steps, targets at 512, trigger effects at 10, walls at 254, and segments at 9000. The wall and trigger-link limits match engine limits, but internal planning should not silently lose data because of the serialization shape. Internal vectors can be dynamic while the stable C/JSON projection reports explicit overflow if a representable limit is exceeded.

### P2: Ranking Needs Stronger Determinism

Current route ranking uses `double` distances and has no final segment-ID tie-break in the heap comparison. Shared C++ planning should rank with fixed or 64-bit integer costs and explicit lexicographic tie-breaking. Floating-point distance can remain a reporting metric after the route is selected.

### P2: Adapter Policy Is Not Fully Rationalized

The adapter has metadata and non-metadata variants for trigger opener discovery, with keyed target behavior differing by call site. D1 also uses synthetic negative trigger-type values. The shared domain model should normalize wall and trigger semantics once and eliminate raw engine constants from planner decisions.

## Proposed Architecture

### 1. Immutable Topology Snapshot

Create a C++ `RouteTopology` built once per loaded level. It contains:

- Segment adjacency, reverse sides, centers, vertices, and side centers.
- Stable side and wall identities.
- Trigger source walls, trigger effects, and reverse-wall pairs.
- Control-center effects and exit sides.
- Static target identities for keys, reactor candidates, bosses, and other progression objects.
- Static wall properties such as hidden clip and buddy-proof policy.
- A topology generation and stable hash for diagnostics.

The adapter captures callbacks into contiguous arrays once. Search should not repeatedly call through engine globals for every edge.

### 2. Dynamic State Snapshot

Create a `RouteState` containing only mutable facts:

- Start segment and fixed-point position.
- Owning player's key mask.
- Wall type, flags, lock/open/blasted state, and mutable key policy.
- Trigger flags and disabled/fired-relevant state.
- Control-center state and current control-center effects.
- Live target presence, position, signature, and segment.
- Owner-local automap explored bitmap.
- Separate generations for walls, triggers, keys, objects, reactor, automap, owner, and level.

Snapshots must be immutable during a planner call. Changes that occur while planning schedule a later replan rather than mutating the active search.

### 3. Explicit Profiles and Query

Recommended conceptual API:

```cpp
struct RouteQuery {
    RouteEndpoint endpoint;
    RouteProgressionProfile progression;
    RouteNavigatorProfile navigator;
    FixedPosition start;
    RoutePolicy policy;
};
```

`RouteEndpoint` supports `end_of_level`, `unexplored`, and `segment`. End-of-level and unexplored use the same dependency solver; only endpoint discovery and ranking differ.

The canonical JSON query uses a player progression profile and player navigator profile from the player start. The live Guide-Bot query uses the same player progression profile plus a companion navigator profile from the Guide-Bot position.

### 4. Rich Edge and Action Decisions

Replace the coarse passable/progress/blocked result internally with a rich decision:

```cpp
struct EdgeDecision {
    TraversalState traversal;
    BlockerKind blocker;
    RequiredAction action;
    StableSideId side;
    StableWallId wall;
    StableTriggerId trigger;
    CompletionPredicate completion;
    RouteCost cost;
};
```

Required actions include key acquisition, ordinary door opening, shoot switch, directed trigger crossing, hidden-door opening, blastable-wall destruction, reactor destruction, boss destruction, and exit entry.

The existing C API remains available through wrappers. `level_metadata_scan_route_edge_cost()` projects the rich result back to the old three-value enum until all callers migrate.

### 5. Route Plan as the Shared Product

The shared planner returns a `RoutePlan`, not just display steps:

```cpp
struct RouteWaypoint {
    RouteObjective objective;
    RequiredAction action;
    ActionResponsibility responsibility;
    CompletionPredicate completion;
    std::vector<StableSegmentId> segment_chain;
    FixedPosition terminal_pose;
    FixedPosition aim_point;
    DirectedSide crossing;
    std::vector<StableSideId> required_effects;
    std::vector<StableSideId> informational_effects;
};

struct RoutePlan {
    RouteStatus status;
    RouteEndpoint resolved_endpoint;
    std::vector<RouteWaypoint> waypoints;
    RouteFrontier best_reachable_frontier;
    RouteStateKey state_key;
    uint64_t deterministic_hash;
};
```

The current JSON schema remains stable initially by projecting `RoutePlan` into `level_metadata_route_step`. New player-guidance and completion fields stay internal until their format is mature. Debug introspection may expose them immediately.

### 6. Shared Step Evaluation

The planner library exposes `evaluate_waypoint(waypoint, state)`. It returns:

- `pending`: required state has not changed yet.
- `complete`: the exact required predicate is true.
- `invalidated`: the target identity or assumptions changed and the route must be recomputed.
- `impossible`: a one-shot or destroyed dependency can no longer produce the required effect.

`escort.c` should not inspect trigger links, key objects, boss lists, or wall state to reinterpret planner intent after this migration.

### 7. Thin Live Guidance Bridge

`escort.c` remains responsible for command selection, messages, ownership, save intent, and the classic Guide-Bot state machine. The route bridge is responsible only for:

1. Select the first pending semantic waypoint.
2. Hand its goal segment to the ordinary classic `create_path_to_segment()` lifecycle.
3. Expose the exact activation position, aim target, crossing side, and required player action through messages, introspection, and automap guidance.
4. Observe the shared completion predicate after the player changes world state.
5. Replan on completion, invalidation, owner change, or relevant world-state change without creating a physical path itself.

The action policy is:

| Action | Responsibility |
| --- | --- |
| Pick up key | Owning player |
| Open keyed/ordinary door | Owning player; Guide-Bot's unchanged generic forward flare may incidentally open an ordinary door |
| Shoot switch | Owning player |
| Open hidden door | Owning player |
| Cross trigger | Owning player |
| Destroy blastable wall | Owning player |
| Destroy reactor or boss | Owning player |
| Enter exit | Owning player |

Guide-Bot never performs a route action, aims at a route target, or fires a route-specific flare. Its existing generic forward-flare behavior remains byte-for-byte classic and may incidentally affect the world exactly as before.

### 8. Classic Movement Boundary

Do not add a route-path materializer. The shared segment chain and exact pose remain diagnostic and player-guidance data. Live movement receives only a goal segment and uses the classic pathfinder.

The boundary requires:

- Restore the original `aipath.c` doorway predicate and remove `Metadata_route_path_depth` plus `create_path_to_segment_metadata_route()`.
- Route goals call ordinary `create_path_to_segment()` exactly once at the same classic scheduling point as any other Guide-Bot goal.
- Preserve classic random side ordering, simulation RNG consumption, path polishing, five-second refresh, return-to-player interruptions, AI modes, steering, and forward-flare behavior.
- Remove route-specific exact-pose injection, path chunking, path lifecycle suppression, immediate command-time path creation, and duplicate path polishing.
- Given identical Guide-Bot state, goal segment, and RNG state, route and non-route callers produce identical `Point_segs`, RNG deltas, and subsequent movement.

### 9. Partial Route and "Closest Possible"

Every search retains the best reachable frontier on the selected dependency proof. The frontier includes the reachable segment, pose, unresolved side, required action, and reason progress cannot continue.

The planner selects a reachable frontier as a high-level fallback before requesting movement. Guide-Bot then makes one ordinary classic path request to that frontier segment and reports the next obstruction. The route bridge must not run a second fallback path search or use metadata edge semantics inside `aipath.c`.

### 10. State Generations and Cache Policy

Build cache keys from the topology hash, relevant dynamic generations, endpoint, profiles, and start state. Do not use periodic full rescans as correctness machinery.

- Topology is rebuilt once per level load.
- Wall and trigger changes invalidate edge and dependency results.
- Key changes invalidate owner progression.
- Object destruction or relocation invalidates only affected objective data.
- Automap changes are coalesced and invalidate unexplored endpoint selection.
- Owner handoff invalidates the navigator start, key owner, and owner-local automap state.
- Ordinary movement advances through classic `Point_segs` and does not force full semantic replanning.

### 11. Multiplayer Policy

Only the peer that locally owns Guide-Bot plans its guidance and runs the classic Guide-Bot simulation. Other peers render synchronized robot state and retain route intent for UI/ownership transfer.

- Keep synchronizing target mode and owner generation.
- Do not put segment chains or firing poses in ownership packets initially.
- On adoption, abdication, disconnect transfer, or restore remap, clear transient guidance state and recompute from synchronized world state plus the new owner's local automap.
- Ensure an aimed trigger action is emitted once by the owner and propagated through existing trigger/door network messages.
- Expose plan hash and state generations in introspection so two-peer tests can distinguish intentional owner-local differences from stale ownership.

### 12. Determinism Policy

- Planner searches use integer/fixed ranking and stable ID tie-breaks.
- Shared planning, shadow comparison, guidance presentation, and completion observation consume no simulation RNG. Classic Guide-Bot movement continues consuming simulation RNG exactly as the original pathfinder does.
- Shadow planning must have no gameplay side effects and no RNG calls.
- Treat existing input demos as regression evidence, not compatibility artifacts that constrain a corrected engine.
- After intentional route behavior changes, record new fixtures and require the shared deterministic route chain to match on record and replay.
- Fix any new record/replay divergence in the engine rather than adding planner-specific replay behavior.

## Migration Phases

Each phase must be independently buildable, testable, and reversible. The classic physical path remains authoritative throughout every phase. Planner shadow mode logs differences but does not alter gameplay.

### Phase 0: Freeze Baselines and Characterize Gaps

Work:

- Capture a normalized full-corpus baseline keyed by mission identity, archive hash, level number, route status, problem category, semantic step signature, endpoint, and travel distance.
- Record the current corpus snapshot: 1,281 level records, 1,180 `ok`, 47 `partial`, and 47 `failed` as audited on 2026-07-11.
- Keep Descent and Counterstrike strict through `test_base_mission_route_status.ps1`.
- Add characterization fixtures for hidden-door player guidance, shoot-switch player guidance, directed player trigger crossing, blastable walls, buddy-proof walls, disabled triggers, relevant-link completion, and partial frontier selection.
- Split live tests into planner-selection, classic-movement-parity, and player-assisted-completion tests.
- Resolve the stale `periodic_refresh` expectation before using that script as a gate.
- Add plan/state hashes and exact path/action diagnostics to introspection.

Exit gate: all existing intentional baselines are reproducible and every known mismatch has a failing or characterization test.

Rollback: no behavior has changed.

### Phase 1: Introduce C++ Domain Model and Snapshot Builder

Work:

- Add engine-neutral `RouteTopology`, `RouteState`, profiles, IDs, and workspace types under `android/app/src/main/cpp/shared`.
- Build snapshots from the existing `level_metadata_scan_view` adapter.
- Normalize D1 and D2 wall/trigger constants into shared enums.
- Keep `level_metadata_scan.c` as the active planner.
- Build the new code in D1, D2, host tests, headless metadata, and Android targets through an `extern "C"` facade.

Exit gate: snapshot hashes are stable across repeated scans and shadow capture does not change metadata output or RNG state.

Rollback: remove the unused snapshot builder and CMake entries.

### Phase 2: Centralize Rich Edge Evaluation

Work:

- Port edge and blocker classification into the C++ planner.
- Preserve current three-state results through the C wrapper.
- Run old and new classifiers for every segment side in unit fixtures and corpus levels.
- Add explicit progression/navigator capability tests.
- Represent blastable and buddy-proof behavior without changing live behavior yet.

Exit gate: zero unexplained edge-decision differences for the same profile and snapshot.

Rollback: switch the C wrapper back to the old classifier.

### Phase 3: Port Semantic Dependency Planning

Work:

- Port deterministic path ranking, target selection, keys, triggers, hidden doors, boss/reactor, exits, unexplored components, and partial-route production.
- Preserve exact segment chains and terminal poses that are currently discarded.
- Keep metadata JSON projection byte-stable where ordering and numeric formatting are intentionally unchanged.
- Run the legacy planner and shared C++ planner in corpus shadow mode.
- Categorize every difference as bug fix, deterministic tie resolution, or regression before accepting it.

Exit gate: base campaigns remain strict; full-corpus status does not regress; all changed semantic signatures are reviewed; synthetic parity tests pass in D1 and D2.

Rollback: retain the legacy planner as the active implementation.

### Phase 4: Separate Canonical and Live Plans

Work:

- Store canonical metadata and live Guide-Bot route results separately.
- Make end-of-level and unexplored commands construct the same `RouteQuery` with different endpoint policies.
- Make `escort.c` select the first pending shared waypoint instead of rebuilding satisfaction logic.
- Preserve legacy `ESCORT_GOAL_*` only as a UI/AI state bridge.
- Add shadow comparison between the old selected goal and the new first pending waypoint.

Exit gate: planner-selection scripts pass without debug-only semantic differences, and canonical metadata no longer changes when Guide-Bot replans.

Rollback: restore the old step scanner while retaining separate result storage.

### Phase 5: Restore and Enforce the Classic Movement Boundary

Work:

- Remove `Metadata_route_path_depth`, `create_path_to_segment_metadata_route()`, and the metadata edge override from `aipath.c`; restore the original inline doorway/openable-door predicate.
- Make every route goal call ordinary `create_path_to_segment()` through the same classic scheduling and polishing lifecycle as other Guide-Bot goals.
- Remove `escort_route_goal_has_pending_path_for()` and restore the classic return-to-player interruption behavior.
- Remove `escort_start_default_goal_now()` path creation. Commands set route intent and let the normal Guide-Bot frame create the path; eliminate the current duplicate polish.
- Remove live visibility/reachability guidance searches that replace the shared waypoint segment. Use the shared first waypoint's segment directly and retain its exact activation and aim positions only as player-guidance data.
- Move "closest possible" entirely into high-level waypoint selection. Select a frontier before path creation, then issue one classic path request instead of performing a failed request followed by a route-specific fallback request.
- Keep the shared segment chain available only in planner tests and introspection diagnostics; never write it into `Point_segs`.
- Add path/RNG parity instrumentation and fixtures comparing an ordinary goal with a route goal given identical object state, goal segment, and RNG state.

Exit gate: for identical Guide-Bot state, goal segment, and RNG state, route and ordinary goal paths have identical `Point_segs`, path indices, AI modes, RNG call deltas, and return-to-player timing. End-of-level, unexplored, and closest-frontier guidance still select the intended high-level waypoint.

Rollback: disable shared high-level route selection while retaining the restored classic movement code. Do not restore the metadata path override.

### Phase 6: Add Passive Player Guidance and Completion

Work:

- Define every semantic progression step as a player action plus a passive completion predicate. Guide-Bot has no route-action state machine.
- Present the exact activation position, aim point, target wall/object, crossing side, and concise player instruction through messages, introspection, and automap markers.
- Navigate only toward the waypoint segment through classic movement. Do not move Guide-Bot to an exact XYZ point, alter its orientation, aim it, fire a route-specific flare, or force it across a trigger side.
- Preserve the existing generic forward-flare behavior without route-specific conditions or target selection.
- Observe only the effects required by the selected route, then invalidate/replan after the owning player picks up the key, shoots or opens the wall, crosses the trigger, destroys the boss/reactor, or enters the exit.
- Add player-assisted live tests: automation moves or acts as the player, verifies the guidance data first, performs the action, and confirms the route advances.
- Add a negative fixture proving Guide-Bot does not orient toward, fire at, cross, or autonomously complete a pending route action while the player waits.

Exit gate: KCXF2 hidden-door and switch scenarios identify the correct player pose and target, remain pending without Guide-Bot action, and advance only after the player performs the required action. Classic flare and movement traces remain unchanged for the same state and goal segment.

Rollback: disable enhanced guidance presentation while retaining semantic planning and classic movement. There is no route action executor to fall back from.

### Phase 7: Complete Invalidation, Caching, and Multiplayer

Work:

- Wire generation changes at wall, trigger, key, object, reactor, automap, save restore, and owner transitions.
- Coalesce automap invalidations so unexplored routing remains responsive without scanning each frame.
- Replan moving boss waypoints without rebuilding static topology.
- Run two-peer owner, observer-host, disconnect adoption, voluntary abdication, save restore, and slot-remap scenarios.
- Verify owner-local guidance, passive completion transitions, unchanged classic flare behavior, and owner-local unexplored selection.

Exit gate: no stale-plan failures in dynamic tests, no nonowner planner execution, and multiplayer ownership tests pass repeatedly.

Rollback: retain explicit event invalidation but route live selection through the prior plan cache.

### Phase 8: Remove Superseded Code

Delete only after all prior gates have been green for at least one full regression cycle:

- Duplicate live visibility sampling and reachable-segment BFS in `escort.c`; the shared planner's exact player-guidance pose is authoritative.
- `escort_find_nearest_reachable_goal_segment()` and related optimistic fallback logic.
- Live route-step satisfaction reconstruction.
- Old C semantic planner and global planner scratch arrays.
- Duplicate keyed/non-keyed opener policies that are replaced by normalized actions.
- Dead route kinds or legacy mappings that no longer have a producer.
- Stale tests and debug fields superseded by plan/action diagnostics.

`Metadata_route_path_depth` and `create_path_to_segment_metadata_route()` are removed in Phase 5 rather than deferred cleanup. Keep the classic generic AI pathfinder, its doorway predicate, path scheduling, RNG consumption, path polishing, return-to-player behavior, steering, collision handling, and generic flare behavior unchanged.

## Verification Matrix

| Layer | Required coverage | Acceptance |
| --- | --- | --- |
| Build | D1, D2, host unit targets, headless metadata, Android | All compile and link with C ABI intact |
| Edge unit tests | Every wall, key, trigger, control-center, hidden, buddy-proof, and blastable state | Exact rich decision and C projection |
| Planner unit tests | Keys, trigger chains, loops, multiple openers, hidden doors, boss/reactor, exits, unexplored | Exact deterministic plan signature |
| Completion tests | Required link subset, disabled trigger, missing target, reclosed door | Correct four-state evaluation |
| Differential tests | Legacy planner versus shared C++ planner | No unexplained difference |
| Corpus | All 1,281 checked-in records plus archive regeneration where available | No unreviewed status or step regression |
| Base campaigns | Descent and Counterstrike | Existing strict policy remains green |
| Live planner scripts | KCXF2, Obsidian, unexplored | Correct first pending shared waypoint |
| Live guidance scripts | Switch, hidden door, trigger crossing, blastable wait, fallback | Correct player instruction and marker; progress only after player action |
| Classic movement parity | Same state, goal segment, and RNG state through ordinary and route-goal bridges | Exact `Point_segs`, AI mode, path timing, and RNG delta match |
| Save/restore | Mid-path and while awaiting a player action for both endpoint modes | Intent preserved, transient plan rebuilt, classic movement resumed |
| Multiplayer | Owner, observer host, handoff, abdication, disconnect, slot remap | One owner plans guidance and simulates classic movement; intent persists |
| Determinism | Input-demo state/RNG matrices with newly recorded route fixtures | Stable record and replay with no route-specific replay patch |
| Performance | Planner count, snapshot build, rays, classic path request count, frame time | No periodic spikes, no per-frame full search, and no extra physical path request |

## Required New Regression Scenarios

1. A switch is visible only from an off-center valid pose. Guidance must expose that exact player pose and aim target; Guide-Bot must not aim or fire, and the route advances after the player hits the wall.
2. A hidden door is in the current segment but outside Guide-Bot's initial forward vector. Guidance must identify it without changing Guide-Bot orientation or flare behavior; the player opens it and the route continues.
3. A pass-through trigger requires crossing a specific side. Guidance identifies the side, Guide-Bot does not deliberately cross it, and only the player's crossing completes the step.
4. A blastable wall lies on the only route. Guide-Bot identifies the player action and continues after the player destroys it without attempting to damage the wall.
5. A trigger has several effects but only one is required for the selected route. Completion follows the required effect set.
6. A one-shot trigger is disabled while its required wall remains blocked. The plan becomes impossible or chooses another route; it is not skipped.
7. A target object disappears or a boss relocates after planning. The waypoint invalidates and replans without a full topology rebuild.
8. A shared planner chain differs from the classic Guide-Bot path. The chain remains diagnostic only; movement follows the classic path to the same high-level goal segment.
9. The largest unexplored component changes while the current target remains unvisited. Coalesced automap invalidation selects the new endpoint without a frame spike.
10. Ownership changes during navigation and while awaiting a player action. The old owner stops planning immediately and the new owner replans from local automap and synchronized world state without altering classic movement behavior.

## Corpus Policy

- Preserve a frozen pre-migration baseline rather than accepting freshly generated JSON as automatically correct.
- Compare status, problem category, semantic steps, activation kinds, stable target identities, and deterministic plan hash.
- Treat `ok` to `partial` or `failed` as a hard regression.
- Treat `partial` to `failed` as a hard regression.
- Treat an unexpected semantic-step or endpoint change as review-required even if status remains `ok`.
- Allow `partial` or `failed` to improve only with a focused fixture and reviewed explanation.
- Keep known base-game exceptions explicit and minimal.
- Regenerate from source mission archives when validating parser or adapter changes; checked-in JSON alone can be stale.

## Performance Plan

- Establish baseline counters before behavior changes: planner calls, topology builds, state snapshots, expanded nodes, edge decisions, visibility rays, classic path requests, RNG deltas, and replans by reason.
- Build trigger-to-effect and side-to-opener indices once per topology.
- Reuse planner workspaces to avoid repeated large allocations.
- Cache visibility by topology generation, target identity/position generation, navigator profile, and candidate pose.
- Stop visibility search as soon as the deterministic best valid pose is settled.
- Never run full semantic planning each frame or on the five-second physical path refresh.
- Replan unexplored endpoints on coalesced automap generation changes, not render writes individually.
- Add an automated budget based on measured baseline before setting numeric thresholds.

## Risk Register

| Risk | Mitigation |
| --- | --- |
| Large rewrite obscures regressions | Small phases, shadow mode, old implementation retained |
| Canonical metadata changes unintentionally | Frozen plan signatures and strict base campaigns |
| High-level goal changes perturb classic path RNG | Shared planning consumes no RNG; preserve classic path call timing and RNG behavior exactly for the same goal segment, and treat only a changed goal segment as intentional divergence |
| Shared C++ cannot be called cleanly from C-in-C++ engine files | Stable C-linkage facade and C-compatible result projection |
| Player and companion rules are conflated | Separate progression and navigator profiles |
| Dynamic state causes replan storms | Generation keys, relevance filtering, and coalescing |
| Multiplayer peers choose different unexplored targets | Only active owner plans; mode, not plan, is synchronized |
| Exact player-guidance pose is invalid | Validate player occupancy and visibility in the planner; do not move Guide-Bot to the exact pose |
| Corpus green but Guide-Bot still stalls | Player-assisted guidance tests plus classic movement parity and path-request diagnostics |
| Internal dynamic results exceed legacy ABI | Explicit projection overflow, no silent truncation |

## Proposed File Boundaries

Names are provisional, but responsibilities should remain separated:

- `shared/route_planner.h/.cpp`: domain model, edge evaluation, dependency search, endpoint policies, completion evaluation.
- `shared/route_planner_c.h/.cpp`: stable C ABI and projection into existing metadata structures.
- `shared/route_snapshot.h/.cpp`: immutable topology/state builders independent of D1/D2 globals.
- `shared/secret_area_game_adapter.c`: D1/D2 global-state adapter only.
- `shared/level_metadata_scan.c`: metadata aggregation wrapper during migration, deleted or reduced after cutover.
- `d2/main/escort.c`: command, ownership, messaging, passive completion observation, and a thin goal-segment bridge into classic movement.
- `d2/main/aipath.c`: unmodified classic generic AI paths; no metadata or route-planner dependency.
- `android/tests/test_route_planner.cpp`: engine-neutral planner unit tests.
- `android/tests/test_route_planner_parity.cpp`: old/new differential and projection tests during migration.

## Definition of Done

- End-of-level and unexplored use the same planner and differ only by endpoint policy.
- Metadata and Guide-Bot consume the same semantic `RoutePlan`; live code does not reconstruct route meaning.
- Guide-Bot receives only the selected goal segment and reaches toward it through the unmodified classic path lifecycle; shared chains and exact poses never control movement.
- Switches, hidden doors, crossing triggers, keys, blastable walls, bosses, reactors, and exits have explicit player actions and shared passive completion predicates.
- Exact activation and aim positions are exposed as player guidance without changing Guide-Bot orientation, firing, collision, steering, or path points.
- For identical Guide-Bot state, goal segment, and RNG state, route and ordinary goal movement produce identical paths, RNG deltas, AI modes, and interruption timing.
- "Closest possible" comes from the same route proof and names the next obstruction.
- Canonical metadata and live plans are stored separately.
- Dynamic state and owner changes invalidate only relevant cached work.
- The full corpus, base campaigns, player-assisted guidance tests, classic movement parity tests, multiplayer tests, performance checks, and determinism matrix pass.
- Duplicate route BFS, visibility, satisfaction, fallback, and metadata path-depth shims are removed.

## Recommended First Implementation Slice

Start with Phase 0 and Phase 1 only. The first behavior-changing slice should not be "convert everything to C++." It should be the smallest end-to-end proof:

1. Capture a topology/state snapshot.
2. Produce a rich edge decision and exact route plan in shadow mode.
3. Compare it with current metadata across synthetic fixtures and the corpus.
4. Expose the shared segment chain and exact activation pose in introspection.
5. Use one focused hidden-door or shoot-switch fixture to prove that the plan contains enough information to guide the player and passively detect completion without controlling Guide-Bot movement or actions.

That sequence uses the regression corpus immediately, preserves easy rollback, and attacks the information loss that currently sits between a correct metadata route and a stuck Guide-Bot.

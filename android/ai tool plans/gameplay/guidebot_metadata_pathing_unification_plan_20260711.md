# Guide-Bot and Metadata Pathing Unification Plan

Date: 2026-07-11
Status: Implementation started

## Executive Decision

Move toward one engine-neutral C++ route planner, but do it as a staged replacement rather than a rewrite. The first goal is observable behavioral parity. The second is to make metadata generation and live Guide-Bot navigation consume the same route result. The final cleanup can then remove the duplicate searches and interpretations that currently allow the two systems to drift.

The core invariant will be:

> Given the same topology snapshot, dynamic state, endpoint policy, and actor profile, every consumer receives the same semantic steps, segment chain, activation pose, required action, completion predicate, and partial-route frontier.

The Kotlin layer is not currently a second pathfinder. It deserializes and displays native metadata results. The routing split is inside native code: the metadata analyzer plans in `level_metadata_scan.c`, `escort.c` reinterprets those steps, and `aipath.c` independently constructs the physical path.

## Objective

Bring live Guide-Bot navigation and metadata route analysis to behavioral parity, then converge duplicated routing decisions into shared C++ code while preserving game-engine integration, deterministic simulation, multiplayer ownership, save compatibility, and the existing mission regression corpus.

Parity has two separate meanings and both must pass:

1. **Planner parity:** the same state produces the same progression, dependencies, target, and fallback.
2. **Executor parity:** Guide-Bot can physically reach the selected pose, perform or wait for the required action, detect completion, and continue.

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
  - Autonomous action/execution fixtures remain pending.
- [ ] Phase 1 in progress: add the engine-neutral topology/state snapshot in shadow-only form.
  - Added deterministic C++ topology and mutable-state snapshots, hashes, validation, C ABI summary, unit tests, and canonical level-load capture for D1 and D2.
  - Expanded the shadow snapshot with normalized D1/D2 trigger kinds, trigger effect links, per-side opener source walls, mutable trigger flags/disabled state, and live progression-object identity, containment, position, boss, and companion state.
  - Added a separate live Guide-Bot snapshot summary for route-only replans; canonical level-load diagnostics are no longer the only observable snapshot and are cleared independently on level changes.
  - Exposed the canonical snapshot summary through introspection without changing route selection or simulation RNG.
  - Added normalized wall kinds, key requirements, lock/open/hidden state, endpoint kinds, progression and navigator profiles, and the explicit route-query domain type.
  - Added engine-derived side centers and stable wall target points to shared topology; the C analyzer now consumes the same side-center callback with its vertex average retained as a headless fallback.
  - Added independent mutable-state fingerprints and live generations for start, progression, navigation, trigger, object, and automap domains, exposed for canonical/live introspection and future cache keys.
  - Preserved each semantic step's exact analyzer-selected activation position and distinct wall/object aim point through the shared route-step ABI and live introspection; off-center visible firing positions are no longer discarded.
  - Navigator-radius occupancy validation, action-specific hit validation, and event-driven generation hooks remain pending.
- [ ] Phase 2 in progress: centralize rich edge evaluation after snapshot parity is established.
  - Added a shared C++ edge evaluator with rich blocker/action results and a legacy three-state projection.
  - Added canonical per-side shadow comparison against the active C evaluator, mismatch introspection, and progression/navigator capability unit coverage.
  - Confirmed zero edge-cost mismatches in synthetic D1/D2 fixtures and the Counterstrike level 1 emulator route scan while preserving all 1,274 reviewed corpus route projections.
  - Add an opt-in strict headless shadow gate and a no-copy host corpus mode so archive regeneration can execute old/new edge parity across every loaded level without rewriting reviewed JSON.
  - The C planner remains authoritative; corpus-wide strict shadow execution and any discovered semantic gaps remain under review.

## Non-Goals

- No route behavior changes during this planning pass.
- No mission-specific exceptions.
- No immediate conversion of upstream D1 or D2 AI source files to C++.
- No replacement of every classic Guide-Bot goal. Initial unification covers end-of-level and unexplored route goals.
- No deletion of legacy path behavior for classic Guide-Bot and robot goals until their callers are intentionally migrated.
- No synchronization of full route plans over multiplayer. Route intent remains synchronized and the active owner computes the transient plan.

## Constraints

- Metadata generation must remain headless and independent of a rendered game loop.
- Live execution must respond to doors, triggers, keys, destroyed objects, automap exploration, save restore, and cooperative ownership changes.
- The route planner must not read D1 or D2 globals directly.
- D1 and D2 must use the same planner through a C ABI and game-specific adapters.
- Input-demo recording and replay must remain deterministic after route behavior changes and new fixtures are recorded.
- The checked-in mission JSON corpus is an acceptance oracle, not proof of live executability.
- Planner ranking must not depend on platform floating-point tie behavior, unordered iteration, or simulation RNG.
- Guide-Bot and player capabilities are not identical. Legitimate differences must be explicit actor-profile data, not separate algorithms.

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

## Parity Matrix

| Decision | Metadata today | Guide-Bot today | Unified target |
| --- | --- | --- | --- |
| End-of-level dependencies | Native analyzer | Same emitted steps | One `RoutePlan` |
| Unexplored dependencies | Same analyzer with another endpoint | Same emitted steps plus local target copy | Same planner, endpoint policy only |
| Dynamic start | Player start for canonical metadata | Guide-Bot object for live route | Explicit query start |
| Actor restrictions | Usually player-like | Companion buddy-proof restriction | Explicit progression and navigator profiles |
| Edge semantics | Rich internal blocker, coarse public cost | Coarse public cost only | Shared rich `EdgeDecision` |
| Path ranking | Weighted Dijkstra | BFS by segment count | Shared deterministic ranking |
| Segment chain | Computed then discarded | Recomputed randomly | Preserved in plan and consumed live |
| Firing position | Exact sampled position computed then discarded | Visible segment recomputed; position discarded | Exact occupiable pose and aim target |
| Trigger traversal | Source side known internally | Routes to source segment only | Required directed crossing in action spec |
| Hidden door | Semantic step emitted | Routes near wall; generic flare may miss it | Explicit aim, fire, verify state |
| Blastable wall | Treated immediately passable | Treated passable although buddy flare cannot damage it | Explicit player-required obstruction action |
| Step completion | Analyzer simulation | Reimplemented from live globals | Shared completion evaluator |
| Nearest fallback | Partial semantic result | Separate optimistic BFS heuristic | Frontier from the same failed plan proof |
| State invalidation | Fresh headless scan | Mostly keys, commands, restore, handoff, target visited | Generation-keyed snapshots and coalesced invalidation |
| Multiplayer | Not applicable | Owner-local plan, mode synchronized | Preserve owner-local planning |
| Determinism | No RNG, but double ties | Physical BFS consumes simulation RNG | Integer ranking and RNG-free route execution |

## Audit Findings

### P0: Semantic Steps Are Not Executable Contracts

`level_metadata_route_step` stores a segment, side, wall, trigger, and label, but not the path, exact terminal pose, aim point, directed transition, action owner, or completion predicate. The analyzer can prove that a switch is visible from a sampled point and then discard that point. Guide-Bot later navigates to the segment center and may have no line of fire.

This is the clearest explanation for a route that is correct in metadata but stalls in-game.

### P0: Physical Path Selection Still Diverges

Metadata uses weighted Dijkstra over segment-center distances and progress cost. `aipath.c` uses randomized BFS, a maximum depth, legacy safety polishing, and partial-path fallback. Sharing edge classification does not make these searches equivalent. The physical endpoint can differ from the planner's endpoint even when both use the same passable-edge rule.

### P0: Route Actions Are Mostly Unimplemented

The companion's generic flare code does not target the selected route wall. `openable_doors_in_segment()` explicitly excludes hidden doors and recognizes only ordinary unlocked doors. Shoot-switch and hidden-door objectives therefore depend on incidental orientation.

Fly-through and pass-through trigger steps have a second gap: the source side is known, but Guide-Bot is sent only to the source segment. Standing in that segment does not guarantee crossing the required side and firing the trigger.

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

The shared planner should return an occupiable pose validated for the navigator radius and an action-specific line trace that confirms the intended wall or object can be hit.

### P1: Nearest-Progress Fallback Is a Different Planner

The fallback in `escort.c` computes current reachability and an optimistic reverse BFS, then ranks candidates primarily by segment count. It does not reuse the analyzer's weighted route, dependency proof, avoided-trigger state, or exact blocker. It can choose a point that is close in a graph that requires an impossible sequence of actions.

The route search should retain its best reachable frontier and the next unresolved action. "Get as close as possible" can then follow the same selected route rather than inventing another one.

### P1: Dynamic Invalidation Is Incomplete

Full metadata rescans are now dirty-driven, which avoids earlier CPU spikes, but the invalidation model is implicit. Keys, commands, save restore, owner handoff, and an explored target cause replanning. Arbitrary wall/trigger changes, control-center links, object removal, boss movement, and changes elsewhere in an unexplored component do not have a unified generation signal.

The five-second Guide-Bot refresh currently resets the legacy goal and physical path, but does not mark metadata dirty. The automation script `test_guidebot_unexplored_goal.json5` still expects a `periodic_refresh` replan reason that no longer exists in the code. That stale expectation must be resolved before the script can be treated as a migration gate.

### P1: Canonical and Live Route State Are Conflated

A live route-only rescan copies its route fields into global `Level_metadata_state`, replacing the canonical route generated at level load. Static metadata, live owner state, route introspection, and Guide-Bot execution should have separate result objects built by the same planner.

### P1: Player Progression and Guide-Bot Movement Need Separate Profiles

Metadata describes how the player can complete a level. Live routing asks where the Guide-Bot can move while escorting that progression. Buddy-proof walls and navigator radius are real differences, while keys belong to the owning player and some actions can be performed by either the owner or companion.

A single "actor" flag is insufficient. A query needs both:

- A **progression profile** for owner keys and actions that make the level advance.
- A **navigator profile** for Guide-Bot traversal, physical radius, and actions it can perform itself.

Semantic steps should remain the same. The live plan adds executable Guide-Bot waypoints or a well-defined near-side frontier when the companion cannot occupy the player's destination.

### P1: Existing Live Tests Stop Before Execution

Current KCXF2 and Obsidian scripts validate step selection and then directly grant keys or fire triggers through debug controls. The hidden-door script checks that a path remains pending, not that Guide-Bot opens the door. These are useful planner tests but not executor tests.

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

The current JSON schema remains stable initially by projecting `RoutePlan` into `level_metadata_route_step`. New executable fields stay internal until their format is mature. Debug introspection may expose them immediately.

### 6. Shared Step Evaluation

The planner library exposes `evaluate_waypoint(waypoint, state)`. It returns:

- `pending`: required state has not changed yet.
- `complete`: the exact required predicate is true.
- `invalidated`: the target identity or assumptions changed and the route must be recomputed.
- `impossible`: a one-shot or destroyed dependency can no longer produce the required effect.

`escort.c` should not inspect trigger links, key objects, boss lists, or wall state to reinterpret planner intent after this migration.

### 7. Thin Live Executor

`escort.c` remains responsible for command selection, messages, ownership, save intent, and the Guide-Bot state machine. A new route executor is responsible for:

1. Follow the shared segment chain in bounded chunks.
2. Reach the exact validated terminal pose.
3. Orient toward the exact aim point or crossing side.
4. Perform a Guide-Bot-capable action once, or wait for the owning player.
5. Verify the shared completion predicate.
6. Replan on timeout, invalidation, displacement, or path-materialization failure.

The action policy should initially be:

| Action | Responsibility |
| --- | --- |
| Pick up key | Owning player |
| Open keyed/ordinary door | Either, using owner keys |
| Shoot switch | Guide-Bot may fire an aimed flare |
| Open hidden door | Guide-Bot may fire an aimed flare |
| Cross companion-compatible trigger | Guide-Bot may cross the directed side |
| Destroy blastable wall | Owning player |
| Destroy reactor or boss | Owning player |
| Enter exit | Owning player |

This preserves gameplay capabilities instead of making the planner's assumptions alter game rules.

### 8. Physical Path Materializer

Add a Guide-Bot route-path entry point that accepts the planner's segment chain and final pose. It writes compatible `Point_segs` and applies existing safety geometry checks without rerunning a graph search.

The materializer must:

- Validate each directed segment transition for the companion profile.
- Respect global path storage and emit bounded path chunks.
- Use the planner's exact terminal pose for the final point.
- Return a structured failure containing the first invalid transition.
- Never silently substitute an unrelated partial path.
- Avoid simulation RNG for route goals.

Classic Guide-Bot goals may continue to use legacy randomized path creation during the initial migration.

### 9. Partial Route and "Closest Possible"

Every search retains the best reachable frontier on the selected dependency proof. The frontier includes the reachable segment, pose, unresolved side, required action, and reason progress cannot continue.

When a complete physical path cannot be materialized, Guide-Bot follows this frontier and reports the next obstruction. This replaces the independent nearest-point BFS and gives end-of-level and unexplored goals identical fallback behavior.

### 10. State Generations and Cache Policy

Build cache keys from the topology hash, relevant dynamic generations, endpoint, profiles, and start state. Do not use periodic full rescans as correctness machinery.

- Topology is rebuilt once per level load.
- Wall and trigger changes invalidate edge and dependency results.
- Key changes invalidate owner progression.
- Object destruction or relocation invalidates only affected objective data.
- Automap changes are coalesced and invalidate unexplored endpoint selection.
- Owner handoff invalidates the navigator start, key owner, and owner-local automap state.
- Ordinary movement advances through the existing segment chain and does not force full replanning.

### 11. Multiplayer Policy

Only the peer that locally owns Guide-Bot plans and executes its route. Other peers render synchronized robot state and retain route intent for UI/ownership transfer.

- Keep synchronizing target mode and owner generation.
- Do not put segment chains or firing poses in ownership packets initially.
- On adoption, abdication, disconnect transfer, or restore remap, clear transient execution and recompute from synchronized world state plus the new owner's local automap.
- Ensure an aimed trigger action is emitted once by the owner and propagated through existing trigger/door network messages.
- Expose plan hash and state generations in introspection so two-peer tests can distinguish intentional owner-local differences from stale ownership.

### 12. Determinism Policy

- Planner searches use integer/fixed ranking and stable ID tie-breaks.
- Route execution consumes no simulation RNG.
- Shadow planning must have no gameplay side effects and no RNG calls.
- Treat existing input demos as regression evidence, not compatibility artifacts that constrain a corrected engine.
- After intentional route behavior changes, record new fixtures and require the shared deterministic route chain to match on record and replay.
- Fix any new record/replay divergence in the engine rather than adding planner-specific replay behavior.

## Migration Phases

Each phase must be independently buildable, testable, and reversible. Until Phase 8, retain the old path behind a debug or compatibility switch. Shadow mode logs differences but does not alter gameplay.

### Phase 0: Freeze Baselines and Characterize Gaps

Work:

- Capture a normalized full-corpus baseline keyed by mission identity, archive hash, level number, route status, problem category, semantic step signature, endpoint, and travel distance.
- Record the current corpus snapshot: 1,281 level records, 1,180 `ok`, 47 `partial`, and 47 `failed` as audited on 2026-07-11.
- Keep Descent and Counterstrike strict through `test_base_mission_route_status.ps1`.
- Add characterization fixtures for hidden-door aiming, shoot-switch aiming, directed trigger crossing, blastable walls, buddy-proof walls, disabled triggers, relevant-link completion, and partial frontier selection.
- Split live tests into planner-selection tests and autonomous-execution tests.
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
- Run the old C planner and new C++ planner in corpus shadow mode.
- Categorize every difference as bug fix, deterministic tie resolution, or regression before accepting it.

Exit gate: base campaigns remain strict; full-corpus status does not regress; all changed semantic signatures are reviewed; synthetic parity tests pass in D1 and D2.

Rollback: retain the C planner as the active implementation.

### Phase 4: Separate Canonical and Live Plans

Work:

- Store canonical metadata and live Guide-Bot route results separately.
- Make end-of-level and unexplored commands construct the same `RouteQuery` with different endpoint policies.
- Make `escort.c` select the first pending shared waypoint instead of rebuilding satisfaction logic.
- Preserve legacy `ESCORT_GOAL_*` only as a UI/AI state bridge.
- Add shadow comparison between the old selected goal and the new first pending waypoint.

Exit gate: planner-selection scripts pass without debug-only semantic differences, and canonical metadata no longer changes when Guide-Bot replans.

Rollback: restore the old step scanner while retaining separate result storage.

### Phase 5: Consume the Shared Segment Chain

Work:

- Add the segment-chain path materializer and exact final pose support.
- Use it only for Android end-of-level and unexplored route goals.
- Keep legacy path creation for classic goals that have not been migrated.
- Replace silent partial path output with structured failure and the shared frontier.
- Track path chunk, chain index, materialization failure, and frontier in introspection.

Exit gate: physical endpoints and chain transitions match the planner in automated levels; route goals consume no simulation RNG; newly recorded input-demo determinism tests pass.

Rollback: switch route goals back to `create_path_to_segment_metadata_route()`.

### Phase 6: Add Explicit Action Execution

Work:

- Add navigate, orient, act/wait, verify, timeout, and replan states.
- Aim flares at exact shoot-switch and hidden-door targets.
- Materialize directed crossing for fly-through/pass-through triggers.
- Stop on the near side of player-only blastable, key, boss/reactor, and exit actions.
- Verify only the effects required by the selected route.
- Add autonomous live tests that do not call `fire_trigger` to advance.

Exit gate: KCXF2 hidden-door and switch scenarios autonomously reach and activate their target, then advance to the next semantic step.

Rollback: disable route action execution while retaining shared planning and path chains.

### Phase 7: Complete Invalidation, Caching, and Multiplayer

Work:

- Wire generation changes at wall, trigger, key, object, reactor, automap, save restore, and owner transitions.
- Coalesce automap invalidations so unexplored routing remains responsive without scanning each frame.
- Replan moving boss waypoints without rebuilding static topology.
- Run two-peer owner, observer-host, disconnect adoption, voluntary abdication, save restore, and slot-remap scenarios.
- Verify exactly-once route actions and owner-local unexplored selection.

Exit gate: no stale-plan failures in dynamic tests, no nonowner planner execution, and multiplayer ownership tests pass repeatedly.

Rollback: retain explicit event invalidation but route live execution through the prior plan cache.

### Phase 8: Remove Superseded Code

Delete only after all prior gates have been green for at least one full regression cycle:

- Duplicate visibility sampling and reachable-segment BFS in `escort.c`.
- `escort_find_nearest_reachable_goal_segment()` and related optimistic fallback logic.
- Live route-step satisfaction reconstruction.
- `Metadata_route_path_depth` and `create_path_to_segment_metadata_route()`.
- Old C semantic planner and global planner scratch arrays.
- Duplicate keyed/non-keyed opener policies that are replaced by normalized actions.
- Dead route kinds or legacy mappings that no longer have a producer.
- Stale tests and debug fields superseded by plan/action diagnostics.

Keep the legacy generic AI pathfinder for classic robot and classic Guide-Bot behavior unless a later project deliberately replaces it.

## Verification Matrix

| Layer | Required coverage | Acceptance |
| --- | --- | --- |
| Build | D1, D2, host unit targets, headless metadata, Android | All compile and link with C ABI intact |
| Edge unit tests | Every wall, key, trigger, control-center, hidden, buddy-proof, and blastable state | Exact rich decision and C projection |
| Planner unit tests | Keys, trigger chains, loops, multiple openers, hidden doors, boss/reactor, exits, unexplored | Exact deterministic plan signature |
| Completion tests | Required link subset, disabled trigger, missing target, reclosed door | Correct four-state evaluation |
| Differential tests | Old C planner versus C++ shadow planner | No unexplained difference |
| Corpus | All 1,281 checked-in records plus archive regeneration where available | No unreviewed status or step regression |
| Base campaigns | Descent and Counterstrike | Existing strict policy remains green |
| Live planner scripts | KCXF2, Obsidian, unexplored | Correct first pending shared waypoint |
| Live executor scripts | Switch, hidden door, trigger crossing, blastable wait, fallback | Autonomous progress or explicit player wait |
| Save/restore | Mid-path and mid-action for both endpoint modes | Intent preserved, transient plan rebuilt |
| Multiplayer | Owner, observer host, handoff, abdication, disconnect, slot remap | One owner executes; intent persists |
| Determinism | Input-demo state/RNG matrices with newly recorded route fixtures | Stable record and replay with no route-specific replay patch |
| Performance | Planner count, snapshot build, rays, path materialization, frame time | No periodic spikes; no per-frame full search |

## Required New Regression Scenarios

1. A switch is visible only from an off-center valid pose. Guide-Bot must reach that exact pose and hit the intended wall.
2. A hidden door is in the current segment but outside Guide-Bot's initial forward vector. It must orient, fire, observe the door opening, and continue.
3. A pass-through trigger requires crossing a specific side. Reaching the source segment without crossing must not count as complete.
4. A blastable wall lies on the only route. Guide-Bot must stop near it, identify the player action, and continue after the player destroys it.
5. A trigger has several effects but only one is required for the selected route. Completion follows the required effect set.
6. A one-shot trigger is disabled while its required wall remains blocked. The plan becomes impossible or chooses another route; it is not skipped.
7. A target object disappears or a boss relocates after planning. The waypoint invalidates and replans without a full topology rebuild.
8. A physical chain transition fails companion-radius validation. The planner tries an alternate route or exposes the matching partial frontier.
9. The largest unexplored component changes while the current target remains unvisited. Coalesced automap invalidation selects the new endpoint without a frame spike.
10. Ownership changes during navigation and during an aimed action. The old owner stops immediately and the new owner replans from local automap and synchronized world state.

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

- Establish baseline counters before behavior changes: planner calls, topology builds, state snapshots, expanded nodes, edge decisions, visibility rays, path materializations, and replans by reason.
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
| Guide-Bot path changes demo RNG timing | RNG-free route path, then record new deterministic fixtures |
| C++ cannot be called cleanly from C engine files | Stable `extern "C"` facade and C-compatible result projection |
| Player and companion rules are conflated | Separate progression and navigator profiles |
| Dynamic state causes replan storms | Generation keys, relevance filtering, and coalescing |
| Multiplayer peers choose different unexplored targets | Only active owner plans; mode, not plan, is synchronized |
| Exact pose is visible but physically invalid | Navigator-radius occupancy and transition validation |
| Corpus green but Guide-Bot still stalls | Autonomous action/executor tests and timeout diagnostics |
| Internal dynamic results exceed legacy ABI | Explicit projection overflow, no silent truncation |

## Proposed File Boundaries

Names are provisional, but responsibilities should remain separated:

- `shared/route_planner.h/.cpp`: domain model, edge evaluation, dependency search, endpoint policies, completion evaluation.
- `shared/route_planner_c.h/.cpp`: stable C ABI and projection into existing metadata structures.
- `shared/route_snapshot.h/.cpp`: immutable topology/state builders independent of D1/D2 globals.
- `shared/secret_area_game_adapter.c`: D1/D2 global-state adapter only.
- `shared/level_metadata_scan.c`: metadata aggregation wrapper during migration, deleted or reduced after cutover.
- `d2/main/escort.c`: command, ownership, messaging, and thin route executor bridge.
- `d2/main/aipath.c`: generic AI paths plus segment-chain materializer entry point.
- `android/tests/test_route_planner.cpp`: engine-neutral planner unit tests.
- `android/tests/test_route_planner_parity.cpp`: old/new differential and projection tests during migration.

## Definition of Done

- End-of-level and unexplored use the same planner and differ only by endpoint policy.
- Metadata and Guide-Bot consume the same semantic `RoutePlan`; live code does not reconstruct route meaning.
- Guide-Bot follows the planner's segment chain and exact terminal pose without a second graph search.
- Switches, hidden doors, and crossing triggers have explicit executable actions and shared completion predicates.
- Blastable and buddy-proof obstructions are modeled according to actual companion capability.
- "Closest possible" comes from the same route proof and names the next obstruction.
- Canonical metadata and live plans are stored separately.
- Dynamic state and owner changes invalidate only relevant cached work.
- The full corpus, base campaigns, autonomous live tests, multiplayer tests, performance checks, and determinism matrix pass.
- Duplicate route BFS, visibility, satisfaction, fallback, and metadata path-depth shims are removed.

## Recommended First Implementation Slice

Start with Phase 0 and Phase 1 only. The first behavior-changing slice should not be "convert everything to C++." It should be the smallest end-to-end proof:

1. Capture a topology/state snapshot.
2. Produce a rich edge decision and exact route plan in shadow mode.
3. Compare it with current metadata across synthetic fixtures and the corpus.
4. Expose the shared segment chain and exact activation pose in introspection.
5. Use one focused hidden-door or shoot-switch fixture to prove that the plan contains enough information for a future executor.

That sequence uses the regression corpus immediately, preserves easy rollback, and attacks the information loss that currently sits between a correct metadata route and a stuck Guide-Bot.

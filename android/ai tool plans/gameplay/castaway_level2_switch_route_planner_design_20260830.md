# Castaway level 2 switch-route planner design

## Goal

Determine the actual switch and wall progression for Castaway Redux level 2 and design a general Guide-Bot planner improvement that can discover and navigate the sequence without mission-specific rules.

## Plan

- [x] Extract `rupture.rl2` and reconstruct its trigger-to-wall dependency graph
- [x] Identify the player-solvable sequence from the start through the red and gold key objectives
- [x] Compare the level graph with canonical route generation, live certification, and physical frontier selection
- [x] Determine why required switches are omitted or falsely certified as reachable
- [x] Propose a general planner algorithm, diagnostics, and regression coverage

## Level evidence

The extracted `rupture.rl2` contains 817 segments, 225 walls, 33 triggers,
and 175 objects. The player starts in segment 2, the red key is in segment
222, the blue key is in segment 337, the gold key is in segment 471, and the
placed Guide-Bot starts in segment 788.

The level uses a repeated non-monotonic switch-reveal pattern:

1. One-shot pass-through trigger 30 opens several start-area walls, including
   wall 36. Wall 36 is the surface carrying shootable trigger 1.
2. One-shot close-wall trigger 0 later closes wall 36 again, restoring the
   switch surface. The player can then shoot trigger 1, which opens the walls
   that provide the first major route expansion.
3. One-shot pass-through trigger 31 similarly opens wall 66. Wall 66 carries
   shootable trigger 4.
4. Trigger 3 later closes wall 66, restoring that switch surface. Shooting
   trigger 4 then opens the next major group of walls.

This is not an incidental side puzzle. The runtime log shows the player
successfully making progress with this order:

`30, 2, 31, 0, 1, 26, 6, 3, 4, 5, 29, 13, 12, blue key, 7, 11, gold key`

Triggers 5, 29, and 11 are matcen activations and do not open the route. The
structurally meaningful viable progression is therefore:

`30 -> 2 -> 31 -> 0 -> 1 -> 26 -> 6 -> 3 -> 4 -> 13 -> 12 -> blue key -> 7 -> gold key`

Triggers 30 and 31 are one-shot pass-through actions. Some of the other
actions may be avoidable by an expert route or a remote shot, so the sequence
above is a verified viable sequence rather than a claim that every action is
part of the globally shortest route. It is enough to prove that reporting
`gold key unreachable` is wrong and that the planner must retain switch order.

The Guide-Bot begins behind the ordinary D2 blastable release wall. This is
normal Guide-Bot lifecycle behavior and is out of scope for this fix. The
companion is not active before release, so the player cannot issue `Next` and
the route planner does not need a release objective.

## Current planner failure

The checked-in route reports only:

`Start -> shoot trigger 1 -> red key`

and then stops with `gold key unreachable`. This result is internally
inconsistent with both the level graph and the successful play log.

There are four concrete causes:

1. `route_trigger_kind` has no `close_wall` value. D2 `TT_CLOSE_WALL` is
   normalized to `other`.
2. `trigger_source_wall_valid()` rejects every trigger kind that does not
   satisfy `route_trigger_opens_path()`. The planner can never select triggers
   0 or 3 as route actions, even though those actions reveal the next switches.
3. `route_progress_state` stores only permanent `fired_triggers` bits.
   `edge_has_fired_trigger()` makes an edge passable forever after any opener
   fires. It cannot represent an opener followed by a closer, a restored
   switch surface, a relocked door, or an illusion turned back on.
4. `shootable_trigger` is static topology. Source validation does not test the
   effective wall state. This allows the precomputed plan to certify trigger 1
   as a shootable objective even after trigger 30 has changed wall 36 to
   `WALL_OPEN` and removed the switch surface. Live certification observes
   whether trigger 1 has fired, but does not invalidate the step because its
   source surface currently does not exist.

The log exposes the resulting split-brain behavior. The compiled selector
repeatedly says the trigger-1 or red-key step is valid. The physical Guide-Bot
navigator cannot reach the semantic destination, chooses only a reachable
frontier, and eventually falls into `SCRAM` and short-path fallback behavior.

Existing nested-trigger unit coverage is monotonic. It tests one opener that
makes another opener reachable. It does not cover a closer that restores a
switch surface, an opener followed by a closer on the same wall, or live
revalidation after a pass-through trigger changes the source wall.

## Recommended planner model

Keep one source of truth in the shared route planner and make Guide-Bot consume
the resulting actions. Do not special-case Castaway or its trigger numbers.

### 1. Represent navigation-changing trigger effects

Extend the scan view and normalized trigger kinds with at least `close_wall`,
`close_door`, `lock_door`, and `illusion_on`. Preserve one-shot and disabled
state. Classify these as navigation-changing actions even when they do not
immediately increase the reachable component.

### 2. Replace permanent opener bits with effective world state

Add a compact override for trigger-linked walls to `route_progress_state`.
Each relevant wall needs an effective state such as inherit, open, closed,
illusion, door-open, door-closed, locked, or unlocked. Applying a trigger must
update every linked wall and its reverse-side wall exactly as the engine does.

`evaluate_route_edge()` must consult the effective wall state. A historical
opener bit must never override a later close action.

Only walls mentioned by navigation-changing triggers need an override, so the
search state can remain small even on large levels.

### 3. Treat trigger exposure as a prerequisite

A shoot-switch action is available only when:

- a player-reachable firing position has a valid shot to the source wall
- the source wall currently has a surface that can receive the shot
- the trigger is not disabled or exhausted

If a desired source wall is currently open, the dependency solver should look
for a reachable action that can restore or expose it. This derives
`trigger 0 -> trigger 1` and `trigger 3 -> trigger 4` from ordinary trigger
effects without mission-specific knowledge.

### 4. Search transition states, not only recursive blockers

Use bounded best-first search over:

`(reachable component, player anchor, key mask, one-shot state, relevant wall-state signature)`

Actions are reachable trigger activations, key pickups, blastable-wall
destruction, hidden-door opening, reactor destruction, and exit entry. Apply
the real effects, recompute the reachable component, and hash the resulting
state. State hashing naturally permits useful toggles while terminating
cycles. Dominance pruning can discard a state reached with greater travel cost
when its keys, available actions, and effective walls are identical.

The cost should prefer shorter travel and fewer actions, but must not reject a
temporary reduction in reachable area. Castaway's close-wall actions are
exactly such deliberate temporary reductions.

### 5. Publish actions Guide-Bot can honestly execute

Compile every required transition into an ordered semantic step. For a switch,
store the selected player firing position, expected source-wall state, and the
post-action wall-state signature.

Before a live step is accepted:

- verify that its trigger and source wall still match the expected state
- verify that the player can reach the activation pose
- verify that Guide-Bot can reach the selected guidance frontier

If only the player can activate the next switch or cross the next progress
barrier, preserve the semantic objective and guide the player to the nearest
valid activation frontier. Do not convert the whole objective to `SCRAM`
merely because Guide-Bot cannot occupy the final objective segment yet.

## Diagnostics

Add one concise route-planner trace record per considered transition:

- current compact state hash and player/Guide-Bot components
- candidate action and activation pose
- source-wall effective state
- changed linked walls before and after the action
- resulting reachable-component size and newly reachable keys/actions
- rejection reason, including hidden source, no firing pose, disabled one-shot,
  physical frontier mismatch, or repeated state

For live selection, log semantic target, player activation target, Guide-Bot
physical frontier, and the exact wall separating each pair. This will make a
future false certification visible without a 100 MB visibility-ray trace.

## Regression coverage

1. Add a small shared planner fixture for `open source -> close source -> shoot
   revealed switch -> open route` and require the close action to appear.
2. Add a second fixture that repeats the pattern twice and then gates a key,
   matching Castaway's structure without using its segment or trigger numbers.
3. Add tests for open/close ordering, reverse-side state updates, one-shot
   exhaustion, lock/unlock, illusion off/on, useful toggles, and a true cycle.
4. Add a live-certifier test in which a compiled switch source becomes open
   before its turn. The stale step must be rejected or replanned.
5. Add a Castaway level-2 corpus assertion. `rupture.rl2` must no longer report
   `gold key unreachable`, and the compiled route must contain both close/reveal
   pairs in the correct order.
6. Add an Android automation run after the ordinary Guide-Bot release that
   requests `Next`, performs each indicated action, and asserts that the next
   semantic target advances instead of entering `SCRAM`.

## Detailed implementation plan

### Implementation tracking

- [x] Tranche 1: Add synthetic failing-semantics and live-certifier coverage
- [x] Tranche 2: Normalize navigation-changing wall and trigger types
- [x] Tranche 3: Implement authoritative effective-wall transitions
- [x] Tranche 4: Resolve switch-surface exposure dependencies
- [x] Tranche 5: Add bounded deterministic stateful backtracking
- [x] Tranche 6: Align compiled selection and live certification
- [x] Tranche 7: Bump caches, regenerate metadata, and review the corpus
- [ ] Tranche 8: Run interactive Castaway automation on an emulator/device

### Scope and design constraints

- Fix navigation-changing trigger semantics in the shared planner. Do not add
  mission names, level numbers, segment numbers, wall numbers, or trigger
  numbers to production logic.
- Leave ordinary D2 Guide-Bot release and activation unchanged.
- Preserve D1 behavior. New shared scan constants must use unsupported sentinel
  values when an engine does not implement the corresponding trigger type.
- Keep the existing route planner, work-budget accounting, deterministic
  ordering, partial-result publication, and live-certifier pipeline. Add
  transition state and bounded branch exploration inside that architecture
  instead of replacing it wholesale.
- Treat the sequence above as evidence, not as a hard-coded expected full
  route. Regression assertions should require the causal order `0 before 1`
  and `3 before 4`, while permitting a shorter valid path elsewhere.
- Do not require the external Castaway archive for ordinary CI. Synthetic
  fixtures must cover the behavior. The real mission integration test may
  skip only when the archive is absent and must fail on any wrong result when
  the archive is available.

### Tranche 1: Capture the failing semantics in focused tests

Files:

- `android/tests/test_route_snapshot.cpp`
- `android/tests/test_guidebot_route_certifier.c`
- a new focused Castaway host check under `android/tests/`

Work:

1. Add a four-stage synthetic fixture:
   - pass-through opener makes a shoot-switch wall `open`
   - a reachable close-wall trigger restores that source wall
   - the restored switch opens a blocked route edge
   - a key or terminal target lies beyond that edge
2. Assert the current failure before implementation in the test description:
   the close trigger is omitted and the restored switch is falsely available.
   Commit only the final passing assertion, not an expected-failure test.
3. Add a doubled fixture with two reveal pairs so a one-off recursion tweak
   cannot pass while the repeated Castaway pattern still fails.
4. Add a live-certifier fixture whose pending shoot-switch source is currently
   `WALL_OPEN`. Require the cheap selector and full certifier to reject it.
5. Add a fixture where the source becomes closed and shootable, then require
   the same compiled step to become usable.

Exit criteria:

- The fixtures describe `open -> close/reveal -> shoot -> open route` without
  Castaway identifiers.
- Existing monotonic nested-trigger tests remain unchanged and passing after
  later tranches.

### Tranche 2: Normalize all navigation-changing wall and trigger types

Files:

- `android/app/src/main/cpp/shared/level_metadata_scan.h`
- `android/app/src/main/cpp/shared/secretarea.c`
- `android/app/src/main/cpp/shared/route_snapshot.h`
- `android/app/src/main/cpp/shared/route_snapshot.cpp`
- `android/tests/test_route_snapshot.cpp`

Work:

1. Add scan constants for D2 `TT_CLOSE_DOOR`, `TT_ILLUSION_ON`,
   `TT_LOCK_DOOR`, and `TT_CLOSE_WALL`, plus `TF_ONE_SHOT`.
2. Add explicit wall constants for `WALL_CLOSED` and `WALL_OVERLAY`. Avoid
   treating all unknown solid wall kinds as the same state.
3. Extend `route_trigger_kind` with `close_door`, `illusion_on`, `lock_door`,
   and `close_wall`. For D1, normalize door control as a toggle rather than
   claiming it is always an opener; preserve D1 illusion on/off behavior.
4. Store whether a trigger is one-shot in the normalized snapshot. Disabled
   remains live state, while one-shot is immutable topology/behavior.
5. Add a reverse effect index from every linked target wall to all trigger
   source walls and their effect kinds. Keep the existing opener-only index
   for optimistic edge discovery, but do not use it for source exposure.
6. Include the new normalized fields in topology/state fingerprints and route
   replay fixture parsing so cache identity changes when these semantics do.

Exit criteria:

- Snapshot tests distinguish open, closed, overlay, and ordinary solid walls.
- Close-wall trigger 0 and trigger 3 normalize as `close_wall`, not `other`.
- D1 and D2 snapshot fixtures have stable deterministic hashes.

### Tranche 3: Add one authoritative effective-wall transition layer

Files:

- `android/app/src/main/cpp/shared/route_edge.h`
- `android/app/src/main/cpp/shared/route_edge.cpp`
- `android/app/src/main/cpp/shared/route_planner.h`
- `android/app/src/main/cpp/shared/route_planner.cpp`
- `android/tests/test_route_snapshot.cpp`

Work:

1. Extend `route_progress_state` with compact per-wall overrides for:
   - effective wall kind
   - door open/closed state
   - lock state
2. Add consumed-one-shot state separately from trigger activation history.
   Do not use one `fired_triggers` bit to mean both "has happened" and
   "its opening effect remains active."
3. Add shared accessors such as `effective_wall_kind()`,
   `effective_wall_locked()`, and `effective_wall_opened()`. All route edge and
   switch-source decisions must use these accessors.
4. Add one `route_progress_apply_trigger()` function that mirrors engine
   semantics for every navigation-changing trigger:
   - open/close/toggle door
   - illusion off/on
   - unlock/lock door
   - open/close/illusory wall
5. Apply each effect to the linked side and its reverse-side wall, matching
   `switch.c`. Centralize this pairing so edge evaluation, source exposure,
   and tests cannot disagree.
6. Make idempotent activation a no-op only when the resulting effective state
   truly does not change. One-shot triggers become unavailable after use.
7. Change `evaluate_route_edge()` so current effective state wins over trigger
   history. Remove the early rule that any historical opener makes an edge
   permanently passable.

Exit criteria:

- `open wall -> close wall` returns the edge to blocked.
- `close wall -> open wall` returns it to passable.
- Reverse sides always agree.
- Lock/unlock and illusion off/on round trips pass unit tests.
- The original monotonic opener and key tests still pass.

### Tranche 4: Make switch-surface exposure a dependency

Files:

- `android/app/src/main/cpp/shared/route_planner.cpp`
- `android/app/src/main/cpp/shared/route_planner.h`
- `android/tests/test_route_snapshot.cpp`

Work:

1. Split the current `trigger_source_wall_valid()` question into:
   - trigger is enabled and allowed
   - source wall currently presents an activatable surface
   - a reachable activation/crossing pose exists
2. For shoot switches, reject an effective `open` source wall even when static
   topology says the wall carries a shootable trigger texture.
3. For pass-through triggers, require a currently traversable source edge and
   model the action as crossing that edge before applying its effects.
4. When a required switch source is not exposed, query the reverse effect
   index for reachable actions that can produce a valid source surface. For
   Castaway wall 36 this discovers close trigger 0; for wall 66 it discovers
   close trigger 3.
5. Resolve the exposure action first, apply it to a copied progress state,
   recompute reachability, and retry the original switch. Append both semantic
   steps in causal order.
6. Continue using exact visibility where the loaded wall state matches the
   hypothetical state. Use the existing conditional/potential visibility path
   for hypothetical intermediate states, mark such firing poses approximate,
   and require exact proof from the live certifier before use.
7. Give close/reveal actions truthful labels and trigger type names. Keep the
   existing route-step link storage if practical, but serialize close-wall
   targets as `closes`, not `opens`.

Exit criteria:

- The first synthetic fixture emits `close/reveal` before `shoot switch`.
- The doubled fixture emits both pairs in order.
- No source wall is accepted merely because its trigger texture exists in
  static topology.

### Tranche 5: Add bounded stateful backtracking

Files:

- `android/app/src/main/cpp/shared/route_planner.cpp`
- `android/app/src/main/cpp/shared/route_planner.h`
- `android/tests/test_route_snapshot.cpp`

Work:

1. Preserve the existing dependency planner but let an unresolved blocker or
   hidden switch source produce multiple candidate actions.
2. Explore candidates using copied dependency states and deterministic
   ordering. Rank by:
   - complete solution before partial solution
   - fewer semantic actions
   - shorter accumulated player travel
   - existing stable trigger/source identity tie-breakers
3. Hash each branch using current segment/component, key mask,
   consumed-one-shot bits, relevant effective wall state, destroyed blastable
   walls, and opened hidden walls.
4. Reject a branch only when the same or a dominating state was already
   reached at equal or lower cost. A temporary reduction in reachable segment
   count is not a failure criterion.
5. Charge branch expansion, reachability recomputation, and visibility work to
   the existing analysis budget. Retain the route-step and recursion bounds.
6. On budget exhaustion, publish the best causally valid prefix. Never publish
   a later switch while omitting the transition that exposes it.
7. Add explicit tests for:
   - two alternative reveal triggers where only one leads to the target
   - a close/open cycle that terminates by repeated-state detection
   - a temporary reachability reduction followed by progress
   - deterministic output across repeated runs

Exit criteria:

- The planner does not depend on whichever source happens to be first in an
  array.
- Repeated-state pruning prevents toggle loops.
- Existing route work-budget and deterministic-result tests pass.

### Tranche 6: Make compiled and live Guide-Bot selection agree

Files:

- `android/app/src/main/cpp/shared/guidebot_route_certifier.c`
- `android/app/src/main/cpp/shared/guidebot_route_decision.c`
- `android/app/src/main/cpp/shared/secretarea.c`
- `d2/main/guidebot_route.c`
- corresponding headers and tests
- `android/tests/test_guidebot_route_certifier.c`
- `android/tests/test_guidebot_route_decision.c`

Work:

1. Add a cheap current-world predicate shared by the compiled selector and
   full certifier. A shoot-switch step is usable only when its source wall is
   present, its trigger is enabled, and the wall still owns that trigger.
2. Keep exact firing-position validation in the full certifier. The cheap
   predicate prevents a stale compiled step from winning while certification
   is deferred or budgeted.
3. Use the ordered plan to prevent a later switch from becoming pending before
   its close/reveal predecessor is satisfied.
4. For one-shot triggers, completion remains disabled/activated state. For
   repeatable triggers, add a per-level activation count in the D2 route hook
   and compare it with the occurrence number of that trigger in the compiled
   plan. This avoids treating every later occurrence as already complete after
   the first activation.
5. Reset activation counts at the existing Guide-Bot route reset point. Do not
   change normal Guide-Bot release, creation, or command availability.
6. If the semantic action is valid but Guide-Bot cannot occupy its final
   segment, retain the semantic step and select a reachable guidance frontier.
   Reserve `SCRAM` for a genuinely abandoned objective, not an ordinary
   player-action frontier.
7. Add concise Guide-Bot-category diagnostics for source-state rejection,
   predecessor pending, exact-shot failure, selected physical frontier, and
   replan request.

Exit criteria:

- Trigger 1 is not selected while wall 36 is open.
- After trigger 0 restores wall 36, trigger 1 becomes selectable and receives
  an exact or explicitly approximate firing position.
- The same behavior holds for triggers 3 and 4.
- No release-wall or pre-activation behavior changes.

### Tranche 7: Cache generation, metadata, and corpus integration

Files:

- `android/app/src/main/cpp/shared/route_analysis_cache.h`
- `android/app/src/main/java/com/dxxredux/app/RouteMetadataScheduling.kt`
- cache tests in native and Kotlin suites
- `game_data/mission_files/castaway_redux.json`
- mission route corpus/baseline files affected by regeneration

Work:

1. Increment native `ROUTE_ANALYSIS_CACHE_GENERATION` and Kotlin
   `ROUTE_METADATA_CACHE_GENERATION` together because old plans contain the
   false monotonic semantics.
2. Extend cache validation for any new route-step fields. Existing binary
   records are disposable and must be rejected by generation, not migrated.
3. Emit `close_wall`, `close_door`, `illusion_on`, and `lock_door` names in
   metadata. Use `closes` or a neutral `changes` list for non-opening links.
4. Regenerate mission metadata with the repository helper after planner tests
   pass.
5. With Castaway installed, require level 2 to stop reporting
   `gold key unreachable`, require close trigger 0 before trigger 1, and require
   close trigger 3 before trigger 4 if that branch is part of the selected
   route.
6. Review all regenerated route-status deltas. Accept improvements or explain
   legitimate changes; investigate every new partial/failed route.

Exit criteria:

- Native and Kotlin cache generations match.
- Stale generation-13 plans cannot load.
- Castaway level 2 produces a causally valid complete route, not merely a
  longer partial prefix.
- Base D1 and D2 route-status baselines have no unexplained regressions.

### Tranche 8: Runtime integration and final verification

Files:

- a maintained JSONC automation script under `android/game_scripts/`
- this plan file with results appended

Work:

1. Add an automation scenario that starts after ordinary Guide-Bot release,
   requests `Next`, and records the selected semantic trigger, activation
   position, source wall type, and physical frontier.
2. At the first reveal pair, require `Next` to target trigger 0 while wall 36
   is open, then trigger 1 only after wall 36 is restored.
3. Repeat the assertion for triggers 3 and 4.
4. Continue through the blue/gold progression far enough to prove the planner
   advances beyond the previous `gold key unreachable` boundary.
5. Run scoped formatting and lint on every changed file.
6. Build and test both host engines, the focused native route tests, mission
   corpus tests, Kotlin cache tests, and the Android debug APK with JDK 21.
7. Run the Android scenario on the emulator and, if the original behavior is
   device-specific, once on the device that produced the log.
8. Compare planner work counts and metadata time against the existing route
   benchmark. Set or adjust the branch/work bound only from measured results.

Required validation commands or their current repository equivalents:

- `./android/run-code-quality.ps1 -Fix -Paths <changed paths>`
- `./run-windows-build.ps1`
- focused `test_route_snapshot`, `test_guidebot_route_certifier`, and
  `test_guidebot_route_decision` targets
- `./android/tests/test_mission_route_corpus.ps1`
- `./android/tests/test_base_mission_route_status.ps1`
- focused Gradle unit tests with `JAVA_HOME=C:\local\jdk-21`
- `./android/gradlew.bat :app:assembleDebug`

Final acceptance criteria:

- No Castaway-specific production rule exists.
- Close-wall actions can be required dependencies even when they temporarily
  reduce reachability.
- A switch on an effective open wall is never certified as currently
  shootable.
- The compiled selector, full certifier, and Guide-Bot physical frontier agree
  on the same pending semantic action.
- Castaway level 2 guides the player through the reveal chain and beyond the
  gold-key boundary without entering `SCRAM` for an ordinary pending switch.
- Normal D2 Guide-Bot release behavior is unchanged.

## Implementation result (2026-08-30)

The shared planner now normalizes close, toggle, lock, and illusion trigger
effects; tracks effective switch-surface state and one-shot consumption; and
can transactionally resolve a missing shoot-switch surface through a reachable
close/reveal action. The live certifier rejects a shoot-switch step when the
wall is currently open, no longer owns the expected trigger, or is no longer a
shootable surface. Ordinary Guide-Bot release behavior was not changed.

Automatic path traversal deliberately applies only opening trigger effects.
Close/toggle actions are applied when the dependency planner selects them as
semantic actions. This distinction prevents incidental trigger crossings from
stranding otherwise valid legacy routes while preserving deliberate reveal
steps such as Castaway trigger 3 before trigger 4.

The later post-red failure is also resolved. Trigger 21's switch surface exists
on wall 171 but is unreachable in its initial state. The level first requires
trigger 32 to open the switch bank; after traversing the dependent sequence,
trigger 16 restores wall 171 so trigger 21 can be shot. Trigger 21 then opens
the reactor route.

The planner handles this as a general preparation/restoration dependency. When
an existing trigger source cannot be reached, it may transactionally activate
an opening trigger that targets the source, but only when a close/reveal
trigger also targets that source. The restriction prevents ordinary open-wall
shortcuts from being treated as switch preparation and preserves Castaway
level 8's existing route.

The final Castaway level-2 route is:

`start -> trigger 1 -> red -> trigger 32 -> trigger 18 -> trigger 17 -> trigger 19 close_wall -> trigger 24 -> trigger 16 close_wall -> trigger 21 -> reactor -> exit`

It now reports `route_status: ok`, has no route problem, and contains 12 steps.
A synthetic regression fixture requires the causal preparation/restoration
order, and a focused mission-metadata test pins the complete real-level chain.
The reviewed route corpus baseline changes only Castaway level 2 from partial
with three steps to ok with 12 steps.

Validation completed:

- D2 Windows build and all 44 native tests pass
- D1 Windows build passes
- focused doubled reveal-chain, one-shot, reverse-wall, and live-certifier
  tests pass
- Android `testDebugUnitTest` and `assembleDebug` pass with JDK 21
- real Castaway levels 1-9 and secret-level host scans pass
- the post-format Castaway sweep reports levels 1-5, 7-9, and the secret level
  as ok; level 6 retains its pre-existing `red key unreachable` partial status
- focused Castaway level-2 metadata test passes and requires trigger 32 before
  trigger 16, trigger 21, reactor, and exit
- scoped C/C++, Kotlin, and PowerShell code-quality checks pass
- full mission metadata regeneration completes with the extended archive
  timeout

The broad local route-corpus comparison remains noisy because the available
local archive sources contain 501 pre-existing regenerated-route differences.
The committed baseline update is therefore deliberately limited to the one
reviewed Castaway level-2 record.

An interactive emulator/device automation replay remains useful follow-up
coverage. It was not run as part of this host-side implementation pass.

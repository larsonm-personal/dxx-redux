# Remove legacy metadata travel calculation

## Goal
Make the executable metadata route planner the only authority for route status,
target reachability, distance, and travel time, then delete the older independent
travel traversal and the temporary status reconciliation.

## Plan
- [x] Inventory every producer and consumer of legacy travel fields and document
  the modern route planner's current target and distance semantics.
- [x] Define travel metrics as the modern end-route progression through keys,
  obstruction actions, reactor/boss, and exit.
- [x] Delete `collect_travel_time`, its private-only traversal helpers, and the
  exit reconciliation workaround.
- [x] Update scanner serialization and launcher presentation to use the unified
  status and distance result.
- [x] Regenerate generated mission metadata to use the unified result.
- [x] Add focused regressions for hidden doors, trigger barriers, unreachable
  targets, distance, and route status.
- [x] Run scoped quality, D1/D2 builds and CTest, Android tests/build, full host
  metadata regeneration, and a focused emulator route test.
- [x] Audit guidebot refresh, Unexplored reuse, public naming, host/launcher
  schema parity, and built-in mission generation for remaining duplication.

## Findings
- The legacy calculation was the original approximate completion estimator. It
  greedily visited hostages, reactor, and exit, with separate key-door logic and
  trigger-opened sides treated as passable shortcuts.
- The modern route planner was later added beside it because its original change
  explicitly preserved the old travel fields. It models ordered keys, trigger
  actions, hidden doors, boss/reactor, and exit, and is also consumed by
  guidebot.
- Keeping hostage-tour status would require a second route execution with a
  different objective order. That would preserve two authorities. The unified
  travel estimate therefore measures the same end-of-level progression shown in
  `route_steps`; hostage count remains ordinary level metadata.
- `travel_distance` and `travel_time_seconds` are retained as route metrics, but
  are now calculated by summing `distance_from_previous` across modern route
  steps. Duplicate travel status/problem/note/target/key-detour fields are
  removed; route status/problem/note are the sole result.
- The route status enum, constants, and formatter were renamed from Travel to
  Route. A dead metadata `obj_type_none` field was removed, and dynamic route
  refresh now copies route notes with the rest of the route result.
- The host normalizer was another schema authority that reinserted deleted
  fields. It now emits the unified schema and regenerates built-in Counterstrike
  in addition to archive missions.
- End-route guidebot navigation and Unexplored both call the shared modern
  progression and obstruction-aware movement functions. Unexplored replaces
  only the final destination after the calculated progression prefix.

## Validation
- Scoped code quality passed for C/C++, Kotlin, PowerShell, tests, and this plan.
- Windows D1 and D2 builds passed.
- D1 CTest passed 13/13; D2 CTest passed 14/14.
- Android `:app:testDebugUnitTest :app:assembleDebug` passed with JDK 21.
- Host regeneration passed built-in Counterstrike plus 109 archives; one archive
  without a mission descriptor was skipped and none failed.
- Corpus audit covered 1,281 levels: zero legacy field hits and zero mismatches
  between `travel_distance` and the sum of route-step distances.
- Host and Android launcher Counterstrike results matched on all 30 levels for
  status, problem, note, distance, and route-step signatures.
- Emulator base Counterstrike metadata analysis passed 3/3 steps.
- Emulator KCXF2 hidden-door guidebot regression passed 34/34 steps.
- KCXF2RM level 4 is `ok`; its 12,394.6401-unit route is Start, blue key, gold
  key, red key, Open hidden door, Boss robot, Exit.

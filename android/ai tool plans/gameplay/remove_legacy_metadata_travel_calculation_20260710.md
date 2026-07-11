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
- [ ] Regenerate generated mission
  metadata to use the unified result.
- [ ] Add focused regressions for hidden doors, trigger barriers, unreachable
  targets, target counts, distance, and status.
- [ ] Run scoped quality, D1/D2 builds and CTest, Android tests/build, full host
  metadata regeneration, and a focused emulator route test.

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

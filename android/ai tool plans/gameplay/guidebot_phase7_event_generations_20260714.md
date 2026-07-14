# Guide-Bot Phase 7 Event Generations

## Goal

Replace stale or polling-only live route updates with coalesced, owner-authoritative event invalidation for route-relevant world state, while preserving classic Guide-Bot movement, timing, flare behavior, and RNG use.

## Plan

- [x] Add wrap-safe wall, trigger, object, reactor, and automap event generations to the high-level Guide-Bot route state.
- [x] Notify route state from central gameplay mutation points without planning or constructing a physical path in those hooks.
- [x] Filter object invalidation to the active semantic objective so ordinary robot and weapon churn does not cause replans.
- [x] Coalesce multiple world and newly visited automap events into at most one owner-only semantic rescan per monitor interval.
- [x] Re-evaluate Unexplored after newly visited segments even when the prior target itself remains unvisited.
- [x] Expose generations, pending event masks, and coalescing counters through introspection.
- [x] Add native policy coverage and extend a live route fixture with planner-count assertions.
- [x] Run scoped quality, D1/D2 builds and native suites, Android all-ABI build, corpus gates, and focused emulator coverage.
- [x] Update the Phase 7 master plan with results and the remaining multiplayer work.

## Boundary

Event hooks may only increment diagnostic generations and dirty high-level semantic route intent. They must not call the planner, build `Point_segs`, alter Guide-Bot AI state or timing, aim, fire, move an object, or consume simulation RNG. The existing quarter-second owner monitor is the sole coalescing point.

## Results

- Central wall, trigger, object deletion, reactor destruction, and automap visitation hooks only advance generations and dirty semantic intent. They do not plan, build a classic path, move or aim the Guide-Bot, fire, or consume RNG.
- Active-object filtering covers contained-key carriers, key powerups, bosses, and reactors. Owner authority is checked before scheduling work, and nonowner notifications remain diagnostic only.
- Unexplored remains event-sensitive after the Guide-Bot reaches its physical waypoint but before the owning player explores the target component.
- The live Counterstrike level 1 fixture reports zero shared/legacy selector mismatches, keeps the unexplored target unvisited, coalesces two same-frame automap transitions into one rescan, and performs no additional rescan during six idle seconds.
- Validation passed: scoped quality, supported Windows D1/D2 builds, 19 D1 and 22 D2 native tests, Android all-ABI assembly, 1,274 reviewed route fingerprints, base campaign statuses, and the focused emulator fixture.
- The combined host wrapper exceeded its tool timeout during rebuild, so its already-built CTest suites were run directly and both passed in full.

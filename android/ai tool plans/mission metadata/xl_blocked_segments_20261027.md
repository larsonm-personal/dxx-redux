# Modern XL blocked segments

Continue the authorized eight-hour run on xl_more_21_blocked, stacked on PR #165.
Keep PRs unmerged and require latest-head green CI. The survey contains 29 modern
levels with 6,037 blocked segments; counts prioritize work without qualifying missions

## Source decisions

Pinned reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

- CSegment::IsPassable blocks entry by players, robots and powerups when the
  destination has the blocked property. Leaving is allowed. Weapons, debris,
  other actors and null-object visibility queries retain ordinary wall behavior
- The property does not turn open scenery into a globally opaque wall. Rendering,
  sound and actorless queries must retain their existing wall flags
- The source AI path builder and boss destination traversal explicitly pass NULL.
  Preserve those queries. MoveObjectToLegalSpot passes its actor and must not
  move a robot through a blocked destination. Validate real FVI/physics calls
  rather than inventing an across-the-board pathfinding restriction
- Default-function blocked segments receive the source grey automap cue before
  normal door/secret/revealed overrides. Multiplayer powerup redistribution is
  outside this singleplayer scope; the no-clip cheat can cross blocked regions
- Keep immutable properties on the retained XL source; disk/rewind rederive them.
  No new mutable state or save schema is needed. Redux remains the alternate

## Implementation and acceptance

1. Pin/extract the actual passability function, audit every blocked consumer and
   compare actor/wall/direction combinations against the independent reference
2. Add small XL-owned property/query helpers, dispatch in FVI, player containment
   and robot legal-position recovery, and preserve the automap gameplay cue
3. Use a derived authored fixture with blocked, dry, water/lava/protected and
   generator combinations. Test real entry rejection, exit, projectile traversal,
   visibility, robot/pickup movement, recovery, cheat behavior and source restore
4. Register/run an owned Android integration with staged poses and real thrust,
   projectiles, automap evidence, disk/rewind and stock transitions. No autonomous
   navigation and no promotion from successful loading
5. Reinspect the 29 original candidates, keep unsupported requirements explicit,
   run focused regressions and catalogs/formatting, record scope, commit/push and
   open a stacked PR. Continue the run while requiring latest-head green CI

## Acceptance

Completed implementation and local checks: 2,304 source comparisons on MSVC and
Linux GCC, all 14 focused regressions, all 66 Android integration steps, both
catalogs and all 12 private-policy tests. Protected emulator files were restored
with matching hashes. All 29 original candidates were reinspected; 22 decode,
seven reject, none is gameplay-qualified. Repeated audit preserves bytes/mtime

See [blocked-segment evidence](../../XL_compat/evidence/blocked-segments.md) for
source decisions, hashes and lifecycle scope. Commit/PR CI remains the final
acceptance step; no PR will be merged during this run

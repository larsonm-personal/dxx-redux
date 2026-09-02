# Castaway `ok` to `partial` route investigation

## Plan

- [x] Identify every Castaway level whose route status regressed
- [x] Compare previous and current route objectives, masks, distances, and diagnostics
- [x] Reproduce the affected level directly through the current Windows worker
- [x] Trace the unresolved objective into the route planner and recent routing changes
- [x] Determine the root cause, affected scope, and smallest safe correction

## Findings

- Castaway levels 5 and 9 both changed from `ok` to `partial` in the Windows
  regeneration commit.
- Level 5 already contained unresolved trigger 13 and trigger 20 steps in its
  prior `ok` route. A later planner change removed the special case that called
  such diagnostic continuations complete. Its new `partial` status is honest;
  the underlying switch-routing limitation predates the status change.
- Level 9 is a real regression, but its former boss route was also incorrect.
  The boss is inside a sealed 63-segment enclosure whose six boundary walls
  have no opener triggers. There is no navigable or visible boss firing point.
- Level 9 also contains a reachable reactor. The planner nevertheless treated
  the presence of a boss as an unconditional choice and never tried the
  reactor when boss routing failed.
- Falling back to the reactor produces the complete prerequisite route:
  trigger 31, blue key, gold key, trigger 14, red key, trigger 26, reactor,
  and exit.
- Restoring permissive detailed visibility was tested and rejected. It caused
  false shortcuts such as reducing Obsidian level 1 to start, reactor, exit.
  The existing collision-radius firing-point check must remain in place.
- The same regeneration has 67 corpus-wide `ok` to `partial` changes: 42
  `route target unreachable`, 23 `switch activation route unresolved`, one
  excluded-key trial, and one dependency loop.
- The reviewed route-corpus test did not report these regressions because it
  ingested `.simulation.json` files, whose level records do not contain route
  steps, and crashed before reaching its baseline comparison.

## Recommended correction

When a level contains both a boss and reactor, continue to prefer the boss when
its route succeeds. If boss routing fails, restore the exact pre-attempt planner
state and try the reactor as the alternate control-center actor. Add a synthetic
boss-isolated/reactor-reachable test and checked-artifact assertions for the
complete Castaway level 9 objective sequence.

Keep Castaway level 5 classified as partial until triggers 13 and 20 receive
actual navigable activation routes. Do not restore the former status override.
Repair the route-corpus test so future status regressions fail immediately.

## Implementation

- [x] Prefer a routable boss, then restore planner state and try a reactor when
  the boss is unreachable
- [x] Add checked-artifact assertions that Castaway level 9 completes through
  trigger 26, its reactor, and the exit while level 5 remains honestly partial
- [x] Add a synthetic isolated-boss/reachable-reactor integration case
- [x] Exclude `.simulation.json` artifacts from the metadata route corpus test
- [x] Build both Windows metadata workers and regenerate Castaway
- [x] Regenerate and compare the full route corpus for collateral changes
- [x] Confirm no `ok` route became `partial` or `failed`, and retain Obsidian
  level 1's key and switch prerequisites
- [x] Refresh and pass the reviewed 1,732-level route corpus baseline
- [x] Run focused integration tests, CMake tests, and scoped code quality

# Castaway level 2 co-op GuideBot can't-reach log investigation

## Goal

Identify why GuideBot briefly reported that an early Castaway level 2 route
objective was unreachable even though the route appeared open and subsequent
routing worked well.

## Plan

- [x] Extract the mission, level, co-op, objective, and can't-reach timeline
- [x] Correlate canonical certification with current keys, triggers, and walls
- [x] Separate strategic route reachability from physical path generation
- [x] Trace the implicated decision through current shared routing code
- [x] Record the root cause and narrow fix direction without changing behavior

## Constraints

- Treat this turn as diagnosis only
- Do not infer a missing level prerequisite solely from a transient path failure
- Preserve the distinction between the shared route planner, live certifier,
  physical GuideBot pathfinder, and co-op ownership/session behavior

## Findings

- The early hiccup is route index 3, `Shoot switch trigger 6`.  Trigger 0
  completed at 16:51:36 and trigger 1 completed at 16:51:37; the compiled
  selector then published trigger 6 at segment 308 as valid.
- GuideBot reported `Can't reach next` five times between 16:51:40 and
  16:52:12.  This is not a movement stall: the navigation samples have no
  retry or stall accumulation.  Each message is produced because the actual
  generated path does not end at the requested physical target.
- Trigger 26 fired at 16:52:51.791 (`Door opened!`).  On the next state audit,
  the selector retained route index 3 but changed its guidance segment from
  308 to firing position 306.  GuideBot immediately generated a successful
  eight-point path from segment 227 to 306.  Trigger 6 eventually latched at
  16:53:15.779.
- The checked-in route does not contain trigger 26.  The run therefore exposes
  a missing local access dependency (or an unavailable alternate firing
  position): the semantic selector certifies trigger 6 while no usable firing
  position is physically reachable, then only discovers segment 306 after the
  player independently opens trigger 26's door.
- The message/fallback path magnifies the defect.  `escort.c` treats any path
  whose final segment differs from the requested target as a hard failure,
  prints `Can't reach next`, changes the legacy goal to `SCRAM`, and overwrites
  the failed path with a player/random fallback.  The subsequent `path
  reason=created` trace is therefore the fallback while still carrying the
  semantic route target, so it cannot reveal the original failed endpoint.
- The narrow corrective direction is to make switch certification require a
  currently reachable firing position, and, when none exists, expose the
  action that opens one (trigger 26 here) as a prerequisite.  Independently,
  retain the semantic route goal across a partial physical path and log the
  failed endpoint/blocking edge before fallback so this condition is neither
  mislabeled nor repeatedly announced.

## Implementation plan

- [x] Add certifier coverage for a target with no complete strategic route but
  with physically reachable approach segments
- [x] Publish the nearest physical approach when no strategic progress frontier
  exists, rather than returning the unreachable semantic target
- [x] Preserve the semantic route goal when the engine path ends at a partial
  physical frontier instead of changing the escort goal to `SCRAM`
- [x] Log the requested target and actual partial endpoint before retaining the
  partial path, and let the existing Buddy message throttle expose the
  `navigating as close as possible` notification
- [x] Regenerate Castaway metadata if the corrected certifier changes its
  canonical route, then verify Castaway levels 1 and 2 do not regress
- [x] Run scoped formatting, certifier tests, mission regression tests, and the
  required Windows CMake build and test suite

The physical-frontier correction does not change canonical metadata, so no
regeneration was required.  The checked-in Castaway route regression test
continues to report both levels as `ok`.

## Implementation result

- A target with no complete strategic chain now falls back to the physically
  reachable segment nearest the semantic goal instead of returning the
  unreachable goal itself
- If the engine pathfinder still returns a partial path, GuideBot retains that
  path and the semantic objective, publishes the partial endpoint as nearest
  progress guidance, and no longer changes the route to `SCRAM`
- The partial-path diagnostic records the start, requested physical target,
  actual endpoint, semantic target, and path length before the route continues
- The existing post-path notification can now display `navigating as close as
  possible` because no forced hard-failure message consumes its throttle slot
- The D2 Windows build, all 45 D2 CTest tests, the focused certifier test, the
  Castaway route regression, scoped code quality, and the Android debug APK
  build all pass

## Follow-up: valid route misclassified as partial

- [x] Reconstruct the 17:51 log from initial segment 127 through the first
  direct path to trigger 6
- [x] Identify why the frontier model selects segment 541 while the engine
  pathfinder later reaches segment 308 without a progression event
- [x] Add a regression that distinguishes a genuinely blocked goal from a
  valid physical route rejected by the strategic frontier model
- [x] Correct frontier selection so a valid engine path always wins over an
  approximate approach heuristic
- [x] Rebuild and run the focused, D2 host, Castaway, and Android checks

The log shows the route model repeatedly selecting segment 541 from segments
127 through 145.  GuideBot then follows the player through the supposedly
unreachable component boundary using the classic engine pathfinder.  From
segment 412, with no prerequisite activation at that transition, the route
pathfinder immediately produces a complete path ending at segment 308.  The
disagreement came from applying the metadata portal-clearance estimate to the
live physical frontier even though the engine pathfinder does not apply it.

Live physical passability now matches the engine and ignores that conservative
clearance estimate.  Strategic route certification still uses clearance.  The
new regression constructs a portal narrower than the metadata navigator radius
and verifies that strategic progress rejects it while the physical frontier
and engine-compatible passability still reach the real goal.

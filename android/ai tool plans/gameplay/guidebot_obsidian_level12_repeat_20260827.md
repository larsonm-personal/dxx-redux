# Guide-Bot Obsidian Level 12 Repeat Investigation

## Goal

Determine why the post-fix Guide-Bot still abandons or fails to approach the
first switch objective in Obsidian level 12, and establish whether the switch
has a valid physical route.

## Plan

- [x] Confirm the tested build contains the intended switch-navigation fixes.
- [x] Reconstruct objective selection, physical paths, recalls, stalls, and
  recovery decisions from the supplied log.
- [x] Correlate the selected switch firing position with route feasibility and
  any avoided edges or blocked walls.
- [x] Compare the observed behavior with the current implementation and report
  the supported root cause and safest next fix.

## Findings

- The tested build is `56acb239`, and it contains the midpoint suppression,
  completed-path hold, stalled-edge keying, and four-per-second recalculation
  limiter from the prior fix.
- The supplied filename is
  `C:\Users\first last\Downloads\debuglog_20260827_140429.txt`; the requested
  path included an extra `debuglog` directory that is not present.
- The live full planner selects switch trigger 3 at firing segment 342.  The
  restored Guide-Bot is already in segment 342 and settles about 1.7 game units
  from the exact one-point endpoint.  No semantic-route midpoint recall occurs
  after that route becomes active.
- When the canonical route cache becomes available, the live certifier reuses
  canonical step 1 for the same switch but publishes its mine-start firing
  segment 320.  The log changes from `route_full_planner ... seg=342` to
  `route_certifier ... prepared_first=1 selected=1 segment=320`, followed by the
  physical path `[336,325,324,323,322,321,319,320]` and a `cache_ready`
  adoption.
- Guide-Bot reaches segment 320 and settles about 1.8 game units from that
  endpoint.  It is not stuck trying to approach the switch there; the route
  system considers the canonical firing waypoint complete and deliberately
  holds it.
- Segment 320 is accepted by the certifier as reachable and its stored position
  is accepted as having a shot to wall 1.  Segment 342 is also accepted by the
  full live planner.  Therefore this is not evidence that the switch has no
  navigable firing position.  It is evidence that late cache adoption can
  replace an already-reached, context-sensitive firing position with an older
  and visibly less helpful canonical one.
- No `path_recalc` limiter event or `stalled_edge` recovery occurs in this log.
  The only later rebuilds are the ordinary five-second refreshes while moving
  toward segment 320.

## Recommended Fix

Treat cache readiness as advisory for an active semantic objective.  If the
new route names the same trigger, wall, activation kind, and objective, retain
the incumbent live firing position and physical path while that position is
still reachable and still has a valid shot.  Replace it only when world state
invalidates it or when the new candidate is demonstrably better from the
Guide-Bot's current position.  Add a regression that starts with an active
live target at 342, publishes the canonical target at 320 for the same switch,
and verifies that cache adoption retains 342.

Separately, terminal firing-position guidance should orient Guide-Bot toward
the switch wall.  That would make either valid firing waypoint visibly useful,
but orientation alone would not correct the backward cache downgrade shown in
this log.

## Implementation

- [x] Reduce route publication to one incumbent-aware adoption path instead of
  allowing cache readiness to mutate the active goal indirectly.
- [x] Retain an active same-objective firing waypoint across cache publication,
  while still replacing it for a changed or invalidated objective.
- [x] Add focused adoption coverage for a live firing waypoint followed by a
  different canonical waypoint for the same switch.
- [x] Add or extend a high-level route test that exercises late cache readiness.
- [x] Run scoped code quality, focused tests, the Windows D2 build, and Android
  native builds for every configured ABI.

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

## 15:29 Retest

- [x] Confirm the retest build contains the incumbent-publication fix.
- [x] Reconstruct the selected switch, target position, physical path, and
  terminal hold from the new log.
- [x] Identify the first decision that prevents useful switch guidance and
  correct the owning layer instead of adding another cache exception.
- [x] Add regression coverage and rerun code quality, tests, and builds.

The retest was built from `f86a3df1`, which includes the incumbent-publication
fix.  Unlike the earlier run, this save begins with the canonical route already
ready: its first certification selects segment 320 and the Guide-Bot is loaded
in that segment.  No live segment-342 firing waypoint ever exists for the
publication fix to retain.  The bot reaches the exact segment-320 activation
position and holds there, while repeated audits continue to accept that same
technically shootable but remote waypoint.

The certifier now reranks a cached shoot-switch waypoint against reachable,
currently shootable segment centers and memoizes the winning firing position.
The full route planner also gives switch-shot distance greater weight, so newly
generated canonical routes favor useful near-switch guidance instead of
recreating the remote target.  Regression coverage verifies both reranking and
cache reuse.  All 43 native tests, the Windows D2 build, and Android debug
native builds for arm64-v8a, armeabi-v7a, and x86_64 pass.

## Oblique Shot Audit

- [x] Trace the actual wall-shootability callback through snapshot capture and
  collision testing, including transparent grate handling.
- [x] Determine whether segment 320 is a true line-of-fire result, an overly
  permissive approximation, or stale mission metadata.
- [x] Define a conservative switch-shot validity rule and diagnostic logging
  that distinguish occlusion from an excessively oblique approach.
- [x] If the correction is locally verifiable, implement regression coverage
  and rerun code quality, tests, and builds.

The live projectile collision path uses `FQ_TRANSPOINT`, which samples the
actual texture pixel where the projectile crosses a transparent surface.  The
route shootability callback instead uses `FQ_TRANSWALL`, which allows the ray
through every rendered-through wall without testing whether it crossed a hole
or opaque grate material.  Segment 320 is therefore only proven by the looser
semantic dependency approximation, not by the engine's projectile collision
rule.

A direct global replacement is not safe: an earlier corpus experiment found
six valid semantic routes disappeared when dependency discovery used finite
`FQ_TRANSPOINT` samples.  The clean correction is a two-tier result.  Preserve
`FQ_TRANSWALL` only to establish that a switch is a potential prerequisite,
but require `FQ_TRANSPOINT` for a claimed firing waypoint.  When no sampled
waypoint passes the projectile-accurate test, retain the switch objective as
unverified guidance, navigate toward its reachable frontier, and emit one
diagnostic identifying the target switch and approximate-only rejection.  An
incidence-angle policy can be an additional conservative usability filter, but
must not substitute for the opaque-pixel collision test.

### Implementation Plan

- [x] Separate approximate semantic switch discovery from projectile-accurate
  firing-position validation without losing unresolved prerequisites.
- [x] Rank confirmed firing positions with a soft incidence-angle penalty and
  expose a steep-shot warning only when the selected valid shot requires it.
- [x] Make cache identity and invalidation cover the revised shot model, and
  add concise diagnostics for exact, approximate-only, and steep selections.
- [x] Add focused high-level regressions, then run scoped code quality, native
  tests, Windows D2, and Android all-ABI builds.

The implemented model now uses the projectile engine's `FQ_TRANSPOINT` check
for confirmed firing positions and retains `FQ_TRANSWALL` only as an
approximate dependency fallback.  Confirmed shots steeper than 75 degrees are
penalized during ranking and produce a one-time Guide-Bot instruction warning,
but remain valid.  Approximate-only objectives remain navigable and explicitly
say that a clear shot could not be confirmed.

Fresh Obsidian level 12 analysis rejects the old segment-320 grate shot and
selects segment 503 for switch trigger 3.  The replacement passes the exact
projectile check and is marked `confirmed_steep`, with incidence cosine
0.242523.  The focused Obsidian regeneration, scoped code-quality pass, Windows
D1 and D2 builds, all 43 native tests, and Android debug builds for arm64-v8a,
armeabi-v7a, and x86_64 pass.

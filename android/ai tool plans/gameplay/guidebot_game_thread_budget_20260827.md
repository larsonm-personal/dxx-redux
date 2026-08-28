# Guide-Bot Game-Thread Work Budget

## Goal

Make it structurally impossible for semantic Guide-Bot analysis to visibly
stall gameplay.  Route quality may update later, but the current game frame
must not wait for a complete mission plan or an unbounded switch-ray search.
This work takes priority over correcting the moving-boss audit invalidation.

## Supported Cause

The Android route metadata service already provides an isolated background
analysis path, but live state audits can still call `route_planner_plan_view`
synchronously from `escort_route_monitor_completion()` inside
`GameProcessFrame()`.  The existing collision-work count is not a frame-time
budget, and the physical-path recalculation limiter does not cover semantic
planning or switch firing-position certification.  Multiple CPU cores cannot
protect the game loop from work invoked synchronously on its own thread.

## Non-Negotiable Invariants

- A gameplay refresh must never run the complete semantic route planner on the
  game thread.
- Live certification and firing-position work must be resumable and limited to
  a 2 ms scheduled budget per frame.  Work that is not finished is deferred.
- An unchanged valid goal and physical path remain published while refinement
  is pending.  If no valid goal exists, Guide-Bot reports that calculation is
  pending or follows a bounded reachable frontier.
- Discrete safety-critical events may request immediate validation, but they
  may not bypass the per-frame work budget.
- Full canonical analysis remains in the isolated Android metadata worker, the
  headless analyzer, and explicit non-gameplay analysis paths.
- While the initial current-level calculation is still running, opening the
  automap may raise the isolated worker from its normal 10 percent duty cycle
  to 100 percent.  Closing the automap restores the gameplay duty cycle.  This
  does not raise or bypass the game-thread live-work budget.

## Phase 1: Immediate Main-Thread Containment

- [x] Add a dynamic in-game worker duty control shared with the isolated
  metadata process.  Drive it from automap state transitions, use 10 percent
  during normal gameplay and 100 percent while the automap is open, and reset
  it on a new request or activity shutdown.
- [x] Give live rescans an explicit gameplay execution policy and reject the
  `route_planner_plan_view` fallback when that policy forbids expensive work.
- [x] Preserve the last certified goal when a live refresh cannot finish, or
  publish a calculating/frontier result when there is no safe incumbent.
- [x] Coalesce repeated audit requests into one pending refinement instead of
  invoking one planner per audit event.
- [x] Add a defensive semantic-replan governor.  Audit-only background
  requests are limited to one outstanding request and one launch per five
  seconds; discrete progression events replace the queued request rather than
  multiplying it.
- [x] Log `route_work_deferred` with reason, stage, elapsed time, pending event
  mask, and whether an incumbent route was retained.

## Phase 2: Bounded Live Certification

- [x] Split live certification into persistent work state that can resume on a
  later frame instead of scanning the whole level in one call.
- [x] Limit reachability expansion and exact switch-ray candidates by both
  elapsed time and work count.  Check the monotonic deadline inside every
  segment/candidate loop, not only at planner boundaries.
- [x] Keep confirmed firing positions in the live cache and validate that one
  position first.  Search alternative segment centers incrementally only when
  the cached position becomes invalid.
- [x] Prefer firing candidates precomputed by the background canonical plan;
  never move engine collision calls to an arbitrary thread because they read
  mutable global level state.
- [x] Expose counters for deferred ticks, completed ticks, maximum tick time,
  candidate cursor, and any attempted game-thread full-plan call.

## Phase 3: Remove the Audit Storm

- [x] Stop treating future boss position and segment changes as whole-route
  semantic changes.  Preserve boss identity and alive/dead state in the audit.
- [x] Track a currently selected moving boss through the live objective path,
  without rebuilding unrelated earlier prerequisites.
- [x] When an audit finds that the current route certificate remains valid,
  rebase the audited snapshot and retain the route instead of requesting full
  semantic planning.
- [x] Keep immediate invalidation for key pickup/drop, trigger state, stable
  wall connectivity changes, boss/reactor destruction, and other discrete
  progression events.

## Phase 4: Regression and Device Validation

- [x] Add a moving-boss audit-storm regression that runs 100 movement audit
  cycles, proves the progression-object hash remains stable, and still detects
  boss destruction as semantic progression.
- [x] Add a deliberately expensive switch-visibility fixture and verify that
  certification advances across ticks without exceeding its work deadline or
  discarding the incumbent goal.
- [x] Extend introspection with route work timing/governor counters so device
  validation does not depend only on perceived smoothness.
- [x] Add and run an Obsidian switch regression that advances two prerequisite
  triggers, rejects unreachable cached firing cells, navigates to reachable
  angle-aware frontiers, and asserts zero full-plan calls and zero overruns.
- [ ] Re-run Obsidian level 12 and verify segment-503/504 switch navigation
  remains useful, no repeated `route_full_planner` lines occur in gameplay,
  and slowdown captures contain no route-attributable frame gaps.
- [x] Run scoped code quality, all native tests, Windows D1/D2 builds, and the
  Android all-ABI debug build.

## Acceptance Criteria

- `game_thread_full_plan_attempts` remains zero throughout normal gameplay.
- No individual scheduled live-route tick exceeds 4 ms in the device log; the
  target budget is 2 ms and overruns are logged as defects.
- A continuously moving future boss produces no repeated route adoption or
  semantic planner activity while the current switch certificate is valid.
- Guide-Bot continues moving on its incumbent path while refinement is
  deferred, with no goal flicker or periodic return-to-player behavior.

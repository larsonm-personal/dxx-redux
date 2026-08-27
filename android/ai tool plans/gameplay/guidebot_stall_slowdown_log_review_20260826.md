# Guide-Bot Stall And Slowdown Log Review

## Goal

Determine why Guide-Bot failed to approach the first switch target and whether
the later slowdown was caused by route or unexplored-area recalculation.

## Plan

- [x] Locate and inventory the supplied debug log.
- [x] Reconstruct the stalled switch route, path endpoint, and replan events.
- [x] Correlate the slowdown interval with on-slowdown, Guide-Bot, renderer,
  audio, and automap activity.
- [x] Report supported root causes and propose narrowly scoped follow-up work.

## Findings

- The analyzed file is
  `C:\Users\first last\Downloads\debuglog_20260826_173047.txt`, from Android
  build 21220 at commit `f1997ccd`.
- The semantic planner did identify the required switch.  The final route audit
  names `Shoot switch trigger 3`, with firing-position segment 342.
- Guide-Bot reached segment 342 and the selected firing position while the
  player was in adjacent segments 343/348.  The apparent stall is a terminal
  escort-state oscillation: a one-point objective path immediately qualifies
  for `visit_player_path_midpoint`, then the short return-to-player state
  immediately qualifies to resume the objective.
- Before that terminal oscillation, Guide-Bot also genuinely failed the first
  approach to the switch.  At 17:54:49 it reached segment 345 with the exact
  two-point path `[345,342]`.  As soon as path index 1 selected segment 342,
  the generic midpoint rule interrupted the final hop with
  `visit_player_path_midpoint`.  The same interruption repeated throughout
  17:55:04-17:55:06, after which Guide-Bot stopped in segment 345 and entered
  return-to-player behavior.  It eventually reached 342 at 17:56:06 only
  after wandering away and retrying the approach later.
- This interruption also defeats the route-specific stalled-edge recovery.
  That monitor clears its samples whenever Guide-Bot leaves objective mode or
  the path is replaced, so it never reached the threshold needed to reject the
  troublesome 345-to-342 edge.  No `recovery stalled_edge` event appears in
  the log.
- The same oscillation occurred earlier at route target 320.  From 17:47:09 to
  17:47:40 the log contains 95 path creations and 93 midpoint resets.  From
  17:55:57 to the end at 17:56:11 it contains 61 path creations and 62 midpoint
  resets.  Full route planning was much less frequent, so the high-rate work is
  physical escort path churn, not repeated full semantic route searches.
- The on-slowdown recorder fired at 17:41:29, 17:47:32, and 17:51:35.  The last
  two triggers followed 15.10-second and 10.17-second gaps before frame begin;
  normal 25 FPS resumed immediately afterward.  Those captures do not prove
  continuous Guide-Bot CPU load, although one later captured frame spent
  23.9 ms in simulation.  The final, clearest path storm was not profiled
  because the recorder was still in its 300-second cooldown from 17:51:35.
- Opening the map suppresses or pauses the live escort simulation, so it would
  stop this churn immediately.  That observation is consistent with the log,
  but it does not by itself identify the unexplored-area planner as the cost.

## Recommended Follow-up

Make the midpoint return rule semantic-route aware, not merely terminal aware.
While an active route still has an unreached physical endpoint, do not abandon
the last hop just because a short exact path is already at or beyond its
numeric midpoint.  Preserve the existing away-timeout return so Guide-Bot can
still retrieve a genuinely lost player.  Once the endpoint is reached, enter a
terminal-arrival state instead of alternating between objective and player.
For firing-position goals, hold at the selected position and orient toward the
objective wall/trigger so the identified switch is visibly pointed out.

Make stalled-edge sampling survive ordinary semantic path recreation by keying
progress to the physical edge or endpoint, while resetting it only on real
progress or a changed objective.  Keep the existing one-point-path policy for
nearby doors and prerequisites, and cover two-point final approaches,
unreachable final edges, terminal holding, ordinary route targets, and
unexplored routes with regression tests.  Also let a new severe slowdown
replace or extend a cooldown capture so a later, worse incident is not missed.

## Recalculation Limiter Implementation

- [x] Inventory semantic objective path rebuild call sites and existing timing
  state.
- [x] Add a bounded rebuild rate with retained-path and delayed-rerun behavior.
- [x] Add rate-limited diagnostics and reset behavior for level/objective changes.
- [x] Add focused policy tests for allowance, limiting, and next-allowed
  scheduling.
- [x] Run scoped formatting, tests, and the required host build verification.

The limiter permits at most four semantic objective-path rebuilds in any
rolling one-second interval.  Excess requests retain the current path, are
coalesced into one pending rebuild, and produce one `delayed_rerun scheduled`
log followed by one `delayed_rerun executed` log with delay and suppression
counts.  New levels reset the rolling budget.  Objective changes cancel stale
deferred work but retain the budget so rapid goal-flipping cannot bypass the
CPU fuse.

Validation passed:

- Scoped code-quality checks
- `test_escort_goal_policy`, including rolling-boundary, next-allowed-time,
  explicit reset, and clock-rollback cases
- Windows D2 host build
- Android `:app:externalNativeBuildDebug` for arm64-v8a, armeabi-v7a, and
  x86_64

## Switch Navigation And Terminal Churn Implementation

- [x] Add focused policy coverage for semantic-route midpoint suppression and
  terminal holding while preserving the lost-player timeout.
- [x] Prevent the generic midpoint return-to-player rule from interrupting an
  unfinished enhanced route.
- [x] Hold a completed enhanced route at its physical endpoint and cancel stale
  deferred recalculations there.
- [x] Make stalled-edge sampling survive equivalent path recreation so the
  first-switch final hop can reach recovery.
- [x] Re-run scoped code-quality checks, focused tests, the Windows D2 build,
  and the Android native build for every configured ABI.

Semantic routes now suppress only the generic path-midpoint recall.  The
existing lost-player timeout remains higher priority.  The path follower holds
the last point of one-point, two-point, and longer semantic paths instead of
reversing them, terminal arrival cancels delayed rebuild work and skips the
five-second refresh, and stalled-edge tracking keys equivalent work by the
physical segment transition rather than the volatile path index.

Validation passed:

- Scoped code-quality checks and `git diff --check`
- `test_escort_goal_policy`, including explicit two-point final-hop and reverse
  path cases
- Windows D2, headless D2, and headless-metadata D2 builds
- Android `:app:externalNativeBuildDebug` for arm64-v8a, armeabi-v7a, and
  x86_64

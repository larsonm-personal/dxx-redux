# GuideBot base-character route cycle refinement

## Goal

Preserve the original GuideBot visit-player cadence and personality while
finding why current semantic destinations cause premature, repeated route
turnarounds in cooperative play.

## Plan

- [completed] Compare the current objective/player cycle with upstream GuideBot
  behavior and identify which original invariants the semantic route violates.
- [completed] Separate legitimate visit-player expiration from premature path
  replacement, retry recovery, multiplayer state, and route recreation.
- [in progress] Define a fix that preserves the 1996 visit-player behavior and only
  corrects the new routing integration.
- [completed] Record the corrected diagnosis and the tests needed before changing
  gameplay behavior.

## Results

- Upstream and current `time_to_visit_player` are identical. The original
  unseen-player timeout and path-midpoint return are intentional behavior and
  must remain.
- The level 21 log shows the GuideBot directly seeing the player while long
  objective paths at index zero repeatedly become short player paths. That
  rules out the unseen-player timeout for those sampled transitions and points
  to path state becoming midpoint-eligible between one-second navigation
  samples.
- All four semantic route adoption events retained the current path. Route
  audit replacement is not the source of this log's repeated turnarounds.
- Objective paths are recreated much faster than the original five-second
  refresh interval. A current multiplayer change enables the original
  collision retry recovery for the locally owned cooperative GuideBot, while
  upstream disables that recovery in every multiplayer mode. This is a likely
  source of repeated goal recreation, but the existing log does not record the
  transient retry event.
- Android-only event logging now records the exact visit-player reason and the
  path state before periodic goal refresh, collision retry recovery, missing
  path recovery, and circular-path fallback. No gameplay timing or routing
  behavior was changed.
- Scoped code-quality checks passed. The D2 Windows build and Android debug APK
  build both passed. Android emitted only pre-existing warnings in `ai.c`.
- The next phone log should identify the first state mutation before each
  `visit_player_path_midpoint` event. The gameplay fix belongs at that mutation,
  not in `time_to_visit_player`.

## Required validation before a gameplay fix

- Reproduce cooperative GuideBot routing with the `GUIDE-BOT` category enabled
  and correlate `navigation_reset` events with each objective-to-player turn.
- Verify the eventual change in cooperative play and a single-player base-game
  route cycle.
- Keep `time_to_visit_player` behavior unchanged and compare the final focused
  diff with `upstream/main`.

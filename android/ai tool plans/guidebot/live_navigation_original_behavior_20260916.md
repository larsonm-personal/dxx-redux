# GuideBot live navigation and original behavior

## Scope and status

Research and implementation plan for impassable-grate stalls, spinning while
returning to the player, and unreachable Hostages requests in Descent Maximum
level 16. The first implementation and automated Windows engine tests are now
complete; results and the remaining reverse-connector case are recorded below.
The original source observations below describe the pre-change code

Preserve original escort movement, turning, return visits, wandering, messages,
and other quirks. Enhanced routing should retain the requested destination and
select prerequisites and coarse waypoints on a slow timescale. It should not
become a per-frame steering controller. Narrow fixes for demonstrated original
collision/path-following defects are acceptable

## Research findings

### Return-path overrides are the first regression suspect

- `d2/main/aipath.c:1556` chooses the new waypoint-acceptance rule based on the
  global `Escort_route_goal.active` alone
- `d2/main/aipath.c:1591` holds the final forward path point under that same
  global condition, before legacy companion end-of-path handling
- Neither branch checks that the actor is a companion or that the current
  path is an objective path. These conditions can affect other robots too
- Companion-specific repair and steering hooks at lines 1451, 1729, and 1772
  onward still depend on route-active state rather than current path purpose
- `do_escort_frame` creates a normal player-return path and sets
  `AIM_GOTO_PLAYER` without clearing the retained strategic route goal. Retaining
  the goal is useful; treating its existence as ownership of that path is not
- The delayed route recalculation at `d2/main/escort.c:2693` can set
  `AIM_GOTO_OBJECT` before the return-visit logic runs. Trace whether queued work
  repeatedly replaces a newer player-return path
- `time_to_visit_player` has an added visible-and-near geometric shortcut at
  `d2/main/escort.c:2298`. Seeing a nearby player through a grate does not prove
  that the bot has rejoined them

These are confirmed code conditions, not yet confirmed causes of the reported
spin. A stale or partial return endpoint plus endpoint holding is a particularly
strong hypothesis

### Live recovery has grown beyond coarse planning

- `android/app/src/main/cpp/shared/guidebot_path_recovery.c` runs from the path
  follower. After approximately 0.25 seconds without measured progress it can
  sample interior geometry, select an approach point, and zero velocity
- It also repairs path points, attempts alternate paths after about one second,
  and directly caps approach velocity
- `ai_path_set_orient_and_vel` adds portal alignment and door-contact braking
- These are low-level recovery mechanisms even though they are activated by a
  high-level route flag. Some address documented real collision defects; do not
  delete them wholesale without controlled comparisons
- The escort-level stall monitor (`escort.c:2149`) runs once per second and
  retries after seven stalled samples, but only in `AIM_GOTO_OBJECT`. Its stall
  count uses segment/target-segment repetition rather than actual progress
- The path recalculation limiter permits four requests per second
  (`escort_goal_policy.h:4`). It is a burst cap, not a slow planner cadence

### Grate problems can arise after a valid route is selected

- Prior grate work corrected current-state certification and solid-illusion
  passability. See `../gameplay/guidebot_impassable_grate_routing_20260823.md`
- Trace separately: strategic route edges, generated path, polishing, waypoint
  consumption, steering, and the first collision. A valid detour can still be
  lost by skipping nearby points across a wall
- `polish_path` already uses collision checks with flags zero and the actor
  radius; it does not simply use transparent visibility. Preserve its original
  once-per-tick companion guard while investigating
- The new segment-aware waypoint rule attempts to prevent cross-wall skipping,
  but is scoped by a global strategic flag. Explicit legacy commands and return
  paths need independent coverage
- `recover_alternate_path` only retries a presently flyable portal; it is not a
  general recovery for a selected impassable grate
- `guidebot_route_best_physical_frontier_internal` already uses reverse graph
  distance where available, then geometric fallback without a strategic chain.
  Check whether a fallback repeatedly chooses the wrong side of a grate
- Route clearance helpers often use the larger of bot and player radius. Keep
  player-route feasibility separate from the bot's own movement clearance;
  establish whether this contributes before changing it

### Hostages bypass the enhanced prerequisite planner

- Key 6 selects `ESCORT_GOAL_HOSTAGE`. Ordinary explicit commands clear the
  active route, and `escort_set_goal_object` also clears it for a special goal
- `escort_create_path_to_goal` resolves Hostages through `exists_in_mine`
  (`escort.c:1973`). An unreachable result reports failure and clears the
  request. An incomplete legacy path can fall back to SCRAM
- Only route-backed goals get the current physical-frontier and retained
  partial-path behavior
- The local `upstream/main` comparison has the same unreachable-hostage
  abandonment. This part is a missing high-level capability, not established
  evidence of a new movement regression
- Current route target modes cover end-of-level, unexplored, and exit, not a
  persistent explicit hostage target
- Both checked-in Maximum variants have level 16 `psx16.rl2`, two hostages, and
  a canonical route with switches 1, 3, and 4 before the boss. Their canonical
  completion route has no required key. This does not establish the hostage
  access dependencies: inspect the actual variant, hostage segments, and live
  wall/trigger state before assuming the red key is required

### Existing successful simulations do not establish escort fidelity

- `route_confirmation_drive_companion` forces objective mode and calls the path
  follower directly, bypassing the normal escort decision loop while running
- It also has simulation-specific endpoint interactions and uses player-radius
  actors. It is useful traversal coverage, but cannot validate returning to an
  independently moving player
- The Obsidian level 6 Android switch/grate test positions the bot at targets
  and asserts one-point endpoint holds. It does not prove live detour traversal
- The prior Obsidian 9 investigation (`../2026-09-06-obsidian9-original-follower.md`)
  documents a real collision reversal and explicitly distinguishes unboosted
  current movement from original-binary behavior

## Implementation sequence

### 1. Capture failures and establish a behavior baseline

1. Reproduce in the normal Android escort loop using automation and introspection
   at normal game speed. Use user-specified grate/return locations when available;
   otherwise start with Castaway 1, Obsidian 6/10, and Counterstrike 11
2. Record mission archive identity, save/start state, difficulty, player keys,
   triggers, bot/player poses, and the exact command. Confirm which Maximum
   variant reproduces the hostage problem
3. Add targeted `DLOG_GUIDEBOT` diagnostics where existing traces are insufficient:
   actor/signature, AI mode, requested objective, current path purpose/generation,
   cursor/direction/endpoint, replacement reason, visibility, physical movement,
   collision wall/side/flags, and which recovery override ran
4. Use the local upstream code and pre-routing history as source baselines.
   Compare identical scripted scenes with legacy follower behavior and unchanged
   strategic waypoints. Document any retained differences from the original;
   disabling the planner flag alone is not a complete original-behavior baseline
5. Identify the first divergence, rather than treating a final timeout or lack
   of objective completion as the cause

### 2. Restore the boundary between planner and escort behavior

1. Make path purpose explicit enough to distinguish an objective leg, returning
   to the player, and legacy wandering/SCRAM. Retain the strategic objective
   independently. Avoid changing save/network AI layouts unnecessarily
2. Scope every route-specific follower branch to the correct actor and path.
   First isolate the two global branches and reproduce the return scene again
3. Let the original escort state machine own player visits, turn/speed behavior,
   endpoint behavior, and ordinary local path creation. A pending planner result
   must not steal a player-return path or install an obsolete objective leg
4. Publish coarse waypoint changes on objective completion, meaningful world
   changes, invalidation, or sustained failure. Coalesce routine work over a
   multi-second interval; measure a starting 2-5 second interval in live tests
5. Separate immediate rejection of an unsafe edge from strategic recomputation.
   Do not throttle original player-return updates or stop ordinary movement
   while a planner result is pending
6. Evaluate endpoint holds and precision recovery individually. Keep only
   behavior justified for live escorting or a narrowly reproduced geometry bug;
   simulation interaction needs should not redefine normal escort motion

### 3. Repair grate detours at the failing layer

1. Require physical movement connectivity for travel; keep transparent shot and
   visibility queries separate for firing positions and original perception
2. If waypoint skipping causes the failure, add the smallest companion-local
   successor-leg/portal check needed. Preserve ordinary waypoint tolerance,
   smoothing, turning, and intentional backing away
3. If the generated route contains the bad edge, repair passability at path
   construction. If a legal portal cannot be traversed in practice, report that
   failure for a bounded detour replan after sustained lack of progress
4. Allow a detour to initially move away from the destination. Retain a usable
   path while searching, and do not repeatedly select the same failed frontier
5. Distinguish genuine stalls from normal turns, large-room traversal, player
   obstruction, and waiting for a player-operated barrier

### 4. Add retained Hostages intent to the high-level planner

1. Keep the player's Hostages request distinct from the current prerequisite
   or waypoint. Keep existing behavior for directly reachable hostages
2. For unreachable hostages, identify a target and query its actual access
   dependencies using current keys, wall state, and triggers. Reuse existing
   planner machinery where possible; do not substitute an unrelated NEXT route
3. Offer guidance such as `Need RED KEY to reach HOSTAGES` only when verified.
   Hand the reachable prerequisite waypoint to the ordinary escort controller
4. If dependencies cannot be resolved, choose a reachable, useful frontier by
   topology where possible and clearly report the blocked request
5. Retain intent across key/trigger changes and player-return visits; resume
   hostage guidance once accessible. Clear it on completion or a replacement
   command, with existing save/coop semantics considered explicitly

## Acceptance and validation

- Grate: traverse a real alternate corridor, including an initial increase in
  straight-line distance; no repeated steering into the grate or waypoint skip
- Return: player stops, moves, and is visible through a grate; bot physically
  rejoins using original visit behavior, with no persistent endpoint spin or
  planner-driven mode oscillation. Exercise active and inactive strategic goals
- Hostages: Maximum 16 before access, after each relevant prerequisite, and
  after access opens. The command remains meaningful and eventually resumes;
  also cover no-hostages and directly reachable hostages
- Preserve characteristic lead/return/wander behavior in an unobstructed
  baseline scene. Do not define success as uninterrupted forward motion
- Ensure an active guidebot objective does not change other robots' path
  acceptance or end-of-path handling
- Add real-traversal Android integration tests with initial setup positioning
  only; do not teleport the bot or auto-follow the player during assertions
- Extend focused native tests for actual policy failures, then retain existing
  grate, door reversal, clearance, long-path, key, and frontier simulation coverage
- Run serial Android tests with durable result/log inspection, relevant native
  CTests, scoped code quality, and Windows D1/D2 CMake builds after code changes
- Broaden to the established core simulation set after targeted checks pass;
  classify simulation-only differences separately from live escort regressions
- Keep mission-specific constants in test fixtures, not runtime behavior

## Suggested change boundaries

1. Reproductions and targeted diagnostics
2. Actor/path ownership and player-return restoration
3. Evidence-backed grate/local-following correction
4. Persistent Hostages request and prerequisite/frontier planning

Do not combine these into one broad steering rewrite. Each behavior change
should have its own before/after live evidence


## Implementation results

The user requested automated reproduction and comparison rather than manual
legacy/live captures. Added `android/tests/test_guidebot_live_navigation.cpp`
and its PowerShell runner, with a separate headless engine target. The target
compiles the Android escort decision loop and companion velocity smoothing,
loads actual mission levels, and uses `GameProcessFrame` with native physics,
wall animation, and AI. Route-confirmation driving remains inactive. The player
is scripted independently and other combat robots are removed from the fixture

### Retained changes

- Route-specific waypoint acceptance, endpoint holding, and steering recovery
  require a companion actually following an objective. Retaining the strategic
  goal no longer applies these overrides to player-return paths or other robots
- Forward companion paths retire consumed prefixes before the signed-byte cursor
  overflows, including long return paths
- A queued objective recalculation cannot replace an active player-return path
- The visible-near-player shortcut also requires a physically clear return leg
- Companion door-openability rejects closed grates and non-flyable illusion walls
  before the controlling-trigger or key shortcuts. Shot visibility is unchanged
- Unreachable Hostages requests use the existing segment prerequisite planner
  on a three-second cadence, retain the requested hostage across prerequisites,
  and fall back to a reachable physical frontier. Budgeted planner continuations
  can proceed without replacing the live movement path
- Reachable hostages retain ordinary guidance. Rescue completes the request;
  collecting a prerequisite key does not. New commands and goal resets discard
  the retained target, without adding save/network fields
- No blanket speed change, retry-interval change, or new steering model was kept

### Automated evidence

- Counterstrike 11: actual detour from segment 106 toward the player in 51,
  including increasing geometric distance before returning within the original
  40-unit escort distance with a clear physical leg
- A subsequent scripted player relocation to segment 35 is followed correctly
- Actual follower calls compare route-active/inactive endpoint behavior for both
  the companion returning to the player and a non-companion robot
- A closed, trigger-controlled grate is rejected by the native openability code
- Descent Maximum (fixed), level 16: the Hostages request selects switches 1 and
  4, remains active through scripted prerequisite activation, resumes ordinary
  guidance when access opens, and completes on rescue
- Disabling all access triggers produces retained nearest-frontier guidance;
  restoring those triggers restores prerequisite guidance
- A prerequisite powerup notification does not complete Hostages; a mine with no
  remaining hostages clears the request normally
- Both fixture cases produce identical JSON across two runs
- Controlled restoration of the old endpoint gate and grate-openability behavior,
  with Hostages planning disabled, fails the endpoint-isolation, grate, and
  hostage-prerequisite assertions. This comparison is automated and does not
  require operating a legacy executable by hand
- The runner is an always-selected case in `guidebot_route_regression_cases.ps1`

Commands:

```powershell
.\android\tests\test_guidebot_live_navigation.ps1 -NoBuild
.\run-windows-build.ps1 -Target both
```

Validation completed:

- Windows D1 and D2 builds
- Android native Debug x86_64 build
- D2 native CTest: 57/57 passed
- D1 native CTest: 50/50 passed
- Scoped code quality and diff whitespace checks
- Campaign simulation comparison: 88 levels across Counterstrike, FirstStrike,
  Obsidian, and Castaway; 0 status changes and 0 regressions against the
  checked-in baselines

Evidence is under `android/temp/guidebot_live_navigation`,
`android/temp/guidebot_live_core_validation`, and
`android/temp/guidebot_live_castaway_validation`. Build/CTest logs are in `temp/`
with the `guidebot_live_` prefix. No checked-in simulation results or the user's
outstanding-bugs edits were changed

### Remaining investigation: reverse connector

A stronger Counterstrike 11 experiment relocates the player back to segment 106
instead of 35 after the first rejoin. The bot selects the new destination but
loops around segments 209/212/255/256/257. This still occurs with route-specific
return-path steering disabled, so it is not evidence that the endpoint scope
fix failed, nor proof of a regression from the original binary

Reproduction:

```powershell
.\android\tests\test_guidebot_live_navigation.ps1 -NoBuild -ReturnTargetSegment 106
```

This optional case currently fails its rejoin assertion. Increasing the retry
interval and enabling the existing objective approach recovery for returns were
examined and rejected because neither resolved the loop. Those experiments are
not in the runtime changes. The default passing regression does not assert that
this stronger reverse journey succeeds

Next investigation should trace the generated first legs, collision sides, and
repeated path replacements through that connector. Preserve the original return
follower while distinguishing an invalid generated leg from an approach that is
continually restarted. This remains open; do not mark all live-navigation bugs
resolved based solely on the passing fixtures

# Original and enhanced Guidebot routing scope

## Request

Research a selectable original Guidebot routing mode alongside the current
enhanced routing, using upstream Redux as the practical baseline and the
released Descent II source as a historical cross-check

This task scopes the implementation; it does not change gameplay code

## Research completed

- [x] Pin the available upstream and historical reference sources
- [x] Audit the routing extraction and the remaining changes to classic files
- [x] Map configuration, lifecycle, multiplayer, replay, and test integration
- [x] Recommend mode boundaries, implementation phases, and acceptance criteria

## Working assumptions

- Enhanced remains the default for existing and new players
- Original restores routing behavior while retaining modern controls and
  lifecycle conveniences, subject to user clarification
- First fidelity target is native D2 single-player; D1-in-D2 and co-op need
  explicit compatibility policies because neither has a 1996 Guidebot baseline

## Recommendation

This is feasible without rewriting the enhanced planner. Introduce a runtime
choice between `Original` and `Enhanced`, restore the classic escort control
flow against a pinned reference, and move the remaining enhanced decisions
behind narrow hooks. Include physical navigation in the boundary: changing
only the strategic goal selector would not deliver original routing

Treat this as a medium project with five reviewable stages. The settings UI is
small; the main work is separating behavior, preserving Enhanced, and proving
that Original has not inherited subtle movement and timing changes

The research is a source-level scope, not a completed line-by-line fidelity
certification. No gameplay code, saves, configurations, or regression baselines
were changed, and no runtime tests or builds were run for this document

## Reference sources and fidelity promise

- Workspace examined: `2ebbacf3` on `cmake`, initially clean
- Practical baseline: upstream Redux commit
  `9fd90f03513663ce1372c8cfa723b7a73c4219fa`
- Both the local `upstream/main` and the remote GitHub default branch resolved
  to that commit during research. No local branch named `redux-master` was
  found; this document uses the actual upstream `main` revision rather than
  assuming a separate checkout exists
- Historical cross-check: released Descent II source at preservation commit
  `75ba13c44259f1e40c0df70961c78abba898eafe`

The [released-source README](https://github.com/videogamepreservation/descent2/blob/75ba13c44259f1e40c0df70961c78abba898eafe/README.TXT)
identifies the source as version 1.2 and dates the release notice December 14,
1999. It is evidence for the original implementation, not proof of the exact
behavior of every 1996 retail executable

Use [pinned Redux escort.c](https://github.com/dxx-redux/dxx-redux/blob/9fd90f03513663ce1372c8cfa723b7a73c4219fa/d2/main/escort.c)
as the first acceptance target. The historical
[ESCORT.C](https://github.com/videogamepreservation/descent2/blob/75ba13c44259f1e40c0df70961c78abba898eafe/SOURCE/MAIN/ESCORT.C),
[AI2.C](https://github.com/videogamepreservation/descent2/blob/75ba13c44259f1e40c0df70961c78abba898eafe/SOURCE/MAIN/AI2.C),
and [AIPATH.C](https://github.com/videogamepreservation/descent2/blob/75ba13c44259f1e40c0df70961c78abba898eafe/SOURCE/MAIN/AIPATH.C)
provide the second audit reference. This research directly cross-checked the
historical goal selector, exit selector, return-to-player routine, and doorway
predicate; a full historical pathfinder comparison remains implementation work

Suggested user-facing explanation: Original follows the classic Redux Guidebot
behavior; Enhanced understands level prerequisites and adds navigation recovery
Do not promise frame-for-frame DOS emulation: physics, timing, RNG scheduling,
level loading, and other engine behavior have changed outside Guidebot routing

## What the extraction accomplished

The groundwork is commit `b51edf4c6d92117201deaca22b2206f32ff16790`, August 23,
2026, which added `guidebot_route.c`, `guidebot_route_internal.h`, and
`guidebot_extensions.h`. The contemporaneous
[extraction report](guidebot_minimize_upstream_diff_20260823.md) records 1,861
added lines removed from the original escort source/header without changing
runtime behavior

That is useful organizational separation, but not yet an interchangeable
implementation boundary. `guidebot_route_internal.h` exposes route globals,
and `escort.c` still directly manipulates them. Subsequent work has also added
physical navigation changes to `aipath.c` and `ai2.c`

At the examined revision, the full upstream deltas are 3,228 additions / 494
deletions in `escort.c`, and 831 / 30 in `aipath.c`. These include unrelated
replay, thief, co-op, and diagnostic changes; they are not implementation effort
estimates and should not be reverted wholesale

## Proposed behavior contract

| Area | Original | Enhanced |
| --- | --- | --- |
| Default / Next | Original key ordering and boss/reactor/exit selection | Current compiled mission action selection |
| Missing or unreachable target | Original search results, failure handling, return/scram behavior | Current prerequisite and reachable-frontier guidance |
| Exit command | Direct classic exit search/path | Current prerequisite-aware, timer-safe exit guidance |
| Hostages | Classic object search and path | Current hostage prerequisite planning |
| Energy, shields, robots, dropped items, markers, scram | Classic command semantics | Preserve current behavior |
| Doors and grates | Original companion doorway policy, including outbound/return distinctions | Current route passability and clearance rules |
| Movement | Classic path construction, polishing, steering, return thresholds, and cadence | Current waypoint, precision, stall recovery, and smoothing behavior |
| Find Secret / Unexplored | Initially unavailable; neither command has an original counterpart | Available as today |
| Controls, recall/deploy, ownership, helper display | Retain as explicit modern conveniences | Retain |
| Metadata analysis and automap features | Remain available independently of live Guidebot routing | Remain available |

Secret and Unexplored are a proposed product boundary, not a user-approved
decision. Secret could later be retained as an explicitly modern command using
classic path construction, but that would extend the fidelity contract

Keep memory/bounds fixes in both modes. Record any behavioral effect of those
fixes as an intentional exception rather than restoring unsafe operations for
the sake of matching source text

## Remaining behavior that must be separated

Source locations below refer to the examined workspace revision

| Location | Finding and required work |
| --- | --- |
| `d2/main/escort.c:2261`, `escort_set_goal_object` | The fallback is already modified: owner player key flags, different missing-key ordering, reactor-existence checks, and exit fallback. Bypassing metadata does not restore the baseline selector |
| `escort.c:1265`, `1892`, `1060` | Special commands, path creation, and goal completion contain enhanced Exit and Hostage handling. Original needs the original command/path/completion semantics, not merely a different Next goal |
| `escort.c:1819`, `find_exit_segment` | Adds a TT_EXIT fallback for trigger-only community levels; baseline searches external child `-2` only |
| `escort.c:2317`, `time_to_visit_player` | Suppresses return when the player is nearby and visibly following a route. Original must follow its original return rules |
| `escort.c:2678`, `do_escort_frame` | Contains route monitoring and delayed recalculation. Its return-to-goal conditions also unconditionally require the path midpoint and use `MIN_ESCORT_DISTANCE - F1_0/4`; upstream uses the distance threshold without those additions |
| `escort_goal_policy.h`, `escort.c:320` | Four-per-second path recalculation limiter belongs to Enhanced; preserve original cadence and random calls in Original |
| `d2/main/ai2.c:1696`, `ai_door_is_openable` | The added closed-wall/non-flyable-illusion rejection is compile-time gated, not runtime gated. It affects the classic BFS too, including calls with a null object meaning companion |
| `d2/main/aipath.c:489`, `1065` | General path machinery now supports route passability, two avoided edges, and clearance retries. Keep a verified ordinary path through it or restore a narrowly scoped reference path; do not duplicate the entire AI system |
| `aipath.c:1349`, `1451`, `1550`, `1730`, `1755` | Enhanced waypoint arrival, adjustment, approach recovery, and steering affect actual navigation. Gate their application on the effective routing mode and extract cohesive helpers into new files |
| `aipath.c:1942`, `d2/main/input_demo_hooks.c:2568` | Velocity averaging runs for companions on Android/live-test builds even without an active route goal. Clearing `Escort_route_goal.active` is insufficient; Original must use the reference velocity assignment |
| `aipath.c:164`, `1576` | Dynamic temporary path storage, invalid-segment guards, and long-path cursor handling are safety/capacity changes. Audit and retain with documented fidelity exceptions |
| `d2/main/guidebot_route.c:175`, `1855`, `1947` | Route-following predicate, metadata refresh, and completion monitor currently have no Original/Enhanced setting. Prevent enhanced publication and AI mutations in Original |
| `d2/main/game.c:1740` | Android calls the enhanced completion monitor from the game loop, outside `do_escort_frame`; it needs the same boundary |
| `escort.c:2415`, `4157` | Restore and owner-handoff paths rebuild navigation and may refresh enhanced metadata; make these mode-aware |

Two additional fidelity traps deserve explicit fixtures:

- Both the pinned Redux selector and historical selector read key masks from
  `ConsoleObject->flags`; the current selector uses player-owned keys. Preserve
  the observed reference behavior in Original unless a documented exception is
  chosen. Test duplicate keys, key-carrying robots, and unusual key order rather
  than silently replacing the reference with its presumed intended behavior
- Not every notification is optional enhancement work. Trigger activation
  history is latched in `guidebot_route.c:1306` and consumed by metadata through
  `android/app/src/main/cpp/shared/secretarea.c:2904`. Keep world-fact tracking
  while disabling live enhanced decisions; turning every hook into a no-op
  would lose information needed by other features and a later Enhanced session

## Implementation shape

### A small native mode boundary

Add an enum such as `GUIDEBOT_ROUTING_ORIGINAL` and
`GUIDEBOT_ROUTING_ENHANCED`, plus an accessor for the effective session mode
Keep it separate from `escort_route_target_mode`, whose existing values mean
Next, Unexplored, and Exit, not routing algorithms

Use a small D2-specific module, for example `guidebot_routing.c/.h`, to own
mode validation and transitions. Keep the classic escort flow readable in
`escort.c`. Move remaining enhanced control flow to `guidebot_route.c` or a
cohesive new D2 navigation module, leaving explicit hooks with an unambiguous
handled/not-handled result where Enhanced replaces classic work

Do not use a missing enhanced goal as the mode test. Enhanced can legitimately
have no goal while waiting for metadata or following a manual command

Keep portable planner/certifier/recovery policy in the existing Android shared
modules. Engine adapters that depend on D2 objects, path buffers, and globals
belong in new D2 files, consistent with the earlier extraction's placement rule
Avoid a callback framework or two complete copies of escort, AI, and physics

### Build boundaries

Compile capability is not the runtime preference. Today the regular desktop
target defines `DXX_GUIDEBOT_ROUTE_PLANNER` and `DXX_GUIDEBOT_ROUTE_DESKTOP`,
while much of the live escort decision loop uses `__ANDROID__` or
`DXX_GUIDEBOT_LIVE_ESCORT`. The latter is explicitly enabled for the live
integration test target. The normal completion call in `game.c` is Android-only

Thus the ordinary desktop executable is neither a clean Original oracle nor
automatically the same enhanced live loop as Android. Establish an explicit
support matrix, and compile the same selected live routing behavior into the
test target. If both options are exposed on desktop, align its live hooks too
while preserving headless analysis and route-confirmation capabilities

### Settings and session authority

Recommended first release: select the preference outside active gameplay and
apply it at new-game/load boundaries. In-flight mode switching can follow after
transition tests; it need not delay availability of both modes

- Android: follow `EnginePreferencesPage.kt`, `MainActivity.kt`, `jni_main.c`,
  and `ConfigImportExport.kt` for a persisted requested default and native bridge
- Desktop, if exposed: use the same native enum, with native menu/PLX handling
  in `menu.c` and `playsave.c/.h`. Do not change the legacy binary pilot layout
- Native code owns validation and effective session mode; Kotlin must not
  reproduce routing rules or decode player/save formats
- Save files store the effective mode. Loading a save selects its mode before
  escort runtime restoration, without rewriting the user's new-game default
- Co-op uses one host-selected session mode distributed to every peer. A new
  owner inherits that mode; it does not switch to the new owner's preference
- Replays select their recorded effective mode before initialization or restore
  regardless of current launcher settings

This is a proposal for host authority, not a requirement inferred from the
current owner packet format. The existing ownership packets carry target mode
and generations; add algorithm mode through the session settings contract and
cover joins, ownership transfer, host migration, and frozen secret travel

### Mode transitions and restoration

Apply transitions on the game thread. At a transition or non-replay rebuild:

1. Invalidate pending live route work using the existing
   `level_metadata_invalidate_live_route_work` boundary; prevent an old result
   from publishing to the new mode
2. Clear enhanced goals, hostage/unexplored state, path limiter state,
   avoidance/stall/precision state, and stale route messages
3. Reset only the relevant companion navigation state, retaining robot identity,
   health, ownership, and docked/released status. Avoid resetting every robot's
   path as a convenient global shortcut
4. Initialize the selected mode against current world facts. Original must not
   wait for route metadata; Enhanced can refresh its current-world guidance
5. Preserve independent metadata, automap data, and trigger activation history

Replay checkpoint restoration is a separate path: restore recorded timers,
goals, mode, and AI state rather than applying normal save-load normalization

For Android saves, the current extension is
`android_save_meta.h` version 7 and already stores `guidebot_route_target_mode`;
add routing mode separately through `state_android_shared.c` and the D2 restore
path. Follow the repository's disposable pre-release format policy instead of
adding migration machinery. Keep legacy engine save/pilot compatibility intact
and document a deterministic fallback when importing upstream saves with no
mode metadata

Input demos need the mode for both level-start and checkpoint-start recordings
The existing checkpoint structure in `input_demo_fixture.h` has escort goals
and timers but no algorithm mode. Update capture, parsing, serialization, replay
setup, and any affected pre-release fixtures together. Do not use replay
compensation to mask a routing difference

### Commands and diagnostic presentation

The native mode check must also cover controller/touch commands, automation,
and replay dispatch, not only hide unsupported menu items. In Original, Next
means the classic default objective; Exit and Hostages use their classic paths
Secret and Unexplored should report that Enhanced is required under the proposed
initial command policy, without silently changing modes

Expose routing mode in `game_introspect.cpp`, automation setup, and Guidebot
Info. Original should show its classic goal/path without a perpetual metadata
pending indicator or a stale enhanced instruction. Helper lines and persistent
messages may remain as presentation conveniences if they reflect that path

## Validation and acceptance

### Original reference coverage

Build a small reference test arrangement from the pinned upstream routines,
with only required ABI/time-type adaptations recorded. It can be a test-only
reference module or an isolated reference executable. Give both implementations
the same world, player/bot state, frame times, and RNG seed; reset all mutable
state between runs

Compare goal/special-goal/index, AI mode, path points and order, return/replan
timers, velocity/orientation changes, and RNG state/call count. Comparing the
current ordinary-path wrapper with the current enhanced-path wrapper is useful
but not an independent Original oracle

Required cases:

- Ordinary and unusual key order, missing/duplicate/unreachable keys, key
  carriers, boss/reactor presence, and post-reactor exit
- Each classic special command, marker toggling, goal completion, and failure
  return/scram behavior
- Visible/lost player, crossing the return distance threshold, path midpoint,
  periodic goal refresh, and short-path wandering
- Open/keyed/locked/hidden/buddy-proof doors, blastable walls, triggered walls,
  and the difference between outbound and return paths
- Ordinary steering and polishing, long paths, and documented safety exceptions
- Original with route metadata absent, pending, or changing: no enhanced goal
  publication, live selector/certifier work for the bot, or enhanced movement
  recovery. Independent metadata analysis may still run

Use the existing `android/tests/test_guidebot_live_navigation.cpp/.ps1` harness
as the starting point for real escort-loop and physics integration coverage
Run meaningful Counterstrike scenarios with the real player-following loop;
repeat identical fixtures to distinguish routing changes from nondeterminism

### Enhanced and integration coverage

- Keep existing enhanced assertions and reviewed route/simulation baselines
  unchanged as the initial comparison target
- Run native escort goal/exit/owner policy, route decision/certifier, and live
  navigation coverage, then the relevant route regression suite
- Cover saved-world restoration, secret travel, key pickups, pre-reactor Exit,
  hostage prerequisites, long paths, and precision/stall recovery
- Cover both modes through co-op join, owner handoff, save resume, and migration
- Cover both replay start types with a recorded mode different from the user's
  preference; verify recording itself does not change Original movement
- Check D1-in-D2's optional bot separately. Native D1 has no original Guidebot;
  this is D2 routing adapted to D1 levels, not a historical fidelity claim
- Build Windows D1/D2 and Android configured ABIs; run scoped code quality and
  relevant native tests. Keep emulator tests serial

The broad mission simulations remain useful Enhanced regression coverage, but
`route_confirmation.cpp` explicitly selects objectives and drives the companion
through `route_confirmation_drive_companion`. Its complete-mission success
criterion is not suitable for Original, which intentionally lacks prerequisite
planning. Do not require Original to pass Enhanced mission-completion targets

## Delivery stages

| Stage | Deliverable | Exit condition |
| --- | --- | --- |
| 1. Freeze the contract | Pinned references, per-function difference ledger, minimal independent reference fixtures, decided exceptions | Goal, doorway, return cadence, and velocity differences are reproducible |
| 2. Finish the separation | Native mode boundary and extracted enhanced hooks, still defaulting to Enhanced | Existing enhanced live fixtures retain behavior and RNG outcomes |
| 3. Restore Original | Reference goal selection, classic commands, paths, movement, and cadence | Original reference cases pass, including no accidental enhanced work |
| 4. Integrate selection | Settings, session authority, saves, replays, commands, and diagnostics | Selected mode survives all supported initialization/restore/ownership paths |
| 5. Certify the feature | Live fidelity fixtures, Enhanced regressions, co-op/D1-in-D2 checks, cross-platform builds | Both modes meet their own acceptance contract with documented exceptions |

Stage 1 is the best next implementation task. It should settle the historical
quirks and establish evidence before restructuring the large escort function
The existing extraction and live harness make stages 2 and 3 practical; the
largest uncertainty is how many shared-engine differences appear in trajectory
comparisons. Exact retail DOS matching would be a separate, larger investigation

## Decisions carried forward

These are recommendations pending user refinement, not blockers to the scope:

- Enhanced stays the default
- Original includes classic physical navigation as well as goal selection
- Modern controls, deployment/recall, and co-op infrastructure remain
- Original initially excludes the new Secret and Unexplored commands
- Co-op routing mode is host-selected and stable across owner changes
- Start/load selection ships first; live switching is a later extension
- Preserve safe historical quirks where the reference can be reproduced;
  enumerate safety and modern-engine exceptions rather than claiming DOS identity

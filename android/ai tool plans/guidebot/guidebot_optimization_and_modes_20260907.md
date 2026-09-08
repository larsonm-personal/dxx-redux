# GuideBot optimization, simplification, and Original / Enhanced modes

Status: proposed implementation plan, based on source inspection on 2026-09-07

This task changes documentation only. No compilation, benchmark, simulation,
emulator, formatting process, or metadata regeneration was run. Another task is
actively changing routing and metadata code; implementation must start from its
completed results, not from the transient worktree inspected here.

The recommended order is to establish behavior baselines, extract the enhanced
controller, introduce a complete runtime bypass, and then optimize measured
costs behind that boundary. Do not combine the extraction with new routing
heuristics. The result should be easier to reason about even if profiling finds
less performance headroom than expected.

## 1. What is already present

Paths below are relative to the repository root. Function names are the durable
references; line counts describe this inspection, not targets for deletion.

| Area | Current implementation | Consequence for this work |
| --- | --- | --- |
| Classic behavior plus extensions | `d2/main/escort.c`, about 4,347 lines | Goal selection, route reset, physical replan scheduling, secret commands, deploy/recall/warp, ownership, replay, and classic escort/thief behavior still share one file |
| Extracted enhanced controller | `d2/main/guidebot_route.c`, about 2,107 lines; `guidebot_route_internal.h` | Extraction has started, but dozens of writable globals are shared with `escort.c`; moving the file again alone would not simplify ownership |
| Physical path and motion | `d2/main/aipath.c`, about 2,355 lines | Enhanced passability, blocked-edge retries, waypoint adjustment, long-path handling, portal recovery, and motion history are interleaved with general robot code |
| Engine metadata integration | `android/app/src/main/cpp/shared/secretarea.c`, about 4,700 lines | Secret discovery, topology, geometry callbacks, persistent caches, worker publication, snapshots, live selection, and auditing are coupled; `level_metadata_scan.c` is not the main orchestration layer |
| Initial semantic analysis | `shared/route_planner.cpp`, about 4,936 lines | Dependency search and switch geometry already have work limits and caches; a portal/component graph is built per planner instance |
| Runtime selection and certification | `shared/guidebot_route_certifier.c`, `guidebot_route_decision.c` | Ordinary end-of-level selection already filters compiled route steps against current facts and chooses bounded guidance candidates |
| Physical confirmation | `shared/route_confirmation.cpp`, about 1,998 lines | Uses real engine paths, but drives the companion through its own controller and applies objective actions; a simulation pass does not by itself prove ordinary escort-frame behavior |
| Background scheduling | `RouteMetadataScheduling.kt`, `LevelMetadata.kt`, `android_route_metadata.c` | Initial Android analysis is isolated; active gameplay work uses a 10 percent worker duty setting and automap calculation can use 100 percent |
| Build selection | `d2/main/CMakeLists.txt`, Android native CMake | Desktop defines `DXX_GUIDEBOT_ROUTE_PLANNER`, but several ordinary escort hooks remain Android-only; compile support is not evidence of identical live behavior |

`shared/` in this document means
`android/app/src/main/cpp/shared/`. Kotlin paths are under
`android/app/src/main/java/com/dxxredux/app/`.

Specific observations that shape the plan:

- `escort_set_goal_object()` already has a compiled-route path followed by a
  classic fallback. Making that one branch optional would leave many enhanced
  behaviors enabled elsewhere
- `escort_route_monitor_completion()` polls publication, validates the incumbent,
  audits world domains, refreshes selection, and applies adoption policy. Similar
  adoption decisions also occur in `escort_route_next_goal()`, and metadata owns
  another published decision. These responsibilities need one explicit owner
- `level_metadata_rescan_current_level_internal()` can rebuild a live snapshot,
  synchronize visibility, and compute domain hashes before starting a fresh
  2 ms selector/certifier budget. The current budget is not a bound on the whole
  routing call chain
- `level_metadata_prepare_guidebot_path_view()` is a view refresh, not itself
  proof of an expensive whole-level rebuild. Measure its topology prerequisites
  and downstream callbacks before adding another cache
- `aipath.c` contains companion-wide motion tracking and velocity handling in
  addition to branches guarded by `Escort_route_goal.active`. An inactive route
  goal is not a complete Original-mode gate
- `create_path_points_avoiding()` has many avoidance arguments and recursive
  blocked-path retries. Goal selection, frontier search, path construction, and
  validation can traverse related connectivity more than once
- `secret_area_navigator_radius()` currently selects an actor radius, then a
  companion radius, then a player fallback. It does not simply take the larger
  of ship and companion. The analysis-profile hash already includes navigator
  radius and the switch projectile model, so a radius-policy change must flow
  through that existing identity
- The persistent route cache already has generation and analysis-profile keys
  and visibility checkpoint support. Reuse these mechanisms; do not invent a
  parallel cache hierarchy

The August 27 simplification plan reports selector times of 2 to 21 microseconds
on its fixtures. Those are historical measurements, not measurements of today's
code or of the complete game-frame path. They argue for investigating surrounding
work before optimizing the selector itself.

Relevant previous plans:

- `android/ai tool plans/gameplay/guidebot_route_architecture_simplification_20260827.md`
- `android/ai tool plans/gameplay/guidebot_game_thread_budget_20260827.md`
- `android/ai tool plans/gameplay/guidebot_android_extraction_review_20260823.md`
- `android/ai tool plans/performance/guidebot_live_calculation_benchmark_20260827.md`
- `android/ai tool plans/2026-09-05-guidebot-long-path.md`
- `android/ai tool plans/2026-09-07-core-four-routing.md`

The first four contain work already implemented. This plan extends their current
boundaries instead of proposing the compiled selector or extension header again.
The long-path plan also records that disabling `move_towards_outside()` caused
regressions; do not repeat that change as a simplification.

## 2. Define what the user is switching

Proposed setting: **GuideBot behavior: Original / Enhanced**. This is a runtime
choice, distinct from build availability and from the current routing command
(`Next`, `Exit`, or `Unexplored`).

Use the repository's upstream D2 escort implementation as the auditable
Original reference. The locally available `upstream/main` at inspection is
`6e76c5d8dafd02dc32a1c6312ce9440dc95b1aca`. Pin the chosen reference when work
begins; do not promise byte-identical retail Descent II behavior or silently
track a moving upstream branch.

Original should reproduce classic goal selection, door policy, return-to-player
behavior, path construction, and steering, with enumerated shared engine safety
and platform fixes. A fallback assembled from today's modified branches is not
sufficient evidence of that behavior.

| Feature | Original | Enhanced |
| --- | --- | --- |
| Stock object/key/energy/hostage/marker/exit commands, scram, name and messages | Classic semantics | Available alongside enhanced routing |
| Default progression | Classic key/boss/reactor/exit selection | Compiled mission progression and current-state selection |
| Switch, hidden-door, blastable-wall and key-carrier guidance | No enhanced objective substitution | Enhanced objective model |
| `Next`, enhanced exit preview, secrets and unexplored-area commands | Hidden or rejected as unsupported; a default-goal action can invoke classic selection without pretending to be enhanced `Next` | Available |
| Enhanced clearance/frontier/waypoint/recovery policy | Bypassed | Enabled |
| Added deploy, recall/dock and warp commands | Hidden for ordinary use, with a deploy action only to recover an already docked bot | Available under existing ownership rules |
| Enhanced path line and instruction overlays | Suppressed; preserve the user's saved display preferences | Respect existing display preferences |
| Touch/controller access to classic commands | Available | Available |
| Multiplayer ownership, pose replication, save object remapping, crash and bounds fixes | Shared infrastructure | Shared infrastructure |
| Independent automap objectives, mission metadata and secret statistics | Governed by their own options | Governed by their own options |

These are proposed product defaults, not permission gates. If a separate
"classic navigation with extra commands" preset is desired later, derive it
from this boundary rather than adding several independent AI toggles now.

Keep Enhanced as the Android default to preserve current behavior. On desktop,
first establish which ordinary hooks actually execute; preserve the existing
default until both modes pass the desktop gameplay checks. Both modes should be
available in supported D2 builds, without making a preprocessor macro double as
a user setting. Native D1 has no GuideBot and should not acquire one from this
work. D1 missions running inside D2 use the D2 mode implementation; Original
does not automatically add a bot to a mission that has none.

Changing mode must not rewind world state. A bot previously spawned or moved by
the user remains where it is; destroyed walls, picked-up keys, health, inventory,
and ownership are unchanged. An already docked bot stays docked until the user
deploys it. Returning to Original should not delete it or silently teleport it
back to an authored cage.

## 3. Architecture that makes skipping enhanced behavior reliable

Use a small mode dispatcher and one owned enhanced runtime. Keep the original
engine functions and ordinary robot code recognizable. Avoid a generic plugin
framework, event bus, virtual interface hierarchy, or duplicate complete AI loop.

```text
UI / bindings / replay / network command
                  |
          guidebot mode dispatcher
             /               \
       Original             Enhanced controller
       escort policy        compiled actions + live facts
             |               |
       classic path     enhanced navigation policy
             \               /
         engine path storage / collision / ordinary steering

Level metadata service -> immutable compiled actions -> enhanced controller
          |
          +-> automap / launcher / explicit analysis and confirmation tools
```

Recommended source boundaries:

| Module | Responsibility and placement |
| --- | --- |
| `guidebot_mode` | Mode resolution, capabilities, transition request, epoch, and command dispatch; new shared policy code under `android/`, with a thin D2 adapter |
| `guidebot_runtime` | Own enhanced command intent, active guidance, pending events/work, adoption, replan limiter, recovery and diagnostic snapshot; portable state/policy under `android/` |
| `guidebot_navigation` | Cohesive D2 adapter for enhanced physical targets, path requests, waypoint repair and recovery; begin by extracting from existing `guidebot_route.c` and `aipath.c` into a new D2 integration file |
| Metadata analysis and runtime selection | Extract cohesive route orchestration from `secretarea.c` into shared `level_route_analysis.c` and `level_route_runtime.c`; retain engine geometry callbacks in a narrow adapter and secret discovery in `secretarea.c` |
| Existing planner, edge, snapshot, decision and certifier modules | Reuse their current representations; split large functions by analysis/selection responsibility only where it removes coupling |
| Existing confirmation controller | Remain an explicit tool/test consumer of production navigation, with its simulated player actions separate |

New reusable logic belongs under `android/`, per repository instructions. Small
new D2 integration files are appropriate for code that directly uses `object`,
`ai_local`, FVI and `Point_segs`; do not create dozens of callbacks merely to
move engine-dependent lines into a folder named `shared`. Minimize edits to
original D1/D2 files. There is no reason to extract unrelated thief AI as part
of this project.

The key cleanup is data ownership:

- Replace writable `Escort_route_*` externs with one private runtime value,
  grouped into intent, guidance, pending work, recovery, and diagnostics
- Keep the compiled program immutable. Store stable action identity and the
  current guidance separately; a moving object's position is not its identity
- Treat completion as an authored predicate over current facts, not a permanent
  "visited step" bit. Keys, destroyed reactors and one-shot switches differ
  from repeatable triggers and doors that another trigger can restore. Repeating
  an action must have current-world evidence and a new execution identity;
  failure to reach its successor is not such evidence
- Use one adopt operation for explicit commands, world events, cache readiness
  and restored intent. Its explicit results are retain, replace, pending, or no
  usable guidance; distinguish completed objective from temporarily blocked path
- Expose one copied diagnostic snapshot to introspection, overlays, replay and
  tests. Remove scalar getters once callers migrate; keep the public command
  and event surface small
- Reset private state through one lifecycle operation. Preserve nonzero and
  negative sentinels explicitly; a blanket `memset` is not a sufficient reset
- Consolidate avoidance edges and retry bookkeeping into one bounded path
  request/recovery record, replacing pairs of parallel scalar arguments
- Keep one small event coalescer. Remove only state that has no remaining
  consumer after the new adoption flow is proven; do not simply rename today's
  flags into a large struct and call the work complete

Representative boundary operations are `request_mode`, `command`, `reset`,
`notify_world_change`, `tick`, `get_guidance`, and `get_snapshot`. Names and exact
C structs should follow existing style. A command result must distinguish
"handled, calculating" from "not handled" so pending enhanced work cannot
accidentally fall through to classic goal selection.

Engine hooks must check effective mode and ownership before reading enhanced
state. Original event hooks should return immediately. Do not treat clearing
`Escort_route_goal.active` as the enforcement mechanism.

Audit every integration category, not just files with "guidebot" in the name:

| Hook category | Known entry points/files to inspect |
| --- | --- |
| Init, reset, restore | `init_buddy_for_level`, `escort_rebuild_runtime_state_after_restore`, `state.c`, `gamesave.c`, D1-in-D2 setup |
| Goal, command, menu | `escort_set_goal_object`, `escort_create_path_to_goal`, `set_escort_special_goal`, menu/hotkey and wheel handlers |
| Frame and movement | `game.c`, `ai.c`, `do_escort_frame`, `ai_follow_path`, `ai_path_set_orient_and_vel`, `create_path_points_avoiding` |
| World notifications | Wall, trigger, powerup, object, boss/reactor and automap hooks, including direct writes used by tests/restoration |
| Asynchronous publication | `android_route_metadata.c`, route cache loading and automap-driven adoption |
| Other state consumers | Introspection, helper line/HUD, save metadata, input-demo capture/restore and direct commands |
| Multiplayer | Owner packets, target mode, join/rejoin, disconnect, host migration, pose replicas |

Classify each upstream delta as enhanced behavior, shared safety/platform fix,
diagnostic-only, or unrelated code. In particular, evaluate long-path cursor
overflow protection and invalid-segment checks as shared safety fixes, while
enhanced portal nudges and velocity policy need the behavior gate. Keep a short
allowlist of deliberate Original differences and tests for the affected cases.

## 4. Optimize initial calculation

Initial calculation means the latency to usable guidance and to a complete
compiled mission route. Physical confirmation has separate time and correctness
results and must not be confused with compilation of that route.

### Measurement first

Extend the existing metadata benchmark and stage timers. Record cache lookup,
snapshot/topology preparation, clearance/transit geometry, target/source
discovery, dependency search, switch candidates/FVI, projection, cache encode
and publication/adoption separately. Track work counts, allocation/high-water
memory and record sizes as well as elapsed time.

Measure four cases: a cold analysis, a complete cache hit, restart from a partial
visibility checkpoint, and an in-game request while lower-priority analysis is
already running. Report CPU time separately from wall time under worker duty
throttling; increasing the duty percentage is not an algorithmic speedup.

### Prioritized changes

1. **Avoid repeated static construction.** The planner constructor currently
   discovers targets and builds its switch guidance graph. Inspect retries and
   strict/conditional planning variants and share immutable target indices,
   portal adjacency, geometry, and graph decompositions for compatible queries
   within one analysis. Separate actor-radius-dependent topology from facts
   that are genuinely static. Do not reuse a graph built under another query
   policy without including that policy in its identity
2. **Index authored relationships once.** Build compact reverse indices for
   trigger sources, walls affected by triggers, linked doors, key carriers and
   exits. Reuse these in dependency resolution and guidance instead of repeatedly
   scanning every trigger/link or object. Prefer arrays and small vectors to
   new general-purpose services
3. **Reduce repeated search setup.** Count shortest-path expansions, duplicate
   queries and copies of `dependency_state`. Reuse scratch queues/distances and
   reserve result capacity first. If state copies dominate, checkpoint vector
   lengths and changed progress fields within a local search branch. Introduce
   rollback machinery only if the measured saving justifies its complexity
4. **Make switch work proportional to plausible candidates.** Preserve shared
   visibility sample caches, their query namespaces, and bounded retained
   guidance poses. Rank by reachable region, geometry and existing quality
   rules before expensive rays. Share negative results only for identical
   geometry, world assumptions, radius and shot model. Keep a bounded fallback
   for difficult geometry; do not improve speed by silently dropping cases
5. **Make cache hits actually cheap.** Time topology/profile validation and
   decoding before attempting a fast path. Reuse already loaded geometry and
   known content identity, but do not bypass identity checks to skip analysis.
   Preserve staged readiness and atomic publication; inspect fixed-size route
   payload copy/zero cost before changing the format
6. **Finish useful work once.** Keep one in-flight request per compatible
   level/profile, preserve checkpoint progress on preemption, and avoid
   restarting it for repeated UI requests or a mode flip. Use existing worker
   scheduling and generation mechanisms. The concurrently running Vertigo
   checkpoint work must settle before changing this area

Do not start with A*, a new navigation mesh, all-pairs path tables, more worker
processes, or a wholesale dependency-planner rewrite. The existing graph search
models keys, authored transitions and switch visibility; an ordinary shortest
path replacement is not equivalent. Prioritize the stages accounting for most
CPU on the actual corpus.

Simplify the existing planner as those stages are touched: separate target
discovery, prerequisite resolution, path/shot feasibility, and result projection.
Give strict, conditional and approximate candidates one explicit ordered
evaluation policy and typed failure reasons, preserving their current order
initially. Centralize candidate comparison and branch restoration instead of
adding another fallback at each failed mission. Keep world-state simulation
distinct from immutable graph data and explain why each retained retry exists.
Memoized failures, if justified by measurements, must include the relevant
simulated progress state; a wall unreachable before a trigger may be reachable
after it. Avoid introducing a general rules engine to express this finite set
of policies.

Original should request no analysis solely for GuideBot use. Shared metadata
may still be needed by automap, mission statistics, the launcher or an explicit
analysis tool. Track those consumers explicitly, preferably with a small
consumer mask; disabling one consumer must not cancel another's job. With all
route consumers disabled, Original gameplay should perform no route analysis.

## 5. Optimize routing while the game runs

### Use one end-to-end work budget

Start the budget before enhanced runtime work begins, not immediately before
the selector. Account separately for polling/adoption, view/snapshot work,
audits, selection, frontier search, BFS/path construction, FVI, smoothing, and
recovery. Use a cheap platform-neutral monotonic timer for measurements and
bounded deterministic work units for resumable algorithms.

Retain the existing 2 ms scheduled-work target and 4 ms defect threshold as
provisional device criteria, but apply them to the complete scheduled routing
service. Ordinary movement/collision stays part of the game simulation and is
measured separately; enhanced per-frame steering probes need their own bound.
Do not claim that a deadline can preempt an individual FVI call. Measure maximum
single-call latency and bound the number of expensive calls started per tick.

Where path construction is not resumable today, measure its worst case first.
If it breaks the budget, build a candidate incrementally in private scratch
storage and commit a complete path through the D2 adapter. Never leave a partial
candidate in the shared `Point_segs` pool for the normal AI to follow.

### Remove repeated work around the selector

1. Keep one current fact view for each runtime update. Separate immutable
   topology from keys, relevant walls/triggers, progression-object identities,
   reactor state and automap discovery. Avoid rebuilding a complete snapshot
   merely to prove an unrelated domain did not change
2. Make events invalidate only relevant work. A moving future boss should not
   invalidate a switch objective; a moving selected target updates guidance.
   Stable connectivity changes differ from door animation frames. Preserve
   notification coalescing and a low-frequency repair audit for missed events
3. Budget the audit itself by domain and cursor. Existing staggered audit timing
   must not conceal a large single-domain scan. Rebase audited facts after a
   harmless change instead of scheduling identical work repeatedly
4. Share one connectivity traversal across compatible selection/frontier/path
   preparation in an update when profiling shows duplication. Reuse across
   updates only with a complete identity: level, access generation, keys/owner,
   actor profile, start or reachable component as appropriate, target mode and
   directed avoidance edges. Exact path distances still depend on the start
5. Retain a valid objective and physical path when publication refines an
   unrelated action. Rebuild for an invalid path, changed command or meaningful
   changed endpoint, not simply a new metadata revision
6. Revalidate the next physical leg when its dependencies change. Avoid
   rescanning every waypoint each frame. Cache a repaired waypoint only while
   its path revision, actor profile and relevant geometry remain valid
7. Consolidate stall handling into one documented escalation: verify progress,
   attempt bounded local repair, retry with a specific directed obstruction,
   choose a useful frontier, then report waiting/blocked. Give each stage an
   attempt bound and reset condition. Waiting at a valid switch position or
   keyed door is not automatically a navigation stall
8. Store diagnostics separately and aggregate counters. Building strings or
   exporting a large snapshot should not be unconditional game-frame work

Do not introduce a new fact-result cache solely for a microsecond selector. Do
not return to whole-mission planning or whole-mine firing-pose search during
ordinary `Next`. The existing bounded non-progression/unexplored tools should
have explicit APIs rather than sharing a boolean-heavy function with cold
analysis and a full-planner fallback.

The final runtime flow should be readable as: gather changed facts, advance one
bounded pending job if needed, select/adopt guidance, request or retain a path,
and perform bounded recovery. Completed actions cannot be resurrected when the
next action is harder to reach. A temporarily unavailable result must not cause
goal flicker or random wandering.

Authored restoring/toggling triggers are an explicit exception to permanent
completion: if the action's effect is undone, current facts can require another
execution. Preserve this evidence in the decision rather than relying on a
simulation-only restorer workaround or a blanket "never revisit" rule.

### Preserve geometry correctness

Treat the open ship-versus-GuideBot radius question as a separate behavior
change after extraction. Recommended policy for a route the player must follow
is `max(effective player collision radius, effective companion collision radius)`.
Read effective mission replacements through the engine; do not hard-code stock
sizes or use a potentially different display/model radius without auditing it.

Keep physical movement at the bot's actual collision radius, and keep projectile
visibility at the projectile radius. Distinguish player access from companion
access at buddy-proof doors. Apply the chosen navigation profile consistently
to initial analysis, live reachability, path checks, confirmation and cache keys.
Test both a larger player and a larger custom companion. This change may alter
route outcomes, so do not bury it in a performance-only commit or erase its
effect by accepting new baselines automatically.

## 6. Switching mode, persistence, and multiplayer

### Transition at a simulation boundary

UI callbacks enqueue a request. The authoritative game thread applies it before
the next relevant AI update:

1. Validate availability and ownership; repeated requests for the current mode
   are no-ops
2. Advance a mode epoch, invalidate pending enhanced work and publication
   subscriptions, and clear enhanced intent/guidance/recovery/limiter state
3. Discard the bot's obsolete path through its own path state. Do not globally
   reset `Point_segs` or other robots' paths. Clear enhanced movement history
   and preserve position, health, velocity continuity where valid, and ownership
4. In Original, preserve a compatible explicit classic command; map enhanced
   commands to classic default behavior and create a fresh classic path. Do not
   carry over enhanced target indices, marker-key history or stale timers
5. In Enhanced, initialize facts from the current world and reuse compatible
   cached analysis. Preserve compatible classic commands; otherwise select
   enhanced default guidance. While data is pending, use an explicit safe
   follow/wait state that does not claim to have a route
6. Publish effective mode/capabilities to UI and diagnostics in one snapshot

Pending work should carry at least level epoch, mode epoch, owner generation,
command revision and analysis profile. Existing generations should be reused
where their meaning matches. Static cache output may remain reusable after a
mode switch, but it may not adopt a live goal from an obsolete request. A worker
must never hold references to mutable gameplay objects; FVI-dependent work stays
on the engine thread or in the existing isolated analysis context.

Switching is allowed while no bot exists, while caged/docked/dead, during
calculation, on a long path, and during the exit countdown. It changes behavior,
not the world. The enhanced-mode selector uses current countdown state when
enabled; it must not replay already completed prerequisites.

### Setting and UI plumbing

Use one native setting/effective-mode API and one persisted owner of the user's
preference. Prefer the existing player-option route (`playsave.h/.c` plus menu
integration) for cross-platform behavior. Android launcher and in-game controls
should call that API for the selected pilot rather than maintain a conflicting
SharedPreferences value. If launcher access requires a new bridge, keep pilot
file parsing in `playsave.c`.

Add the choice to the D2 gameplay settings and the existing GuideBot UI where it
is easy to switch during play. Derive wheel actions, helper line and HUD text
from effective capabilities. Guard native command entry points as well as UI,
so key bindings, scripts, or replay commands cannot bypass the setting. Include
the preference in the appropriate configuration export path without storing two
authoritative copies. Preserve existing binary player/save layouts; use an
extensible configuration field for the new preference.

### Save/load and replay

- Keep ordinary user preference authoritative when loading a save. Save the
  mode associated with guidebot intent as context; restore that intent only
  when compatible with the effective mode, otherwise rebuild for the selected
  mode. Loading a save should not silently rewrite global user preferences
- Extend the existing added save metadata for mode-tagged intent as needed;
  never serialize worker pointers, pending jobs or navigation scratch buffers
- Save object remapping and `escort_rebuild_runtime_state_after_restore()` stay
  shared. In Original, enhanced restoration is skipped even if enhanced route
  fields exist in the save
- Input demos record initial effective mode and mode-change commands at frame
  boundaries. Playback applies a session override and restores user preference
  afterward. Include relevant mode state in checkpoints and state comparisons
- Freeze current-mode record/replay fixtures before refactoring. Do not add
  replay-specific motion compensation to make changed behavior appear equal
- New Android-only formats can replace their pre-release predecessor directly,
  following repository policy. Preserve engine/upstream format compatibility
  and fail clearly on unsupported replay schemas

### Co-op authority

Use one effective behavior mode for the shared bot, controlled by its current
owner under existing ownership rules. Store each pilot's preferred mode
separately from that session state. Nonowners display the authoritative mode
and cannot cause local planning or change it through their preferences.

Synchronize mode and mode generation with the existing owner/target-mode state
through explicit protocol fields, not a guessed spare bit. On owner handoff or
host migration, preserve the session's effective mode; the new owner may then
change it. Include mode in join/rejoin/save restoration snapshots and reject
stale-owner or stale-generation requests. Keep companion pose replication and
disconnect cleanup enabled in both modes.

Audit existing protocol capability/version mechanisms before extending packets.
Peers lacking mode support must negotiate a supported behavior or receive an
explicit incompatibility result; do not silently let different controllers run.
General engine/network compatibility is not waived by disposable Android
launcher formats. Replay mode overrides and explicit simulation sessions must
not change a live multiplayer session or a persisted pilot preference.

## 7. Verification and performance gates

All commands/tests in this section are future implementation work. They were
not run during this planning task.

### Establish trustworthy baselines

After the current routing task finishes, capture revision, source/data hashes,
cache generation, actor/shot profile, mode, fixed simulation seed, platform and
build configuration. Use existing artifact retention before creating reports.
Do not mix results from changing source files or overwrite another task's
benchmark files. Record unavailable assets as gaps, not passes.

Use First Strike in D2, Counterstrike, Vertigo, The Enemy Within, Obsidian and
Castaway as the full correctness set, including secret levels. Use the existing
88-level core-four set for faster iteration. Expand benchmark coverage with
large/slow levels and failure classes rather than treating the old 11-level
timing corpus as complete. Keep native D1 metadata parity where shared analysis
changes affect it.

Keep three distinct reports:

| Report | What it proves |
| --- | --- |
| Metadata semantic route results | Which authored prerequisites and route statuses the planner produces |
| Engine physical confirmation | Whether the confirmation controller traverses and performs those objectives with its declared assistance |
| Ordinary live GuideBot scenarios | Actual escort-frame commands, return behavior, stalls, UI, mode switching and ownership |

Confirmation should explicitly select Enhanced for its own session and restore
the previous effective mode on completion, cancellation or error. Record this
mode and the confirmation/controller generation in results. Keep its sandbox,
robot removal, accelerated motion, objective application and flare fallbacks
visible in diagnostics; do not accidentally promote them into normal gameplay.
Use a separate Original behavior harness that does not install enhanced goals.
As navigation is extracted, migrate confirmation's goal/path/frontier requests
to the same production entry points. Audit any remaining direct writes to
`Escort_route_goal` or path state. Its simulated player actions can remain
specialized; its passability and path-adoption policy should not become a
second implementation that masks defects in normal play.

### Required integration coverage

- Original fixed-seed traces for stock commands, key order, exit, return to
  player, path points and RNG usage against the pinned reference plus the
  explicit shared-fix allowlist. Whole-program retail binary equality is not
  the criterion
- Enhanced extraction preserves previously passing levels and deterministic
  objective ordering. Check each result; an unchanged aggregate percentage can
  hide a new regression offset by a newly passing level
- Toggle both ways while idle, pathing, waiting at a switch/keyed door, pending
  analysis, pending live work, and after a mode switch with a late publication
- Repeated toggles, level reload and secret-level transitions, no bot, caged,
  docked, destroyed, and previously deployed bot
- Save in one mode and load under each preference; checkpoint replay including
  mode changes; long paths across cursor compaction; other robots keep paths
- Co-op owner/nonowner, owner transfer during work, join/rejoin, save restoration
  and host migration in each mode
- Existing difficult routing cases: grates, hidden doors, shoot/fly-through
  triggers, remote door openers, trigger restorers/cycles, moving key carriers,
  boss movement, pre-countdown exit preview and post-boss exit
- Missed notification recovery using existing suppression controls; audit
  eventually repairs stale facts without constant replanning
- Profile identity rejection for changed mission geometry, actor size or shot
  model; identical static metadata can serve independent consumers in Original

Extend maintained runners instead of building a new competing harness:

- `android/tests/test_level_metadata_benchmark.ps1` and its benchmark manifest
- `test_guidebot_calculation_benchmark`, route decision/certifier native tests
- `android/helpers/verify_route_confirmation.ps1`
- `android/helpers/regenerate_all_guidebot_simulations.ps1` and
  `guidebot_simulation_regression.ps1`
- Existing long-path, waypoint-clearance, successive-frontier, key-liveness,
  headed/headless parity and co-op tests under `android/tests/` and
  `android/game_scripts/`

Add a focused reusable mode-switch integration runner and extend it through
lifecycle/co-op coverage. Unit tests are useful for the small transition/adoption
table; do not create tests for every getter or mechanically mirror the new code.

### Acceptance criteria

| Area | Gate |
| --- | --- |
| Original isolation | Zero enhanced selector/certifier/frontier/recovery/adoption invocations from ordinary gameplay; zero GuideBot-only worker requests; no enhanced mutation or extra simulation RNG calls from dormant hooks |
| Isolation with metadata UI active | Metadata can still be produced for that consumer, but cannot alter Original bot intent or motion |
| Live semantic cost | Zero ordinary gameplay full-planner calls; no whole-mine switch geometry search in ordinary `Next` |
| Frame responsiveness | Full scheduled routing work targets 2 ms; investigate any tick above 4 ms on the agreed device. Report p50/p95/p99/max and bounded FVI/work counts, with ordinary motion costs separately visible |
| Reaction latency | Measure event-to-guidance and command-to-motion latency, including pending work and worst-case audit recovery; reducing work must not introduce multi-second ordinary command delays |
| Initial performance | Preserve the benchmark's existing significance policy and digest checks; report CPU/wall time, cold/warm/checkpoint cases, first-useful and complete readiness. Aim for a material gain in the dominant measured stages, without promising an unmeasured percentage |
| Quality | No previously successful enhanced level regresses; no timeout extensions, excluded levels, relabeled failures or relaxed collision checks to manufacture a pass-rate improvement |
| Simplification | No writable enhanced runtime globals exported to classic code, one adoption path, one reset/transition path, one diagnostic snapshot, and documented reasons for retained fallbacks |
| Compatibility | Windows D1/D2 and Android all-ABI builds, Linux/macOS CI coverage where available, native tests and serial emulator scenarios pass after implementation |

Use scoped formatting only on implementation files changed by that step, after
the concurrent task is finished. Do not accept benchmark baselines or regenerate
the mission corpus merely to conceal a semantic difference introduced by cleanup.

## 8. Implementation sequence and completion criteria

1. **Baseline and hook inventory.** Reconcile the concurrent core-four/Vertigo
   changes; freeze successful cases and timings; classify original-file deltas;
   decide the explicit Original shared-fix allowlist and mode capability matrix.
   Complete when reference behavior and measurement provenance are reviewable
2. **Extract without changing behavior.** Own enhanced state in one runtime,
   replace globals with commands/snapshots, move reset and adoption together,
   and extract cohesive navigation hooks. Keep the current effective behavior
   during this step. Complete when enhanced outputs/traces match and unrelated
   robot/thief behavior is unchanged
3. **Implement runtime dispatch and teardown.** Add internal Original/Enhanced
   selection, complete bypass, lifecycle epochs and late-result rejection.
   Prove Original isolation and enhanced parity before exposing the UI. Keep
   temporary test controls local to introspection/automation
4. **Ship the switch as a coherent feature.** Wire preference, menus/wheel,
   capabilities, save/load, replay and co-op authority. Complete only when a
   user can switch safely during gameplay and persistence/network edge cases
   are covered; do not stop at a settings checkbox
5. **Simplify metadata/runtime coupling.** Separate cold analysis from current
   facts/selection in `secretarea.c`, retain shared metadata consumers, remove
   superseded adoption/fallback branches after call-site and test evidence.
   Complete when a reader can follow a single live update without tracing
   several overlapping dirty/pending mechanisms
6. **Optimize cold analysis.** Apply the top measured static/index/search/cache
   improvements one at a time. Complete with comparable cold/warm/checkpoint
   timing and unchanged semantic correctness results
7. **Optimize live navigation.** Extend the budget to the full service, reduce
   repeated snapshots/traversals, bound path repair and unify recovery. Complete
   with device latency data, zero full-planner gameplay calls and stable paths
8. **Separate behavior refinements and finish validation.** Evaluate the actor
   radius policy as its own change, run the full supported corpus and ordinary
   live scenarios, remove temporary comparisons, and document the final mode
   contract and retained fallback reasons

Each step should be a small reviewable series where necessary. Keep extraction,
mode behavior, performance, and geometry changes independently revertible.
Do not delete the retained original implementation or bring old controller
copies along indefinitely as a hidden third mode.

The first useful deliverable is a verified separation and working mode switch.
The highest-priority performance work is then the measured cost surrounding
selection and physical path construction, plus repeated initial-analysis work.
Another round of pass-rate-specific exceptions should not be the organizing
principle for this cleanup.

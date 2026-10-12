# Asynchronous prerequisite navigation for Guide-Bot destinations

Status: proposed design, not implemented

## Recommendation and scope

Use one destination-routing pipeline for Hostages, Energy Center, and Exit.
Try direct navigation and a matching prepared plan first. When neither can
answer, send the current routing state to the existing isolated metadata
worker process. Continue gameplay, show a calculating status, and adopt a
returned candidate only after checking it against the current game.

Reuse the native prerequisite planner, collision implementation, process
supervision, and route-decision machinery. Add a live-query protocol and
destination-aware validation. This is a moderate integration project, not a
new pathfinding algorithm or a one-line switch to enable the full planner.

The initial release applies to Enhanced routing in the D2 engine, including
D1-in-D2 and coop. Original routing keeps its existing behavior. Keep the new
unreachable-hostage fallback until each asynchronous destination passes its
acceptance tests. Native D1 has no Guide-Bot consumer to add; shared planner
changes must still build and preserve D1 metadata analysis.

Aim for a useful answer in roughly 1-3 seconds with a warm worker. This is a
performance target to measure, not a property established by the supplied log.
Complicated switch geometry, cold asset loading, and thermal throttling can
take longer. A deadline must yield an honest incomplete result, never freeze
the game or imply that a timed-out search proved a destination unreachable.

## What already exists, and what does not

| Component                              | Existing capability                                                                                           | Required addition                                                                                                         |
| -------------------------------------- | ------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| `route_planner.cpp`, `route_planner.h` | `plan_segment` resolves dependencies for a selected segment; end-of-level and unexplored endpoints also exist | Destination sets, explicit destination terminal semantics, configurable interactive budgets                               |
| `route_snapshot.*`                     | Owned topology/state structures, fingerprints, replay fixtures                                                | A complete live-query transport including object identities and collision-visible changes                                 |
| `secretarea.c`                         | Prepared progression routes, current-state selection, live summaries                                          | Per-intent plan ownership and bounded result adoption; no reuse of unrelated progression state                            |
| `guidebot_route_certifier.*`           | Current-state validation and resumable workspaces                                                             | Validation bound to the requested destination, including a connection from the bot's current position                     |
| `guidebot_route_decision.*`            | Calculating/ready/partial/failed statuses, dependency hashes, path adoption                                   | Request identity, outcome reasons, and destination completion distinct from prerequisite completion                       |
| `LevelMetadata.kt`                     | Isolated native worker services, serial execution, cancellation, watchdogs, atomic result publication         | Interactive query request/result type and priority; return a candidate to the game rather than install a level-wide cache |
| `RouteMetadataScheduling.kt`           | Active/next/fill priorities and CPU duty control                                                              | A foreground-command priority and separate latency policy                                                                 |
| `jni_level_metadata.cpp`               | Loads exact mission assets, calls the native planner                                                          | Restore a live routing overlay into the worker's engine state and execute one destination query                           |

The present worker analyzes level files, including active mission assets. It
does not receive the live doors, collected keys, rescued hostages, or current
Guide-Bot pose merely because its source type is `active_level`.

The current C++ snapshot is also not enough to run the existing wrapper on a
game-process background thread. `route_planner_plan_view` attaches visibility
callbacks to an engine-backed `level_metadata_scan_view`. Those callbacks can
consult mutable engine geometry/state. Planner work counters are process-global
as well. Copying the snapshot but retaining those callbacks would create races
and answers based on two different worlds.

## One intent, several possible destinations

Keep the player's requested intent separate from the current intermediate
action. For example, `intent=hostages`, `selected_destination=hostage 135`, and
`next_action=shoot switch 4` are three different facts. Completing the switch
does not complete or silently replace the Hostages request.

Use a native request descriptor with:

- Engine/content identity, mission asset revision, level topology identity
- Session/level-instance epoch and restore epoch
- Request sequence, intent kind, target policy, and policy version
- Guide-Bot identity, owner identity/authority epoch, start segment and pose
- Navigator radius and access rules, plus the owner's progression capabilities
- Eligible destination identities and their completion predicates
- A routing-state revision and the relevant fingerprint domains
- Work limit, deadline, and cancellation identity

Object destinations need an index plus signature and level-instance identity.
An index alone can identify a different object after deletion or restore.
Energy centers use stable segment identity within that level instance. Exits
use the existing external-endpoint/trigger selection rules, not just a room
number. Endpoint identity participates in every request, plan, and cache key.

| Intent               | Eligible targets                                          | Completion                                                              | Initial policy                                                                                                               |
| -------------------- | --------------------------------------------------------- | ----------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| Hostages             | Live, unrescued hostage objects                           | Selected hostage rescued; preserve existing command-completion behavior | Prefer directly reachable hostages, otherwise find a usable prerequisite route to an eligible hostage                        |
| Energy Center        | Actual refueling-center segments recognized by the engine | Arrival at a usable center                                              | Prefer a directly reachable center, otherwise a usable prerequisite route; arrival must not demand full energy               |
| Exit                 | Existing normal exit endpoint candidates                  | Navigation arrival/handoff; engine owns actual level transition         | Preserve current exit guidance and reactor/countdown rules; do not treat this as a command to visit every optional objective |
| Future object/marker | Explicit target identity and target policy                | Policy-specific object/arrival predicate                                | Reuse the same pipeline rather than copy Hostages-specific logic                                                             |

The target catalog comes from the engine. Kotlin transports and schedules the
request; it does not interpret keys, walls, triggers, or object types.

For multiple targets, group targets in the same segment and share snapshot,
reachability, and visibility caches across candidate attempts. Prefer direct
routes first, then rank complete prerequisite plans by existing route cost with
stable target-identity ties. A first version may evaluate a bounded ordered
candidate list and return the best usable answer found. It must label an
unsearched remainder as incomplete, not prove all hostages/centers unreachable.
Euclidean distance can order attempts; it must not discard distant candidates
that may have a much easier actual route. Keep an adopted destination stable
while valid instead of switching whenever a marginally better estimate arrives.

Non-exit destinations must not deliberately end the level to reach themselves.
Initially exclude plans requiring a level transition or initiating reactor/boss
destruction that starts an escape countdown. Exit retains the existing policy
for those actions. This restriction is explicit in the query and its cache key.

## Request flow and visible behavior

```text
Player chooses destination
        |
        v
Bounded direct/matching-prepared-route check
        | usable                    | needs deeper search
        v                           v
Follow route                 Capture live request
                                    |
                             Worker plans privately
                                    |
                             Candidate result arrives
                                    |
                             Bounded current-state check
                               /                 \
                         acceptable             obsolete/invalid
                             |                        |
                         Follow route          refresh or fail honestly
```

1. Create a new request identity immediately and retire any different intent's
   status. An old red-key action may not acquire a new `Hostages:` prefix
2. Use ordinary navigation for a directly reachable destination. Use a prepared
   route only if it actually proves progress toward this request's endpoint.
   The standard exit/progression sequence is not a generic hostage plan
3. If a bounded check needs more work, enter calculating state. Continue a
   still-valid route for the same intent, or let the bot follow/return to its
   owner. Do not send it toward a guessed key or a blocked wall
4. Submit one background request. Poll an in-memory completion mailbox from
   the game loop; do not parse files or wait for JNI/service completion there
5. Validate the result and current first action before publishing movement or
   the status message. If validation needs another slice, remain calculating
6. Once following, skip completed prerequisites and validate later actions as
   current world state changes. Only a genuine need for another search creates
   another background job

Suggested messages: `Finding a route to hostages...`, `Hostages: find red key`,
`Energy center: activate switch`, and the existing `Can't reach hostages` when
the supported search finishes without a usable route. Deadline/worker failure
instead uses `Couldn't find a route yet` and returns to ordinary escort
behavior. Calculating is a state with a final outcome, not an endless HUD label.

Display calculating after a short debounce, about 200 ms, so fast answers do
not flash a message. No dialog, loading screen, or automatic player action is
needed. Coalesce duplicate commands while the same query is pending. After a
terminal failure, an explicit repeat command may request another attempt.

## Worker choice and scheduling

Extend the existing `:levelmeta_d2` service with an `interactive_route` request
kind. Keep its native execution serial. Reuse the matching native headless
worker protocol for host tests. Do not create a second independent scheduler
that races the current metadata lock or mounts engine assets concurrently.

Add interactive priority above ACTIVE/NEXT/FILL. An explicit command preempts
speculative next-level/fill work through the existing owned cancellation
mechanism. If current-level preparation is required by the query, perform it
as a dependency of the interactive request rather than cancelling/restarting
that preparation forever. Cache reusable topology and shot evidence even when
a particular query is superseded.

Allow one running interactive request and one replacement containing the latest
desired state. Repeated notifications update the replacement; they do not grow
an unbounded queue. A new intent cancels the old one. Routine movement does not.
Worker cancellation must be checked in graph search, candidate enumeration,
snapshot construction, and visibility work, not just between collision calls.

The existing ACTIVE policy uses 10 percent CPU duty and the mission background
job permits a ten-minute overall window. Those are not appropriate defaults
for an interactive command. Start measurement with one background-priority
worker, a proposed 25 percent duty target, a 5-second soft deadline, and a
10-second hard deadline including queue/load time. Tune against frame-time and
thermal measurements. Thermal/lifecycle controls may reduce duty; they do not
extend a calculating message indefinitely. During an escape countdown prefer
the existing fast exit/frontier guidance; do not wait for this slow path.

Result outcomes are: usable complete plan, useful validated partial plan,
no supported route, budget exhausted, cancelled, and worker failure. Only a
finished search covering all eligible candidates can report no supported route.
Planner relaxation, incomplete geometry, and unsupported dependencies cannot
be promoted to a complete physical route just because a function returned true.

Keep dynamic results in a separate short-lived request store. Never overwrite
the canonical level cache with a plan containing one player's keys or one
session's rescued hostages. Reuse existing static asset caches with their
proper topology/content/actor identities.

Use a dedicated JNI query branch that loads/reuses the baseline, applies the
overlay, and calls the planner directly. Do not run the ordinary static metadata
publication path after applying live state. Warm worker residency is best
effort: retain one level's baseline for a bounded active-game lease, release it
under memory/lifecycle pressure, and handle a cold process identically. Correct
results cannot depend on Android keeping a stopped service's process resident.

## Capturing a consistent live world without a hitch

Do not invoke a save-game operation, full introspection JSON dump, full
`build_route_snapshot`, or expensive collision queries on the game thread just
to submit a job. The observed 17-22 ms refresh overhead is part of the problem
to avoid, even if the eventual search runs elsewhere.

Prepare immutable geometry and asset identity at level preparation. At an
end-of-frame boundary, copy the bounded mutable routing inputs into a
preallocated native buffer. The initial implementation can copy the relevant
arrays in one pass, then measure its worst-case cost before deciding whether a
maintained mirror is justified. Serialization, compression if any, disk I/O,
and transport occur after ownership of that buffer passes to an I/O thread.
Only that thread handles data serialization; it never reads engine globals.

Capture all state read by planning or its collision callbacks, including:

- Owner key/capability state and actual Guide-Bot actor geometry
- Logical door access, locks, keys, blastable/illusion state, trigger flags and
  links, control-center/boss state, and relevant wall animation state
- Side texture/overlay and effect changes that affect switch identity,
  transparency, or collision; wall flags alone are insufficient
- Progression objects, key carriers and their contents, current boss positions,
  hostage identities, and any objects used by the selected collision queries
- Destination catalog and relevant exploration state for policies using it

The worker loads the exact pinned mission/assets, restores a clean baseline,
and applies the captured overlay to its private engine before planning. Both
the planner snapshot and FVI callbacks must then see the same overlaid world.
Validate and relink worker objects through native helpers. Do not assume loaded
level object indices still match the running game's indices. Keep a mapping
back to original object identities for results. Do not simulate gameplay,
consume live RNG, award keys, run triggers, or write player saves while applying
the overlay. Clear request-local caches and state before every new overlay.

Reuse snapshot/replay structures and serializers where complete, but extend
them explicitly for object signatures, target catalogs, and collision state.
The current semantic snapshot has raw object types but no distinct hostage
kind, no object signature, and no general energy-center destination catalog.
Existing replay fixtures are useful test inputs, not proof of full engine-state
transport. A callback-by-callback read-set audit is an implementation gate.

If copying the necessary state exceeds the frame budget, use a maintained
native mirror or versioned chunk copies with a final consistency check.
Never assemble an unversioned multi-frame snapshot. Do not make progress depend
on the entire moving world remaining unchanged throughout a long copy.

## Result acceptance while the player keeps moving

The game owns truth and movement. Worker results are proposals. Carry request
identity, captured dependency fingerprints, selected destination, semantic
steps, first-action guidance, planner outcome, and proof/dependency information
in the result. Bound lengths and validate indices before using them.

Hard reject mismatched intent sequence, level/restore epoch, mission asset
revision, owner authority, actor identity, or an invalid selected destination.
This rejects late results from previous games even if the mission name and
level number happen to match.

Do not require the current exact start-position hash to equal the submitted
position: the player and bot will naturally move during a several-second job.
Use resumable current-state reachability to connect the bot's current position
to the returned first usable action. Failure to reconnect requests another
plan; successful reconnection can reuse the semantic plan. Do not call an
unbounded physical search under the label of result validation.

Apply current completion predicates before showing an action. A collected red
key is removed from the pending sequence even if the worker started before the
pickup. A rescued hostage invalidates that object and can cause reselection
within the same Hostages intent. Reused object slots never pass this check.

For the first version, conservatively reject changes to relevant wall, trigger,
or progression dependencies unless the existing certifier can establish that
the candidate remains usable within its budget. Allow the special case of
newly acquired keys by skipping those actions and certifying the next action
against otherwise unchanged dependencies. Opening a door is not automatically
safe to classify as monotonic: another trigger may close or relock it.

Use logical access revisions rather than every intermediate door-animation
frame to decide whether another semantic job is needed. Ignore unrelated
projectiles and cosmetic changes. Keep full domain fingerprints available as
a periodic audit, spread across frames, for missed invalidation events.

A narrow certificate authorizes the next action, not blind execution of every
future step. Revalidate as steps advance. Reaching a nearby frontier is not
the same as proving the player can reach the destination. Preserve distinct
player progression and bot physical-access profiles, especially around
buddy-proof passages and narrow openings. Reject or mark partial any route
whose only success came from the planner's relaxed-radius fallback.

On rejection, coalesce one fresh request from the latest state. Avoid starvation:
do not cancel merely because the start pose moved; attempt useful adoption,
and retain a certified same-intent action where possible. Cache failures by
intent, logical access state, capability state, target set, and relevant start
reachability region. Invalidate on meaningful changes or explicit retry, not a
fixed per-frame/per-three-second loop. A timeout is not a permanent negative.

## Coop, restores, and deterministic replay

Only the current Guide-Bot authority submits and adopts interactive jobs. Use
the existing owner-key policy, not a new union of all players' inventory.
Verify actor-specific traversal also uses that policy consistently. Nonowners
receive existing replicated movement and persistent status, including the
calculating/failure message; do not broadcast the entire planner input.

Abdication, owner disconnect, Guide-Bot replacement, restore, rewind, level
transition, and game exit retire request identities. The new owner can submit
its own request. Do not serialize worker handles, queued results, or wall-clock
deadlines in ordinary saves. Preserve existing accepted-route save continuity;
a saved pending intent restarts from the restored world rather than adopting
a pre-restore answer.

Asynchronous completion timing must not silently break input replay. Record the
normalized accepted decision and its adoption simulation tick, including any
target reselection; replay supplies it at that tick with live validation and
does not race a wall-clock worker. If that record format is not yet implemented,
disable the asynchronous path during recording/replay and preserve the current
fallback explicitly. Replay tests inject controlled result delivery ticks.

## Proposed code boundaries

- New shared native `guidebot_destination_request.*`: intent/endpoint identity,
  status transitions, bounded snapshot handoff and result mailbox, independent
  of Android transport
- Extend `route_snapshot.*`: typed query payload and live collision-state
  transport; native read/apply validation and host parity fixtures
- Extend `route_planner.*`: configurable budget, destination candidate reuse,
  terminal destination representation, and cancellation coverage
- Extend `guidebot_route_decision.*` and `guidebot_route_certifier.*`: request-bound
  projection, completion, validation and adoption with a bounded work slice
- `secretarea.c`: keep canonical progression metadata separate from accepted
  per-destination plans and remove retained-unrelated-plan fallbacks for this path
- `d2/main/guidebot_route.c`, `escort.c`: thin destination hooks for Energy Center,
  Hostages and Exit; classic movement and existing success/failure presentation
- Extend `LevelMetadata.kt`, `RouteMetadataScheduling.kt` and native JNI dispatch:
  reuse process supervision and priority arbitration; a small transport adapter
  owns query files and hands parsed native results to the game mailbox
- `game_introspect.cpp`: report request ID, intent, phase, latency and result
  disposition from stored counters; do not recompute planner diagnostics on read

## Implementation sequence and acceptance gates

1. Define intent/result semantics and capture the D1-in-D2 level-7 stale-key
   situation as a reproducible fixture. Keep ordinary fallback enabled
2. Audit all planner/collision inputs. Prove that a live snapshot produces the
   same normalized route in a private worker world as in a frozen host world
3. Add interactive transport, scheduler priority and cancellation, initially
   with one explicit segment and no gameplay adoption
4. Add bounded validation and adoption. Cover key collection during a job,
   movement during a job, old results and worker errors before enabling it
5. Enable Hostages and Energy Center destination sets; preserve direct routing
   and existing command completion. Extend Exit only when its current fast
   selection cannot supply usable guidance
6. Validate two-peer coop, restores, D1-in-D2, deadlines and frame time. Enable
   Enhanced asynchronous routing only after the correctness and latency gates

Do not first re-enable synchronous full planning on Android as an interim step.

Integration coverage must include reachable/unreachable/missing targets; a
farther reachable center versus a closer blocked one; key and switch chains;
all keys already held; target rescue/deletion/reused slot; switch disable and
door relock during a job; player-openable and buddy-proof frontiers; return to
the same level after restore; rapid intent changes; worker cancellation/crash;
owner handoff/disconnect and nonowner duplicate suppression; ordinary exit,
external D1 flyout, and post-reactor countdown; and deterministic result
delivery during replay. Register new top-level automation tests in the existing
catalog and provide reusable host/device runners.

Proposed frame-time gates: under 1 ms p95 for capture and under 1 ms per adoption
slice, with no query-related game-thread slice above 2 ms on the reference
phone. These are acceptance targets requiring measurements on large levels.
Measure cold/warm end-to-end latency, cancellation latency, memory high-water,
snapshot bytes, queue delay, CPU time, FVI work, and jobs per command. Include
slow-device and thermal cases. No disk I/O, blocking IPC, full-plan call, or
large diagnostic hashing is allowed on the game thread for this feature.

A useful first release need not find an optimal route or solve every custom
mission. It must preserve the requested destination, avoid obsolete actions,
give a useful answer or an honest bounded failure, and leave gameplay smooth.

## Design review checklist

- [x] Inspect actual worker isolation, scheduling, and native load behavior
- [x] Inspect existing segment planning, snapshots, callbacks and result adoption
- [x] Define destination semantics, stale-result handling, coop and replay policy
- [x] Specify proposed performance targets and implementation/validation stages
- [ ] Implementation and measured device validation, outside this design task

# Route metadata background precompute study

## Objective

Design a simple, bounded route-metadata pipeline that shares launcher work with
the live game, avoids synchronous level-load stalls, degrades Guide-Bot behavior
predictably while analysis is incomplete, and preserves safe progress across
interrupted work.

## Executive recommendation

Use one native cache format and one native planner implementation, but keep two
execution contexts:

1. The live game performs only cheap snapshot/key construction, cache lookup,
   validation, and game-thread installation.
2. The existing isolated D1 or D2 metadata service performs every cache miss.
   It loads the level in its own process and atomically publishes the result.
3. A launcher-process coordinator feeds exactly one level at a time to exactly
   one of those services. It never runs D1 and D2 workers concurrently.
4. The game polls its already-known cache filename at a low rate and installs a
   valid result only at a safe game-thread boundary.

The first implementation should also stop using Android `versionCode` as the
route-cache namespace. Replace it with an explicit cache format generation and
planner behavior generation. The current snapshot hashes already identify level
topology and state, so invalidating every level after an unrelated APK update is
unnecessary and is the main reason this regression repeats per app version.

Do not put the planner on a C/C++ background thread inside the game process.
The metadata adapter calls FVI and reads engine global arrays and static caches.
Those are not safe to use concurrently with simulation. An isolated process
also preserves the existing protection against malformed third-party levels.

For resumable work, first persist completed deterministic visibility samples in
small immutable checksum-protected chunks. These samples are memoized pure
inputs to the expensive planner work and can be reused without changing planner
decisions. Do not initially serialize the planner call stack and speculative
backtracking state. That is substantially more code and has a much higher risk
of accepting an invalid or behavior-changing continuation.

## What exists now

### Launcher analysis

- `LevelMetadataAnalyzer` creates a temporary request directory beneath the app
  cache directory and starts either `LevelMetadataD1AnalysisService` or
  `LevelMetadataD2AnalysisService`.
- The services run in `:levelmeta_d1` and `:levelmeta_d2` processes. A malformed
  level can kill one worker without killing `SetupActivity`.
- Each service serializes native calls on one executor and uses ownership,
  cancellation, checkpoint, result, and watchdog files.
- The native analyzer loads each requested level and calls
  `secret_area_rescan_current_level()`, which invokes the shared route planner.
- The metadata dialog retains the resulting JSON only in UI state. The request
  directory and result are removed after the dialog operation finishes.
- The current watchdog has a 120 second inactivity timeout which is extended by
  progress. This detects a stalled worker, but it is not a total job deadline.

### Live-game analysis

- D1 and D2 both call `secret_area_rescan_current_level()` directly from
  `LoadLevel()` after the level file is loaded.
- On a cache miss, the shared code runs the full route planner synchronously.
  In D2 that work is inside the profiled file-load phase, which accounts for the
  observed multi-second pause.
- The D2 Android Guide-Bot asks the same shared metadata code for its current
  route. It can reuse the canonical plan after completed route steps are
  projected onto current world state. If reuse fails, the current code can fall
  back to another synchronous plan.
- D1 computes the same route metadata for metadata/reporting purposes, although
  D1 does not have D2's Guide-Bot consumer.

### The route cache is already partly shared

The launcher analyzer and live game both call the same
`route_analysis_cache` implementation. Android PhysFS setup selects the same
per-game app-private write directory for every process:

- D1: `d1x-redux/route-cache/...`
- D2: `d2x-redux/route-cache/...`

Therefore, opening the launcher's native metadata viewer can already warm the
binary route cache used by the game. A separate common D1/D2 directory is not
needed. The important boundary is launcher versus game, not D1 versus D2.

What is not shared today is the dialog's complete display JSON, and there is no
automatic launcher scheduler. Level preview is intentionally separate because
it uses a temporary preview write directory and should continue not to publish
authoritative route data.

### Current identity and invalidation

The current key contains:

- Android `versionCode`
- D1 or D2
- topology hash
- progression hash
- trigger hash
- object hash

The filename is under `route-cache/<versionCode>/...`. This explains the
reported one-time cost for every level in every new game version.

The record is a fixed native struct with a magic value, key, metadata state,
plan summary, and checksum. Reads validate exact length, key, checksum, string
termination, route status, and array bounds. Writes use a PID-specific temporary
file followed by rename. That is already a sound base for atomic publication.

## Proposed architecture

```text
launcher inventory and priority queue
                 |
                 v
     one global coordinator
        | D1 or D2, never both
        v
 existing isolated metadata service
        |
        | native load + pure planner inputs
        v
 per-game authoritative route cache
        ^                         ^
        |                         |
metadata viewer             live game
persistent display row      lookup/poll/install only
```

The authoritative artifact should stay native. Kotlin can schedule targets and
display native status, but it should not parse or reproduce route structs or
cache-key rules. Add a small native probe API for the launcher to ask whether a
target level is missing, partial, complete, failed, or stale.

The launcher's persistent metadata display rows and the game's binary route
artifacts have different consumers and can remain separate:

- Persist normalized per-level JSON for the metadata viewer.
- Persist the compact native route artifact for live-game use.
- Generate both in the same worker pass.

## 1. Stable shared cache

Replace `build_number` in `route_analysis_cache_key` with explicit generations:

- `format_generation`: bump when encoded fields, layout, or validation change.
- `planner_generation`: bump only when route behavior changes enough that an old
  route should not be reused.

For the first implementation, these can be one `cache_generation` constant if
keeping one manual bump point is preferred. Keeping two values avoids discarding
valid encoded data after changes which do not alter planner behavior, but is not
required for correctness.

Keep the snapshot hashes and add a native-derived analysis profile hash which
covers route-affecting inputs not guaranteed to be represented in the current
snapshot hashes, including:

- navigator radius or ship clearance profile
- switch projectile radius and relevant render behavior
- any derived asset metadata used by visibility or target classification

This is preferable to hashing Kotlin file paths or mod names. Two differently
named archives which produce identical planner inputs should share a cache; two
mod stacks which change a planner input must not.

A practical first filename is:

`route-cache/g<generation>/<game>-<input hashes>.bin`

The current raw native record can remain initially because exact record length,
generation, and checksum reject incompatible files. The same Android device also
keeps the same ABI across app updates. Move to field-by-field portable encoding
before shipping caches in the APK, moving them between devices, or supporting
cross-ABI cache transfer.

Publication rules:

- Write a complete temporary file in the destination directory.
- Flush and close it before rename.
- Decode and validate it through the normal reader before considering the task
  complete.
- Never replace a complete artifact with a lower-readiness artifact.
- Treat a corrupt, truncated, wrong-generation, or wrong-key record as a miss.

No migration of `versionCode` directories is necessary. They can be ignored and
later pruned by a bounded cache cleanup. This keeps the first change simple.

## 2. Launcher precompute coordinator

Build the coordinator in Kotlin, but reuse `LevelMetadataTargets` to enumerate
base HOGs, mission descriptors, direct levels, HOGs, imported mission ZIPs, and
extracted bundles. Do not create another content parser.

The coordinator should own one persistent priority queue and bind to one existing
isolated analysis service at a time. The D1 and D2 service-local single-flight
guards are not enough because they are in separate processes and can currently
run concurrently.

Each job should contain exactly one target level. The current analyzer can scan
an entire mission, but one-level jobs provide cancellation, fairness, progress,
and time bounds without needing to interrupt a mission-sized native call. Add an
optional level selector to the existing request rather than a second protocol.

Recommended priority order:

1. Exact mission and level from `ResumeSaveOptions.latestOverall`.
2. First missing level in that save's game, active base mission first.
3. Remaining levels in the other installed base game.
4. Enabled mission-bearing mods in launch order.
5. Other imported mission-bearing mods, with lower idle priority.

Within a mission, use declared level order, including secret levels. Add modest
queue aging so a repeatedly updated recent-save candidate cannot permanently
starve all other content.

The resume-save bridge already exposes `game`, `missionName`, `levelNum`, and
`levelName`, so prioritizing the exact likely next load is simpler and more useful
than prioritizing only D1 or D2.

Skip visual-only mods, metadata patch bundles without levels, incompatible
content, and missing files. If the same level is visible through multiple source
paths, the native cache key will deduplicate the result.

Persistent scheduler state should be advisory, not authoritative. Record source
identity, selected level, native cache filename/key, outcome, attempts, last
progress, and timestamps. On startup, ask the native probe to validate the
artifact rather than trusting a Kotlin `done` flag.

Suggested task outcomes:

- `missing`
- `running`
- `next_ready`
- `complete`
- `partial`
- `unsupported`
- `too_expensive`
- `crashed`
- `stale`

Cancellation caused by launching a game or closing the launcher is not a
failure and should not consume a retry attempt.

### Lifecycle

The initial implementation can run while `SetupActivity` is visible. It should
cancel or pause as soon as the user launches a game. To serve a cache miss for
the currently loaded level, bind the same isolated service from the foreground
game activity for the lifetime of that one request. This is more reliable under
Android background execution limits than expecting an unbound launcher service
to survive after the launcher leaves the foreground.

The launcher and game coordinators must share a small lock/lease file so they
cannot start D1 and D2 analyzers at the same time. A stale lease must include PID
and heartbeat validation, like the current worker ownership files.

## 3. Nonblocking live-game load

Split the current all-in-one rescan into explicit operations:

1. `prepare`: build the scan view, canonical summary, snapshot hashes, and cache
   key. This remains on the game thread.
2. `try_load`: decode the exact cache artifact. If it is valid, install it.
3. `request`: on a miss, enqueue the current level in the isolated worker and
   return from `LoadLevel()` without planning.
4. `poll`: check the known final filename about once per second while pending.
5. `install`: validate and copy the immutable result on the game thread, then
   mark Guide-Bot route metadata dirty for its next safe update.

Measure the `prepare` phase. If snapshot construction itself is too slow on very
large levels, split topology construction into bounded game-thread slices later.
The log evidence points to route planning as the large regression, so that extra
refactor should not be a prerequisite.

The live game must not perform the existing synchronous fallback route plan while
the canonical artifact is pending. If canonical projection later cannot serve a
changed world state, report a distinct `live_replan_needed` state. A future
snapshot-only worker protocol can address that case, but it should not be hidden
behind another multi-second synchronous fallback.

Gameplay always wins over precomputation:

- Starting a level never waits for the worker.
- Launching the game preempts unrelated launcher jobs.
- The current level becomes the only high-priority worker request.
- Exiting the level cancels that request unless its result is already being
  atomically published.

## 4. Guide-Bot readiness and predictable behavior

Expose an explicit native readiness state:

- `calculating`: no validated route target is available.
- `next_ready`: a validated next step or path terminal is available.
- `complete`: the full canonical artifact is available.
- `partial`: planning ended cleanly with a useful validated prefix/frontier.
- `failed`: no useful route can be produced for this exact input.

Readiness must be based on data, not percent complete. A route is `next_ready`
only if all of these pass the current bounds checks:

- `first_pending_step` identifies a targetable route step, or a validated partial
  frontier exists.
- The selected step's next path terminal or waypoint segment is valid.
- The artifact key exactly matches the current canonical snapshot and analysis
  profile.

Behavior while pending:

- Route-dependent Guide-Bot requests show `Still calculating` and do not choose
  a classic fallback target which might disagree with the eventual route.
- Do not emit the message every frame. Show it only for an explicit request, or
  use a short HUD cooldown for repeated requests.
- Unrelated explicit functions, such as finding the player or a marker, can stay
  available if they do not consume route metadata.
- The Guide-Bot may keep a currently valid assigned goal. Do not revoke it merely
  because a more complete artifact is being calculated.

When a `next_ready` or `partial` artifact appears, latch the chosen current
objective. A later complete artifact may replace the remaining plan only after
that objective completes or the player explicitly requests a new goal. This
prevents visible target thrashing.

The existing D2 consumer already understands `first_pending_step`,
`first_pending_path_terminal_segment`, and `partial_frontier_segment`. Extend its
failure return with readiness/reason rather than adding a parallel goal system.

### Multiplayer and input-demo determinism

Worker completion time is nondeterministic. It must not alter simulation on an
arbitrary frame in deterministic modes.

Use this conservative first policy:

- Multiplayer and input-demo recording/replay may use an artifact only if it is
  valid at the deterministic level-start boundary.
- If it is missing then, background work may continue for the next load, but the
  route must not hot-activate during that level.
- Do not add demo-file exceptions or timing records to compensate for async
  completion.

Normal single-player play may hot-install an artifact, but Guide-Bot motion should
begin only at the next explicit help request or another existing deterministic
goal-selection boundary. Installation itself must not advance RNG or simulation.

A later host-authoritative network readiness event is possible, but it adds
protocol, ownership-transfer, and save/restore cases. It is not justified for the
first implementation.

## 5. CPU, time, memory, and retry bounds

Android thread priority does not provide an exact CPU percentage. Use several
simple controls together:

- One analyzer process globally.
- Set the service executor/native caller thread to
  `Process.THREAD_PRIORITY_BACKGROUND` before entering JNI.
- Retain the native FVI and planner-work limits and report their consumption.
- Add cooperative yield checks in the shared analysis budget, not in Kotlin
  progress polling.
- Use a soft duty-cycle limit against one core, initially about 25 percent while
  the launcher is interactive. Measure thread CPU time versus monotonic wall time
  and sleep only at coarse checkpoints, such as every few hundred work units.
- Pause on severe thermal status or power-save mode. Do not start low-priority
  catalog jobs while the live game is running.

The exact percentage is a responsiveness target, not a security boundary. Low
thread priority, single concurrency, and preemption are more important than a
precise quota.

Use two independent time bounds:

- Stall deadline: no progress or heartbeat for 120 seconds, matching the intent
  of the current watchdog.
- Total attempt deadline: a real upper bound which progress cannot extend.

Do not give every level the same total budget. Derive a complexity class after
load from segment, wall, trigger, object, and route-candidate counts. A practical
policy is:

- ordinary level attempt: 30 to 60 seconds wall time
- large level attempt: progressively larger work budget
- exceptional level such as Uneasy 4: up to 5 to 10 minutes total wall time,
  subject to yields and saved sample checkpoints
- hard native array, memory, FVI, and total-work ceilings remain non-negotiable

This allows 30x to 50x content to make progress without letting a malformed or
pathological level run forever.

Retry policy:

- User/game preemption: resume later, no backoff.
- Total deadline with saved progress: retry later from saved samples.
- Same crash or no-progress signature: exponential backoff.
- Three identical crash/no-progress attempts: mark `too_expensive` or `crashed`
  until content identity or planner generation changes, or the user requests a
  manual retry.
- Clean `failed` or `unsupported`: cache the reason so it is not retried on every
  launcher visit.

## 6. Safe partial persistence

There are two different meanings of partial and they must not be conflated:

- Partial route result: a finished planner result with a validated usable prefix
  or frontier. This may be consumed by Guide-Bot.
- Computation checkpoint: data which accelerates a later attempt. This is never
  directly consumed as a route.

The current cache validator already accepts `OK`, `PARTIAL`, and `FAILED` route
statuses when the enclosing result is valid. The planner can return a semantic
partial route. However, budget exhaustion and cancellation are currently treated
as invalid, so the wrapper discards the result.

### Recommended first checkpoint: visibility samples

The expensive planner repeatedly asks deterministic visibility and wall-shot
questions. The current in-memory cache keys those queries. Persist only canonical
level samples, keyed by the full route cache key plus exact query fields.

Use immutable chunk files:

- Header: magic, format/planner generation, full input key, chunk sequence,
  record count, and checksum.
- Body: fixed validated visibility-query/result records.
- Publish every bounded record count or elapsed interval using temporary file
  plus rename.
- Ignore an invalid or truncated chunk. Losing the current unpublished chunk on
  process death is acceptable.
- Deduplicate records when loading. Bound the per-level entry count and total
  cache size.
- Once a complete route artifact is validated, old sample chunks may be removed
  by low-priority cleanup.

This is safe because reuse changes only whether a pure query is recalculated. It
does not restore speculative planner decisions, local backtracking state, or an
engine pointer.

The cache key must include every world and analysis-profile input to the query.
Do not persist samples from a live, mutated level unless the dynamic snapshot is
fully represented in that key.

### Partial route publication

A cleanly returned partial route can be stored in a distinct `next` artifact.
The complete artifact and next artifact should have separate filenames, or the
publisher must enforce a monotonic completeness rank. A complete result must
never be overwritten by a prefix from a later cancelled attempt.

Do not publish route steps directly from `append_step()`. The dependency planner
copies and restores local state while exploring alternatives, so an observed
step can later be rolled back. Publish only after the planner returns a validated
result or after a new explicit commit point proves that the prefix cannot be
rolled back.

### Deferred option: full planner continuation

True continuation would need to serialize the dependency stack, candidate
indices, route progress, keys, avoided triggers, pending paths, backtracking
copies, and visibility cache. That continuation format would be tightly coupled
to planner internals and difficult to validate across versions. Defer it unless
visibility-sample checkpoints prove insufficient on Uneasy 4.

## Additional user-experience improvements

- Show a small launcher status such as `Level routes: 18/30 ready`, current
  mission/level, pause reason, and retry action. Do not block launch.
- When the user presses the recent-save helper, promote that exact job before
  launch. Do not wait for it.
- Persist metadata viewer rows as each one-level job finishes so reopening the
  viewer does not rescan an entire mission.
- Add bounded LRU cleanup by cache generation and total bytes rather than
  deleting all cache data on every update.
- Log planner phase, cache hit/miss/rejection, worker CPU/wall time, work/FVI
  count, checkpoint samples written/reused, and publish/install time under
  Profiling. Log Guide-Bot readiness transitions and refusal reasons under Game
  Logs. Avoid per-frame messages.
- Expose `retry`, `pause`, and `clear route cache` in Advanced diagnostics, with
  exact size and task counts.

## Recommended implementation phases

### Phase 1: remove the recurring regression

- Replace version-based identity with explicit cache/planner generation and a
  native-derived analysis profile hash.
- Split load-time prepare/lookup from planning.
- Return immediately on a miss and enqueue one isolated current-level job.
- Poll and install only on the game thread.
- Add explicit Guide-Bot `calculating` handling and disable hot activation in
  multiplayer/input-demo modes.
- Preserve atomic cache publication and strict validation.

This phase gives the largest benefit and should be kept small enough to validate
independently.

### Phase 2: launcher coordinator and persistent viewer rows

- Add one global priority coordinator around the existing D1/D2 services.
- Reuse `LevelMetadataTargets` and add one-level request selection.
- Prioritize the exact recent-save mission and level, then its game.
- Persist advisory task state and native-validated display results.
- Add background priority, preemption, total deadlines, retry classifications,
  and launcher status.

### Phase 3: bounded large-level progress

- Add coarse native duty-cycle yields and complexity-scaled work limits.
- Persist checksum-protected canonical visibility sample chunks.
- Resume a killed/expired job from samples and verify that its final result is
  identical to an uninterrupted calculation.
- Publish clean semantic partial routes only at validated planner return/commit
  points.

### Phase 4: only if measurements require it

- Refactor planner continuation into a serializable state machine, or add a
  portable snapshot-only worker protocol for live mutated-world replans.
- Consider host-authoritative multiplayer hot activation.
- Consider portable field-wise cache encoding or bundled base-game caches.

Do not begin Phase 4 based only on anticipated complexity. Measure Phase 3 on
Uneasy 4 first.

## Validation plan

### Native unit tests

- Cache identity survives an Android version change but changes for format,
  planner behavior, game, snapshot, navigator, and route-affecting profile input.
- Wrong length, checksum, key, generation, enum, string, and bounds are rejected.
- Concurrent temporary writers can publish only valid records.
- A complete artifact cannot be downgraded by a next/partial artifact.
- Cancellation produces either no route artifact or a validator-approved partial
  artifact, never speculative steps.
- Sample chunks survive truncation of the newest chunk and deduplicate correctly.
- Resumed and uninterrupted analysis produce the same status, objective
  sequence, next waypoint, distances, and summary.
- Mirror shared integration coverage through both D1 and D2 builds.

### Kotlin tests

- The exact latest-save level wins queue priority.
- Only one D1/D2 worker runs globally.
- Base games, enabled mission mods, and remaining imported missions are ordered
  correctly and visual-only content is skipped.
- Launch preemption does not count as failure.
- Stall, total deadline, crash backoff, terminal failure, manual retry, stale
  lease, and content-change invalidation behave as specified.
- Scheduler state never overrides a failed native cache validation.

### Emulator/integration tests

- Analyze a level in the metadata viewer, launch it, and verify a route-cache hit
  with no synchronous route-planning stall.
- Upgrade to a new app version without changing planner generation and verify the
  same artifact is reused.
- Launch a cold uncached level and verify gameplay becomes interactive promptly,
  Guide-Bot says `Still calculating`, and a later valid result becomes usable at
  the allowed boundary.
- Kill the worker during a large level, restart, verify sample reuse, and compare
  the final route to a clean uninterrupted run.
- Record/replay an input demo and run cooperative multiplayer with a cold miss;
  verify that async completion does not hot-activate behavior mid-level.
- Confirm malformed mod analysis can crash/timeout its isolated worker without
  affecting the launcher or game process.

## Explicitly rejected first-pass designs

- A planner thread inside the live game process: unsafe access to global engine
  state and avoidable simulation contention.
- Kotlin reconstruction of the native key, record, or route: two sources of
  truth and higher corruption risk.
- Running D1 and D2 workers together: needless latency and thermal contention.
- Exact CPU-percent enforcement as the only bound: Android priority is not a CPU
  quota and a monolithic native call cannot be reliably duty-cycled externally.
- Publishing steps while the planner is exploring: backtracking can revoke them.
- Serializing the entire planner continuation before measuring sample reuse:
  high complexity and a broad new validation surface.
- Mid-level async activation in multiplayer or input-demo modes: completion time
  is nondeterministic.

## Study completion

- [x] Map launcher metadata generation, storage, identity, and worker lifecycle
- [x] Map live-game cache generation, storage, Guide-Bot consumption, and invalidation
- [x] Determine a safe shared cache format and ownership boundary
- [x] Design launcher prioritization, CPU/time bounds, cancellation, and retry policy
- [x] Design nonblocking live-game readiness and deterministic Guide-Bot behavior
- [x] Assess resumable partial computation and corruption-safe persistence options
- [x] Recommend phases, tests, migration policy, and explicitly rejected complexity

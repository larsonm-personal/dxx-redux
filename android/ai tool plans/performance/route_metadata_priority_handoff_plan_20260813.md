# Route metadata priority and handoff study

## Goal

Study the current route-metadata scheduler and define a simple, bounded plan
that precomputes forward from the player's current level, starts immediately
after CD or mod import, preserves partial work across launcher/game handoff,
and avoids retry loops for deterministic failures.

## Findings

All three requested behaviors have gaps in the current implementation.

### Launcher scheduling

- The launcher has one polling coordinator and one analyzer at a time.
- Its ordering favors the most recent save, enabled content, base games, and
  low level numbers. It has no explicit newly-imported-content focus.
- When no work is found it sleeps for 30 seconds. Successful CD, ISO, GOG, and
  mod imports refresh the UI but do not wake the coordinator, so new content is
  eventually discovered rather than started immediately.
- The coordinator is stopped when the game starts. Its active request is
  cancelled and later restarted by the game rather than handed off explicitly.
- Completed jobs are persisted, but failure attempts and partial progress are
  only inferred from cache files and in-memory retry state.

### In-game scheduling

- The game requests metadata only for the currently loaded level.
- Kotlin receives only that level's number and filename, not the mission's
  ordered level list or possible next-level candidates.
- There is no in-game queue for the next level, later levels, wraparound levels,
  or secret-level candidates.
- Starting on level 5 does preempt launcher work indirectly because the launcher
  is stopped, but there is no durable scheduling rule that makes level 5, then
  6 onward, precede levels 1 through 4.

### Priority and worker ownership

- Analysis runs on an Android background-priority service thread and native
  code yields for 1 ms every 64 cancellation checks.
- There is only a background boolean. Current, next, and speculative jobs cannot
  receive different OS priorities or CPU duty-cycle limits.
- D1 and D2 services share one global single-flight lock, but requests are FIFO
  and have no priority-aware preemption protocol.
- Cancellation currently asks both the caller and the worker watchdog to kill
  the isolated process promptly. This is safe for the app, but can lose the
  not-yet-flushed tail of an in-memory checkpoint chunk.

### Cache and partial progress

- Launcher and game analysis already publish into the same D1 or D2 route-cache
  directory. Cache identity is based on planner generation and level snapshot
  hashes, not on the launcher request path.
- Final and partial route records use temporary-file plus rename publication.
- Expensive visibility samples are stored in numbered, checksummed chunks and
  loaded on a later analysis run. An interrupted launcher run therefore retains
  most completed collision work for the in-game worker.
- A partial route is useful to the live game, while the isolated analyzer ignores
  that partial route and resumes expensive work from visibility checkpoints.
- The remaining gap is orchestration and reporting: there is no durable progress
  token, explicit handoff state, or graceful checkpoint window during preemption.

### Failure handling

- Launcher failure counts are not persisted and reset on process restart.
- Busy, crash, timeout, route failure, and no-progress partial outcomes are not
  represented by a structured scheduler outcome.
- Three non-busy failures suppress a job only for the current launcher session.
  The same bad level can be retried after every app restart.
- Interrupted work must not count as a failed attempt, and partial work that adds
  checkpoint chunks must not be mistaken for a stuck level.

## Recommended design

Keep a single analyzer and the existing content-addressed cache. Do not try to
move a live Java thread or native process from the launcher process to the game
process. Use checkpoint, cooperative stop, and deterministic requeue as the
handoff protocol.

### 1. Add a shared scheduling model

Introduce a small launcher-library scheduling model used by both the launcher
coordinator and the in-game coordinator:

- Job identity: game, mission source identity, level number, level filename,
  cache generation, and analysis profile.
- Focus: imported source or active mission plus current level.
- Desired milestone: `next_ready` or `complete`.
- Priority class: `active`, `next`, or `fill`.
- Outcome: `complete`, `partial_progress`, `preempted`, `transient_failure`, or
  `deterministic_failure`.

Persist an atomic versioned ledger in app-private storage. Protect read-modify-
write operations with a cross-process file lock so launcher and game lifecycle
overlap cannot lose updates. A stale `running` entry becomes `preempted` on the
next process start and does not consume a failure attempt.

The ledger should store only scheduling facts and progress tokens. The route
cache and visibility chunks remain the source of truth for computed data.

### 2. Give the game the authoritative mission order

Extend the existing Android route-metadata JNI request in both D1 and D2 to pass:

- the active mission identity
- current level number and filename
- ordered normal and secret level filenames
- engine-derived possible immediate successors when a secret exit or secret
  return makes the next level conditional

The engine remains the source of truth for mission ordering and mod mounts.
Kotlin must not reimplement mission-file progression rules.

On every level load, replace the in-game focus and rebuild the queue. If the
player starts on normal level N, use this deterministic order:

1. N, if it is not complete
2. possible immediate successors, with N+1 first for ordinary progression
3. N+2 through the final normal level
4. levels 1 through N-1
5. remaining secret or otherwise non-forward levels

If a new level is loaded while another job runs, recompute the order immediately.
Preempt only when the new current-level job outranks the running job. This makes
starting on level 5 stop level 1 work, resume level 5 from any compatible chunks,
then continue with level 6 onward before returning to levels 1 through 4.

### 3. Use three bounded priority tiers

Pass an explicit priority class through the request JSON into the service and
native analysis budget. Set the service thread priority for each request, not
once for the executor's lifetime.

Suggested initial limits, to be tuned from profiling on a mid-range device:

| Tier | Use | Android thread priority | Maximum analyzer CPU duty |
| --- | --- | --- | --- |
| Active | Current level until `next_ready` | background | 10 percent of one core |
| Next | Current completion and immediate next level | lower background | 5 percent of one core |
| Fill | All farther and wraparound levels | lowest | 1 percent of one core |

Enforce duty cycle cooperatively using thread CPU time against monotonic elapsed
time. OS thread priority alone is not a sufficient graphics-impact bound. Pause
speculative work in power-save mode, under severe thermal pressure, and during
multiplayer or input-demo playback.

Run analysis in bounded work quanta. Normal quantum exhaustion is a partial
outcome, not a cancellation or failure. It must flush visibility chunks and,
when the planner has a valid frontier, atomically publish a partial route. After
each quantum the scheduler rechecks current level and import focus before
selecting the next job.

The current level is demoted from Active to Next as soon as its readiness becomes
`next_ready`. Its usable guidebot waypoint is then available immediately while
the remaining route completes without monopolizing CPU.

### 4. Wake immediately after imports

Add one explicit `notifyContentImported(sourceHint)` call after successful data
registration in every CD, ISO, GOG, direct-file, mission ZIP, and mod import path.
The notification should:

- invalidate discovery results
- wake the sleeping launcher coordinator without waiting for the 30-second poll
- focus the imported game or mod's first uncomputed level
- retain the periodic discovery scan as a fallback for missed or external changes

Fire the notification after the imported files and mod registration are committed,
not while they are still being copied. This avoids analyzing an incomplete source.

When the player launches, the launcher cancels its coordinator as it does today,
but cancellation becomes cooperative first: request cancellation, allow a short
grace period for native unwind and checkpoint flush, then kill the isolated worker
only if it does not exit. The game coordinator requeues the active level at Active
priority. Matching route-cache keys and visibility chunks preserve prior progress.

### 5. Make retries progress-aware and durable

Record a progress token before and after each quantum. The token can contain the
latest visibility chunk sequence, published route step count/readiness, and cache
filename. Apply these rules:

- User launch, lifecycle stop, priority preemption, stale `running` recovery, and
  worker busy outcomes do not increment a failure counter.
- Timeout or budget exhaustion with a changed progress token is
  `partial_progress`; requeue it after higher-priority work.
- A valid partial route is progress even if it is not complete.
- Known unsupported/corrupt input or a structured unroutable result is a
  deterministic failure and can be suppressed after one attempt.
- An internal analyzer error or crash gets one retry. Two identical failure
  fingerprints with no progress mark the level failed.
- Failure suppression is cleared automatically when the source identity, route
  cache generation, or analysis profile changes. Also expose a manual retry action
  for diagnostics.

Do not classify failures by matching human-readable problem strings. Add a small
stable failure-kind field to the native result and preserve the readable problem
for logs and UI.

### 6. Keep guidebot adoption deterministic

- Background analysis operates only on isolated level state and cache files. It
  must never mutate the live game world or consume simulation RNG.
- The live game accepts only a cache whose complete snapshot key matches the
  loaded level.
- Cache availability may unblock a pending guidebot request, but must not replace
  an already-valid active guidebot segment mid-flight. Adopt improvements at an
  existing route-planning boundary such as goal request, waypoint reached, or
  invalidated goal.
- `next_ready` is sufficient to help. Show `still calculating` only when the next
  required waypoint is absent.
- Disable live adoption and in-game speculative scheduling for multiplayer and
  input-demo playback to preserve deterministic replay and network behavior.

### 7. Add bounded observability

Log one structured event per job transition under the existing profiling/game
logging path: enqueue, focus change, tier, quantum start/end, progress token,
preempt, resume, complete, and durable failure. Avoid per-collision logging.

Expose scheduler state through setup/game introspection so tests and device logs
can report the focused level, running tier, next queued level, progress token,
retry count, and failure kind.

## Implementation phases

### Phase 1: Scheduling and failure model

- [x] Add priority, milestone, outcome, failure-kind, and progress-token value types.
- [x] Replace the completed-only launcher state with the locked atomic ledger.
- [x] Add unit tests for forward ordering, wraparound, focus replacement, secret
  candidates, progress-sensitive retries, restart recovery, and generation reset.

### Phase 2: Bounded worker execution

- [x] Carry tier and work-quantum fields through Kotlin request JSON and JNI.
- [x] Apply per-request Android thread priority and CPU-time duty throttling.
- [x] Make normal budget exhaustion return partial progress.
- [x] Add graceful cancellation before watchdog/owner forced termination.
- [x] Test single-flight ownership, higher-priority preemption, checkpoint flush, and
  lower-priority non-preemption.

### Phase 3: Immediate import scheduling and handoff

- [x] Wire explicit successful-import notifications for every import path.
- [x] Wake and focus the launcher coordinator on the first incomplete imported level.
- [x] Start the game coordinator from the active-level request and verify that it
  resumes launcher-generated visibility chunks rather than starting from zero.
- [x] Add an integration test that imports content, begins a partial level-1 pass,
  launches the game, and observes monotonic progress to `next_ready`.

### Phase 4: In-game forward queue

- [x] Extend the D1 and D2 Android hooks with authoritative mission order and next
  candidates.
- [x] Replace `computeActiveLevel` with the focus-aware in-game scheduling loop.
- [x] Reprioritize on level changes and current-level jumps.
- [x] Add tests for starting at level 5, advancing normally, entering/returning from
  a secret level, and reaching end-of-mission fill order.

### Phase 5: Guidebot boundary and device validation

- [x] Gate help on next-waypoint availability rather than full completion.
- [x] Ensure partial-cache adoption occurs only at guidebot planning boundaries.
- [x] Add introspection fields and high-level automation covering `still calculating`,
  first-waypoint readiness, and later cache improvement.
- [x] Profile and validate Active, Next, and Fill tiers, including a very
  large level such as Uneasy 4. Tune the centralized duty-cycle constants until
  graphics impact is negligible, then run scoped code quality, Android tests,
  and the relevant D1/D2 build and regression checks.

## Implementation results

- The launcher and game now use one locked, atomic scheduling ledger while route
  records and visibility checkpoints remain the shared computation source of truth.
- Successful CD, image, installer, direct-file, and mod imports wake the launcher
  worker immediately. Launch waits for cooperative checkpoint publication before
  starting the game process.
- The game receives the engine's complete mission order and schedules current,
  next, forward-fill, wraparound, and secret levels with Active, Next, and Fill
  CPU bounds. A level jump rebuilds the queue and preempts only equal or lower
  priority work. If the live cache already reports `complete`, in-game work skips
  the current level and begins with its immediate successor.
- Retry state distinguishes progress, interruption, transient errors, and stable
  deterministic failures. Two identical non-progress failures suppress future
  automatic attempts until the content identity or cache generation changes.
- Guide-Bot cache improvements are adopted only when no route goal is active or
  after the current goal is cleared. A partial route is usable when it contains
  the next waypoint; otherwise the HUD reports `Still calculating`.
- Uneasy 4 exposed a synchronous guidebot-accessibility route search in the live
  level-summary path. Deferring that metadata-only query to the isolated analyzer
  reduced measured level preparation from 89.55 seconds to 46.7 milliseconds,
  with zero visibility searches on the game thread. Active-game workers also
  defer the launcher-only accessibility query so their budget goes directly to
  end-of-level waypoints.
- Emulator automation passed import handoff, level-5 current/next/fill ordering,
  cache adoption, partial Guide-Bot readiness, and a 60-second Uneasy 4 gameplay
  liveness run. The available emulator did not expose useful SurfaceFlinger frame
  samples, so device-specific thermal and frame-time tuning remains a hardware QA
  check rather than a correctness blocker.

## Study checklist

- [x] Map launcher discovery, ordering, import triggers, and persisted state
- [x] Map in-game active-level requests, next-level knowledge, and worker priority
- [x] Verify partial checkpoint publication and cancellation handoff behavior
- [x] Classify interrupted, retryable, unroutable, and deterministic failures
- [x] Define scheduling, priority, retry, and starvation rules
- [x] Define implementation phases and automated validation

## Constraints retained

- Route computation must not alter simulation state or multiplayer behavior
- Only one analyzer may own the global worker at a time
- Cache records and partial checkpoints must remain atomically published
- Foreground play must take precedence over speculative metadata work
- Failed levels must not create a polling or relaunch loop

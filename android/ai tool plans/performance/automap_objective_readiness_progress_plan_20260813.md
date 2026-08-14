# Automap objective readiness and metadata progress plan

## Goal

Give the launcher a useful monotonic estimate for one level's metadata analysis,
show the same estimate while automap objectives are still being calculated, and
make an already-open automap adopt and display newly available objectives at a
safe game-thread boundary.

This plan builds on the current route-metadata background precompute and
priority-handoff work. It does not introduce another analyzer, cache, scheduler,
or route representation.

## Research findings

### Secret areas do not wait for route precalculation

`secret_area_prepare_current_level()` still runs `secret_area_scan_level()`
synchronously during the cheap live-game preparation pass. The resulting
`Secret_area_state` is what `draw_secret_labels()` and
`secret_area_should_draw_segment_edges()` consume.

Therefore:

- found and revealable secret areas are ready when the level becomes playable
- revealing unfound secrets can continue rebuilding the automap edge list
  immediately
- the readiness message must not say or imply that secret areas are unavailable
- route completion should not force an edge-list rebuild unless the existing
  secret reveal toggle changed

### Automap objectives do wait for route metadata

Objective labels, connectors, and the upper-left objective list consume the
canonical or live `level_metadata_state` and its route-plan summary. On a cold
cache miss, live preparation deliberately leaves route readiness as
`calculating` and does not run the expensive planner.

Opening the automap currently calls `level_metadata_rescan_route_from_object()`.
That route-only rescan cannot synthesize a missing canonical route while
expensive planning is disabled, so no objective route is available yet.

The renderer already reads objective state on every automap draw. Once a valid
cache has been installed and a route-only rescan has run, the next draw will
naturally render the labels, connectors, and objective text. There is no retained
objective vertex buffer or Compose state to invalidate.

The missing refresh boundary is cache adoption. Background completion only sets
the native background-result flag. D2 Guide-Bot gameplay polls that flag, but a
paused automap can remain open without running the Guide-Bot poll. D1 has no
Guide-Bot poll at all. An open automap therefore needs its own throttled,
game-thread cache adoption check.

### The launcher currently has two progress bars

`LevelMetadataAnalysisProgressView` renders:

1. overall completed levels in the selected mission or pack
2. the current raw native task, such as one visibility search

The second value is intentionally task-local. The native checkpoint writer
increments `task_id` whenever the phase or total changes, or when a counter moves
backward. Route planning may run many `route_visibility` and
`route_target_visibility` tasks, so the raw bar repeatedly reaches 100 percent
and resets to zero.

The native outer phases are ordered as follows:

1. `secret_areas`
2. `level_topology`
3. `level_summary`
4. `route_planning`

`route_visibility` and `route_target_visibility` are repeated inner tasks of
route planning, not additional top-level phases. This is enough information to
add a third estimated per-level bar without changing the exact raw task bar.

## Recommended design

### 1. Add a monotonic per-level estimator in shared launcher code

Add a small `LevelMetadataLevelProgressEstimator` beside the existing progress
models in the launcher-library layer. It consumes parsed checkpoint fields, not
human-readable labels:

- level identity from `detail`
- `stage`
- `phase`
- `task_id`
- raw `completed` and `total`

Keep the existing raw `MetadataLoadProgress` unchanged. Add an estimated current
level value to `LevelMetadataAnalysisProgress`, represented in permille or another
fixed integer scale so JNI and tests do not depend on floating-point equality.

Use these initial phase bands:

| Phase | Estimated level band |
| --- | --- |
| Secret-area scan | 0 to 10 percent |
| Topology | 10 to 20 percent |
| Level summary | 20 to 30 percent |
| Route planning | 30 to 100 percent |

These weights are intentionally simple and reflect the measured fact that route
planning dominates cold-cache time. Keep them as centralized constants so device
profiling can tune them without changing checkpoint or JNI schemas.

Within the route-planning band, treat every new inner `task_id` as one estimated
step. Credit a step with 10 percent of the remaining phase rather than a flat 10
percentage points:

`estimated_inner = 1 - 0.9 ^ (completed_inner_tasks + raw_task_fraction)`

This has the requested old-installer behavior: it moves quickly at first, keeps
moving as additional uncounted work appears, and cannot reach the end of the
phase before native route planning actually completes. The final
`route_planning 1/1` or `level_done` event sets the estimate to exactly 100
percent.

Rules:

- reset the estimator only when the level identity changes
- use `task_id` differences so checkpoint polling can skip intermediate files
  without losing all progress credit
- clamp every emitted value to `max(previous, candidate)`
- clamp an unfinished level below 100 percent
- preserve the last estimate on cancellation or failure for diagnostics, but
  start a new level at zero
- treat an unknown phase conservatively as work within the current band and
  never move backward
- never use the estimated percentage for deadlines, cache acceptance, route
  readiness, Guide-Bot decisions, retry classification, or correctness

The estimator is presentation state. The existing visibility checkpoint sequence
and validated route artifact remain the sources of truth for durable computation
progress. A launcher-to-game handoff may begin with a conservative displayed
estimate, but it retains actual analysis work and will never regress a value
already reported within the receiving game process.

### 2. Render the third launcher bar

Extend `LevelMetadataAnalysisProgressView` to retain both existing bars and add:

- label: `Estimated level progress`
- determinate bar using the synthesized fraction
- optional approximate percent text, clearly marked `estimated` if shown

Recommended order:

1. `Overall analysis N/M`
2. raw current operation and exact native counter
3. `Estimated level progress`

Keeping the raw operation visible is valuable for diagnosing a truly stalled
visibility task. The estimated bar is the human-oriented level progress and must
not replace the exact task bar.

### 3. Forward current-level estimated progress to native game state

Add an `onProgress` callback to the current-level call made by
`RouteMetadataBackground.computeMission()`. Forward progress only while the
analyzed target is the currently loaded level. Progress for the next and fill
levels must not appear in the current level's automap.

Add a narrow generation-checked JNI callback in `MainActivity` and
`android_route_metadata.c`:

- request generation
- estimated permille
- state: calculating, useful route ready, complete, or failed

Store the current generation, state, and permille in atomics. Reject stale
callbacks exactly as the existing completion callback does. Reset the state when
a new level request invalidates the previous generation. Keep the automap string
static so no cross-thread mutable string buffer is needed.

Continue forwarding estimates after a `next_ready` partial artifact is published.
This allows `Next` objective mode to become usable immediately while `All` or
`Remaining` can still say that more objectives are being calculated. Set exactly
100 percent only after complete route metadata is published.

### 4. Add an automap readiness note and progress bar

Put the drawing implementation in `automap_metadata_overlay.c` so D1 and D2 need
only matching small Android hooks. Draw it near the upper-left objective list and
derive its vertical position from the number of objective lines already drawn so
the two do not overlap.

Suggested states:

| Objective mode and route state | Automap behavior |
| --- | --- |
| Objectives off | No readiness note or bar |
| Calculating, no usable objective | `Objectives still calculating` plus bar |
| Partial/next-ready with a usable objective | Draw available objectives |
| Partial/next-ready in All or Remaining mode | Draw available objectives and `More objectives calculating` plus bar |
| Complete | Draw objectives and hide readiness UI |
| Terminal failure | `Objectives unavailable`, no moving progress bar |

Do not show a secret-area warning. Secret labels and revealed secret edges remain
available independently while objectives calculate.

Use the ordinary game font and a small bordered rectangle drawn with existing
`gr` primitives. Clamp permille before converting it to pixels. Do not animate or
advance the bar locally: it should move only on real analyzer checkpoints.

### 5. Adopt ready metadata while the automap remains open

Add one shared automap update helper and call it from both D1 and D2 automap event
handlers. Run it on the game thread, rate limited to about once per second while
route readiness is neither complete nor failed.

The helper should:

1. check the current generation/background state
2. call `level_metadata_try_load_pending_cache()` only when a new or improved
   artifact may exist
3. on successful adoption, run
   `level_metadata_rescan_route_from_object(Players[Player_num].objnum)`
4. request no special graphics rebuild; the normal next draw reads the new route
5. leave the secret edge list alone

Add a monotonically increasing native route-revision counter when a canonical
artifact is adopted. The automap records the last revision it displayed and
rescans when the revision changes. D2 Guide-Bot records the same revision but
keeps its existing rule: an active goal is not replaced until the current goal
clears or another established planning boundary occurs. This prevents automap
refresh from bypassing Guide-Bot's cache-improvement latch.

Retain the existing multiplayer and input-demo restrictions. Background timing
must not hot-activate route behavior in deterministic modes. The readiness UI can
report the state, but cache installation and route rescan must follow the same
allowed-boundary policy as the Guide-Bot path.

## Expected files

Primary implementation:

- `android/app/src/main/java/com/dxxredux/app/LevelMetadata.kt`
- `android/app/src/main/java/com/dxxredux/app/MetadataLoadProgress.kt`
- `android/app/src/main/java/com/dxxredux/app/RouteMetadataBackground.kt`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `android/app/src/main/cpp/shared/android_route_metadata.c`
- `android/app/src/main/cpp/shared/android_route_metadata.h`
- `android/app/src/main/cpp/shared/secretarea.c`
- `android/app/src/main/cpp/shared/secretarea.h`
- `android/app/src/main/cpp/shared/automap_metadata_overlay.c`
- `android/app/src/main/cpp/shared/automap_metadata_overlay.h`
- `d1/main/automap.c`
- `d2/main/automap.c`
- `d2/main/escort.c`

Tests and introspection:

- `android/app/src/test/java/com/dxxredux/app/LevelMetadataCheckpointProgressTest.kt`
- a focused estimator unit test file if separating it keeps the cases readable
- `android/app/src/main/cpp/shared/game_introspect.cpp`
- one maintained automap cold-cache automation script, with both D1 and D2 where
  practical

## Implementation phases

### Phase 1: Progress model and launcher UI

- [x] Preserve structured stage, phase, task ID, and level identity in parsed
  checkpoint updates
- [x] Implement the monotonic fixed-band estimator and skipped-task-ID handling
- [x] Add the estimated current-level value to the analysis progress model
- [x] Render the third metadata-viewer bar without changing the two existing bars
- [x] Add focused unit tests for phase transitions, repeated 0-to-100 tasks,
  skipped task IDs, unknown phases, cancellation, level reset, and exact completion

### Phase 2: Game progress bridge

- [x] Forward only current-level analyzer progress through the in-game scheduler
- [x] Add generation-checked JNI progress state and atomic native getters
- [x] Keep progress updates active after `next_ready` until complete
- [x] Add tests for stale-generation rejection, next/fill filtering, monotonic
  clamping across retries, reset on level change, and terminal failure

### Phase 3: Automap readiness UI and refresh

- [x] Draw objective readiness text and the estimated bar in the shared overlay
- [x] Add matching minimal D1 and D2 automap update/draw hooks
- [x] Poll pending cache artifacts at a bounded rate while the automap is open
- [x] Add the canonical route revision and preserve D2 active-goal adoption rules
- [x] Rescan the route after adoption and verify the next draw shows objectives
- [x] Confirm secret labels and revealed secret edges remain available and are not
  rebuilt merely because route metadata completed

### Phase 4: Regression and device validation

- [x] Expose estimated permille, readiness-note state, route revision, and
  automap refresh count through introspection
- [x] Unit test the estimator and checkpoint parser through Gradle
- [x] Run a cold-cache automap test that observes the note/bar, leaves the
  automap open, then observes objective labels without closing and reopening it
- [x] Run objective Off, Next, Remaining, All, partial, complete, and failed cases
- [x] Run the secret-reveal automap test during cold objective computation
- [x] Verify an active D2 Guide-Bot goal does not change when automap adopts a
  better artifact, then does adopt it at the next allowed planning boundary
- [x] Verify multiplayer and input-demo runs do not hot-activate route data
- [x] Run scoped code quality, Android unit tests and APK build, native D1/D2
  tests, Windows host builds, and target-device visual/performance checks

## Acceptance criteria

- Secret areas remain visible without route metadata
- A cold-cache automap with objectives enabled explains why objectives are absent
- The automap bar never moves backward and never reaches 100 percent before
  native completion
- The metadata viewer shows whole-pack, raw-task, and estimated-level bars
- Repeated raw 0-to-100 route tasks do not reset the estimated bar
- An automap left open begins drawing newly available objectives without being
  closed and reopened
- `Next` mode uses a validated next-ready route before full analysis completes
- Active Guide-Bot behavior, multiplayer, input demos, and simulation RNG are not
  changed by asynchronous completion timing

## Study status

- [x] Trace secret-area and objective overlay data sources
- [x] Trace cache adoption and open-automap refresh behavior
- [x] Trace native progress phases, task resets, checkpoint throttling, and UI bars
- [x] Define the monotonic estimator and launcher/native ownership boundary
- [x] Define refresh, Guide-Bot safety, deterministic-mode, and validation rules
- [x] Implement and validate the plan

## Validation results

- Scoped `run-code-quality.ps1 -Fix` passed for the changed C, C++, Kotlin,
  CMake, and test files
- Focused Gradle estimator, checkpoint, deadline, and current-level progress
  tests passed; `assembleDebug` built all three configured Android ABIs
- The full Gradle unit suite ran 807 tests; its only failure was the pre-existing
  environment-dependent `ModManagerMissionZipTest.missionRarImportsReetusAndStagesAtMissions`
  SevenZip initialization failure. The focused tests and APK build passed
- Windows D1 and D2 builds passed through `run-windows-build.ps1`
- Native host suites passed: D1 33/33 and D2 40/40, including the new generation,
  monotonicity, partial-useful, failure, and completion progress-policy test
- `test_automap_objective_readiness_progress.json5` passed twice on the emulator,
  including the final rebuilt APK run `48e7ce31362241fe9c7667affab33e6c`.
  It observed a cold calculating note and sub-1000 progress, secret labels and
  edges during calculation, and objective labels appearing on the still-open
  automap after a route revision refresh
- `test_secret_reveal_automap_d2.json5`, D1 `test_launch_to_automap.json5`, and
  `test_route_metadata_background_priority.json5` passed on the emulator
- `test_kcxf2_guidebot_route_next.json5` passed its All, Remaining, Next, and Off
  automap matrix and active route-goal preservation checks. It later failed at
  unrelated step 131 because an expected `enter_exit` transition remained
  `fly_through_trigger`; this occurred after the relevant checks passed
- Multiplayer and input-demo installation are blocked at both the level-start
  request boundary and the automap adoption boundary. Existing input-demo native
  tests passed in both D1 and D2 suites
- Visual and timing validation used the Android emulator. A physical target-device
  performance pass remains recommended because no phone was attached

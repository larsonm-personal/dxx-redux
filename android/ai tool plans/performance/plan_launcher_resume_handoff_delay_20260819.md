# Launcher resume handoff delay

Date: 2026-08-19
Status: research complete, implementation not started

## Goal

Reduce or explain the silent delay between selecting `Load Last Save` in the
launcher and seeing the game process loading overlay. Show immediate, accurate
launcher feedback for every launch preparation phase without changing save file
semantics or duplicating save parsing in Kotlin.

## Constraints

- Planning and code research only in this tranche
- Do not use the emulator because another task may own it
- Keep save parsing and restoration in the existing native engine code
- Preserve D1 and D2 behavior through shared Android launcher code
- Do not allow launcher route analysis to keep competing with the game after a
  launch

## Findings

### Primary cause

The strongest static cause is the route metadata launcher-to-game handoff added
by commit `0fbf4eb8` on 2026-08-13.

`SetupActivity.startGameAfterRouteMetadataHandoff()` does not call
`startActivity()` until `RouteMetadataPrecomputeCoordinator.stopForGameLaunch()`
returns. When an analyzer is active, cancellation enters
`LevelMetadataAnalyzer.cancelOwnedWorker()`, which deliberately waits up to
`LEVEL_METADATA_CANCELLATION_GRACE_MS`, currently exactly 2,000 ms, for the
isolated worker to flush a checkpoint. Polling occurs every 200 ms. Only after
that grace period does it kill an unresponsive worker. Commit `6dfb707d` later
added an 8,000 ms outer timeout and a final process sweep, but retained the
2,000 ms normal cancellation grace.

This explains all important characteristics of the reported symptom:

- It feels new because the blocking handoff was added recently
- It is commonly 1 to 2 seconds when background route analysis is active
- The launcher remains visible because `MainActivity` has not started yet
- The in-game loading bar cannot appear because its view belongs to
  `MainActivity`
- The delay may disappear when route analysis is idle, as recorded by the prior
  active-precompute verification that completed the handoff in 5 ms

Stopping launcher-owned route analysis before gameplay is required. Waiting the
full two-second checkpoint grace before even starting the game activity is a
policy choice, not a save-load requirement.

### Other synchronous launcher work

Before the route handoff starts, `prepareGameLaunchFiles()` runs synchronously
from the Compose click callback. It checks mod compatibility, scans D1-in-D2
readiness, reads pilot music preferences, writes the active set path and CD
playlist, writes enabled mod paths, and updates game configuration files. Some
of these calls were expanded between 2026-08-11 and 2026-08-17. Static review
does not show a fixed delay here, but this work currently blocks the launcher
main thread and has no per-phase timing, so it should be measured rather than
assumed cheap.

Resume candidate path and callsign resolution are string and path operations.
The launcher does not deserialize the save during the silent interval. Native
save restoration starts later in `startup_resume_save_from_cmdline()` after the
game process initializes. The existing game loading overlay begins during
OpenGL bitmap preparation, so it cannot cover launcher preflight or process
handoff.

### Feedback gap

`routeMetadataLaunchJob` prevents duplicate handoffs but is not exposed as
Compose state. `ResumeSavePanel` therefore leaves `Load Last Save` looking
enabled and unchanged while preparation runs. The route monitor writes a
`pausing_for_game` phase to its diagnostic file, but the main launcher screen
does not render that state.

## Recommended implementation

### Phase 1: Add measurement before changing timing

- Add monotonic elapsed timing around each material
  `prepareGameLaunchFiles()` operation
- Retain the existing launcher-to-game handoff timing and add one end-to-end
  `launch requested` to `startActivity called` measurement with launch type and
  game, excluding save paths and other sensitive values
- Record whether a metadata worker was active, whether it exited cooperatively,
  whether it was terminated, and the time spent in grace and final join
- Use the existing launcher and route metadata diagnostic logs

This separates main-thread preflight cost from metadata shutdown and gives a
baseline for idle and active-analyzer launches.

### Phase 2: Add immediate launcher feedback

- Add one activity-owned launch preparation state with an active flag, launch
  kind, phase label, and monotonic start time
- Set it before running preflight so the first Compose frame responds to the
  button press
- Render a small modal or full-width launcher overlay with an indeterminate
  linear progress bar and phase text:
  - `Preparing saved game`
  - `Pausing background analysis`
  - `Starting Descent 1` or `Starting Descent 2`
- Disable launch controls while active and retain the existing job guard
- Clear the state on preflight failure, activity destruction, or a failed
  `startActivity()` call
- Reuse the same state for ordinary, multiplayer, replay, and resume launches so
  the central handoff cannot regress into another silent path

An indeterminate bar is preferred. There is no honest continuous percentage for
filesystem preflight or process shutdown. The discrete phase label supplies the
useful progress information without inventing a percentage.

### Phase 3: Remove the avoidable two-second launch wait

- Split coordinator cancellation into `cancel and return job` and `await job`
  operations so game launch can use a different shutdown policy from ordinary
  analyzer preemption
- On explicit game launch, cancel the coordinator, allow only a short bounded
  checkpoint opportunity, terminate any remaining isolated metadata worker,
  then await coordinator cleanup
- Reduce `LEVEL_METADATA_POLL_MS` from 200 ms to 100 ms and start with a 100 ms
  launch grace so the bounded grace remains one poll interval; adjust only from
  measurements
- Keep the 2,000 ms grace for background-to-background priority preemption where
  preserving the newest checkpoint is worth the wait
- Keep the final process sweep and a bounded join before game startup so an
  analyzer cannot continue consuming CPU or owning route files during gameplay

The simpler fallback is immediate worker termination on explicit launch. Route
chunks and final cache artifacts are atomically published, so already-published
work remains usable, but the last in-memory checkpoint tail may be lost. The
100 ms bounded grace is the recommended balance until device measurements show
otherwise.

### Phase 4: Move launch preflight off the UI thread

- Run file and configuration preflight on `Dispatchers.IO` inside the same
  single launch pipeline
- Keep Compose state changes and `startActivity()` on the main dispatcher
- Audit called helpers for assumptions about the main thread before moving them
- Preserve existing error messages and storage failure handling

This phase should follow measurement. It improves responsiveness even if
preflight is not the dominant elapsed time and lets the feedback frame render
immediately.

### Phase 5: Test and verify

- Extend JVM tests for launch phase labels, state transitions, duplicate launch
  rejection, and failure reset
- Extend route metadata tests with a launch-shutdown policy test proving that the
  launch grace is bounded independently from normal preemption grace
- Add `launch_preparation` state to setup introspection so automation can assert
  the launcher feedback without screenshots
- Extend the existing unified autosave resume script to assert that preparation
  becomes visible, launch completes, and the restored game reaches the expected
  level
- Run scoped code quality, launcher unit tests, the Android build, and existing
  save-resume tests
- When the emulator is free, measure tap-to-`startActivity` and tap-to-first-game
  progress for both idle metadata and actively analyzing metadata. Record several
  runs for D1 and D2 rather than relying on one sample

## Expected result

With an active metadata worker, the pre-engine handoff should fall from roughly
2,000 ms to approximately the new 100 ms poll interval plus process cleanup. The
launcher should respond on the first frame and explain the remaining wait. With
no active worker, behavior should remain near the existing few-millisecond handoff plus
normal file preflight and Android activity startup.

## Files likely involved

- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupResumePanel.kt`
- `android/app/src/main/java/com/dxxredux/app/RouteMetadataPrecomputeCoordinator.kt`
- `android/app/src/main/java/com/dxxredux/app/LevelMetadata.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupAutomationApi.kt`
- `android/app/src/test/java/com/dxxredux/app/ResumeSavePanelTest.kt`
- `android/app/src/test/java/com/dxxredux/app/RouteMetadataPrecomputeMonitorTest.kt`
- `android/game_scripts/test_autosave_resume_unified.jsonc`

No D1 or D2 engine source change is expected.

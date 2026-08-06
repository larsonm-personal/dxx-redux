# Android dormant-state power audit

## Goal

Determine why the game process may continue consuming power while minimized or idle on a menu, and define a safe Android-specific sleep policy for menus and paused single-player games without disrupting multiplayer or other platforms.

## Plan

- [x] Read repository instructions and locate Android lifecycle, polling, rendering, audio, and multiplayer keep-alive paths
- [x] Trace native lifecycle calls and establish which work continues after `onStop`
- [x] Separate background, foreground-menu, paused-single-player, and multiplayer states and identify safe suspend boundaries
- [x] Recommend a phased implementation and verification strategy, including battery/process diagnostics
- [x] Move lifecycle engine inspection and pause/autosave actions from the Android UI thread to the game thread
- [x] Suspend OpenSL callbacks and park MIDI/redbook producers during background intervals
- [ ] Make the single-player checkpoint transactional, then park the native event/draw loop

## Findings

- `MainActivity.onStop()` stops gyro and Kotlin overlay polling, releases input, and calls `nativeOnPause()`
- The existing comment says `nativeOnPause()` injects Escape to open the pause/game menu; this is game-state pausing, not yet evidence that the native render/event loop sleeps
- The activity unconditionally sets `FLAG_KEEP_SCREEN_ON` during creation
- `android_surface_pause()` only makes the framebuffer blit/EGL swap return early. `event_process()` still polls, dispatches draw events, calls `gr_flip()`, and repeats
- Static menus and the pause window rate-limit their idle handler to 50 FPS, so an invisible game can continue walking the full native draw path about 50 times per second even though presentation is skipped
- Baseline behavior kept background music and redbook producer threads polling every 20 ms and left the SDL audio device active; the first implementation tranche now suspends all three without changing user pause state
- The central 100 ms overlay poller is removed in `onStop()`, but independent coop, warp, video, and network overlay handlers are not centrally suspended; coop polling is started unconditionally after the native thread starts
- The lifecycle JNI code reads engine globals and calls `window_get_front()` from the UI thread even though its adjacent comment warns that the game thread mutates the window list

## Recommended state model

Keep classification native because the engine owns the authoritative window and multiplayer state:

- `ACTIVE`: gameplay, movies, briefings, transitions, automation, and multiplayer
- `VISIBLE_EVENT_DRIVEN`: a static main/options menu or paused single-player window while the activity is visible
- `BACKGROUND_SUSPENDED`: activity stopped and no live multiplayer session requires simulation/network progress

Treat Activity visibility as a separate input to that model. Do not infer background state from `nativeIsInGame()`: it only says whether `Game_wind` is the front window and groups unrelated menus, pause windows, movies, and briefings together.

## Resume-crash history follow-up

- [x] Reconstruct relevant minimize/resume crash reports and fixes from git history
- [x] Trace EGL surface/context generation handling and identify which GPU resources are invalid after wake
- [x] Trace minimize autosave timing and determine whether a deeper suspend needs additional single-player state capture
- [x] Refine the sleep/wake design with separate thread-suspend, graphics-reload, and process-death recovery paths

### Historical findings

- `0b26c505` (2026-03-03) added Home/minimize Escape injection and mutex protection around the software `ANativeWindow` bridge
- `dfaec291` (2026-03-12) fixed two Home-button crash roots: render/surface teardown raced, and UI-thread `window_get_front()` traversed a game-thread-owned linked list. It added the mutex-protected surface pause gate and replaced the traversal with simple state reads
- `6c12c2fe` (2026-03-13) fixed the approximately 60-second-background black screen. The resumed Activity received a new `ANativeWindow`, but EGL still targeted the dead old window. The fix marked the EGL surface stale, recreated the EGL surface, preserved the context when possible, and invalidated/re-cached textures on the context-loss fallback
- `5204c45f` (2026-03-16) isolated `MainActivity` in `:game` to avoid a separate `libhwui.so` RenderThread destroyed-mutex crash during Compose-to-SurfaceView transition
- `3e0a480a` (2026-03-27) reintroduced `window_get_front()` in `nativeOnPause()` to avoid closing an already-open game menu. That partially regressed the March 12 UI-thread safety fix and contradicts the surviving warning immediately above the lifecycle code
- `ef218f17` (2026-05-13) added queued minimize autosaves and direct startup restore. The design notes already identify thumbnail capture versus surface teardown as a lifecycle hazard
- `ef3506d4` (2026-07-12) centralized the duplicated D1/D2 EGL lifecycle code while deliberately preserving its behavior
- `2af58200` (2026-08-01, BR-0204) replaced borrowed native-window access and a one-bit stale flag with reference-counted, generation-tagged snapshots. Repeated D1/D2 background/resume automation verified two EGL surface recreations per game

### Remaining graphics recovery gap

The August review ledger already records open defect `BR-0251`, which matches the deeper-sleep concern:

- nominal surface replacement is handled by the window generation and normally preserves all GL objects because the EGL context survives
- actual context loss is not safely handled: recreation requests GLES1 after destroying a GLES3 context, does not validate the replacement context or second `eglMakeCurrent`, only smashes/re-caches ordinary textures, and still reports success
- context-owned programs, uniforms, the GLES3 shim program/VAO/VBO, pixel buffers, GPU queries, MSAA framebuffer/renderbuffer objects, batch textures, and other GL caches are not comprehensively rebuilt
- `eglSwapBuffers()` failure and `EGL_CONTEXT_LOST` are ignored, so context loss after a nominal wake does not enter recovery

### Single-player checkpoint findings

- The minimize save is an asynchronous integer request set from Android lifecycle callbacks and consumed later by the game thread on `EVENT_IDLE`
- There is no acknowledgement that the save has completed before surface teardown, process eviction, or a future engine-thread park
- If the dedicated pause window is frontmost, `android_handle_pause_saveload_request()` closes it so the game window can consume the save. Closing the pause window resumes songs, and the save path eventually calls `start_time()`. A suspend implementation must not let this transient window manipulation leave simulation active in background
- The save writes directly to the target slot and is not a transactional temp-file publication, so process death during a deep-suspend checkpoint can damage the new checkpoint
- A normal same-process thread park or EGL-surface release does not require reloading the gameplay save. RAM simulation state remains authoritative; only presentation resources need reconstruction
- A destroyed EGL context requires a complete renderer resource rebuild, not a gameplay restore
- Process death requires the completed autosave plus a small durable suspend-session record if returning should transparently restore rather than merely show the launcher's existing resume offer

### Refined suspend and wake protocol

1. Publish a lifecycle command to the game thread instead of inspecting engine windows on the UI thread
2. On the game thread, classify the front window and multiplayer state, freeze simulation time, release input, and create a transactional single-player checkpoint if a level is loaded
3. Preserve whether the player was already paused, but do not persist raw window pointers or the window stack. If a process restart restores the checkpoint, recreate a fresh pause window before allowing simulation to advance
4. Acknowledge `CHECKPOINT_COMMITTED`, then stop frame submission and enter `PARKED`
5. For ordinary background sleep, destroy/recreate only the EGL surface and retain the GLES3 context when the driver permits. Existing window generations cover this nominal path
6. For deliberate GPU-memory release or detected `EGL_CONTEXT_LOST`, increment a separate context generation and rebuild every context-owned renderer resource under one checked GLES3 recovery transaction before publishing `RENDERER_READY`
7. Resume audio, polling, input, and simulation only after a valid current context, renderer rebuild if needed, and a fresh foreground surface are all ready
8. If the process died, validate the committed suspend-session record and autosave, start through the existing direct-restore path, and hold the restored game paused until explicit user input

## Detailed implementation roadmap

### Scope and safety boundaries

In scope:

- stop recurring CPU, render, audio, sensor, and overlay work while a non-multiplayer game activity is backgrounded
- retain the existing nominal EGL surface-generation recovery path
- make minimize checkpoints complete and durable before the engine parks
- make actual EGL context loss recover all context-owned renderer resources
- optionally restore a committed suspended single-player session after process death
- reduce foreground static-menu work only after background suspension is stable

Out of scope for the first release of this work:

- suspending live multiplayer simulation or networking
- serializing raw engine pointers, window objects, GL handles, or audio decoder objects
- loading a gameplay save merely because an EGL surface or context was recreated in the same process
- changing desktop lifecycle behavior
- broad D1/D2 engine refactoring unrelated to the Android hooks

### State model and ownership contract

Use separate state axes rather than one overloaded dormant boolean:

1. Android visibility
   - `FOREGROUND`
   - `BACKGROUND_REQUESTED`
   - `BACKGROUND`
2. Engine workload
   - `NO_LEVEL`
   - `STATIC_UI`
   - `SINGLE_PLAYER_ACTIVE`
   - `SINGLE_PLAYER_PAUSED`
   - `TIME_DRIVEN_SCREEN`
   - `MULTIPLAYER_ACTIVE`
3. Suspend transaction
   - `RUNNING`
   - `QUIESCING`
   - `CHECKPOINTING`
   - `PARKED`
   - `WAKING`
4. Graphics readiness
   - `NO_WINDOW`
   - `WINDOW_AVAILABLE`
   - `SURFACE_READY`
   - `CONTEXT_REBUILD_REQUIRED`
   - `RENDERER_READY`

Ownership rules:

- the Android UI thread owns Activity visibility callbacks and `Surface` publication
- the surface bridge mutex owns `ANativeWindow` replacement, reference retention, pause fencing, and window generation
- the game thread alone reads or traverses engine windows and decides engine workload
- the render/game thread alone creates, destroys, or publishes EGL and GL objects
- lifecycle JNI calls may publish atomics, signal a condition variable, and use the surface bridge mutex, but may not inspect the engine window list
- every request and acknowledgement carries a monotonically increasing generation so stale resume, surface, checkpoint, and automation events cannot satisfy a newer transition

Policy matrix:

| Activity and engine state | Simulation | Rendering | Audio | Checkpoint | Park |
| --- | --- | --- | --- | --- | --- |
| Foreground active single-player | run | run | run | periodic policy only | no |
| Foreground paused single-player | frozen | event-driven later | user pause policy | no | later |
| Foreground static menu | none | event-driven later | menu music policy | no | later |
| Background single-player level | frozen | stop | suspend device | required | yes |
| Background menu/no level | none | stop | suspend device | none | yes |
| Background time-driven screen | frozen | stop | suspend device | checkpoint only if safely saveable | yes |
| Background live multiplayer | run | stop | suspend local output | existing multiplayer policy | no |

### Chunk 0: Baseline diagnostics and reproducible measurements

Goal: prove where work continues and make every later transition observable without high-volume logging.

Status: implementation complete; physical-device Dormancy-log and multiplayer baselines pending

Tasks:

- [x] Add a shared Android lifecycle diagnostic snapshot with visibility, workload, suspend state, request generation, acknowledgement generation, window generation, context generation, and last transition reason
- [x] Add coarse counters for native event-loop iterations, draw dispatches, swaps attempted, swaps presented, audio callbacks, music producer wakeups, redbook producer wakeups, central overlay polls, and independent overlay polls
- [x] Expose the snapshot through the existing introspection JSON in D1 and D2
- [x] Add a dedicated `Dormancy` selector to Advanced debug logging and emit lifecycle plus 60-second counter snapshots only while that category is enabled; do not log each loop or callback
- [x] Extend the automation runner's existing `SCRIPT_BACKGROUND` support so it can vary background duration and optionally lock the screen
- [x] Add `android/game_scripts/test_background_dormancy_unified.json5` with entry points for main menu, active single-player, pause window, and game menu
- [x] Record an emulator baseline by enabling the Advanced-tab `Dormancy` category, running the foreground/background scenarios, and reading the exported debug-log bundle
- [ ] Record the equivalent physical-device and multiplayer baselines through exported Dormancy logs

Validation completed 2026-08-05:

- D1 and D2 debug native builds passed for arm64-v8a, armeabi-v7a, and x86_64
- Android debug unit tests and APK assembly passed
- automation catalog validation passed
- `test_background_dormancy_unified.json5` passed on the emulator for D1 and D2, including a locked-screen main-menu cycle plus active-game, pause-window, and game-menu cycles
- both successful runs ended with request and acknowledgement generations matched, one retained EGL context generation, and four recreated window surfaces
- the exported D1 Dormancy log confirmed the current background work directly: during a 13.1-second background main-menu interval, event loops advanced by 82, draw dispatches by 83, audio callbacks by 2,205, redbook producer wakeups by 515, and independent overlay polls by 39 while swaps correctly remained flat
- a 6.6-second background single-player interval still advanced event loops by 14, draw dispatches by 14, audio callbacks by 1,162, redbook producer wakeups by 269, and independent overlay polls by 23 while swaps remained flat

Likely files:

- `android/app/src/main/cpp/shared/game_introspect.cpp`
- a new shared Android lifecycle diagnostics `.c/.h` pair under `android/app/src/main/cpp/shared/`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- the game-script runner/helper that handles `SCRIPT_BACKGROUND`

Completion gate:

- the four single-player/menu scenarios and one multiplayer scenario report unambiguous state and monotonically increasing counters before any sleep behavior changes
- no diagnostic counter introduces a data race or per-frame file write

### Chunk 1: Game-thread lifecycle coordinator

Goal: remove engine inspection and pause decisions from the Android UI thread.

Tasks:

- [ ] Add a shared `android_app_lifecycle.c/.h` coordinator with atomic request publication, a mutex/condition variable, request generation, acknowledgement generation, and explicit transition states
- [x] Replace `nativeOnPause()` engine-global and `window_get_front()` inspection with atomic request publication plus the immediate surface fence
- [x] Preserve the immediate surface safety fence on the UI thread by keeping `android_surface_pause()` under the surface bridge mutex before Android can destroy the window
- [x] Add a game-thread lifecycle tick at a stable boundary before simulation or draw work
- [x] Classify `NO_LEVEL`, `STATIC_UI`, `SINGLE_PLAYER_ACTIVE`, `SINGLE_PLAYER_PAUSED`, `TIME_DRIVEN_SCREEN`, and `MULTIPLAYER_ACTIVE` from game-thread-owned state
- [ ] Identify the pause window by callback on the game thread; do not publish or retain the raw window pointer across threads
- [x] Convert minimize autosave classification and Escape injection into game-thread actions
- [ ] Make repeated `onPause`, `onStop`, `onTrimMemory`, `onResume`, and surface callbacks idempotent by generation
- [ ] Ensure `onPause` followed by a resumed Activity without `onStop` cancels or supersedes the older request safely
- [ ] Add native unit tests that drive request ordering, duplicate callbacks, resume-before-park, and stale-generation cases

Likely files:

- `android/app/src/main/cpp/android_input.c`
- `android/app/src/main/cpp/android_surface.c`
- new shared lifecycle coordinator files
- mirrored small hooks in `d1/arch/sdl/event.c` and `d2/arch/sdl/event.c`, or one new shared hook called by both
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`

Completion gate:

- lifecycle JNI code no longer calls `window_get_front()` or other engine window accessors
- all workload classification and engine pause changes execute on the game thread
- existing short and 60-second D1/D2 background/resume automation still passes before parking is enabled

Implementation note, 2026-08-05:

- the existing shared lifecycle diagnostics pair now owns atomic visibility request and acknowledgement generations, avoiding a second overlapping coordinator
- `nativeOnPause()`, `nativeOnResume()`, and `nativeQueueMinimizeAutosave()` no longer inspect engine globals or traverse windows; the mirrored D1/D2 event boundary consumes those requests
- the first post-change D1/D2 four-state emulator matrix passed before audio suspension was enabled; the final audio-enabled rerun is recorded with Chunk 4

### Chunk 2: Durable single-player suspend checkpoint

Goal: make a background single-player checkpoint a completed transaction, not a best-effort flag.

Tasks:

- [x] Add checkpoint phases `NOT_NEEDED`, `REQUESTED`, `WRITING`, `COMMITTED`, `SKIPPED`, and `FAILED` to the coordinator
- [ ] Define save eligibility on the game thread for ordinary level play, death state, secret levels, final-boss sequences, demo playback/recording, and other existing blockers
- [x] Keep multiplayer excluded from the single-player checkpoint path
- [x] Call the save writer from a stable game-thread boundary without relying on a future generic `EVENT_IDLE`
- [x] Preserve whether simulation was already paused and whether the pause window was frontmost
- [x] Stop time under explicit lifecycle ownership and release only the pause ownership acquired by the coordinator
- [x] Remove the need to close the dedicated pause window merely to deliver the autosave request
- [x] Decouple minimize thumbnail creation from the disappearing surface by using a cached last-presented gameplay thumbnail or a documented blank-thumbnail fallback
- [x] Write the new checkpoint to a temporary path, close and validate it, then publish with `PHYSFSX_rename()` using the existing transaction patterns in `playsave` and coop level restart
- [x] Preserve the previous valid minimize checkpoint until the replacement has been validated and published
- [ ] Publish checkpoint result, slot, path, and request generation in introspection and Game Logs
- [x] Do not enter `PARKED` until the coordinator observes `COMMITTED`, an explicit safe `SKIPPED`, or a bounded failure policy

Tests:

- [ ] Native tests for eligible save, already-paused save, every skip reason, failed open/write/close/rename, and preservation of the previous checkpoint
- [ ] Kill or fault injection at each publication phase to prove the prior or new checkpoint remains readable
- [x] D1 and D2 automation that backgrounds from active play and from an existing pause window, then verifies the checkpoint and paused state on wake
- [ ] Verify checkpoint metadata, thumbnail policy, callsign repair, D2 secret companion handling, and launcher candidate selection

Completion gate:

- every suspend generation has one terminal checkpoint result
- a successful acknowledgement means the final checkpoint is closed, validated, and durably published
- an already-paused game remains paused throughout the transaction and after wake

#### D2 secret companion transaction tranche

- [x] Stage the new slot-specific secret companion without changing the published companion
- [x] Represent the required absence of a companion when `secret.sgc` does not exist
- [x] Preserve backups of the published main save and secret companion until both new states are installed
- [x] Roll back both files if either publication step reports failure
- [x] Remove temporary and backup artifacts after commit or rollback
- [x] Keep D1 on the simpler single-file transaction
- [x] Add host-native transaction tests for new pair, absent companion, failed publication, and prior-pair preservation
- [x] Rebuild all Android ABIs and rerun the D1/D2 dormancy matrix

Operation-failure evidence, 2026-08-05:

- the shared pair publisher installs the D2 secret companion first and the validated main save last, retaining both old files as backups until the main-save commit point
- host-native D1 and D2 suites inject failure at every rename phase; all cases retain the prior readable pair and remove transaction artifacts
- D1 passed 28/28 host tests and D2 passed 32/32; JVM tests and all configured Android ABIs built successfully
- the maintained D1/D2 four-state device matrix passed; active and already-paused D2 checkpoints committed, menu checkpoints safely skipped, parked counters stayed flat, and wake succeeded
- no `.tmp` or `.bak` transaction artifacts remained after the device run
- sudden process death inside the publication window is intentionally still tracked by the unchecked kill-injection test above; operation rollback alone does not prove crash recovery

### Chunk 3: Native engine park and wake

Goal: eliminate the background event/draw loop without releasing the EGL context.

Tasks:

- [x] After checkpoint resolution, stop frame submission and transition `QUIESCING -> PARKED` on the game thread
- [x] Wait on a condition variable with a predicate loop, not `SDL_Delay` polling
- [ ] Make resume, quit, fatal shutdown, surface replacement, and debug automation capable of waking the parked thread
- [ ] Drain or reset stale touch, key, mouse, joystick, gyro, and axis-mailbox state before the first resumed frame
- [x] Keep timer accounting frozen while parked so the first resumed frame does not receive the entire background duration
- [ ] Require both foreground visibility and a usable current surface generation before leaving `WAKING`
- [x] Keep multiplayer simulation unparked; continue to suppress drawing while the Activity is hidden
- [ ] Define what happens if a multiplayer session ends while backgrounded: reclassify on the game thread and park once no live session requires progress
- [x] Expose park entry count, wake count, parked duration, and wake reason through introspection

#### Multiplayer 20-minute background deadline tranche

Goal: preserve short background multiplayer sessions, but stop an unattended host or client and enter full engine sleep after 20 continuous minutes outside the foreground.

- [x] Start the deadline from the Activity background transition only while the multiplayer foreground service owns a runtime session
- [x] Cancel and reset the full 20-minute interval immediately on every Activity foreground transition
- [x] Own the deadline in the foreground-service process so a frozen game process cannot leave matchmaking or relay work alive overnight
- [x] At expiry, send one explicit command to the game process and take the engine's normal multiplayer quit path on the game thread
- [x] End hosted matchmaking state, relay/proxy ownership, runtime IPC, and the multiplayer foreground service at expiry
- [x] Reclassify the engine as non-multiplayer after normal disconnect processing and enter the existing background `PARKED` state
- [x] Record deadline start, reset, expiry, native disconnect, service shutdown, and final park under the launcher-controlled Dormancy log category
- [x] Make duplicate background, foreground, expiry, and stale-session messages idempotent
- [x] Add focused JVM tests for deadline reset, stale callback suppression, and single expiry
- [ ] Add D1/D2 integration coverage using a debug-only shortened deadline, including reopen-before-expiry and continuous-background expiry cases
- [ ] Verify the short-background case remains connected and advances required network simulation while render/audio/UI counters remain suppressed
- [ ] Verify the expired case has no advancing engine, audio, overlay, matchmaking, proxy, or foreground-service work

Implementation evidence, 2026-08-05:

- the service schedules one in-process deadline callback plus one `ELAPSED_REALTIME_WAKEUP` safety alarm with no periodic polling; the default production interval is 1,200,000 ms
- foreground visibility invalidates the alarm generation and cancels the pending alarm; duplicate background notifications do not extend an existing interval
- expiry gives the native engine five seconds to complete its ordinary multiplayer close before the service applies a bounded matchmaking, proxy, IPC, notification, and service shutdown fallback
- the native game thread requests `multi_quit_game`, observes multiplayer state clear, reports completion to the Activity, and requests the existing background park transition
- focused JVM deadline tests, all configured Android ABI builds, D1 28/28 host tests, and D2 32/32 host tests pass
- the post-change D1/D2 four-state single-player device matrix also passes; two-device multiplayer deadline/reset validation remains open

Completion gate:

- less than 20 continuous background minutes never disconnects a healthy multiplayer session
- reopening the Activity grants a new full 20-minute interval
- 20 continuous background minutes causes one normal engine disconnect and then the same parked sleep used by single-player/menu states
- host failure remains visible through the engine's ordinary multiplayer outcome rather than a separate synthetic save/restore path

Tests:

- [ ] Native lost-wakeup stress test with request/resume races and repeated generations
- [x] Background main menu and single-player tests asserting event-loop, draw, and swap-attempt counters stop advancing after acknowledgement
- [ ] Resume tests asserting exactly one wake for one generation and no large first-frame time step
- [ ] Multiplayer background test asserting network/simulation counters continue while render counters stop

Completion gate:

- a background non-multiplayer process has no advancing engine frame counters after `PARKED`
- 100 rapid Home/resume cycles do not deadlock, miss a wake, double-resume time, or use a stale window

### Chunk 4: Audio, sensors, UI polling, and screen policy

Goal: remove the remaining Java and audio wake sources after the engine parks.

Tasks:

- [x] Add one `suspendUiWork()` and `resumeUiWork()` lifecycle boundary in `MainActivity`
- [x] Stop and later restore the central overlay poller, coop stats, warp status, video diagnostics, network stats, network events, music controls, touch drain handlers, and other known self-posting game Activity callbacks
- [x] Make every overlay poller idempotently startable/stoppable
- [x] Pause gyro and other sensors, release mixed input state, and cancel pending touch callbacks
- [ ] Clear `FLAG_KEEP_SCREEN_ON` when backgrounded and restore it only while the game Activity is foreground and policy requires it
- [x] Suspend the OpenSL-backed SDL audio device so its callback stops instead of repeatedly emitting silence
- [x] Replace the MIDI and redbook producer threads' background 20 ms loops with condition-variable waits
- [x] Keep background pause separate from the user's music pause setting, current track, decoder position, and looping state
- [x] For background multiplayer, stop local audio output and nonessential UI work while leaving required network/game progress active

Tests:

- [ ] JVM tests for the complete poller registry and keep-screen-on policy
- [ ] Native audio tests for background pause, user pause plus background pause, resume ordering, quit while paused, and no lost wake
- [ ] Introspection assertions that audio callback and producer-wakeup counters stop after suspend and resume from their prior logical state

Completion gate:

- all known recurring Handler, sensor, SDL audio, MIDI, and redbook callbacks stop in background non-multiplayer states
- background and user-controlled music pause states remain independent

Validation evidence, 2026-08-05:

- the audio-enabled D1/D2 four-state emulator matrix passed, including EGL recreation after main-menu, active-game, dedicated-pause, and game-menu background cycles
- in the exported D1 Dormancy log, active-game audio callbacks remained exactly `7852 -> 7852` from background acknowledgement through foreground request; the dedicated-pause interval remained exactly `13432 -> 13432`
- redbook producer wakeups advanced only once while entering each background wait, replacing the prior hundreds of 20 ms polling wakes
- event and draw counters still advance by design; native engine parking remains gated on the durable checkpoint work in Chunks 2 and 3
- Android native builds and unit tests passed for all configured ABIs, the debug APK assembled, scoped code quality passed, and Windows D1/D2 builds passed

Checkpoint, park, and UI-quiescence evidence, 2026-08-05:

- lifecycle checkpoint writes now run directly at the stable D1/D2 game-thread event boundary instead of waiting for a later `EVENT_IDLE`
- active play and already-open pause/game-menu cases reached `REQUESTED -> WRITING -> COMMITTED` for minimize slot 9 before background acknowledgement; main-menu cases reached an explicit safe `SKIPPED`
- minimize and highest-progress saves use blank thumbnails while the surface is paused, so checkpointing makes no GL readback or render call
- the main save is written to a temporary path, reopened and metadata-validated, then published with `PHYSFSX_rename()`; D2 installs its staged secret companion and main save as a rollback-capable pair, with true process-kill recovery still open
- non-multiplayer background states now transition `QUIESCING -> PARKED`, wait on a predicate condition variable, and wake for foreground visibility or native quit
- lifecycle-owned `stop_time()` and `start_time()` calls are balanced around the wait, preserving an already-owned pause count
- D1 and D2 each passed the maintained four-state matrix: main menu, active play, dedicated pause window, and game menu
- in final Dormancy v3 logs `debuglog_20260805_212717.txt` and `debuglog_20260805_212840.txt`, all eight parked intervals kept event loops, draw dispatches, swap attempts/presents, OpenSL callbacks, MIDI wakes, redbook wakes, central UI polls, and independent UI polls exactly flat through wake
- the same runs completed four foreground wakes per game, recreated each Android window surface, and retained EGL context generation 1
- scoped C/C++ and Kotlin code quality passed; native builds for `arm64-v8a`, `armeabi-v7a`, and `x86_64`, JVM tests, and debug APK assembly passed

### Chunk 5: Nominal EGL surface wake hardening

Goal: retain the proven low-risk wake path where only the Android window and EGL surface change.

Tasks:

- [ ] Keep the EGL context alive during ordinary background parking
- [ ] Keep using retained, generation-tagged `ANativeWindow` snapshots from BR-0204
- [ ] Do not clear the surface pause fence until Android visibility is foreground and a current non-null window generation is published
- [ ] Recreate the EGL surface on the render thread for the new window generation
- [ ] Validate every detach, destroy, geometry, create-surface, and make-current operation before publishing `SURFACE_READY`
- [ ] Make surface replacement transactional where EGL permits: do not claim the new generation until its surface is current
- [ ] Return a typed result such as `READY`, `RETRY_NO_WINDOW`, `RETRY_BAD_SURFACE`, `CONTEXT_REBUILD_REQUIRED`, or `FATAL`
- [ ] Bound retries and keep the engine in `WAKING` rather than issuing GL calls against an invalid drawable
- [ ] Preserve the existing recreation counter and add failure/retry counters and the last EGL error

Tests:

- [ ] Extend the existing two-cycle D1/D2 background test to short, 60-second, screen-off, lock-screen, and rapid-cycle variants
- [ ] Add injectable EGL wrapper tests for no window, failed geometry, failed surface creation, and failed make-current
- [ ] Assert that no draw or texture upload occurs until the expected window generation is current

Completion gate:

- ordinary background/resume never destroys the GLES3 context or reloads gameplay state
- repeated surface replacement either reaches `RENDERER_READY` or remains in a bounded, diagnosable retry state

### Chunk 6: Complete EGL context-loss recovery (BR-0251)

Goal: make actual context loss and deliberate GPU-memory release safe before enabling the deepest same-process sleep tier.

Tasks:

- [ ] Introduce a context generation separate from the native-window generation
- [ ] Detect `EGL_CONTEXT_LOST`, `EGL_BAD_CONTEXT`, and recoverable surface errors from both `eglMakeCurrent()` and `eglSwapBuffers()`
- [ ] Recreate the same required GLES3 context contract; remove the GLES1 fallback
- [ ] Validate context creation and make-current before making any GL call
- [ ] Make `gles3_shim_init()` return success/failure and clean up partial shaders, programs, VAOs, and VBOs in reverse order
- [ ] Add a single renderer context-loss teardown/rebuild entry point rather than scattered ad hoc reloads
- [ ] Inventory and reset every context-owned object in D1 and D2, including:
  - bitmap and replacement texture handles plus texture-binding caches
  - GLES3 shim program, uniforms, VAO, VBO, and cached state
  - OGL merge programs and uniform locations
  - pixel buffers and render-sized allocations
  - MSAA framebuffer, renderbuffer, texture, and nesting state
  - GPU timer query objects, ring indexes, and in-flight state
  - menu/background and batched bitmap texture caches
  - merged-wall diagnostic or probe GL objects
  - any enabled xmodel VBO/texture objects
- [ ] Reapply baseline GL state, viewport, palette/gamma state, capabilities, graphics preferences, shaders, and level texture cache in deterministic order
- [ ] Publish the new context generation and `RENDERER_READY` only after all required resources succeed
- [ ] On partial failure, tear down the new generation completely and enter a bounded retry or explicit fatal-error path
- [ ] Add a debug-only context-loss injection command that runs on the render thread and is usable by automation

Tests:

- [ ] Injectable failure tests for every EGL and GLES initialization stage listed in BR-0251
- [ ] Resource-generation tests proving every registered GL resource rebuilds exactly once per context generation
- [ ] D1/D2 automation through gameplay, menus, automap, merged walls, MSAA, texture filtering, and diagnostic overlays before and after injected context loss
- [ ] Assert no stale GL handle is used and no recreation counter advances on failed recovery

Completion gate:

- BR-0251 can be marked resolved with failure-injection evidence
- only after this gate may a memory-pressure policy deliberately destroy the EGL context while retaining the game process

### Chunk 7: Process-death suspend-session restore

Goal: allow Android to kill a deeply sleeping single-player process without losing the last committed suspend point.

Tasks:

- [ ] Define a small versioned suspend-session record owned by native save logic: game id, callsign, checkpoint path, save digest or generation, commit time, and `resume_paused`
- [ ] Publish the record atomically only after the checkpoint itself is committed and validated
- [ ] Clear or supersede the record on normal game exit, abort, explicit load/new game, incompatible data change, or successful one-shot consumption as appropriate
- [ ] Distinguish a live same-process wake from a new-process task recreation using PID/process generation and the existing game activity state
- [ ] Reuse the existing pending resume/direct startup restore path instead of adding a second save parser in Kotlin
- [ ] Validate the record and save before launching; fall back to the launcher resume offer if validation fails
- [ ] On automatic restore, prevent simulation from advancing before a fresh pause window is created and foreground rendering is ready
- [ ] Make the restore token one-shot so recents/task recreation cannot replay it repeatedly
- [ ] Decide product policy separately: automatic transparent restore, explicit Resume button, or launcher offer only. Keep the underlying record compatible with all three

Tests:

- [ ] Force-stop or kill the `:game` process after checkpoint acknowledgement, then recreate the task for D1 and D2
- [ ] Verify restore reaches the same level, player state, mission, callsign, and paused state
- [ ] Test missing, truncated, stale, digest-mismatched, and already-consumed records
- [ ] Verify a menu-only background creates no gameplay restore record

Completion gate:

- process death after `CHECKPOINT_COMMITTED` produces either a validated paused restore or a clear launcher fallback, never a corrupt or silently advanced game

### Chunk 8: Foreground event-driven menus and paused games

Goal: stop unnecessary foreground redraws after the safer background work is complete.

Tasks:

- [ ] Add an explicit game-thread classifier for static menus and the dedicated single-player pause window
- [ ] Exclude movies, briefings, credits, transitions, end-level screens, automation, input-demo playback, multiplayer menus with live network work, and any screen with time-based animation
- [ ] Add a redraw-invalidated bit and optional next-animation deadline to the native window/event system on Android
- [ ] Wait for SDL input, Android axis-mailbox notification, automation/JNI command, network notification, surface change, redraw invalidation, or the next animation deadline
- [ ] Ensure every Kotlin/native input producer wakes the wait, including axis-only input that does not currently enqueue an SDL event
- [ ] Draw once when entering a static screen and again only after invalidation
- [ ] Keep menu music asynchronous and avoid waking the render loop merely to feed audio
- [ ] Start with a conservative low-rate throttle behind a debug preference if a fully event-driven screen lacks complete invalidation coverage

Tests:

- [ ] Hold each supported menu and pause screen unchanged and assert draw counters stop after the initial frame
- [ ] Exercise keyboard, touch, controller buttons, controller axes, text input, menu scrolling, slider repeats, automation, and surface resize from the waiting state
- [ ] Verify excluded time-driven screens continue animating at their intended rate
- [ ] Run D1/D2 controller-only and touchscreen menu regression scripts

Completion gate:

- every admitted static screen wakes on all supported inputs and does not redraw without invalidation
- no time-driven or network-sensitive screen is accidentally classified dormant

### Chunk 9: Rollout, regression matrix, and battery acceptance

Implementation order:

1. Chunk 0 diagnostics
2. Chunk 1 lifecycle coordinator
3. Chunk 2 durable checkpoint
4. Chunk 3 native park
5. Chunk 4 remaining wake sources
6. Chunk 5 nominal surface hardening
7. Chunk 6 context-loss recovery
8. Chunk 7 process-death restore
9. Chunk 8 foreground event-driven rendering

Each chunk should be independently reviewable and should leave the next deeper sleep tier disabled until its completion gate passes.

Required scenario matrix for D1 and D2:

- main menu and nested options menu
- active single-player level
- dedicated Pause-key window
- Escape game menu over single-player
- automap
- death and end-level states
- movie, briefing, and credits where applicable
- host, joiner, lobby/select-players, and active multiplayer
- short background, 60-second background, overnight-equivalent extended background, screen off, lock screen, rapid Home/resume, rotation/configuration change, low-memory trim, and killed process

Required validation per implementation chunk:

- [x] update or add one high-level integration test and run it to completion
- [x] run focused native/JVM unit tests
- [x] run scoped `android/run-code-quality.ps1 -Fix` for touched files
- [x] build the Android debug APK with JDK 21 for all configured ABIs
- [x] run both D1 and D2 background/resume automation when lifecycle, surface, graphics, or shared engine hooks change
- [x] run Windows D1/D2 builds when inherited engine files change
- [x] inspect exported Dormancy logs for transition order, checkpoint result, EGL generation, and recovery errors

Final deterministic acceptance criteria:

- background main-menu and single-player cases reach `PARKED` with no advancing event-loop, draw, swap, audio callback, producer-wakeup, sensor, or overlay-poll counters
- live multiplayer remains connected and advances required simulation/network counters while background rendering and local UI/audio work stop
- every successful wake uses the current native-window generation and a valid GLES3 context generation before drawing
- already-paused single-player stays paused across checkpoint, park, wake, and process-death restore
- failed checkpoint publication preserves the previous valid save and reports a terminal result
- injected surface and context failures never publish `RENDERER_READY` prematurely and never use stale GL handles
- ordinary surface-only wake does not reload gameplay state or unnecessarily rebuild the context
- no lifecycle callback traverses engine-owned window structures from the Android UI thread

Final physical-device battery acceptance:

- collect the same-duration idle samples before and after implementation with charging disconnected and the same device/display/network conditions
- require zero engine-frame progress after park acknowledgement
- export the category-controlled Dormancy log after each run and compare app-owned event, draw, swap, audio, producer, overlay, lifecycle-generation, and graphics-generation deltas
- require background counter rates to remain flat after park, except for explicitly allowed multiplayer networking work
- retain the exported Dormancy logs and summarized results in this plan or a linked dated validation report

# Android dormant-state power audit

## Goal

Determine why the game process may continue consuming power while minimized or idle on a menu, and define a safe Android-specific sleep policy for menus and paused single-player games without disrupting multiplayer or other platforms.

## Plan

- [x] Read repository instructions and locate Android lifecycle, polling, rendering, audio, and multiplayer keep-alive paths
- [x] Trace native lifecycle calls and establish which work continues after `onStop`
- [x] Separate background, foreground-menu, paused-single-player, and multiplayer states and identify safe suspend boundaries
- [x] Recommend a phased implementation and verification strategy, including battery/process diagnostics

## Findings

- `MainActivity.onStop()` stops gyro and Kotlin overlay polling, releases input, and calls `nativeOnPause()`
- The existing comment says `nativeOnPause()` injects Escape to open the pause/game menu; this is game-state pausing, not yet evidence that the native render/event loop sleeps
- The activity unconditionally sets `FLAG_KEEP_SCREEN_ON` during creation
- `android_surface_pause()` only makes the framebuffer blit/EGL swap return early. `event_process()` still polls, dispatches draw events, calls `gr_flip()`, and repeats
- Static menus and the pause window rate-limit their idle handler to 50 FPS, so an invisible game can continue walking the full native draw path about 50 times per second even though presentation is skipped
- Background music and redbook producer threads remain alive and poll their paused flags every 20 ms; the SDL audio device is not suspended by the lifecycle call
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

Tasks:

- [ ] Add a shared Android lifecycle diagnostic snapshot with visibility, workload, suspend state, request generation, acknowledgement generation, window generation, context generation, and last transition reason
- [ ] Add coarse counters for native event-loop iterations, draw dispatches, swaps attempted, swaps presented, audio callbacks, music producer wakeups, redbook producer wakeups, central overlay polls, and independent overlay polls
- [ ] Expose the snapshot through the existing introspection JSON in D1 and D2
- [ ] Log lifecycle state changes and one before/after counter summary through `debug_log(DLOG_GAME, ...)`; do not log each loop or callback
- [ ] Extend the automation runner's existing `SCRIPT_BACKGROUND` support so it can vary background duration and optionally lock the screen
- [ ] Add `android/game_scripts/test_background_dormancy_unified.json5` with entry points for main menu, active single-player, pause window, and game menu
- [ ] Record an emulator baseline and at least one physical-device baseline using `dumpsys cpuinfo`, per-thread `top`, and `dumpsys batterystats`

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
- [ ] Replace `nativeOnPause()` engine-global and `window_get_front()` inspection with a request-only JNI entry point
- [ ] Preserve the immediate surface safety fence on the UI thread by keeping `android_surface_pause()` under the surface bridge mutex before Android can destroy the window
- [ ] Add a game-thread lifecycle tick at a stable boundary before simulation or draw work
- [ ] Classify `NO_LEVEL`, `STATIC_UI`, `SINGLE_PLAYER_ACTIVE`, `SINGLE_PLAYER_PAUSED`, `TIME_DRIVEN_SCREEN`, and `MULTIPLAYER_ACTIVE` from game-thread-owned state
- [ ] Identify the pause window by callback on the game thread; do not publish or retain the raw window pointer across threads
- [ ] Convert minimize Escape injection into a game-thread action, or replace it with direct lifecycle pause ownership where possible
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

### Chunk 2: Durable single-player suspend checkpoint

Goal: make a background single-player checkpoint a completed transaction, not a best-effort flag.

Tasks:

- [ ] Add checkpoint phases `NOT_NEEDED`, `REQUESTED`, `WRITING`, `COMMITTED`, `SKIPPED`, and `FAILED` to the coordinator
- [ ] Define save eligibility on the game thread for ordinary level play, death state, secret levels, final-boss sequences, demo playback/recording, and other existing blockers
- [ ] Keep multiplayer excluded from the single-player checkpoint path
- [ ] Call the save writer from a stable game-thread boundary without relying on a future generic `EVENT_IDLE`
- [ ] Preserve whether simulation was already paused and whether the pause window was frontmost
- [ ] Stop time under explicit lifecycle ownership and release only the pause ownership acquired by the coordinator
- [ ] Remove the need to close the dedicated pause window merely to deliver the autosave request
- [ ] Decouple minimize thumbnail creation from the disappearing surface by using a cached last-presented gameplay thumbnail or a documented blank-thumbnail fallback
- [ ] Write the new checkpoint to a temporary path, close and validate it, then publish with `PHYSFSX_rename()` using the existing transaction patterns in `playsave` and coop level restart
- [ ] Preserve the previous valid minimize checkpoint until the replacement has been validated and published
- [ ] Publish checkpoint result, slot, path, and request generation in introspection and Game Logs
- [ ] Do not enter `PARKED` until the coordinator observes `COMMITTED`, an explicit safe `SKIPPED`, or a bounded failure policy

Tests:

- [ ] Native tests for eligible save, already-paused save, every skip reason, failed open/write/close/rename, and preservation of the previous checkpoint
- [ ] Kill or fault injection at each publication phase to prove the prior or new checkpoint remains readable
- [ ] D1 and D2 automation that backgrounds from active play and from an existing pause window, then verifies simulation time and position did not advance
- [ ] Verify checkpoint metadata, thumbnail policy, callsign repair, D2 secret companion handling, and launcher candidate selection

Completion gate:

- every suspend generation has one terminal checkpoint result
- a successful acknowledgement means the final checkpoint is closed, validated, and durably published
- an already-paused game remains paused throughout the transaction and after wake

### Chunk 3: Native engine park and wake

Goal: eliminate the background event/draw loop without releasing the EGL context.

Tasks:

- [ ] After checkpoint resolution, stop frame submission and transition `QUIESCING -> PARKED` on the game thread
- [ ] Wait on a condition variable with a predicate loop, not `SDL_Delay` polling
- [ ] Make resume, quit, fatal shutdown, surface replacement, and debug automation capable of waking the parked thread
- [ ] Drain or reset stale touch, key, mouse, joystick, gyro, and axis-mailbox state before the first resumed frame
- [ ] Keep timer accounting frozen while parked so the first resumed frame does not receive the entire background duration
- [ ] Require both foreground visibility and a usable current surface generation before leaving `WAKING`
- [ ] Keep multiplayer simulation unparked; continue to suppress drawing while the Activity is hidden
- [ ] Define what happens if a multiplayer session ends while backgrounded: reclassify on the game thread and park once no live session requires progress
- [ ] Expose park entry count, wake count, parked duration, and wake reason through introspection

Tests:

- [ ] Native lost-wakeup stress test with request/resume races and repeated generations
- [ ] Background main menu and single-player tests asserting event-loop, draw, and swap-attempt counters stop advancing after acknowledgement
- [ ] Resume tests asserting exactly one wake for one generation and no large first-frame time step
- [ ] Multiplayer background test asserting network/simulation counters continue while render counters stop

Completion gate:

- a background non-multiplayer process has no advancing engine frame counters after `PARKED`
- 100 rapid Home/resume cycles do not deadlock, miss a wake, double-resume time, or use a stale window

### Chunk 4: Audio, sensors, UI polling, and screen policy

Goal: remove the remaining Java and audio wake sources after the engine parks.

Tasks:

- [ ] Add one `suspendUiWork()` and `resumeUiWork()` lifecycle boundary in `MainActivity`
- [ ] Stop and later restore the central overlay poller, coop stats, warp status, video diagnostics, network stats, network events, music controls, touch drain handlers, and any other self-posting `Handler` callbacks
- [ ] Make every overlay poller idempotently startable/stoppable and add a common testable policy helper
- [ ] Pause gyro and other sensors, release mixed input state, and cancel pending long-press/double-tap callbacks
- [ ] Clear `FLAG_KEEP_SCREEN_ON` when backgrounded and restore it only while the game Activity is foreground and policy requires it
- [ ] Suspend the SDL audio device so its callback stops instead of repeatedly emitting silence
- [ ] Replace the MIDI and redbook producer threads' 20 ms paused loops with condition-variable waits
- [ ] Keep background pause separate from the user's music pause setting, current track, decoder position, and looping state
- [ ] For background multiplayer, stop local audio output and nonessential UI work while leaving required network/game progress active

Tests:

- [ ] JVM tests for the complete poller registry and keep-screen-on policy
- [ ] Native audio tests for background pause, user pause plus background pause, resume ordering, quit while paused, and no lost wake
- [ ] Introspection assertions that audio callback and producer-wakeup counters stop after suspend and resume from their prior logical state

Completion gate:

- all known recurring Handler, sensor, SDL audio, MIDI, and redbook callbacks stop in background non-multiplayer states
- background and user-controlled music pause states remain independent

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

- [ ] update or add one high-level integration test and run it to completion
- [ ] run focused native/JVM unit tests
- [ ] run scoped `android/run-code-quality.ps1 -Fix` for touched files
- [ ] build the Android debug APK with JDK 21 for all configured ABIs
- [ ] run both D1 and D2 background/resume automation when lifecycle, surface, graphics, or shared engine hooks change
- [ ] run Windows D1/D2 builds when inherited engine files change
- [ ] inspect downloadable Game Logs for transition order, checkpoint result, EGL generation, and recovery errors

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
- require the non-multiplayer background process to settle to near-zero CPU outside OS bookkeeping and scheduled diagnostic sampling
- review partial wakelocks, foreground services, alarms, jobs, network traffic, and per-thread CPU so an apparent improvement is not hiding work in another process
- retain raw measurement commands and summarized results in this plan or a linked dated validation report

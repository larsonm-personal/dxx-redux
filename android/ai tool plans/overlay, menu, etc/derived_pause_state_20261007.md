# Derived Android pause state and recovery

## Objective

Make the engine thread authoritative for simulation pause and local input gating, publish a coherent UI snapshot, and recover from abandoned pause state without requiring a matching release by the original caller

## Requirements and implementation sequence

1. Audit native pause paths, UI transitions, lifecycle suspension, graphics confirmation, and co-op waits; preserve unrelated worktree edits
2. Introduce a shared Android coordinator that derives persistent reasons from current subsystem state, accepts serialized UI intent, and publishes state plus request acknowledgement
3. Replace Kotlin pause ownership flags with complete modal demand and explicit native actions; show actual pause and whether Resume is available
4. Remove anonymous game-window pause ownership on Android; keep legacy synchronous operations visible and reconcile orphan state only at proven quiescent outer-loop boundaries
5. Cover graphics, background, native pause/menu, save/load, and co-op blockers without blindly clearing a live operation or multiplayer barrier
6. Scope requests to activity/game generations, support explicit Resume and full-state reconciliation, and publish before background parking
7. Extend introspection and meaningful integration tests for overlap, missed close/repeated requests, native-to-overlay feedback, stale-session commands, recovery, resume restrictions, and startup-resume reload in both engines
8. Run scoped formatting, relevant native/JVM builds and tests, catalog checks, emulator integration, and available device verification

## Acceptance evidence

- Effective simulation and input decisions use the same state reported to the UI
- No pause counter mutation or native-window inspection from Kotlin's UI thread through the pause interface
- Closing one of several overlays does not resume gameplay prematurely
- Resume clears recoverable user state; live graphics/lifecycle/co-op operations remain protected with a visible reason
- A forgotten legacy pause is diagnosed and repaired at a quiescent boundary, with no arbitrary timeout of active work
- Previous-session/activity callbacks cannot alter the current session
- Regression coverage runs to completion for D1 and D2; existing pause/save/lifecycle/graphics behavior remains covered

## Initial findings

- nativeIsGamePaused reports the overlay flag and the front pause window, omitting the actual engine counter
- Overlay JNI methods mutate the native counter directly; Kotlin separately tracks adminTrayPausedGame
- Native game-window activation consumes anonymous counter depth, while graphics and co-op code also use that counter for persistent holds
- Engine code uses nested event loops and longjmp, so orphan repair must be restricted to an explicit outer-loop safe point
- Existing unrelated edits: GraphicsConfirmationOverlay.kt and video-overlay-immediate-graphics-20261007.md

## Implemented architecture

- `android_pause.c` is the engine-thread authority. Persistent reasons are derived from live native windows, complete Kotlin modal state, lifecycle visibility, graphics state, and co-op transition phases
- Simulation pause and local input permission are distinct: local UI blocks input in multiplayer while the shared simulation continues; coordinated transitions still freeze the world
- Kotlin publishes its complete current modal mask through a mutex-protected mailbox and periodically reconciles it. The engine publishes one coherent snapshot with reasons, effective simulation/input state, Resume availability, session, consumed UI revision, request acknowledgement, and recovery count
- Activity ownership and session generations reject obsolete callbacks and requests. They do not own an irremovable pause: replacing/detaching an activity drops its demand, missed close callbacks heal through full-state publication, and session changes dismiss stale UI
- Resume, native menu, save/load, and quick-load use serialized engine-thread requests. UI dismissal and its final modal mask accompany the action atomically
- Legacy counter holds remain only for synchronous work. The actual inferno outer loop diagnoses and clears orphaned depth after that work has returned or unwound; nested event loops never perform this repair. Resume cannot bypass a live operation, graphics confirmation, background state, or co-op barrier
- Background resume schedules a session-scoped menu action. Native menus ignore unmatched left-button releases, preventing the overlay's synthetic input release from immediately dismissing a new menu
- The restore barrier retains its frozen phase through sending RUN and completing local release, so deriving pause from phase preserves co-op clocks
- Introspection exposes the native snapshot and Kotlin demand/acknowledgement. The high-level regression injects orphan state and a missed close notification, tests overlapping modals, native pause feedback, duplicate Resume, stale requests, protected operations, save-menu handoff, and graphics confirmation

## Verification

- Scoped mixed-language formatting/lint: passed (`temp/pause_format_final.log`)
- Isolated x86_64 diagnostic APK build and JVM tests: passed, 187 suites / 1,143 tests, zero failures/errors (`temp/pause_final_verified_build.log`)
- ARM64 native builds for D1 and D2: passed (`temp/pause_arm_final_build.log`); only existing fixed-size string initializer warnings
- Final combined pause regression: D1 and D2 passed, including startup-resume / abort / `[auto] best` reload (85/85 D1 and 86/86 D2 fixture steps), zero remaining pause depth, and successful orphan repair (`temp/pause_integration_final.log`, `android/temp/pause-state-20261007-112604/results.json`)
- Lifecycle background/resume and lock-screen cycles: D1 and D2 passed 32/32 steps (`temp/pause_lifecycle_d1_final.log`, `temp/pause_lifecycle_d2_final.log`)
- Co-op D2 countdown save/restore, delayed peer, packet retries, clock freeze/release and restored player inventory: passed (`temp/pause_coop_d2_final.log`)
- Automation catalog and master-suite catalog: passed (`temp/pause_catalog_final.log`, `temp/pause_suite_catalog_final.log`)
- S25 hardware pause suite: D1 and D2 passed on SM-S931U / Android 16 (API 36), serial RFCY703C48J, using the separate `com.dxxredux.app.nsdtest` package. Evidence: `temp/pause_s25_integration.log`, `android/temp/pause-state-20261007-114328/results.json`
- Physical test support uses the shared isolated-package guard instead of an emulator-only guard
- S25 lifecycle testing exposed a harness issue: `logcat -t 400` omitted the second background marker amid noisy device logs. The monitor now reads all matching tagged records and retains existing duplicate suppression; helper process/wait tests pass (`temp/pause_s25_helper_tests.log`)
- Build outputs were isolated under `temp/pause-isolated-build` / `temp/pause-isolated-cxx` because concurrent graphics work uses the normal Gradle output directories. Unrelated graphics and documentation changes were preserved

- S25 lifecycle rerun: D1 and D2 each passed 32/32 steps, including two Home/return cycles, menu pause feedback and input restoration (`temp/pause_s25_lifecycle_d1.log`, `temp/pause_s25_lifecycle_d2_retry.log`). The temporary physical fixture omits `lock_screen=true` because this phone has a secure keyguard; lock-screen coverage remains emulator-only
- S25 diagnostic ARM64 APK build passed (`temp/pause_phone_build.log`); physical-runner formatting and catalog validation passed (`temp/pause_phone_format.log`, `temp/pause_phone_catalog.log`), and harness formatting passed (`temp/pause_s25_format.log`)


## Screen-saver / display-off follow-up

- Dedicated `test_idle_screen_saver.jsonc` passed 69/69 steps in both games on emulator-5582, including saver entry, display lock/off, foreground wake, touch/controller dismissal, pause exit and audio restoration (`temp/pause_idle_d1.log`, `temp/pause_idle_d2.log`)
- This does not establish that the reported older physical-phone black screen is fixed. S25 lifecycle tests covered Home/resume, not secure lock/unlock
- The saver intentionally preserves its black dimmed overlay on OS wake until user input dismisses it; the underlying game can remain normally paused afterward
- Code audit found an independent rendering-recovery gap: an `eglSwapBuffers` failure does not invalidate the current window generation or force recreation. Its graphics-safety failure handler acts only on an active graphics trial. A failed swap with unchanged surface generation could therefore keep retrying an unusable surface/context outside a trial. This is a plausible mechanism, not a reproduced cause of the user's older incident, and is not fixed by pause reconciliation

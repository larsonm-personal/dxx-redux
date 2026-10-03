# Retroid demo launch, MSAA rollback and confirmation styling

## Plan

- [x] Preserve ADB logcat and exit history before reproducing device failures
- [x] Diagnose the demo black screen and immediate MSAA rejection using targeted logs, avoiding speculative renderer changes
- [x] Match the graphics confirmation to shared Kotlin overlay cards/buttons, with a filled selection and thick green controller outline
- [x] Build both Android engines, run confirmation/input integration coverage serially and inspect the resulting overlay
- [x] Run scoped mixed-language quality checks and record evidence and remaining limits

## Initial evidence

- Installed Retroid package `com.dxxredux.app`, versionCode 23730, is not debuggable; private logs and introspection cannot be read through `run-as`
- Captured logcat and Android exit history in `temp/retroid-current/`
- Earlier game process 8022 was killed for low memory at device timestamp 15:57:01, with the low-memory killer reporting 6542632 kB RSS and 224900 kB swap; keyboard-hide calls repeated beforehand
- Later D2 launches loaded the demo assets and reached gameplay; repeated MSAA attempts report RGB10_A2 window bits but no created framebuffer message
- Requested export of the latest private debug log through Advanced > Debug Logging > Save to identify the durable renderer rejection reason

Do not edit `android/outstanding_bugs.md` or unrelated pending packaging changes

## Confirmed MSAA failure and fix

An isolated debug APK reproduces the immediate rollback on the physical Retroid with the exact Mac preview assets. The initial GPU capability report records RGBA8 (32856), with no query errors and shared color/depth sample counts 4/8/16. At the first MSAA allocation, the actual default framebuffer reports RGB10_A2 (32857). The durable renderer failure is `msaa_window_format_changed`. The before-fix script verifies that rejection and full rollback to MSAA off (22/22 steps)

The shared allocator now refreshes capabilities when the actual window format differs, before selecting the supported color/depth sample count. Unknown formats, invalid queries, unsupported sample counts, allocation and resolve failures still reject the candidate. No device-name special case or automatic MSAA acceptance was added

The fixed cold Mac preview run refreshes to RGB10_A2 and successfully creates and resolves MSAA. `test_mac_d2_demo_graphics.jsonc` uses a separate `mac-demo-graphics` asset set and checks cold launch, 2x default Cancel, 4x controller OK, owned-target/window pixels, GL errors and timeout rollback. Its five hashes are now included in the generated game data index

## UI changes

Graphics confirmation and quick-load confirmation share the rounded card and button drawable helpers in `PauseOverlayStyle`. Both use bold white button labels, a filled selected state and a 3 dp green outline matching Video Info's controller focus color. Default Cancel, the independent five-second deadline, input ownership and restoration behavior remain intact. Disabled controls are dimmed

Physical screenshots were inspected for both Cancel and OK selection: `temp/retroid-current/confirmation-cancel.png` and `confirmation-ok.png`

## Verification

- `:app:assembleDebug -PciApk=true`: both engines and all three ABIs pass, including the native allocator fix
- `:app:testDebugUnitTest -PciApk=true`: 1114 tests, zero failures/errors
- Physical Mac preview regression through the standard runner: 41/41 steps
- Physical `test_msaa_render_and_menu.jsonc`: D1 92/92 and D2 122/122 in each of requested RGB565 and RGBA8888 modes
- Physical `test_graphics_confirmation_input.jsonc`: D1 94/94 and D2 94/94, including controller, stick, held-input and touch cases
- Scoped mixed-language formatting/lint and `git diff --check` pass

Tests used the separate `com.dxxredux.app.ci` installation. The regular release package was not replaced or reset. The fixed CI APK is installed for review. Temporary USB stay-awake and graphics log-tag overrides were restored to their original values (0 and unset)

Evidence and build/test logs are in `temp/retroid-current/`. An initial scratch test stopped too early in the start-level menu and another asserted dismissal before asynchronous restoration completed; the corrected before/after scripts and final checked-in test pass. One screenshot attempt ran after display sleep and was repeated with temporary USB stay-awake enabled. These attempts are not counted as renderer failures

## Memory investigation limit

The production app's earlier failure is a confirmed low-memory termination, not an observed native crash or ANR. Android's exit history records 1.9 GB PSS/RSS at its earlier sample; the low-memory killer reports roughly 6.2 GiB RSS at termination, after killing other processes and exhausting swap. Retained logcat starts after startup and shows repeated keyboard-hide calls, so it does not identify the original allocation source or establish whether memory growth caused the initial black output. Later production gameplay uses roughly 210 MiB PSS; fresh Mac preview test launches also succeed

The memory-growth cause remains unresolved. No speculative memory/keyboard patch was made. The production release prevents `run-as`, native stack collection and debug introspection; private log export was requested, but no exported log was available during this task

## First-launch guidebot hypothesis

The retained system-buffer timeline distinguishes the metadata worker from the failing game process. D2 metadata worker PID 7980 starts at 15:55:41.580 and exits with SIGKILL at 15:55:43.432, immediately as game PID 8022 starts at 15:55:43.417. No replacement metadata worker starts during that game session; the next starts after the OOM at 15:57:02.453. The 6.2 GiB RSS therefore belongs to the game, not a running guidebot precompute worker

This matches `RouteMetadataPrecomputeCoordinator.stopForGameLaunch`, which cancels launcher work and explicitly kills metadata worker processes. Android level loading calls `secret_area_prepare_current_level`, which scans with expensive route planning disabled; a cache miss delegates the full computation to the isolated worker. This makes runaway first-launch guidebot precomputation unlikely for this incident. It does not exclude a defect elsewhere in the game's metadata handling, nor identify the allocation source. The repeated keyboard-hide/menu callbacks are another investigation lead, not proof of a leak

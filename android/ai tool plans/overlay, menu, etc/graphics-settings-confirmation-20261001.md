# Android graphics settings confirmation

Status: implemented and verified on the Retroid Pocket 4 Pro and emulator; MSAA format mismatch fixed

## Completed Retroid diagnosis and validation

The Retroid's EGL window has 10/10/10/2 color channels. The renderer incorrectly selected an RGBA8 multisample source, so the direct window resolve failed with `GL_INVALID_OPERATION` and left the background undrawn. A shared channel-size mapping now selects `GL_RGB10_A2` for that window in both production and diagnostic code. The same physical probe changes from failed window resolve to correct pixels with no GL errors; no EGL-config, clipping, synchronization, or device-specific rendering workaround was needed

Physical D1 and D2 tests pass with requested RGB565 and RGBA8888, 2x/4x trials, cold startup, live changes, native menus and background/resume. The GPU selects a 10/10/10/2 window in both requested color modes and allocates 4x for a 2x request. D2 rear-view and missile-camera composition preserve all sampled scene markers. The FOV-aware scene probe excludes the deliberately replaced simulation pass before observing the final visual pass; this fixes a false diagnostic failure at the user's 100-degree FOV without changing game rendering

The physical failure-before-fix test verifies immediate structural-failure rejection and full accepted-settings rollback. After the fix, an unattended 4x trial restores accepted 2x. A separate responsive black-frame trial visibly retains the Kotlin modal with `Cancel (5)` selected and restores accepted 4x after timeout. Device screenshots show the rendered D1/D2 scenes and the independent Kotlin confirmation over black output; short screen recordings are retained as supplementary evidence. The user's subjective observation was requested but is not assumed from silence

Evidence is in `android/temp/retroid-msaa-20261002-073724/`: before/after owned probes and production introspection, per-case scripts/results, D1/D2 mode and resume snapshots, subview results, screenshots, recordings, and `final-diagnostics.tar`. Temporary D1 shareware data used a separate game-data set. All 40 files in the original app-data backup, including every original save, pilot and preference file, were restored byte-for-byte and checked by SHA-256 (`restore-verification.json`). The temporary set/pilot were removed, USB stay-awake was reset to its original value, the original MSAA-off tuple is accepted, and the fixed standard debug APK remains installed

Final Android and Windows builds pass for both engines. Scoped mixed-language formatting/lint passes. The final emulator matrix passes D1 108/108 and D2 138/138 steps under both requested color depths (`android/temp/msaa-render-20261002-080551/`), including custom-FOV scene coverage. Earlier host/JVM and confirmation-specific integration evidence below covers persistence, input, launcher/menu timing, watchdog and lifecycle behavior. Non-rendering replay exclusion is established by the audited call paths because ordinary Android replay does not expose that mode; no new replay product feature was added for this test

## Implementation evidence and remaining work

The shared durable store and multi-key config transaction have host coverage for explicit acceptance, deadline races, premature/stale OK, all-off exemption, full mirrored rollback, unrelated-setting preservation, abandoned attempts and publication failures. Both host suites pass. The standard `:app:assembleDebug` build passes for both engines and all three configured ABIs, including Kotlin compilation. Scoped code quality checks pass after correcting the initial Kotlin formatting violation

The initial implementation adds checked EGL initialization/recovery, actual EGL configuration logging, bounded MSAA binding/clip traces and structural-failure rollback. These source changes are not evidence that the physical Retroid flicker is fixed

The new `test_graphics_settings_confirmation.jsonc` passes serially on D1 and D2 using emulator-5554. It exercises real controller dispatch for default Cancel via A and explicit OK via D-pad/A, then timeout and all-off behavior. The serial runner is `android/helpers/run_graphics_safety_tests.ps1`, which selects a device explicitly and records test output. D2 evidence: `android/temp/graphics-safety-20261001-162250/results.txt`. D1 evidence: `android/temp/graphics-safety-20261001-162425/results.txt`

Launcher and engine now share the file-only `libdxx-graphics-safety.so` service, including the process mutex and cross-process file lock. Launcher edits during a challenge are journaled as individual changed fields and published after the trial resolves; they merge into the accepted rollback target rather than inheriting other rejected values. Process creation times distinguish active owners from reused PIDs. Host coverage exercises both behaviors, including live foreign-owner recovery refusal and failed deferred batch publication

The runtime now exposes accepted/current/requested/queued snapshots. It polls deferred publication after a trial, notifies Kotlin to queue published live options, and preserves next-start resolution/color-depth requests. Resumed games reread actual config instead of relying on the cross-process preferences generation hint. Video Info polling preserves queued values during the debounce, and launcher controls update selection only after successful storage

Latest validation: all 53 native host suites and all 1,101 Android JVM tests pass. Standard `:app:assembleDebug :app:testDebugUnitTest --console=plain` passes for both engines and all three ABIs. The APK contains the new service for all three ABIs; the engine ELF explicitly depends on it. Scoped mixed-language quality passes. Expanded confirmation integration passes serially on emulator-5554: D2 `android/temp/graphics-safety-20261001-171051/results.txt`, D1 `android/temp/graphics-safety-20261001-171222/results.txt`, each 74/74 native steps. These runs include changing back within the debounce, combined filtering/AF edits, suppression of an A button held before the prompt, and actual cross-process launcher edits during a challenge. Cancelling restores the accepted tuple before publishing the deferred fields; subsequent live acceptance does not validate the launcher's unapplied resolution/color-depth request

The new `test_graphics_mode_restore.jsonc` also passes serially on emulator-5554: D2 `android/temp/graphics-safety-20261001-172408/results.txt`, D1 `android/temp/graphics-safety-20261001-172823/results.txt`, each 63/63 native steps. It covers launcher 800x600/RGBA requests challenged only at level start, B restoring accepted 640x480/RGB565 in-level, a combined native resolution/filtering edit remaining unchallenged through nested menus until the outer menu closes, timeout restoring the complete tuple with balanced game-time pause, and all-off resolution changes remaining exempt without replacing accepted values

The first mode-restore run exposed eager level-texture recaching taking longer than the one-second restore watchdog after successful EGL recreation. Rollback now leaves those textures to the existing lazy upload path; both new mode runs restore in-level without closing the game. Dimensions, context recreation and pause state are verified, but those assertions do not replace pixel/resource checks on the physical device

The opt-in `msaa_color_probe` automation action is implemented in shared `ogl_msaa_probe_android.cpp/.h`. It queries actual EGL surface dimensions, window channel sizes/samples and per-format sample support; creates owned matching-format color/depth MSAA targets; and checks four known opaque quadrant-center pixels in direct offscreen/window controls and both resolve destinations. Each destination is cleared to a sentinel before resolving. It preserves framebuffer/renderbuffer bindings, scissor, clear color, color masks and all pixel-pack state it changes, then verifies exact state restoration. It reports operation/readback errors separately and preserves the candidate/accepted/requested snapshots in introspection. Start/result batches use the forced batch logger so full diagnostic JSON survives the ordinary 1,024-byte formatted-message limit

The new `test_msaa_render_and_menu.jsonc` and `android/helpers/run_msaa_render_tests.ps1` pass serially on emulator-5554 for D1 and D2 under both requested color depths, each 61/61 steps. Evidence: `android/temp/msaa-render-20261001-174746/rgb565.txt` and `rgba8888.txt`, with two passes in each file. The test tries 2x then 4x, explicitly accepts each trial, probes during gameplay, with the native Options menu open and after returning to gameplay, and verifies the all-off exemption preserves the accepted 4x tuple. The captured final D2 log `debuglog.txt` contains all six full probe start/result batches and 568 trace events. These probes execute on owned targets before the next engine frame; they prove those pixel transfers, not preservation of an already composed native menu or physical screen presentation

This fixture resets launcher/game state and uses the generic runner's save cleanup, so it is for provisioned test devices. Add a live capture path for an already running Retroid level that avoids pilot/save cleanup, or preserve and restore user data before using the fixture on a personal device. Hardware availability alone does not authorize removing those files

The production trace now shares a flip ID and ordered event index across first color/depth clear, scene-pass completion, native menu preparation, resolve and composition before swap. Its 120-frame budget decrements once per flip rather than once per event; GL queries remain gated by graphics logging. Structural failure captures its configuration evidence before deciding rollback, including a matching durable ATTEMPT before the visible challenge is armed. Native resolution menus return after failed Android mode setup instead of rebuilding buffers for a mode that failed. The integrations below verify both pre-armed callback failure capture and actual mode initialization failure after old-context destruction

The latest standard `:app:assembleDebug --console=plain` build passes for both engines and all three ABIs with the probe, trace and mode-failure guard. Scoped C/C++ and PowerShell quality checks pass. Fresh mode-restore runs after the guard pass 63/63 steps in each engine: D2 `android/temp/graphics-safety-20261001-175030/results.txt`, D1 `android/temp/graphics-safety-20261001-175218/results.txt`

Both device runners now pass the planned directory to artifact retention rather than a file inside a nonexistent directory. The MSAA runner uses a child PowerShell process so host-stream test diagnostics are captured. Old timestamp paths above describe historical passes; retention may remove their raw files while this plan preserves the result summary

The introspection-only `graphics_stall_once` fault consumes a bounded render-thread stall after Kotlin has armed the trial and the engine has applied its candidate. It sleeps outside record/config locks. `run_graphics_recovery_tests.ps1` verifies timeout writes the full accepted tuple to every mirrored config while that stalled process is still alive, followed by bounded process termination, an actual launcher error dialog, and a fresh level using accepted settings without another prompt. It also tests an unreadable record and blocked atomic publication, restoring the exact original permissions in a finally block. These fixtures are restricted to provisioned emulators

Storage fault injection exposed two actual launcher recovery defects, now corrected: the recovery Intent needs CLEAR_TOP/SINGLE_TOP to deliver its message to the existing launcher when the fatal-error file cannot be published, and metadata diagnostic status/log writes must tolerate IOException instead of crashing the launcher. The new unavailable-storage JVM regression passes with all eight metadata monitor tests. Standard `:app:assembleDebug` passes for both engines/all three ABIs. Both Windows engines also build successfully through `run-windows-build.ps1`, including the engine and headless/metadata/test targets

D1 recovery passes for stalled rendering (7,633 ms from the trigger including debounce and the five-second deadline plus one-second watchdog), unreadable record (2,205 ms), and blocked publication (1,060 ms). Evidence: `android/temp/graphics-recovery-20261001-182430/`. The blocked-publication log explicitly records EACCES on the fatal-error file and metadata status file, while the launcher survives and delivers the recovery message. The first D2 full-matrix attempt found a test race: the durable restore marker becomes visible before the subsequent mirrored config publication completes. The runner now waits for publication before asserting the original process is still alive; this does not change the implementation's persistence ordering

The corrected runner passes all three D2 cases: stall 7,717 ms, unreadable record 1,597 ms, blocked publication 1,609 ms. Evidence: `android/temp/graphics-recovery-20261001-183015/results.json` and the per-case fixture, record and logcat files. Together with the D1 cases this verifies all six combinations, including fresh-process repair and accepted gameplay without a prompt. Results are now saved after each successful case so a later failure cannot discard the completed-case summary

The new `android/helpers/capture_msaa_live.ps1 -Serial <adb-serial> -Samples 2` targets an already running debug level. It neither launches a game nor runs reset/save/pilot cleanup or edits graphics settings. It runs an owned-target probe, saves before/after introspection, logcat and the full native debug logs, and checks that the accepted record and all mirrored config texts are unchanged. The probe briefly draws diagnostic colors into the window before the next engine frame. It captures a diagnostic failure rather than requiring the Retroid probe to pass. This owned-target probe still does not test actual composed menu pixels or physical presentation

Live capture passes on the already running D2 emulator level for 2x (`android/temp/msaa-live-20261001-183444/`) and 4x (`android/temp/msaa-live-20261001-183504/`). Both probes pass all owned-target pixel stages and exact GL state restoration, retain the same game PID, and leave the accepted record and all three config texts unchanged. The full forced native probe JSON is retained in the downloaded debug log. Scoped quality checks pass for the new capture/recovery runners and recovery fixes. Only emulator-5554 and emulator-5556 are visible to adb; no physical Retroid evidence has been collected

Responsive black-output injection is now implemented and verified. `graphics_black_once` explicitly enables a fault for the next armed trial; the shared EGL swap hook clears the complete window to opaque black immediately before swapping, without stopping events. It verifies a black window pixel, checks all changed GL state is restored, and records total/valid frame counts. It stops when the trial resolves. These fault APIs require INTROSPECT_ON and explicit automation; normal rendering performs no fault GL work

`test_graphics_black_output.jsonc` passes 51/51 steps in each engine through `android/helpers/run_graphics_black_output_tests.ps1`. D1 evidence: `android/temp/graphics-black-20261001-184750/`, 22/22 valid black frames. D2: `android/temp/graphics-black-20261001-185006/`, 29/29 valid black frames. The independent host observer matches the durable challenge ID before checking that Kotlin's modal is shown and attached, preventing stale native introspection from producing a false result. The normal modal's onDraw handshake is what arms these challenges. Both tests let the five-second timer expire, restore the full accepted/current/requested tuple in the same process, balance the game-time pause, and pass the known-color probe afterward. All protected fields in all three mirrored configs match accepted values; the D1 configs were checked independently after its completed run, and D2 verifies them in the runner

The same integration opens the native Options menu, changes filtering, and injects `graphics_renderer_fail` while the durable attempt is still phase 1 and no gameplay challenge is eligible. The retained `renderer_failure` contains the attempted TexFilt=2 candidate, accepted TexFilt=1 tuple, trial ID, failure reason and attempt phase after rollback. Closing the menu returns to accepted gameplay without a new prompt. This callback injection verifies coordination/evidence retention, not a physical driver failure or a failed EGL creation call

Standard `:app:assembleDebug` passes with these hooks; all six D1/D2 libraries in the APK across the three ABIs contain the new automation hook. A fresh `run-windows-build.ps1 -Target both` check also exits successfully. Scoped mixed-language formatting/lint and whitespace checks pass. Two initial fixture assertions were corrected after inspecting current state: completed restore records clear their active reason, and the native Options label is `menu.subtitle = Options:` rather than `menu.title = Options`. Those fixture failures do not establish an implementation failure

Actual EGL initialization fault injection is now implemented through `graphics_egl_fail_once`. The one-shot failure occurs inside the real shared initialize function after its caller has resized the screen canvas and destroyed the old context. Review identified a recovery defect: Game_screen_mode remains at the old accepted mode on that failure, so tuple equality alone could skip the required renderer rebuild. Restoration now also checks the actual canvas dimensions and whether an EGL context/draw surface is current. A damaged or incomplete renderer triggers accepted-mode reconstruction even when the logical mode tuple is unchanged. The diagnostic color probe now reports `context_unavailable` without issuing GL queries if no usable context is current

`test_graphics_egl_failure.jsonc` passes 54/54 steps in both engines via the existing graphics safety runner: D1 `android/temp/graphics-safety-20261001-185840/results.txt`, D2 `android/temp/graphics-safety-20261001-190014/results.txt`. It explicitly accepts enhanced filtering, opens native resolution settings, combines TexFilt=2 with an 800x600 attempt, fails EGL initialization, then verifies accepted 640x480/TexFilt=1 recovery with the menu still open. Known-color probes pass both there and after returning to gameplay; pause ownership balances, and no later confirmation appears. The retained failure identifies `egl_initialize_failed`, phase-1 attempt, and the 800x600/TexFilt=2 candidate. Final snapshots show accepted/current/requested matching and context generation 2. The standard Android build and scoped C/C++ quality checks pass with this change. This proves recovery from one failed initialization; an accepted-mode rebuild that also keeps failing still needs its bounded process-recovery case

Real Home and screen-lock lifecycle coverage is now implemented in `test_graphics_lifecycle.jsonc`. The host runner verifies an active durable challenge before backgrounding, rejection while backgrounded, preservation of every accepted field, and survival of the original game process. It runs one Home cycle and one Home/screen-lock cycle, then checks foreground gameplay, completed surface recreation/park-wake cycles, balanced game-time pause and a known-color probe after resume

The first D1 run exposed a stale-state watchdog defect: a delayed Kotlin timer checked its cached restoring phase before refreshing native state, and closed a responsive game after screen-lock recovery. The timer now refreshes the coordinator before evaluating deadlines. Failure evidence is retained in `android/temp/graphics-safety-20261001-190905/`, including logcat. Fresh standard Android build and scoped Kotlin/PowerShell quality checks pass. Both lifecycle integrations pass 52/52 steps with the same game PID throughout: D1 `android/temp/graphics-safety-20261001-191748/results.txt`, D2 `android/temp/graphics-safety-20261001-192013/results.txt`. Both return to accepted gameplay with working probe pixels and no extra challenge. These tests cover Home, screen lock and surface replacement, not Activity replacement or every native interruption

The stalled-render recovery regression also passes with the refreshed-state watchdog: D2 `android/temp/graphics-recovery-20261001-192303/results.json`, 8,255 ms from the trigger. It verifies accepted mirrored publication while the stalled process is still alive, bounded process termination, the actual launcher recovery message and accepted gameplay after relaunch without another prompt

Repeated actual EGL initialization failures are now injectable with bounded `graphics_egl_fail_count`. The new `test_graphics_rebuild_trigger.jsonc` changes native filtering and attempts 800x600, then fails both that initialization and successive accepted 640x480 rebuilds. The recovery runner checks both dimensions in actual initialize-failure logs, durable accepted publication while the original process is alive, the specific restore-watchdog launcher message, and accepted gameplay in a fresh process. Initial runs completed recovery in both engines, but their logs exposed timer starvation: each renderer notification removed and reposted the next Kotlin tick. D1 took 5,285 ms from initial failed setup to termination (`android/temp/graphics-recovery-20261001-193221/`); D2 took 1,893 ms (`android/temp/graphics-recovery-20261001-193403/`). Those runs do not validate the one-second restore limit

The overlay now retains one queued tick instead of postponing it on each notification. The strengthened recovery case separately measures time from the observed durable restore phase to process termination and rejects delays over two seconds, allowing host-observation latency around the one-second watchdog. Scoped Kotlin/PowerShell quality checks and the standard Android build pass. Fresh repeated-rebuild cases pass in both engines: D1 `android/temp/graphics-recovery-20261001-193743/results.json`, 1,300 ms observed restore interval; D2 `android/temp/graphics-recovery-20261001-193916/results.json`, 998 ms. Logcat shows actual candidate failure, repeated accepted-mode setup failures and SIG9 termination; both cases verify full mirrored publication before termination, the specific watchdog message and accepted gameplay after relaunch. End-to-end trigger times of 7,646/7,387 ms include native menu navigation before the initial failure and are separate from those restore intervals

The stalled-render regression also passes after the queued-tick change: D2 `android/temp/graphics-recovery-20261001-194120/results.json`, 8,379 ms from the trigger, with durable rollback before process termination, launcher notification and fresh accepted gameplay. Full `:app:testDebugUnitTest --console=plain` passes: 181 suites, 1,102 test cases, zero failures/errors and one skipped case. These checks complete the repeated-rebuild recovery gate and verify the existing stalled-render path with the final timer implementation

Native-menu abandonment integration now passes in both engines through `run_graphics_recovery_tests.ps1 -Fault menu_abandoned`. The new `test_graphics_menu_abandon_trigger.jsonc` opens the native Options menu and changes filtering from explicitly accepted TexFilt=1 to TexFilt=2. The host waits six seconds and verifies the durable attempt remains phase 1 with no armed deadline, saves that record, then kills only the isolated game process. The journal retains the enhanced accepted tuple and unvalidated candidate. A fresh launcher request repairs the attempt, and fresh gameplay has the full accepted/current/requested settings without another prompt. Evidence: `android/temp/graphics-recovery-20261001-194928/results.json`, both cases pass. This covers an in-level edit waiting behind menus; interrupted config-batch repair and normal-exit/staged-request restart coverage remain separate

Actual OK publication failure now passes in both engines through `run_graphics_recovery_tests.ps1 -Fault accept_publication_blocked`. After an armed trial, the host blocks directory publication, then `test_graphics_accept_trigger.jsonc` chooses OK using real D-pad/A dispatch. Shared native decision logging records the trial ID, accept flag, reason, result and monotonic decision/deadline times. The host requires an OK result of -1 before expiry, preventing a timeout from masquerading as this test's evidence. Both retain every old accepted value, surface recovery in the launcher, restore original directory permissions, repair all mirrored configs and reach fresh accepted gameplay without a prompt. Evidence: `android/temp/graphics-recovery-20261001-195210/results.json`, D1 recovery 3,923 ms and D2 3,261 ms from the trigger. The standard Android build for both engines/all ABIs, scoped C++/PowerShell quality and whitespace checks pass. This fault blocks creation/publication of the accepted record; individual sync/rename faults retain their existing host coverage

Normal-exit staging passes in both engines with `run_graphics_recovery_tests.ps1 -Fault normal_exit`. `test_graphics_normal_exit_trigger.jsonc` uses the real native Graphics Options menu to select Trilinear, leaves the edit ineligible behind the menu for eight seconds, and exits through the ordinary Exit to Launcher path. The host verifies cleared trial ownership, unchanged accepted TexFilt=1 and requested TexFilt=2 in every mirrored config. `test_graphics_staged_restart.jsonc` starts a fresh level, requires a challenge for current TexFilt=2 against accepted TexFilt=1, then cancels through B and verifies complete mirrored restoration. D1 evidence: `android/temp/graphics-recovery-20261001-200129/results.json`; D2: `android/temp/graphics-recovery-20261001-200311/results.json`. An initial fixture used the intentionally nonpersisting debug setter and therefore did not exercise the required saved user edit; it was replaced with the actual menu interaction

Interrupted config-repair integration also passes in both engines. The explicit introspection-only `graphics_restore_pause_once` action arms a bounded pause in the shared file transaction after its first target replacement, consumed only when the batch's TexFilt matches the accepted rollback value. The service shares the engines' opt-in introspection support; the default flag is zero and no pause occurs without this action. `test_graphics_repair_interrupt_trigger.jsonc` trials TexFilt=2 against explicitly accepted TexFilt=1 and lets the deadline initiate real rollback. Before killing the isolated game process, the host records phase-4 durable repair state, accepted TexFilt=1 in the root config, rejected TexFilt=2 in both game configs, and the actual publication-pause log. A fresh launcher request repairs every protected field before accepted gameplay, with no new prompt. Evidence: `android/temp/graphics-recovery-20261001-201233/results.json` and per-case partial record/config/logcat captures, both cases pass. The pause deliberately holds the file transaction for this host-directed process-interruption test; it is not the render-thread-stall/watchdog test

The standard Android build with this hook passes for both engines/all three ABIs. Both native host graphics suites were rebuilt with MSVC and pass through CTest, verifying that the Android-only hook stays excluded from the host code path. Scoped mixed-language quality and whitespace checks pass. The recovery runner now also checks the full accepted tuple and all mirrored protected fields after every restart automation completes

Expanded real-input integration now passes in both engines through `test_graphics_confirmation_input.jsonc`, 94/94 steps each: D1 `android/temp/graphics-safety-20261001-203628/results.txt`, D2 `android/temp/graphics-safety-20261001-203811/results.txt`. It starts with explicitly accepted enhanced filtering, selects OK before cancelling with Android Back/Escape, verifies bounded D-pad selection, rejects repeat-only/release-only activation, and keeps a repeat/release with its modal press across dismissal. Hat input is ignored until neutral, then selects OK/Cancel; stick navigation explicitly accepts a trial after neutral. Real touch gestures through Activity dispatch verify that outside-to-OK and OK-to-outside drags keep the challenge open, touch Cancel restores accepted, and a subsequent touch OK accepts the applied candidate. Input flags, game-window ownership, balanced pause and unchanged player energy are checked after dismissal so a transient leaked fire press cannot hide behind a later cleared input flag

The debug controller API now includes read-only confirmation state and obtains laid-out button coordinates for touch automation. It dispatches genuine MotionEvents through the Activity rather than invoking button listeners directly. Each trial also checks the initial `Cancel (5)` label captured at the overlay's first Android draw. An earlier D2 assertion expected that same label when native rendering reported the candidate applied, but observed a legitimate `Cancel (4)` after application latency (`android/temp/graphics-safety-20261001-203004/results.txt`). The assertion was corrected to observe the first-draw label; the absolute live deadline and countdown implementation are unchanged. Standard Android builds and scoped Kotlin quality checks pass with these diagnostic additions

The new actual Video Info/settings-tray integration initially failed in D1 (`android/temp/graphics-safety-20261001-210136/results.txt`, step 42/75). Controller input successfully opened the parent admin tray, selected Video Info, edited filtering/AF and verified their queued display values, but the engine never applied those edits or entered a challenge. Central UI eligibility treated the still-open parent tray as an unrelated blocking menu. It now allows the graphics-editing stack while Video Info is visible; music and keyboard blockers remain. The integration also dispatches rapid controller sequences within one UI callback so emulator frame latency cannot spread them past the 750 ms debounce

The next D1 run reached the challenge but caught a display race (`android/temp/graphics-safety-20261001-210552/results.txt`, step 41/69): polling read the old applied AF after the coordinator cleared queued fields and before the trial was applied. Video Info now uses the prepared/challenged candidate snapshot during that interval. The final standard Android build for both engines/all ABIs, scoped Kotlin quality and whitespace checks pass. D1 Cancel/B/edit-back and parent-pause checks then passed 68/68 steps (`android/temp/graphics-safety-20261001-210918/results.txt`). The fixture was extended to leave a real MSAA trial untouched until timeout with the original settings tray still owning its pause

A subsequent D2 run stalled during initial EGL setup before native automation loaded (`android/temp/graphics-safety-20261001-211055/results.txt`). The retained `startup-logcat.txt` ends at initial EGL window setup, and `startup-graphics-record.json` shows accepted all-off with no attempt. This does not verify a graphics trial or identify the physical Retroid defect. The provisioned emulator was restarted. Its next D2 run passed the trial, displayed rollback and timeout checks, but checked the final closed-tray flag before the existing 200 ms close animation finished (`android/temp/graphics-safety-20261001-211524/results.txt`, step 75/78). The fixture now waits for the native pause-release acknowledgement before asserting that the tray is closed

Final `test_graphics_video_overlay.jsonc` passes serially on emulator-5554 with the final APK: D2 `android/temp/graphics-safety-20261001-211917/results.txt`, 78/78 steps; D1 `android/temp/graphics-safety-20261001-212011/results.txt`, 76/76 steps. The test opens the real settings tray using controller Menu actions, selects Video Info, batches actual filtering/AF edits within the debounce and verifies one final candidate with the parent tray open. Default Cancel via A and MSAA Cancel via B restore the displayed values, preserve the selected row, resume polling and retain the parent's single-player pause. A second actual MSAA edit times out without input and does the same. Three rapid filtering edits back to accepted show no prompt. Closing Video Info alone keeps the parent's pause; closing the parent releases it after its animation. Fire flags/player energy rule out leaked A/B input despite A being assigned to Menu. Final introspection in both games confirms idle state, all ten current/requested fields matching accepted, foreground gameplay and unpaused game time

Creation diagnostics now distinguish generation, binding, color/depth allocation, sample queries, attachment and completeness calls. They retain the first production error even when subsequent calls succeed, and force the failing stage into the exportable graphics log. When graphics logging is enabled, creation also reports bounded per-format supported sample counts (at most 32) and actual renderbuffer object/format/dimensions/samples. These optional diagnostic-query errors are reported separately and do not become production allocation failures. EGL initialization and resume now record operation begin/end events, retain initialization errors before cleanup, and report the actual resumed GL version. Swap begin/result/error records carry the existing real flip ID and use the existing 120-frame MSAA trace budget, plus the first 20 startup swaps; there is no continuous readback or new synchronization call

The introspection-only `msaa_color_alloc_fail_once` action injects negative width into the next actual production color renderbuffer allocation. `test_graphics_msaa_allocation_failure.jsonc` verifies the resulting real GL_INVALID_VALUE, immediate rejection of the phase-3 MSAA=2 trial, complete restoration of an explicitly accepted enhanced TexFilt=1 tuple, balanced pause, known-color/state-restoration probes and subsequent successful explicit acceptance of a valid MSAA=2 trial. Both engines pass 44/44 steps with the final APK: D1 `android/temp/graphics-safety-20261001-214248/results.txt`, D2 `android/temp/graphics-safety-20261001-214421/results.txt`. Captured logcat in each case identifies `allocate_color_multisample` with error/first_error 0x501, preserves that first error through a successful completeness query, reports valid later color/depth buffers and confirms correlated successful swaps. An initial D1 fixture assertion incorrectly assumed accepted HUD filtering was off (`android/temp/graphics-safety-20261001-213923/results.txt`, step 34/44); the diagnostic fixture now writes all ten protected baseline fields explicitly before the first accepted trial

The standard Android build passes for both engines and all three ABIs with these shared diagnostics and guarded automation hook; scoped C/C++ quality and whitespace checks pass. The existing MSAA known-color suite passes all four runs, each 61/61 steps, for D1/D2 and both requested color depths (`android/temp/msaa-render-20261001-214636/rgb565.txt` and `rgba8888.txt`). Captured `rgba-logcat.txt` has no production GL, allocation, initialization or swap errors. These remain owned-target probes during menus, not proof of actual native menu composition or physical presentation

The lifecycle fixture now enables graphics logging to exercise the resume records. Fresh runs pass 53/53 steps in each engine: D2 `android/temp/graphics-safety-20261001-215227/results.txt`, D1 `android/temp/graphics-safety-20261001-215548/results.txt`. Their captured logcat contains paired surface-create/make-current events and actual resumed OpenGL ES 3 version strings. Both final snapshots are idle and unpaused, with working known-color probes and continued swaps. Context generation remains 1 in both, so this covers surface replacement with the existing context preserved, not the full lost-context resource rebuild branch

Actual lost-context coverage now uses `graphics_context_loss_once` to detach and destroy the real EGL context at the next resume. The first run (`android/temp/graphics-safety-20261001-221931/results.txt`) reproduced retained MSAA framebuffer names: the context generation advanced but FBO generation did not, with repeated GL_INVALID_FRAMEBUFFER_OPERATION. Recovery now forgets retired FBO/query/binding/viewport state and clears old texture/program caches before creating the new shim objects. Old program deletion is suppressed during this cleanup to prevent stale names from deleting new-context objects or generating invalid-name errors. The next run passed its original assertions but logged GL_INVALID_OPERATION during scene drawing (`android/temp/graphics-safety-20261001-223450/logcat.txt`): merged-texture programs were cleared without being rebuilt. The engine recovery callback now recreates those programs and the normal GL defaults as well

The strengthened `test_graphics_context_loss.jsonc` passes 62/62 steps serially for both engines: D1 `android/temp/graphics-safety-20261001-224013/results.txt`, D2 `android/temp/graphics-safety-20261001-224129/results.txt`. Both real context retirements succeed, context and production MSAA generations reach 3, accepted enhanced filtering plus MSAA=2 survive both cancelled trials, and known-color/state-restoration plus shader/VBO/2D-batch pixel probes pass after Home and screen lock. New cumulative `msaa.scene_error_count` and retained `msaa.last_scene_gl_error` remain zero, so a successful later resolve cannot hide scene errors. Saved logs confirm both retirements/rebuilds and no scene GL errors. Standard Android builds pass for both engines/all three ABIs, both Windows engines build, both graphics host suites pass, and scoped quality checks pass. This proves emulator lost-context reconstruction, not the physical Retroid cold-launch flicker or complete native-menu/subview pixel composition

The later entries below add restoration-resource, Activity-replacement and multiplayer evidence; the latest remaining-gate summary follows them. Responsive black output, pre-armed failure callback capture, actual EGL initialization failure, actual MSAA color-allocation failure, actual context retirement/rebuild, bounded repeated accepted-mode rebuild failures, armed render-thread stalls, unreadable records, blocked publication/OK acceptance, abandoned native-menu attempts, normal-exit staging, interrupted config repair, expanded key/axis/touch routing, actual Video Info/settings-tray pause ownership and Home/screen-lock lifecycle now have the device integration evidence above. All verification scenarios below remain required. Initial device runs used an older APK because the IDE ABI override wrote its new APK under `intermediates`; those failures are not feature evidence. The successful runs use the fresh standard APK from `outputs`

`test_graphics_native_interruption.jsonc` now passes 53/53 steps in each engine: D1 `android/temp/graphics-safety-20261001-224601/results.txt`, D2 `android/temp/graphics-safety-20261001-224706/results.txt`. A real native Tab event opens automap during an armed filtering trial; the coordinator cancels, restores accepted filtering and dismisses the Kotlin modal. A queued edit stays idle behind automap for six seconds, editing back prevents a prompt on return, and a later retained edit challenges only after automap closes. B restores accepted values with balanced pause and no fire input. An initial test expected idle state to clear the historical deadline field; the corrected test observes phase/modal visibility instead. This adds native interruption and automap eligibility evidence; it does not cover Activity replacement or the other preview/demo eligibility cases

The bounded production MSAA trace now includes read/draw buffer selection and sample/alpha coverage state. Creation logs also capture the default window's color encoding, component type, sample buffers and samples, with diagnostic-query errors separated from allocation errors. The owned-target probe records those fields plus actual GL/vendor/renderer/shader versions. It safely handles unavailable GL strings and now accepts 8x requests consistently with the live-capture helper, reporting the actual allocated sample count. These additions are diagnostic; they do not alter production resolve behavior

The final standard Android build passes for both engines/all three ABIs, and scoped quality checks pass. The expanded MSAA matrix passes all four cases, each 61/61 steps: `android/temp/msaa-render-20261001-225226/rgb565.txt` and `rgba8888.txt`. Final logcat is saved alongside them. Both requested color-depth variants report valid window queries and working known-color transfers/state restoration. The 8x-request live capture also passes (the emulator allocates 4x, as reported) at `android/temp/msaa-live-20261001-225600/after.json`, with unchanged accepted/requested config files and the same game PID. Its explicit retained-game fixture passes 30/30 steps at `android/temp/graphics-live-fixture-20261001-225505/results.txt`; an earlier capture correctly refused because the matrix runner had already stopped its game process

The two-device LAN runner now supports `-GraphicsConfirmation`, using provisioned emulators and the normal host/join path. All six cases pass per engine: Cancel, timeout and explicit OK on the host, then the joining peer. Two snapshots within the same armed trial verify advancing frame counters and remote packet sequence values, an unchanged absolute deadline, two connected peers and unpaused simulation. Full current/requested/accepted tuples and the other peer's independent settings are checked after every decision. D2 evidence: `android/temp/graphics-multiplayer-20261001-230417/results.json`; D1 evidence: `android/temp/graphics-multiplayer-20261001-230723/results.json`. The helper and two owned automation scripts pass scoped quality checks

Actual Activity replacement during an armed challenge is now exercised by the debug-only `recreate_activity` command and the `activity_replaced` recovery case. The initial run (`android/temp/graphics-recovery-20261001-231511/`) durably rolled back but exposed a second native startup from the replacement Activity, rejected by the engine admission guard, with no usable recovery message. MainActivity now remembers that a graphics trial was interrupted across saved Activity state. If that Activity is recreated while its native engine remains alive, it keeps the UI unavailable, returns to the launcher with an explicit message and terminates the isolated game process before another engine startup. A fresh-process restoration continues through the existing durable repair path

Both actual-replacement cases pass with the corrected APK: D1 recovery 1,361 ms, D2 1,379 ms. Evidence: `android/temp/graphics-recovery-20261001-232002/results.json`, including real destroy/create identities, `changing_config=true`, `restored=true`, durable cancellation before the original deadline, no duplicate startup, launcher message, all ten accepted values in every config mirror and fresh accepted gameplay without a prompt. The recovery helper now resolves its support fixture before invoking the generic runner, matching the updated test catalog contract. The standard Android build passes for both engines/all three ABIs

Mode rollback now also checks known-color direct/window/offscreen resolve pixels and actual GLES shader/VBO/2D-batch resource reconstruction after both controller Cancel and timeout. The expanded `test_graphics_mode_restore.jsonc` passes 69/69 steps for D1 (`android/temp/graphics-safety-20261001-232232/results.txt`) and D2 (`android/temp/graphics-safety-20261001-232656/results.txt`), with no scene GL errors. The first D2 attempt in the former run reached EGL initialization during timeout rollback but exceeded the one-second restore watchdog and closed the game. Its durable accepted tuple/restore marker and `d2-restore-watchdog-logcat.txt` were preserved; the unchanged repeat passed. This is evidence of a variable emulator restoration delay, not an identified driver fix. Physical-device restoration latency and actual restored input coordinates still need verification

An additional eligibility source audit confirms that the only calls which prepare/arm a trial are in D1/D2's gameplay draw handlers. The shared level/robot previews have their own window handlers, and their JNI startup does not initialize the graphics coordinator. Metadata paths and non-rendering demo states do not call the gameplay hook. This explains the exclusion even though previews reuse `Game_wind`; it supplements, rather than replaces, the remaining runtime eligibility checks

After the Activity-replacement change, ordinary Home/screen-lock lifecycle regression passes 53/53 steps in both engines, retaining the original game PID across both cycles. Evidence: `android/temp/graphics-safety-20261001-232821/results.txt`. Scoped mixed-language quality passes for the Activity, recovery runner/wrapper and expanded mode script; the changed files also pass `git diff --check`

The opt-in `msaa_menu_probe` now samples actual native-menu pixels immediately after drawing and again after production composition, before EGL swap. It selects flat opaque patches from the paletted menu source, checks expected RGB/alpha with RGB565 tolerance, and records both stages under one flip ID. Scaled menus supply their existing software bitmap; unscaled menus use a private software reference with callbacks disabled. For menus drawn into the production multisampled target, observation resolves into an owned matching-format single-sample target without replacing the production final resolve. All changed GL state is restored and verified. No menu readbacks or reference rendering occur without an explicit debug request

The initial scaled-only implementation could not capture native-size menus, which the first D1 run exposed (`android/temp/msaa-render-20261001-234722/rgb565.txt`). After adding that path, D1 passed 67/67 checks (`android/temp/msaa-render-20261001-235708/rgb565.txt`). The expanded fixture now explicitly exercises both unscaled/final-resolve and 110% zoom/early-resolve paths at accepted 2x and 4x. All four final runs pass 83/83 steps: D1 `android/temp/msaa-render-20261002-000702/`, D2 `android/temp/msaa-render-20261002-001042/`, each with RGB565/RGBA8888 results and logcat. Android builds pass for both engines/all three ABIs, both Windows builds pass, and scoped mixed-language quality passes

`capture_msaa_live.ps1 -Serial <adb-serial> -Samples 4 -NativeMenu` captures an already open native menu over a running level, retaining before/after introspection and native diagnostic logs. It reports bad pixels as diagnostic evidence while requiring state restoration, unchanged accepted/config records and the same process. Live D2 captures pass for Options (`android/temp/msaa-live-20261002-001557/`) and nested Graphics Options (`android/temp/msaa-live-20261002-001733/`): both actual menu stages and the owned color-pattern probe pass, with unchanged settings and PID. This extends the earlier owned-target-only captures; it still cannot establish physical presentation stability

The restored-touch fixture exposed the short restore-watchdog interval again, before it reached input checks. The first D1 run retained the accepted tuple and a pending cancel marker after process termination (`android/temp/graphics-restored-touch-20261002-001807/`). The repeat with graphics logging (`android/temp/graphics-restored-touch-20261002-002158/live-logcat.txt`) identifies successful EGL recreation and shader initialization finishing about 975 ms after timeout rejection, followed by the one-second watchdog killing the process during the remaining rebuild. The independent Kotlin restore allowance is now three seconds, with the five-second Keep/Cancel deadline unchanged. Fault-test timing bounds and the design below reflect the revised recovery allowance

With the revised allowance, both expanded mode fixtures pass 70/70 steps, and the new `run_graphics_restored_touch_tests.ps1` passes four real-input cases: D1/D2 at native size and 110% menu zoom. After cancelling an 800x600 candidate back to accepted 640x480, it obtains actual SurfaceView screen bounds from debug UI introspection, transforms the native item center through the current menu viewport and dispatches `adb input tap`. The expected nested Graphics Options menu opens in all four cases, with the original game PID retained. Evidence: `android/temp/graphics-restored-touch-20261002-002708/results.json`, complete before/after engine/UI states, fixture results and logs. The standard Android build and scoped mixed-language quality pass

The revised watchdog also passes stalled-renderer and repeated-EGL-failure recovery in both engines, including durable publication before termination, the launcher recovery message and a fresh accepted level. Stall end-to-end times are 9,762 ms (D1) and 9,616 ms (D2), including debounce, the five-second decision deadline and recovery allowance: `android/temp/graphics-recovery-20261002-003226/results.json`. Repeated-rebuild evidence is in `android/temp/graphics-recovery-20261002-003431/results.json`. An earlier stall run did not execute its trigger because broad JSONC formatting had added trailing commas rejected by the native parser. The recovery runner now resolves and normalizes its pushed support scripts into strict JSON; all four corrected fault cases pass

The new `run_graphics_preview_tests.ps1` verifies isolated robot and level previews in both engines with deliberately unvalidated filtering/MSAA/AF/color-depth settings. All four cases pass: `android/temp/graphics-preview-20261002-005138/results.json`. Real presentation counters advance and visible pixels remain valid beyond the five-second challenge interval, while native confirmation stays disabled, the full accepted record remains unchanged and all three staged config texts survive return to the launcher. Level-preview launch also executes the real level metadata analysis before rendering. An initial fixture incorrectly used the robot-only `frame_count` property for level previews; the final checks use the shared actual flip counter

The first robot-preview launch exposed a prerequisite parsing failure: recently formatted JSONC assets contain trailing commas, which Android's `JSONArray` interprets as an extra null entry. `Jsonc.strip` now removes trailing commas outside strings, including across comments, while preserving line numbers and literal string contents. Three focused regression tests and all 1,104 executed JVM tests pass (one existing skipped test); the standard Android build passes for both engines/all ABIs. Live D1/D2 robot previews verify the actual Android parser behavior, which differs from the JVM JSON dependency. Scoped mixed-language quality passes

The opt-in `msaa_scene_probe` now writes three small opaque marker patches after the real main-view pass, samples them after every subsequent real scene pass through an owned matching-format single-sample resolve, and samples framebuffer zero again before swap. It preserves the GL state used by the probe and reports actual pass viewports, bindings, pixels, errors and the flip ID. Normal rendering performs no marker drawing or readback unless automation explicitly requests the probe. D1/D2 hooks run before `ogl_end_frame()` resets the pass viewport. The expanded MSAA fixture exercises the main view in both engines and D2 level 3 with the left rear-view pane plus an actual missile-camera pass, for both 2x and 4x. The first complete D2 RGB565 run passes 136/136 steps (`android/temp/msaa-render-20261002-011402/rgb565.txt`). Initial fixture attempts stalled behind level 3's briefing; adding the existing `skip_briefing` action fixed the fixture without a new level-loading hook

`capture_msaa_live.ps1 -Serial <adb-serial> -Samples 2 -ScenePasses 1` adds this composition probe to an already running gameplay capture. Use 2 or 3 only when the corresponding subviews are actually rendering; the probe is bounded to 120 frames. Scene and native-menu captures are separate. Wrong pixels remain diagnostic results, while state restoration, unchanged protected records/configs and the same game process remain required. The probe draws three visible patches for its observed frame(s), so repeat a suspected timing-sensitive failure with probes disabled. The live D2 smoke check passes at `android/temp/msaa-live-20261002-012621/after.json`: owned-target MSAA and one actual scene pass both pass, with unchanged settings and PID. Its retained-game setup passes 30/30 steps in `android/temp/graphics-scene-live-fixture.txt`; this live smoke case has production MSAA off, while the four-case matrix below exercises production 2x/4x

The non-rendering replay exclusion has additional source evidence: both SDL event dispatchers return before gameplay draw when `input_demo_process_fast_replay()` handles the foreground replay, and explicitly suppress the `Game_wind` draw when a dialog is foreground. Only that gameplay draw handler calls `android_graphics_safety_before_main_view()`. Android's ordinary replay JNI startup passes `-inputdemo-replay` but has no option for `SysInputDemoNoRender`; metadata jobs set it in their separate startup path, where the coordinator is not initialized. This establishes the current call-path exclusion, not an Android runtime no-render replay test. Do not add an otherwise unnecessary product replay mode merely to create that test

Final scene/menu matrix validation passes with the rebuilt APK in all four cases: D1 106/106 steps and D2 136/136 steps for each of RGB565 and RGBA8888 (`android/temp/msaa-render-20261002-012030/rgb565.txt` and `rgba8888.txt`). Both sample counts are exercised in each run. D2's final scene report records the main 640x320 viewport and two distinct smaller cockpit viewports, all three preserved marker colors, framebuffer zero at the final sample, restored GL state and zero cumulative scene GL errors. The standard Android build passes for both engines/all three ABIs; the Windows build for both engines and scoped mixed-language quality checks also pass. The temporary level-loader debug action used while investigating the briefing wait has been removed from the final source and APK

The Retroid connected on October 2 as `JYPR42510121028`. Before updating its older app in place, all private files and preferences were archived at `android/temp/retroid-msaa-20261002-073724/original-app-data.tar`, including the existing pilot and three demo saves. The standard debug package was rebuilt because the pre-existing local APK belonged to the separate Legacy application ID. Temporary graphics logging and USB stay-awake were enabled for diagnosis; restore the user's preferences and saved data after testing

The first physical capture identifies a format mismatch, not an unexplained driver failure. The device reports ARM Mali-G77 MC9 / OpenGL ES 3.2 and an actual single-sample 10/10/10/2 window, even with requested RGB565. Both the production allocator and owned probe map any alpha-bearing window to RGBA8. The probe's direct window control and offscreen MSAA resolve pass; its RGBA8-to-window resolve returns `GL_INVALID_OPERATION` (1282) and leaves the cleared black sentinel. The production 2x trial reproduces the same 0x502 resolve failure and the coordinator immediately restores the complete accepted tuple. Requested 2x allocates 4x on this GPU, as reported. Evidence: `owned-probe-before.json`, `production-msaa2-before-state.json`, and `production-before-logcat.txt` in the Retroid directory above

The first isolated correction shares format selection between production and diagnostics and maps actual 10/10/10/2 channels to `GL_RGB10_A2`. No EGL selection, scissor, synchronization, or device-name workaround is combined with it. Rebuild and repeat the same physical probe and production trial before expanding the matrix. The initial scene-marker capture with MSAA off also sees two full-size scene passes before a swap, with the second overwriting the first pass's markers; that result does not establish a subview wipe and needs interpretation separately from the confirmed resolve-format error

Physical correction validation and restoration are complete as summarized above. Non-rendering replay remains covered by the explicit call-path audit rather than a nonexistent Android launcher mode

## Required behavior

Treat a changed graphics configuration as a trial until the user explicitly chooses OK while looking at a playable 3D scene. Cancel, Back, B, and a five-second timeout restore the complete last accepted graphics configuration. Do not use an all-off configuration as the normal rollback target

Compare configuration values, not edit history or the existing graphics settings generation counter. Editing a setting and editing it back before the challenge starts must not show a prompt

The user confirmed that resolution and color depth belong in this feature, in addition to filtering, MSAA, and AF

## Protected configuration and comparison

Use one value snapshot containing:

| Config field                 | Purpose                                                       |
| ---------------------------- | ------------------------------------------------------------- |
| `TexFilt`                    | Nearest, bilinear, or trilinear world filtering               |
| `AnisoLevel`                 | AF                                                            |
| `MsaaLevel`                  | MSAA                                                          |
| `MenuTexFilt`                | Filtering for menus and other non-world textures              |
| `HudTexFilt`                 | Filtering for HUD textures                                    |
| `ResolutionX`, `ResolutionY` | Render dimensions                                             |
| `AspectX`, `AspectY`         | Preserve the aspect settings accompanying a resolution change |
| `ColorDepth`                 | RGB565 or RGBA8888 request                                    |

Gamma, FOV, text insets, pilot visual effects, movie filtering, debug switches, and controller settings are outside this snapshot. They continue to save normally and are not reverted with a failed trial. The protected snapshot covers the risky renderer settings and the selective filtering switches discussed with the user

Maintain three distinct values:

- `requested`: the settings saved by the launcher or most recent menu edit
- `current`: the coherent protected configuration actually being tried by the engine
- `accepted`: the durable snapshot from the last successful OK

At startup, requested and current converge as the engine initializes. In a running engine, fields awaiting application remain requested only. OK must never accept a saved resolution/color-depth change that has not reached the renderer

Normalize values using the same native validation rules used to apply them. Dimensions must pass `android_render_resolution_valid`; ratio values must be positive and reduced consistently. Read active engine dimensions from `Game_screen_mode`, rather than assuming `GameCfg.ResolutionX/Y` tracks a native menu change. Invalid values are rejected rather than becoming a new trial

GPU capabilities and effective sample counts belong in diagnostics, separate from equality. A requested 4x mode falling back to 2x must not repeatedly challenge on every frame. Structural renderer failure immediately rejects the trial; a supported capability fallback can be acknowledged by OK

The decision is:

```text
needs_confirmation = current != accepted && !all_enhancements_off(current)

all_enhancements_off(s) = s.TexFilt == 0
                         && s.MsaaLevel == 0
                         && s.AnisoLevel == 0
```

AF can enable effective filtering even when `TexFilt` is zero, so all three tests are necessary

The exemption uses those three fields exactly, even for a resolution or color-depth change. Menu/HUD switches may remain enabled because they have no effective filtering when both world filtering and AF are off. This honors the requested all-off exemption, but means resolution/color-depth problems in that configuration will not receive a five-second challenge

An exempt configuration does not overwrite `accepted`: the user defines acceptance as choosing OK. For example, accepting bilinear + 2x MSAA, then turning everything off, then trying trilinear + 4x MSAA and cancelling restores bilinear + 2x MSAA, including its accepted resolution and color depth

Use one app-wide accepted snapshot, matching the existing launcher/native behavior that mirrors these settings to root, D1, and D2 configs. Accepting in either game acknowledges that shared preference set. It is not a claim that both engines were separately tested. Level and robot previews, metadata workers, and headless runs cannot validate or replace it

## When the challenge starts

Eligibility must be determined on the game thread:

- A real game level is active and `Screen_mode == SCREEN_GAME`
- `Game_wind` exists and is the front engine window
- The frame will render the main gameplay view, not the automap, briefing, preview, loading screen, or a cockpit subview alone
- The Activity is foreground and its surface is available
- No unrelated Kotlin modal is blocking the gameplay view

Do not infer eligibility from `gameStarted`, a successful EGL swap, or `ogl_start_frame()` alone. The engine draws visible game windows underneath native menus, and the GL entry point is also used by other 3D views

| Source of change                                    | Challenge timing                                                                                                |
| --------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| Launcher before game launch                         | At the first eligible gameplay render attempt after loading/briefings                                           |
| Launcher while a game is retained in the background | After returning, applying its coherent snapshot, and becoming eligible                                          |
| Kotlin Video Info settings                          | After 750 ms without a protected edit, while gameplay is eligible; the Video Info editor itself may remain open |
| Native menus opened mid-level                       | After the complete native menu stack closes, at the next eligible gameplay render attempt                       |
| Native menus before starting a level                | At the first eligible gameplay render attempt after the level starts                                            |

Reset the 750 ms debounce only for an actual protected value change. Compare again when it expires. Turning an option back to its accepted value, or reaching all-off, removes the pending challenge. Returning from native menus needs no extra 750 ms if the user has already stopped editing

For Kotlin live edits, coalesce renderer application along with the debounce. Show the requested values in the editor immediately, but keep rendering the previous configuration during the quiet period. At expiry, compare the proposed next-current snapshot (using the mode actually running), prepare the overlay if needed, and then apply that batch. This keeps the first potentially failing render inside an armed trial and avoids rebuilding the same resources for every tap. If the final batch equals accepted or is all-off, apply it without a challenge

One trial covers the complete final snapshot after the debounce. Once the prompt is active, protected edits and opening new settings menus are blocked until it resolves. This avoids having OK acknowledge a different configuration than the one displayed

If a native menu or lifecycle transition nevertheless interrupts an active trial, cancel it and restore accepted settings. Do not pause/reset its deadline and offer repeated five-second extensions

## Overlay and input

Add a dedicated Kotlin `GraphicsConfirmationOverlay`, above Video Info, touch controls, popups, and the game `SurfaceView` in the Activity's existing `FrameLayout`

Suggested layout:

```text
Keep these graphics settings?
<brief list of changed settings>

[ OK ]   [ Cancel (5) ]
           ^ initially selected
```

- Start with Cancel selected on every trial
- D-pad left/up selects OK; right/down selects Cancel, with bounded movement rather than wraparound
- A, D-pad center, or Enter activates the selected button; A does not always mean OK
- B, Android Back, and Escape cancel immediately
- Touch activates either button; outside touches are consumed without accepting
- Display `Cancel (5)` through `Cancel (1)` using `ceil(remaining_ms / 1000)`; expiry cancels, with no additional second at zero
- Derive display and expiry from a monotonic absolute deadline using `SystemClock.elapsedRealtime`, never game time, frame count, or a sequence of decrement callbacks
- Consume button press and release, D-pad repeats, touch drags, analog/hat navigation, and mapped meta actions before other overlay/native routing
- Do not let a button held while editing activate the new prompt. Require release and a fresh press for activation, and retain the release destination across dismissal using the existing controller dispatch mechanism
- Release gameplay inputs when acquiring modal ownership; keep suppressed inputs from becoming stuck or producing a shot/menu action after dismissal
- Save/restore the underlying Video Info selection and polling state. Refresh its values from the engine after resolution, rather than keeping its optimistic cycle values after rollback

The dialog must remain visible over a black game surface. Keep drawing it in Kotlin rather than with game textures. It uses Android's compositor and is independent of the game's EGL context, though a device-wide compositor/driver failure is outside that guarantee

Pause single-player simulation with the existing owned overlay-time pause, without creating a native menu window or stopping render/event processing. Do not acquire a second unmatched pause if the settings tray already owns one. Release only the pause owned by this flow. In multiplayer, continue normal network processing and rendering; the modal captures local input without freezing the session

## Native coordinator and UI handshake

Implement the shared policy in `android/app/src/main/cpp/shared/android_graphics_safety.cpp/.h`, with a C interface for engine call sites. Compile it for both Android engines and keep hooks in `d1/` and `d2/` small and guarded

The coordinator owns coherent snapshots, generations, eligibility, decisions, and rollback requests. Kotlin owns overlay presentation/input and an independent deadline watchdog. JNI calls enqueue changes/decisions or perform file-only operations; they must not perform GL work on the UI thread

Suggested state transitions:

```text
IDLE -> WAITING_FOR_SCENE_OR_DEBOUNCE -> PREPARING_OVERLAY -> CHALLENGE
CHALLENGE + valid OK -> ACCEPTING -> IDLE
CHALLENGE + Cancel/expiry/interruption -> RESTORING -> IDLE
any pending state + current == accepted or all-off -> IDLE
RESTORING + renderer cannot recover -> RECOVERY_REQUIRED
```

Each request carries a unique trial ID and snapshot generation. State transitions and deadline checks are serialized. Dismiss/OK events with an old ID or generation are ignored. OK received at or after the deadline loses to timeout. Duplicate Cancel/timeout events are idempotent

At the first eligible main-view frame:

1. Compare the fully normalized current snapshot, or the next-current batch awaiting live application, with accepted
2. Prepare a trial ID and publish a durable record of the configuration being tried
3. Notify Kotlin to display the overlay and capture input
4. Kotlin acknowledges that the overlay has drawn and arms an absolute five-second deadline through a file/state-only JNI call
5. On the next eligible render iteration, try the candidate with that armed deadline

While preparing the overlay, skip the candidate gameplay draw and keep pumping the engine event loop. Thus the timer is armed before the first candidate gameplay render and cannot depend on that render completing successfully. If UI preparation fails or does not acknowledge promptly, cancel instead of applying an unmonitored trial. Normally this adds only one UI draw/engine iteration

Native menus can already apply changes to the renderer before returning to gameplay. Record an unvalidated attempt before those changes and defer the visible five-second challenge until the game is eligible. Do not display the confirmation inside a native menu

Pump decisions/rollback at a game-thread event-loop boundary even while the game is paused or a menu is visible. Add a main-view hook before `game_render_frame()` in both engines to provide accurate eligibility. If necessary, split its notification before pending options are consumed in `ogl_start_frame()`, while retaining the gameplay eligibility token so previews/subviews cannot start trials

Poll the deadline natively as a backup when event processing continues. Kotlin also checks it independently, so a native GL hang cannot prevent the timeout decision from being persisted. Use a shared monotonic time domain (`CLOCK_BOOTTIME` on native Android and `elapsedRealtime` in Kotlin), with documented constants/interfaces

## Durable acceptance, pending attempts, and process boundaries

The launcher and `MainActivity` run in different processes (`MainActivity` is `:game`). SharedPreferences generations remain a refresh hint, not the accepted snapshot, a lock, or the authority for graphics state

Store one authoritative record at the app files root, for example `graphics_safety.json`:

```json
{
  "accepted": { "protected fields": "values" },
  "attempt": null
}
```

An attempt records its ID, owner session, candidate, phase, and armed deadline where applicable. A staged launcher edit changes normal config files without an attempt marker. An attempt marker means a running engine has started applying an unvalidated configuration; it is written before protected GL changes, including startup EGL initialization and native-menu mode switches

Use native serialization and file APIs exposed through a small variant-specific launcher bridge, following the existing native preference bridges. Kotlin should not duplicate parsing of engine configuration. Use a process-shared file lock and synced temporary file + atomic replacement. Both engine libraries use the same lock path; a process-local mutex alone is insufficient

Never hold the record/config lock while executing GL calls, waiting for an engine response, invoking Kotlin, or waiting for a UI draw. The Kotlin watchdog must be able to publish a rollback decision while the render thread is stalled

The accepted record is the durable commit point:

- OK checks ID, generation, and deadline, then durably replaces accepted with the exact tried snapshot and clears the attempt in one record replacement
- If acceptance persistence fails, do not dismiss as accepted; restore the previous accepted snapshot and show an ordinary Kotlin save-failure message
- Cancel/expiry first records the decision and accepted restore target, then patches config and queues renderer restoration
- Restore only protected keys, preserving unrelated config/player changes
- Patch every protected key across root and existing D1/D2 configs as one batch. Extend `graphics_config_transaction` beyond its current single-key interface; avoid successive independent transactions for each field
- Keep the restore marker until config patching succeeds. If the process stops between file replacements, startup repairs all protected keys from accepted before reading config/applying GL settings
- After a completed live restore, publish a resolved result to Kotlin and dismiss the overlay; show `Restoring graphics settings...` while recovery is in progress
- Synchronize the launcher's `render_resolution` display with restored config and advance its refresh generation. Make resolution config values authoritative when rebuilding the page, so stale cross-process preferences cannot undo a rollback

Normal config writes and protected launcher/native edits must participate in this same serialization boundary, or be reconciled before publication. Otherwise a native `WriteConfigFile()` or a simultaneous launcher edit can overwrite restored values. Launcher edits arriving during an active challenge are deferred until the current trial resolves; they never mutate the challenged snapshot

On normal exit before any eligible gameplay challenge, clear ownership cleanly and leave requested values staged for a later level launch. On an abandoned attempt after crash/force-stop, restore accepted before renderer initialization on the next launch. A staged launcher change with no abandoned attempt must survive app restarts and still challenge at its first level

At feature initialization with no accepted record, create a documented bootstrap rollback snapshot using conservative native defaults: 640x480, the normal valid display aspect, RGB565, nearest filtering, no AF/MSAA, and disabled menu/HUD filtering. Preserve an enhanced current request as an unvalidated candidate rather than silently blessing it. Once OK succeeds, this bootstrap is replaced permanently by the user's accepted snapshot. No migration/legacy readers are needed for the pre-release format

All-off configurations bypass the visible challenge and do not advance accepted. Do not leave an active five-second trial for them. Pre-level renderer attempt records can still support recovery from an abnormal process exit; normal all-off gameplay needs no confirmation

## Applying and restoring renderer state

Restoration must update active renderer state as well as config files. Otherwise the launcher would look repaired while the game remains black

### Filtering, AF, and MSAA

Reuse the existing texture invalidation and MSAA FBO rebuild paths, applying a coherent snapshot on the GL/game thread. Add a restore reason so setters do not recursively arm a new trial. Refresh `GameCfg`, the corresponding OGL values, and pending flags together. Do not rewrite unchanged fields or rebuild resources for a no-op

Route protected Kotlin JNI setters through a mailbox rather than directly mutating `GameCfg` from the UI thread. Native menu callbacks already execute on the game thread and can use the same coordinator. Keep other existing graphics APIs working

### Resolution and color depth

The launcher currently saves these for engine initialization. `readGraphicsConfigSnapshot()` does not apply them live on resume, and `nativeSetGraphicsOption()` has no corresponding setter. The safety flow must keep them requested-only until actually applied, then include them in the challenged snapshot

Native resolution menus already call `gr_set_mode()` and `game_init_render_buffers()`. On Android, that route destroys/reinitializes EGL using `GameCfg.ColorDepth`; it is a useful starting point for a shared game-thread restore operation

For a running engine, restore accepted dimensions, aspect, and color depth in one mode operation. Update `Game_screen_mode`, `GameCfg`, canvases, cockpit/game render buffers, viewport and touch-coordinate mapping. Rebuild context-dependent textures, shaders, MSAA targets and caches correctly. Do not treat a `SurfaceView` size callback as sufficient to change renderer color depth

Implement a checked Android mode-change adapter rather than assuming the existing `void android_egl_surface_initialize()` succeeded. Report config/surface/context creation failure explicitly, preserve the accepted target, and enter recovery if an accepted-mode rebuild fails. Keep desktop mode-setting behavior intact

Retain the launcher's existing next-start semantics for new resolution/color-depth edits while a game is retained. Other live edits are challenged against the mode actually running. On the next real startup, the requested mode is tried and included in confirmation. In-game native resolution changes remain live and challenge after all menus close

## Black frames, hangs, and recovery

A black image with an otherwise responsive engine should restore live on timeout. No framebuffer-color heuristic is required; the user decides whether the trial works

A GL call that never returns prevents the game thread from applying any new configuration. Kotlin can still persist the accepted restore target and request rollback. If the engine does not acknowledge restoration within three seconds, show a Kotlin recovery message, shut down the isolated game process through the existing controlled exit/recovery path, and return to the launcher with accepted settings repaired. This recovery allowance begins after rejection and does not extend the five-second confirmation deadline. The earlier one-second allowance proved too short for successful EGL/shader recreation. Use a bounded process-exit fallback if normal shutdown is also stuck

Do not claim seamless in-level recovery from a blocked driver. Forced recovery may lose play since the last save; the normal live rollback path preserves the running level. This limitation and the independent persistence-before-GL design are necessary for the original black-screen failure case

If renderer initialization fails before any level starts, recover immediately from the attempt marker without pretending a five-second gameplay trial occurred. Previews/metadata workers use the requested configuration only under their own existing lifecycle; they must never write this acceptance record

A hard stall during initial EGL creation or a native-menu mode switch can occur before the requested gameplay challenge is eligible. The five-second trial cannot cover that interval without violating the menu/level timing requirement. The durable attempt marker provides recovery on process restart; the trial watchdog guarantee begins when the gameplay confirmation is armed

## Retroid Pocket 4 Pro MSAA diagnosis

### Report and investigation priority

The user narrowed the original graphics failure to MSAA: every non-off setting caused a flickering background on the Retroid Pocket 4 Pro. Physical capture confirmed a failed scene resolve caused by the RGBA8/RGB10_A2 format mismatch described above. The investigation procedure below records how the cause was isolated

Investigate the existing renderer first, alongside the confirmation work. The confirmation provides recovery from a bad configuration; it does not establish that MSAA renders correctly

Relevant current implementation:

- `shared/ogl_msaa_android.c` allocates multisampled color/depth renderbuffers, validates completeness and equal effective samples, and resolves directly into framebuffer zero
- `android_ogl_msaa_resolve()` binds separate read/draw framebuffers and blits the full supplied rectangle, but does not disable or save/restore the scissor test
- `ogl_update_window_clip()` can enable scissor for a partial canvas. The first MSAA color clear in `ogl_start_frame()` can also inherit clipping or write-mask state
- `shared/android_egl_surface.c` requests RGB minimum sizes but does not specify `EGL_SAMPLE_BUFFERS`/`EGL_SAMPLES` or an explicit alpha size. The actual window framebuffer can differ from the launcher's nominal color-depth choice
- `shared/ogl_viewport_android.c` deliberately uses logical game dimensions as the drawable size. Confirm those against `eglQuerySurface(EGL_WIDTH/EGL_HEIGHT)` on this device rather than treating either the Java view dimensions or game dimensions as proof
- Scaled native menu drawing calls `ogl_android_prepare_overlay_blit()`, which can resolve early before drawing a menu to framebuffer zero. `gr_flip()` also attempts a resolve later. A late resolve must not overwrite a menu, and later scene/subview passes must not erase earlier content
- The original MSAA helpers discarded pre-existing GL errors, while `ogl_end_frame()` drained errors without recording them. The initial instrumentation now logs these by stage; extend it to distinguish individual allocation, bind, clear, draw and blit errors. A zero final `last_gl_error` does not prove that scene rendering was error-free
- Both engines' lazy ETC2 texture self-tests bind a temporary framebuffer and restore framebuffer zero rather than the prior read/draw targets. If one runs during a multisampled scene pass, subsequent draws can leave the tracked MSAA target. Trace first-use texture uploads and compare a warm-texture run against a cold run; do not assume this branch executes on the failing device
- Existing introspection exposes FBO completeness, effective samples, generation, bound-frame count, resolve count and last-resolved status. Extend it rather than inventing a separate state-query mechanism

Historical reference: [missile MSAA glitch investigation](plan_fix_msaa_missile_glitch.md) records a previously fixed unbalanced pass-depth problem, followed by a subview color clear wiping the main scene. That is relevant regression coverage, not evidence that the same defect explains this device

### First diagnostic run and decision

Start with one short Retroid capture, not the entire matrix: a fresh process, fixed level/camera at 640x480, filtering and AF off, MSAA off -> 2x, then open and close one native menu. Keep a Kotlin indicator visible so missing engine output can be distinguished from an Activity-wide presentation failure. Record the requested and effective MSAA values separately

Capture a real flip ID and the first failing stage, including the actual framebuffer bindings and scissor state before the first clear, before/after resolve, after native menu drawing and before swap. Export this evidence before the confirmation timeout restores settings and destroys the candidate FBO. A successful resolve counter is insufficient evidence that pixels reached the screen

Do not infer the frame cadence from `bound_frame_count`: a single displayed frame may contain several main/subview passes or an early menu resolve. Use one flip ID across every event belonging to that presentation, with an incrementing event index. Report the first inconsistent binding, clipped clear/resolve, GL error or wrong known pixel alongside the protected candidate snapshot. This makes alternating missing-scene/menu frames distinguishable from a consistently bad resolve

Repeat only that failing case with one opt-in diagnostic change at a time:

1. Save, disable and restore scissor around the resolve, leaving other rendering unchanged
2. Resolve known opaque color blocks to a matching single-sample offscreen target, then compare with the window destination
3. If bindings diverge during first-use texture uploads, restore the prior read/draw targets around the ETC2 self-test and compare cold versus warm texture runs
4. If pixels disappear only after opening a menu, trace the early resolve and subsequent writes using the same flip ID; check that no stale scene resolve or backing clear replaces the composed image

These experiments are diagnostic switches, not unconditional fixes. Retest any successful change with the color probe/readback disabled to avoid hiding a timing-sensitive failure. Expand to both color-depth choices, other sample counts, D1/D2 and subviews only after locating the first failing stage

### Leading hypotheses and discriminating evidence

| Hypothesis                                                          | Evidence or isolated experiment                                                                                                                                                                                      |
| ------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Resolve inherits a small scissor rectangle                          | Trace scissor enable/box at clear and resolve; compare with a debug resolve that saves, disables, then restores scissor                                                                                              |
| Window resolve is illegal for the actual default buffer             | Capture actual source/destination formats, color encoding, sample buffers/samples, selected EGL config and per-call resolve errors; compare against a matching single-sample offscreen destination                   |
| A scene draw/clear uses the wrong framebuffer                       | Log actual read/draw bindings alongside tracked `bound` and pass depth; compare known colors at clear, scene end, early menu resolve and final resolve; check bindings immediately before/after lazy ETC2 self-tests |
| Main scene is wiped or menu is overwritten                          | Trace a frame's ordered main-view, cockpit/subview, menu-blit, resolve and swap events; test without subviews and with scaled-menu path bypassed separately                                                          |
| Surface alpha exposes the Activity background                       | Sample alpha as well as RGB; compare RGB565 and RGBA8888, opaque clear and fully opaque diagnostic output                                                                                                            |
| Allocation, context or dimension mismatch                           | Log actual renderbuffer dimensions/formats/samples, per-format sample support, FBO status, EGL size and GL context identity before/after resume                                                                      |
| Device resolve/presentation behavior differs despite legal GL state | Test known-pattern offscreen resolve, direct window resolve and a single-sample texture presentation path independently; compare instrumented and minimally instrumented runs                                        |

The ES rules make the first two hypotheses actionable: scissor affects blits, and multisample resolves require matching source/destination formats and rectangle bounds; an ES 3.0 blit destination cannot itself be multisampled. Format-specific sample support can be queried rather than inferred only from `GL_MAX_SAMPLES`. Verify against sections 4.3.3 and 6.1.15 of the [Khronos OpenGL ES 3.0 specification](https://registry.khronos.org/OpenGL/specs/es/3.0/es_spec_3.0.pdf). These are API constraints, not a diagnosis of the Retroid driver

### Step 1: Capture a small reproducible matrix

Use a fixed single-player level/camera, stock assets and nearest filtering with AF off. Start at 640x480 to reduce allocation pressure, then repeat the relevant failing case at the user's normal resolution

1. Run fresh processes with MSAA off, 2x and 4x, first with RGB565, then RGBA8888
2. For each case, observe live gameplay with no native menu, the Kotlin Video Info overlay, a native pause/options menu over the game, nested menus, and the return to gameplay
3. Separately run off -> 2x -> 4x -> off in a retained engine, and a background/resume cycle. Establish whether failure starts at cold launch, first 3D frame, menu opening or context recreation
4. Repeat the minimal failing case in D1 and D2. Add cockpit/rear/missile views only after the simple case is characterized; reuse D2 level 3 missile coverage for the known historical interaction

Record device model, Android/build version, APK/commit, actual `GL_VENDOR`, `GL_RENDERER`, `GL_VERSION`, shader version, extension/capability summary and actual EGL config. Do not assume a particular GPU model from the product name

Use automation for launch/menu/settings steps and introspection for engine state. Supplement with a short physical-device video to establish which visible layer flickers and whether frames alternate; video is supporting evidence, not a replacement for native state. A Kotlin frame/status indicator can distinguish a healthy Android UI from missing game output

### Step 2: Add bounded, stage-specific diagnostics

Use `DLOG_GRAPHICS` for exportable device logs. Add an opt-in bounded frame trace, for example the first 120 frames following an MSAA change and frames around menu transitions. Keep normal release rendering free of synchronous queries/readback and avoid flooding logs if creation fails every frame

One trace uses a real engine flip/frame ID, pass/subview ID, FBO generation, native front-window type, and clear/resolve reason. Existing `bound_frame_count` is a bind counter and is not a reliable displayed-frame identifier

Capture on context/FBO creation:

- EGL config ID, RGB/alpha/depth/stencil sizes, sample buffers/samples, surface dimensions and window generation
- Actual GL major/minor version and context generation, default framebuffer channel sizes/encoding, sample buffers/samples, read buffer and draw buffer selection
- Supported sample counts for the chosen color format and `GL_DEPTH_COMPONENT16`, requested counts, actual allocated counts, renderbuffer formats and dimensions
- Framebuffer attachment information and separate errors for each allocation/attachment/completeness call

Capture at first color clear, main scene completion, each subview completion, menu preparation/blit, resolve and pre-swap:

- Actual `GL_READ_FRAMEBUFFER_BINDING` and `GL_DRAW_FRAMEBUFFER_BINDING`, tracked bound/depth, source/destination rectangles and buffer dimensions
- Scissor enable/rectangle, viewport, color/depth write masks, clear RGBA and relevant sample-coverage state
- Resolve attempt/success/error counters, whether the scene or menu changed after an early resolve, and each EGL swap result/error

Preserve errors by stage. Log errors already queued before a checked operation as prior-stage errors, then inspect immediately after that operation. Record errors drained at `ogl_end_frame()` instead of silently discarding them in diagnostic mode. Do not blame `glBlitFramebuffer()` for earlier errors or call its lack of error proof of correct pixels

Audit resume specifically: the original lost-context recreation constant was 1 despite initial creation requesting ES 3. The initial implementation now requests ES 3 on recreation as well. Verify the actual recreated version and resource rebuilding on-device; this source correction does not explain a cold-launch MSAA failure unless the trace reaches that branch

### Step 3: Locate where the image disappears

Add an automation-invoked diagnostic probe with a few known opaque color blocks/frame IDs and no game textures:

1. Draw the pattern directly into a single-sample offscreen target and into the window as controls
2. Draw/clear the pattern into the production-style MSAA target, resolve to a matching-format, same-size single-sample texture FBO, and read a few known sample locations
3. Resolve the same known MSAA pattern directly into framebuffer zero and sample those locations before menu drawing, then again after menu composition and immediately before swap
4. Compare a run with no native menu against one with scaled native menu drawing. Keep the Kotlin indicator separate from the engine-rendered menu

Clear each single-sample destination to a different sentinel color before its resolve. Otherwise, a failed/no-op resolve can appear to pass by leaving the earlier control pattern intact. Place sample points away from quadrant boundaries and account for RGB565 quantization; require opaque alpha only when the destination actually has alpha. Query and preserve any pixel-pack buffer binding so host-memory readback cannot be interpreted as a buffer offset

Do not call `glReadPixels()` on the multisampled FBO. Read only from the single-sample resolve destination. Tag RGB/alpha results by frame and stage, and use known-color assertions/tolerances rather than judging a real scene by one average color or exact cross-mode image hashes

Interpretation:

- Both direct single-sample controls fail: investigate base renderer/context rather than MSAA alone
- Controls work but the offscreen MSAA resolve fails: investigate creation, clear/draw state or resolve source before Android window composition
- Offscreen MSAA works but direct window resolve fails: investigate destination format/samples/size or the default-framebuffer resolve path
- Window pixels are correct before a menu and wrong after it: investigate composition order or leaked target/clip state
- Pre-swap game RGB/alpha is correct but the physical output flickers: inspect swap failures, surface generations, alpha and Android presentation; renderer pixel sampling cannot capture all compositor behavior

Save/restore all modified GL bindings/state and invalidate affected engine caches so the probe does not introduce a new defect. Avoid broad `glFinish()` or continuous readback: both can change timing and hide a race. Confirm any proposed fix again with the probe disabled

Keep the first capture small: reproduce one off -> 2x transition with no menu, then open and close one native menu. Identify the first failing stage before expanding to the full matrix. A successful allocation/resolve counter alone must not count as a passing image test. Export the candidate settings, first failure, and frame trace before rollback destroys its FBO so the confirmation feature does not erase the diagnostic evidence

### Step 4: Make one controlled change at a time

After baseline logs, use debug-only switches or small isolated patches:

1. Save/disable/restore scissor around full-image resolve; separately try an unclipped, fully writable first color clear. Preserve subview clears and the historical first-pass-only color-clear behavior
2. Explicitly select a non-multisampled EGL window configuration and compare actual attachment formats; test color-depth variants without combining this with the scissor change
3. Compare the current direct default-framebuffer resolve with MSAA -> matching single-sample texture -> ordinary textured draw to the window. The intermediate resolve still needs a compatible format; do not try a format-converting multisample blit as a workaround
4. Isolate main view from subviews and bypass only scaled native menu rendering for a diagnostic run. Trace actual bindings, including any lazy texture self-test that temporarily binds another FBO and restores framebuffer zero
5. Run a single synchronization experiment only if correct pixels/state and a timing dependence point toward submission/presentation. Do not keep `glFinish()` as the first production fix

Choose the smallest shared renderer fix supported by the resulting evidence, mirror required D1/D2 hooks, and retain the diagnostic failure case as an integration test. Do not add a device-name blacklist or disable all MSAA permanently based only on this report

### Integration with the confirmation feature

During a challenged configuration, confirmed structural failure such as an incomplete framebuffer or failed resolve rejects the trial immediately and restores the complete accepted tuple. Do not silently render without MSAA and then persist an OK as proof the requested mode worked

Pixel flicker with otherwise successful GL calls remains the user's OK/Cancel decision. Do not auto-reject ordinary dark game frames or use the diagnostic color probe continuously in normal gameplay

Rate-limit failure logging and latch repeated creation/resolve failure for that candidate generation; retry on an explicit new candidate or controlled context recovery. Preserve the failure record through rollback so exports identify the original failing configuration and frame

Add a reusable `test_msaa_render_and_menu.jsonc` plus a serial device runner. Extend the existing `test_ogl_runtime_texture_options_unified.jsonc` coverage: its MSAA assertions currently prove allocation/bind/resolve counters for D2, but not stable pixels, menu composition or equivalent D1 MSAA behavior

Acceptance requires physical Retroid runs for 2x/4x, both color-depth choices, gameplay/native menus/Kotlin overlay and live toggles, followed by the known subview regression and confirmation rollback tests. Success means stable known-pattern pixels and visible gameplay/menu output, correct frame/resolve ordering, no new GL/EGL failures, and working return to the last accepted settings. Emulator success alone cannot close the device bug

## Concrete change map

| File or area                                                            | Change                                                                                                           |
| ----------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| New shared `android_graphics_safety.cpp/.h`                             | Snapshot, state machine, locked durable record, mailbox, deadline decisions                                      |
| `shared/android_graphics_options.c/.h`                                  | Coherent protected setters and restore path; keep existing unprotected behavior                                  |
| `shared/graphics_config_transaction.c/.h`                               | Multi-key config batch patching and recoverable restore publication                                              |
| `jni_main.c` and new launcher JNI bridge                                | Trial callbacks, draw acknowledgement, decisions, read/stage/repair APIs                                         |
| `shared/android_egl_surface.c/.h`                                       | Checked context/surface creation and recovery results                                                            |
| `shared/ogl_msaa_android.c/.h`, `shared/ogl_viewport_android.c/.h`      | Stage diagnostics, controlled MSAA probe, trace actual drawable/binding/clip state and evidence-backed MSAA fix  |
| `d1/arch/ogl/ogl.c`, `d2/arch/ogl/ogl.c`, `shared/android_menu_scale.c` | Small diagnostic hooks for clear, scene/subview completion, early menu resolve, final resolve and swap ordering  |
| `d1/arch/sdl/event.c`, `d2/arch/sdl/event.c`                            | Pump game-thread mailbox/deadline/restores while paused or in menus                                              |
| `d1/main/game.c`, `d2/main/game.c`                                      | Main-gameplay eligibility hook before rendering                                                                  |
| `d1/main/menu.c`, `d2/main/menu.c`                                      | Record native resolution attempts and protected changes; preserve deferred prompt behavior                       |
| `d1/arch/ogl/gr.c`, `d2/arch/ogl/gr.c`                                  | Small Android mode restore adapters and resource rebuild hooks                                                   |
| `d1/main/config.c`, `d2/main/config.c`                                  | Recovery before protected config consumption; serialize protected config publication                             |
| New `GraphicsConfirmationOverlay.kt`                                    | Modal UI, countdown, default Cancel, fresh-press input                                                           |
| `MainActivity.kt`                                                       | Highest-priority input routing, lifecycle cancellation, overlay pause ownership, watchdog/recovery               |
| `VideoInfoOverlay.kt`                                                   | Report genuine edits, debounce integration, refresh after restore                                                |
| `GraphicsSettingsPage.kt`, `SetupConfigFiles.kt`, `SetupActivity.kt`    | Stage/read coherent requests through native bridge; repair abandoned trials before launch; refresh resolution UI |
| Android CMake source lists                                              | Compile shared implementation into both engine variants; add host-test target                                    |
| `game_introspect.cpp` and automation                                    | Stable safety state and decision actions for integration verification                                            |

## Implementation sequence

- [x] Capture the failing Retroid MSAA case with logs/introspection and verify the corrected device matrix
- [x] Add bounded stage diagnostics and known-pattern resolve/menu probe; run the failing physical-device case
- [x] Isolate the failure with one controlled change at a time, implement the supported shared renderer fix and retain D1/D2 regression coverage
- [x] Add snapshot normalization, equality/exemption policy, trial state machine, durable record and config batch transaction
- [x] Integrate initialization, attempted-configuration markers, native menu changes, config writers, and abandoned-attempt repair
- [x] Add game-thread mailbox, gameplay eligibility hooks, filtering/FBO restore, and checked resolution/color-depth restore
- [x] Add Kotlin modal, controller/touch routing, absolute deadline and independent watchdog, lifecycle/pause handling
- [x] Integrate launcher staging/resume semantics, UI refresh, introspection and automation
- [x] Build both Android games, run host coverage and serial device integration tests, and verify desktop guards
- [x] Run scoped mixed-language quality tooling on the changed paths and update this plan with results

The physical diagnosis and corrected-device validation are complete; detailed evidence and the test-method limits are recorded above

## Verification

Use meaningful host coverage for the state machine and persistence fault cases, then a reusable Android integration runner for D1 and D2. Expose accepted/current/requested, trial ID/generation/phase, remaining time, restore reason/status, and actual render dimensions/color depth through introspection. Android input automation must exercise real D-pad/A/B dispatch for the modal

Required scenarios:

1. Launcher edit -> first gameplay frame -> Cancel and timeout restore the full previously accepted tuple in engine and every mirrored config
2. OK before expiry persists the tried tuple; a fresh process/next level does not prompt for it
3. Edit -> original value, no-op edits, and multi-setting edits that return to accepted produce no prompt
4. All-off never prompts, including resolution/color-depth changes; it does not overwrite an enhanced accepted snapshot
5. Multiple Video Info edits within 750 ms produce one prompt for the final values; OK cannot accept a later edit
6. Native nested graphics/resolution menus mid-level never show the prompt until all menus close, even while the game draws behind them
7. Main-menu edits survive mission selection and briefings and challenge at the first gameplay attempt
8. Automap, level/robot preview, metadata jobs, demos without gameplay rendering, and loading/menu draws cannot start or accept trials
9. Default Cancel + A cancels; explicit navigation to OK + A accepts; B/Back/Escape cancel; held A/repeat/release and touch drags cannot leak into gameplay or auto-accept
10. A running-engine launcher mode change remains requested-only until next startup; an unrelated live filtering change does not falsely validate that future mode
11. Accepted resolution/aspect/color depth restore recreates a working renderer, buffers and input coordinates, alongside accepted filtering/MSAA/AF
12. Background, surface loss, native interruption and Activity replacement during a challenge cannot extend the deadline or lose the rollback target
13. Inject a black output frame with continuing event processing: Kotlin overlay stays visible and timeout restores live
14. Inject a render-thread stall after trial arming: Kotlin watchdog durably rejects and uses bounded process recovery without waiting on a GL-held lock
15. Kill during an unvalidated renderer attempt or partial config repair: next launcher/startup repairs accepted values before GL initialization; merely staged edits still survive
16. Fail record sync/rename and config batch publication: no false acceptance, preserved old accepted snapshot, repair marker retained and surfaced failure
17. Race OK against expiry and a stale callback; exactly one outcome wins, deadline equality cancels, and unrelated config/pilot values survive rollback
18. Single-player pause ownership balances across existing settings trays; multiplayer keeps processing its session during the prompt
19. Known MSAA patterns resolve to stable single-sample pixels; native menus remain visible across early/final resolve paths, without clipping or subview wipe
20. On the Retroid, off/2x/4x in both color-depth modes survive cold launch, live toggles, menu transitions and resume; structural MSAA failure rejects a challenged configuration and retains its diagnostics through accepted-settings rollback

Run the Retroid Pocket 4 Pro device checks for actual filtering/MSAA/AF and context/mode restoration. Emulator fault injection verifies coordination and recovery; it cannot establish that the physical device's GPU driver is fixed

## Requested popup entry-point verification, 2026-10-02

Completed on the physical Retroid with APK 23630 for both D1 and D2. The emulator attempt was stopped after detecting another full-suite process using that device; it is not counted as validation. All requested popup entry points pass their behavior checks. The additional renderer-health assertion fails after resolution/context rebuild in both engines, so full mode-restoration validation remains open

FOV review: both engines perform the original-FOV pass for gameplay visibility, homing candidates, automap and demo bookkeeping, then a visual-only custom-FOV pass. The original bookkeeping needs to survive; its full GPU drawing could be removed by a separately verified visibility-only implementation. This verification task does not change that rendering design

### Physical entry-point results

Evidence: `android/temp/retroid-popup-20261002-083353/`, including `results.json`, individual executable automation scripts/results, full native snapshots, `final-logcat.txt`, a post-rollback screenshot and before/test/restored app-data archives

| Entry point | D1 | D2 | Observed behavior |
| --- | --- | --- | --- |
| Launcher protected-settings command | 13/13 | 13/13 | Real launcher publication of filtering/MSAA/resolution/color depth stays unchallenged in intro/main menu; first level shows the Kotlin modal with Cancel selected; timeout restores the accepted tuple |
| Native Graphics Options before a level | 24/24 | 24/24 | Real filtering radio edit stays unchallenged in Options and main menu; first gameplay shows the modal; D-pad selects OK and A durably accepts |
| Native Graphics Options mid-level | 16/16 | 16/16 | Real filtering radio edit stays unchallenged through the graphics submenu and outer Options menu; closing all menus shows the modal; B restores accepted filtering |
| Live Video Info overlay | 52/52 | 54/54 | Real controller edits batch filtering/AF, default Cancel via A, MSAA cancellation via B and timeout; underlying values refresh, pause ownership and selected row survive; edit-back suppresses the prompt |
| Native Screen Resolution | Renderer check fails | Renderer check fails | Real 800x600 edit defers through menus, shows the modal on returning to gameplay, and timeout restores 1334x750 and accepted filtering; context generation advances twice and owned-target pixel probes pass, but scene GL errors continue afterward |

The launcher baselines included accepted AF/HUD filtering, so rollback explicitly retains prior enhancements instead of forcing all-off. Candidate launcher color depth was RGBA8888 and accepted depth RGB565. Controller introspection verifies the Kotlin overlay itself, rather than relying solely on the native challenge phase

An additional D1 run passes 21/21 steps: first-draw `Cancel (5)` changes to `Cancel (4)`, B cancels, a native edit followed by editing back does not prompt, and all-off settings remain unchallenged for six seconds without replacing the accepted enhanced tuple

### Newly reproduced mode-rebuild diagnostic failure

Both native-resolution runs fail their final `msaa.last_scene_gl_error == 0` assertion with 1282 (`GL_INVALID_OPERATION`). D2's live overlay run has zero scene errors before the resolution change; errors accumulate after the change/rollback. The same error is present after the earlier combined launcher-mode timeout. MSAA is off after restoration, resolve failures remain zero, and the owned color/resolve probe passes with restored GL state. The D1 post-rollback screenshot shows the scene and cockpit still rendered. This is separate evidence from the earlier RGB10_A2 MSAA resolve defect

A likely source from code review is stale GPU timer queries: normal mode replacement destroys the EGL context, while query IDs and ring state are cleared only in `ogl_smash_texture_list_internal`'s lost-context branch. The timing helper retains nonzero query IDs across normal replacement. This is a hypothesis, not a confirmed attribution; no speculative renderer fix was made during the entry-point test task

Follow-up:

- Add bounded diagnostics around GPU query read/begin/end operations across ordinary mode replacement, identifying old/new context generations and GL errors at the operation that produces them
- If confirmed, retire timer resources while their owning context is valid and reset their IDs/ring state for every new context, using shared Android code and minimal D1/D2 hooks
- Repeat launcher and native resolution rollback on the Retroid in both engines and both requested color depths; require zero new scene errors as well as the existing popup, tuple, context and pixel assertions
- Keep the custom-FOV visibility-only optimization separate: preserve original render-list, homing, automap and demo side effects, draw only the requested view, and verify replay/gameplay equivalence and CPU/GPU frame cost before removing the baseline draw

The app was stopped before restoration. SHA-256 comparison verifies all 57 original backed-up files were restored exactly, with no extra files in the restored files/shared_prefs trees. Temporary D1 data and test saves were removed with the isolated test tree. USB stay-awake was restored, and the launcher again selects the original default set and player.sg9 resume at 281 seconds. No production source or APK changes were made in this verification task

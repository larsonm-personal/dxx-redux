# Overnight full test and regression run

- Read repository instructions and preserve existing user changes
- Run `android/run_all_tests.ps1 -Serial emulator -FullSuite -FullExtracts -ExtendedGraphics -ExtendedMultiplayer` without stopping on failure
- Save complete console output and runner reports under `temp/overnight_full_suite`
- Inspect every failure, timeout, infrastructure skip, and unexpected unexecuted test
- Diagnose failures using durable automation results and targeted logging; fix causes without replacing regression expectations
- Run relevant builds and focused checks for fixes, and format changed source with the scoped quality runner
- Repeat the complete exhaustive suite until a clean run; report any genuinely unavailable manual or external coverage separately
- If extraction checks require additional host workflow coverage, run the canonical CD regression wrapper against existing oracles
- After validation against existing expectations, run `android/regenerate_all_regression_data.ps1 -Category All` serially to cover the canonical regression workflow's CD, fingerprint, mission metadata, and GuideBot simulation stages
- Review regenerated changes and failures; regeneration success alone does not establish replay or extraction fidelity

## First run status

- Full runner in exec session `42376`, suite process ID `28832`, finished with exit 1
- Reports: `temp/overnight_full_suite`, run identifier `20261012_234500`
- Android debug and instrumentation build and emulator/server preflight passed
- Complete first run: 303 passed, 16 failed, no timeouts, 21 skips, no top-level tests unexecuted, elapsed 06:50:53
- Report: `temp/overnight_full_suite/report_20261012_234500.md`
- User/concurrent work is updating code-management ledgers and plans; preserve those changes
- Failures: Android KTX2 mipmaps, Android pause state, SDK lifecycle, async checkpoint, raw controller inputs, graphics first run, hires release, idle saver, LAN orphan restart, mod fuzz, mod save toggle, robot preview, all extracts, Mac SAF extraction, co-op pause, multiplayer
- All-extracts owner stopped at source 19/39 after ADB infrastructure failure; remaining extraction sources are not yet verified

## Fix and reproduction progress, October 13

- KTX2 fixture reproduced; real cockpit asset is now 2048px rather than 4096px. Updated actual-pack test to retain one-level mip fallback with a 1024px simulated device cap. Both games passed all 48 actual engine cases in `ktx2-fixed-followup.log`, session `64230`, exit 0
- Fresh mod fuzz runs now allocate separate timestamped run directories instead of rejecting the previous campaign journal
- Scoped quality for the PowerShell and KTX2 fixture changes passed in `fixes-quality-01.log`
- Initial fresh fuzz run captured existing-pilot selection failure and disk exhaustion; emulator data partition was 99% used. Captured cases remain under `android/temp/mod-fuzz-smoke/run_20261013_071612979`
- Stopped owned diagnostic campaign, restored original stay-awake value 2, cleared only disposable emulator test app data, reinstalled retained suite APK and standard data. Available data space increased to 2.4GiB
- Fuzz generator now clears pilot files for new-game scenarios. An added explicit D1 Enter advanced past the main menu because optional player selection already accepted the input; removed that extra Enter after fresh introspection proved the cause
- Superseded clean smoke session `10718` was stopped after preserving its results; full smoke retry is live in session `11079`, log `mod-fuzz-pilot-fixed-followup.log`
- Do not overlap device checks while the fuzz smoke owns emulator-5554
- Async checkpoint mismatch diagnosed: full disk checkpoints now include `mod_save_write` payload before Android metadata, whereas synchronous memory reference intentionally excludes that payload. Fix fixture to compare against full synchronous disk reference without dropping mod coverage; same assumption appears in archive checkpoint automation
- Pause fixture expected a D2 level-2 briefing but final evidence shows live level 2 with no presentation. Inspect D2 briefing policy rather than blindly changing runtime
- Remaining failures need reproduction and repairs before another complete exhaustive run and canonical all-category regression workflow

## Verification handoff at 07:30

- Mod fuzz default-directory and existing-pilot setup fixes passed D1, D1-in-D2 and D2 baselines. Remaining imports exhausted the old partition even after app cleanup
- Stopped superseded smoke, retained case evidence, and restored stay-awake setting before reprovisioning
- Primary disposable AVD config now requests 16G, original config backed up at `temp/overnight_full_suite/primary-avd-config-before-storage.ini`; wiped only this managed test emulator's userdata. Fresh `/data` reports 16G total and 15G available
- Initial emulator CLI attempt incorrectly applied the system partition size option (limited to 2047MB); corrected startup uses the AVD data partition configuration and verified successful boot
- Enlarged-storage full fuzz smoke is live in session `97100`, orchestration PID `28004`, log `mod-fuzz-large-storage-followup.log`
- Implemented Android-only checkpoint reference helper: disk comparisons use synchronous full save including mod payload, exclude only launcher metadata from equivalence and retain separate full-publication checks. Applies to checkpoint-cycle and archive checkpoint fixtures
- Full Android APK build with checkpoint helper passed, session `12832` exit 0; APK retained at `temp/overnight_full_suite/checkpoint-fixture.apk`
- Scoped quality passed in `fixes-quality-04.log`, session `71766` exit 0
- Serial failure follow-up queue is live in session `72984`, script `temp/overnight_full_suite/run-failure-followups.ps1`; waits for PID `28004` to finish before installing retained APK and running checks
- Queue covers async checkpoints D1/D2, SDK lifecycle D1/D2, Mac SAF, robot preview, controller raw inputs, pause state, first-run graphics, full hires matrix, idle saver D1/D2, mod save toggle and orphan LAN restart
- Queue results publish incrementally to `failure-followups-results.json`; do not start competing device work while either live owner remains
- Full extraction and dual-emulator failures still require follow-up, followed by a complete exhaustive rerun and canonical all-category regression workflow

## Follow-up progress at 07:36

- Enlarged-storage fuzz smoke passed all seven baselines (D1, D1-in-D2, D2, demo D1, demo D2, Mac D1 and Vertigo), then the D1 all-on case; live session `97100` continues
- Co-op pause original failure snapshot contains a null second state. Replaced that one-shot read with bounded fresh-state polling, retaining simulation, network, input and UI assertions
- Mac SAF failure now prints the exact filtered import log before cleanup for diagnosis; source-count expectation remains unchanged
- Added targeted launcher B-button dispatch diagnostics to investigate raw-input dialog dismissal before changing behavior
- Scoped quality for these changes passed, session `89813`, `fixes-quality-05.log`
- Diagnostic APK build session `80034` adds launcher dispatch logs to retained `checkpoint-fixture.apk` after successful build, before the deferred device queue installs it
- Serial network/extraction follow-up queue is live in session `1625`; waits for single-device queue PID `35048`, then provisions both emulators, runs co-op pause for both games, multiplayer with full 90-second soak, and canonical CD regression workflow against existing oracles (no RefreshOracle)
- Network/extraction queue script: `run-network-extract-followups.ps1`; results: `network-extract-followups-results.json`; failures retain both-device logcat
- Full-run completion is still unproven; inspect all queue results, fix remaining demonstrated defects, and rerun the exhaustive runner plus canonical all-category workflow

## Further diagnosis at 07:40

- Controller diagnostic APK build session `80034` finished exit 0 and copied successfully to retained `checkpoint-fixture.apk`
- Original first-run graphics Rearm failure state shows EGL recreation occurred and chooser is already editing, with first_run_pending consumed. Runtime `android_graphics_safety_presented` automatically offers it on the first eligible presented frame after foreground entry
- Fixed Rearm fixture to require that automatic editing state directly rather than wait for a transient idle/pending state and send an extra Escape. Retains EGL count, TexFilt, pending flag, pause, unarmed deadline and cancel/once checks; full matrix is already queued
- Scoped Rearm quality run session `79230`, log `fixes-quality-06.log`
- Preserved original multiplayer host/client phase-8 logcat in `original-mp-host-phase8-logcat.txt` and `original-mp-client-phase8-logcat.txt` before follow-up can overwrite shared files
- Original MP launch rejection followed an actual WebSocket SocketException (software caused connection abort), correctly invalidating network attempt. Fresh full soak remains queued; do not bypass network generation validation
- Enlarged-storage smoke additionally passed all-on D1, D1-in-D2, D2 and demo D1. Device ownership remains the same live serial queue

## 07:41 full mod smoke verified

- Enlarged-storage mod fuzz session `97100` finished exit 0
- All 14 smoke cases passed: seven baseline and seven all-on, including Mac, demo editions, Vertigo, D1-in-D2 and retail games
- Full smoke log: `mod-fuzz-large-storage-followup.log`; journal and summary under latest `android/temp/mod-fuzz-smoke/run_*` campaign
- Single-device follow-up session `72984` has started (PID `35048`), retained APK installed, async D1 check currently executing
- Network/CD follow-up session `1625` remains live waiting for PID `35048`; no overlapping device work

## 07:43 checkpoint fix verified

- Async checkpoint follow-ups passed D1 (47/47 steps) and D2 (62/62 steps), both child exit 0
- `failure-followups-results.json` records both passing results; logs `followup-async-d1.log` and `followup-async-d2.log`
- Full save equivalence now covers mod payloads, disk publication and restore alongside the original in-memory checkpoint path
- Three original failing owners have passing verification so far: KTX2 mipmaps, mod fuzz and async checkpoints
- SDK lifecycle D1 is now the live single-device case; continue polling session `72984` and do not overlap device work

## 07:51 follow-up results and fixture fixes

- SDK lifecycle passed D1 and D2; robot preview passed its full navigation/render/aspect/sound matrix; raw controller test passed all 37 steps without changing UI behavior
- Six original failed owners now have passing follow-ups: KTX2, mod fuzz, async checkpoint, SDK lifecycle, robot preview, raw controller inputs
- Mac SAF reproduced: native log reports 8 extracted files. `extract_sti2_from_hfs` now adds the validated Mac D1 sound-resource extraction to seven matching archive files. Updated strict expectation to 8, preserving both URI modes, staging assertions and exact runtime filenames
- Pause-state reproduced because it calls a `_shareware` save/load fixture with default edition. D2 fixture log explicitly reports shareware and d2demo mission, which lacks the required next-level briefing
- Pause wrapper now explicitly selects retail fixtures; it retains the briefing/UI assertion instead of skipping it
- Full runner now restores standard game data before each single-emulator owner. Shareware fixtures still deliberately clear and SHA256-verify their isolated edition afterward. Prevents shareware default-set replacement from breaking later retail/no-deps owners
- Scoped quality logs `fixes-quality-07.log`, `fixes-quality-08.log`; sessions `58428`, `43395`
- Standard game-data resolution host integration passed; shareware automation profiles passed (including expected negative mixed-data rejection); master catalog passed with 340 entries and 390 support scripts
- Stopped only the idle, not-yet-started network queue PID `2760` to add verification of new Mac/pause fixes and co-op archive checkpoint behavior. Replacement queue session `98596` still waits for live single-device queue PID `35048`
- Replacement network queue covers fixed Mac SAF, fixed pause-state, co-op pause, independent checkpoints (including modified full archive reference), full multiplayer soak and canonical CD workflow without oracle refresh
- First-run graphics full matrix remains live in single-device queue; wait for its Rearm case and final result before counting that original owner fixed

## 07:56 verified wait

- Single-device queue session `72984` / PID `35048` is confirmed live; first-run graphics D1 cases passed through Background, Stall, Unsupported and Crash, with their cross-engine handled checks completed where reported
- Rearm and remaining D2 matrix still pending; no graphics owner completion claim yet
- Replacement network queue session `98596` / PID `27332` is confirmed live waiting for the single-device queue
- Original idle-saver timeout log contains a SCRIPT_BACKGROUND marker but no matching driver background action; investigate marker delivery if the fresh device check reproduces it
- Original hires and mod-toggle stalls occurred across launcher/game transitions; retain fresh launcher results and introspection before attempting runtime changes
- This continuation is a verified wait: polled the current tool session and independently confirmed its process remains live; do not restart it

## 07:58 original Rearm case passed

- D1 Rearm passed all 49 steps, including automatic chooser reopening after recreation, paused editing, unarmed deadline, cancellation and no repeat offer
- D2 Rearm-handled passed all 19 steps, verifying the real shared first-run marker prevents another offer in a new engine process
- Logs retained under `android/temp/graphics-first-run-20261013-074810`
- Full graphics owner now runs D2 fresh-install scenarios; owner completion still awaits all its cases and exit code
- Queue session `72984` / PID `35048` and deferred queue session `98596` / PID `27332` both verified live
- Scoped git diff whitespace check passed for all task code changes

## 08:08 verified wait and fuzz resumption correction

- Full graphics matrix remains live in session `72984`; D2 Background and Stall now passed, with no new graphics failures reported
- Fixed the follow-on default resumption behavior introduced by timestamped fresh fuzz outputs: `-Resume` without an explicit output path selects the latest campaign journal; explicit paths remain untouched and the old fixed-directory campaign remains the fallback when no new journal exists
- Verified the actual PowerShell selector against current campaign directories without running device automation: latest journal, explicit path preservation and fresh-path allocation all passed (`fuzz-output-selector-check.log`)
- Scoped PowerShell quality passed (`fixes-quality-09.log`, session `31837` exit 0)
- Keep both serial queues active; do not claim final success until complete graphics, subsequent failure checks, canonical regressions and the exhaustive rerun all finish cleanly

## 08:17 new graphics launch failure

- Full graphics owner stopped after 41/44 checks passed: D2 Crash passed, but subsequent D1 Crash-handled never launched a game process and timed out after its original 180s deadline
- D1 log stops after preparing game config; cached setup snapshot is only `{screen:setup}`. This is insufficient to identify the blocked launch phase; do not change runtime speculatively
- Single-device queue `72984` continues with the full hires matrix; its original timeout case still needs verification
- Graphics failure capture now retains filtered launcher/route-metadata logcat and requests a fresh full setup snapshot before reporting failure; files are under the case's timestamped report directory
- Replaced only the idle deferred network queue to add a scoped D2 Crash -> D1 handled reproduction before its other cases; new session `72331` waits for single-device PID `35048`
- Scoped quality `fixes-quality-11.log` / session `78665`; prior initial capture quality `fixes-quality-10.log` / session `50976`
- A complete graphics rerun is still required after resolving this startup failure, including the three not-yet-executed checks

## 08:22 current state

- Single-device queue session `72984` / PID `35048` confirmed live; hires stages continue passing through the D1-in-D2 matrix
- Focused graphics launch reproduction remains deferred in session `72331` / PID `28948` after the current serial queue
- Native replay/recorder/fixture files and new auto-recording plan/header are now being edited by another workspace owner. Preserve them; retained focused APK predates those changes. Final exhaustive run must rebuild from the then-current worktree
- Source reading confirms the original launch log's final music-config update occurs near the end of file preparation; without full route-metadata logcat or a fresh launch snapshot, blocked metadata handoff vs stale ownership vs UI scheduling remains unproven
- No speculative runtime fix applied for that stall; capture-enabled focused Crash reproduction is required

## 08:50 verified interference and dedicated test pair

- Hires D1-in-D2 512-on failure exit diagnostics explicitly report PACKAGE UPDATED at 08:25:39; idle-saver exits show USER REQUESTED / FORCE STOP from another PID at 08:26:40
- Independently observed another workspace task running auto-level-demo tests and installing on default emulator-5556, in addition to emulator-5554 use. These outcomes cannot be treated as engine crashes
- Stopped only owned follow-up queue PID `35048` and its idle deferred queue PID `28948`; did not stop or reset either shared default emulator
- Added configurable managed-pair settings: DXX_TEST_PRIMARY_SERIAL, DXX_TEST_SECONDARY_SERIAL, DXX_TEST_PRIMARY_AVD, DXX_TEST_SECONDARY_AVD. Defaults remain 5554/5556 and Nexus5X_Light_1/2
- Full runner inherits configured primary without narrowing to SingleDevice, passes pair parameters to dual-device owners, and fixed internal multiplayer/LAN owners use the configured pair instead of selecting unrelated connected emulators
- Created clean dedicated AVD configs DxxOvernight_1/2 with 16G data partitions; primary emulator-5574 booted and reports 15G available. Secondary emulator-5576 will boot before dual-device checks
- Default and configured target-selection integration tests passed; configured master catalog passed (341 entries, 391 support scripts); managed emulator-start host checks passed
- Isolated single-device follow-up session `52619` / PID `10104` is live on emulator-5574; async checkpoints and SDK lifecycle again passed both games
- Fixed Mac SAF extraction now passed both seekable and pipe modes, exit 0, on the dedicated target
- Deferred dedicated network/CD queue session `94503` waits for PID `10104`; it will provision emulator-5576 and cover paired checks plus full CD workflow
- Shared-device failure logs copied to `temp/overnight_full_suite/shared-device-followup-evidence` before isolated repeats overwrite stable log names
- Pair isolation quality logs: pair-isolation-quality-final.log, session `26121`; automation catalog session `97823`; both must be checked for final exit status
- Do not use or stop default emulator-5554/5556 for this goal going forward; set all four pair environment variables for new commands

## 09:03 isolated reproductions and controller fix

- Configured automation catalog passed: 95 standalone JSON, 391 support scripts, 242 standalone PowerShell tests
- Isolated raw controller test reproduced the original B-dismiss failure; this is a real input-delivery race rather than shared-device interference
- Launcher updates pressed state but consumes B before dialog dispatch; raw dialog otherwise depends on 50ms polling and can miss release. Added B to the dialog-forwarding keys, retaining D-pad/A behavior and existing release handler
- Scoped Kotlin quality passed (`controller-routing-quality.log`, session `95302` exit 0)
- Queued own current-source APK build after independently confirmed pre-existing Gradle wrapper PID `7272`; own build session `10094`, output `controller-routing-build.log`, retained successful APK `controller-routing.apk`
- Corrected retail pause-state follow-up passed both games on emulator-5574, exit 0, including the previously missing D2 next-level briefing
- Retail setup is longer than shareware: D1 full save/load fixture wall time was over four minutes before additional probes. Set only master outer timeout to 1200s for both full retail fixtures and pause probes; individual fixture and assertion deadlines unchanged. Scoped quality passed (`retail-pause-timeout-quality.log`, session `51188` exit 0)
- Focused D2 Crash -> D1 handled reproduction passed on the dedicated emulator (session `52619` queue result `graphics-crash-d2` exit 0); prior shared-device startup stall did not reproduce there
- Full graphics matrix is now running again on the isolated target; do not infer all-case completion from the focused reproduction
- Replaced only idle deferred queue to install newly built controller-routing.apk after PID `10104` completes, then verify fixed raw input and paired/CD cases. Current deferred session `83444`; previous `94503` idle process stopped
- Current old focused APK predates unrelated replay-code edits; own new build and final exhaustive run must incorporate the current worktree

## 09:22 graphics resume driver diagnosis

- Dedicated full graphics matrix failed D1 Rearm after 20 checks; fresh native state showed Game Menu, idle/pending chooser and paused simulation
- Resume used launcher monkey intent followed by BACK. The launcher restored MainActivity directly, so BACK opened Game Menu before a presented frame could offer the rearmed chooser
- Changed driver to explicitly foreground SetupActivity, verify resumed activity within 10s, then BACK to the existing game. All original graphics and lifecycle assertions remain
- Scoped quality passed; test_test_runner_result provisioning/result integration passed
- Full hires follow-up remains live on dedicated primary (session 52619 / PID 10104)
- Replaced only idle deferred queue PID 27808 after confirming empty console; new session 29834 adds focused both-game Rearm and full graphics matrix before paired/CD cases, using controller-routing.apk
- Latest configured master catalog passed 341 top-level entries / 392 support scripts; catalog grew from another owner's ongoing additions
- Resume-driver diff check passed; current hires cases continue passing, queue still live. Do not stop or restart it

## 09:25 repeat preparation / verified wait

- Previous turn made progress: diagnosed and fixed graphics resume driver; queued exact focused and exhaustive verification
- Dedicated hires follow-up still live, with at least 15 completed successful cases and no failure. Deferred paired queue confirmed live waiting for PID 10104
- Prepared and syntax-checked temp/overnight_full_suite/run-complete-repeat.ps1 for the required exhaustive repeat: FullSuite, FullExtracts, ExtendedGraphics, ExtendedMultiplayer, configured dedicated pair and fresh APK selection. Not yet launched while failure follow-ups own the device
- Canonical all-category regeneration remains required after existing-oracle CD validation, plus review of normalized artifact changes

## 09:31 emulator recovery evidence

- Prior turn made progress by preparing the full repeat driver and capturing 74 tracked regression/data hash+mtime baselines; live queues were explicitly verified
- Hires owner reached 18 passing cases, then SetupActivity readiness timed out with emulator ADB transport offline
- Existing runner recovery restarted only configured DxxOvernight_1 on emulator-5574; boot succeeded and automation resumed the interrupted D1-in-D2 case without changing assertions
- No speculative application fix made for infrastructure stall. Keep recovery trace in followup-hires-release.log and require full owner completion

## 09:33 full hires owner passed

- Full hires integration completed all 21 baseline/on/off cases across D1, D2 and D1-in-D2 at 128/256/512, exit 0
- Retained evidence: followup-hires-release.log and android/temp/hires-release/run_cb920ab40ef84a578718fd1abd84fb7f
- Queue now running idle-saver D1, then D2, full mod-save-toggle and LAN orphan restart before handing the pair to focused controller/graphics and paired/CD checks

## 09:35 resume task routing follow-up

- Dedicated idle checks failed the new foreground guard in both games, preserving their original sleep/background assertions
- Explicit am start still uses NEW_TASK by default and can restore MainActivity at the existing task top. Added REORDER_TO_FRONT (0x00020000), already used successfully by LAN return lifecycle tests, to select SetupActivity before BACK
- Added bounded failure diagnostics of am output and resumed activity lines
- Scoped quality session 75946 / resume-reorder-quality.log; runner contract recheck queued separately
- Replaced only idle deferred queue after checking its exact PID 27420 and empty console. New session 34880 adds both idle regression repeats before focused/full graphics and paired/CD owners
- Full mod-save-toggle currently active on primary; do not start competing device tests
- REORDER_TO_FRONT scoped quality passed (session 75946 exit 0); result/provisioning recovery contract passed (session 5497 exit 0)
- Isolated mod-save-toggle D1-in-D2 reached step 42/47 and cold resume, beyond original step 33 launcher-handoff failure. Require final all-four-mode owner exit before claiming pass

## 09:37 full mod-save-toggle owner passed

- All four modes (D1, D2, D1-in-D2, Vertigo) completed installed-mod disable, re-enable, exact resave and cold-resume assertions, exit 0
- Single-device queue now runs final LAN orphan restart, after which deferred controller/idle/graphics/paired/CD queue can start
- LAN orphan restart passed on isolated target, including healthy return PID preservation and fresh-process replacement
- Session 52619 terminal exit 1 reflects earlier controller/graphics/idle failures awaiting newly patched repeats; 11 owner/case rows passed, 4 failed
- Deferred session 34880 / PID 29228 now active, provisioning secondary emulator-5576 (DxxOvernight_2); no competing dedicated-device queue remains

## 09:40 follow-up orchestration correction

- Secondary dedicated emulator booted successfully
- Temporary network queue arguments used @('-'+$entry.Name, $entry.Value), which PowerShell evaluated into a combined parameter argument. Children rejected `Serial emulator-5574`; none of these rows represent executed device tests
- Stopped exact owned queue PID 29228 and its child PowerShell scripts; preserved invalid console as invalid-pair-argument-console.log
- Corrected argument construction to @("-$($entry.Name)", $entry.Value); verified count=2 and exact flag/value. Restarted queue session 52602, devices remain running
- Main run_all_tests pair argument construction already uses separate interpolated strings and is unaffected

## 09:43 controller pass and resume introspection fix

- Real controller-raw-fixed repeat passed all 37 steps, exit 0, including B-dismiss previously failing at step 24
- Resume guard failed despite am return-to-task success. Direct bounded Adb-Timeout introspection returned ResumedActivity: .../.SetupActivity, without mResumedActivity or topResumedActivity
- Added actual ResumedActivity: field form to foreground pattern; verified against live current device dump (True). Original driver deadline and idle/native assertions unchanged
- New scoped quality session 54679 / resume-activity-pattern-quality.log. Full graphics owner may load helper after patch; inspect actual results and repeat idle cases after current queue if they used previous loaded helper
- Pattern quality passed (session 54679 exit 0). Current paired queue session 52602 / PID 35480 remains live
- Focused Rearm fixture started at 09:42 with previously loaded helper and stalled before game surface creation; captured full graphics-startup-stall-logcat.txt and requested owned game PID SIGQUIT stack diagnostic before full graphics owner proceeded
- Explicit controller fix is verified; idle failures using prior helper must be repeated with corrected ResumedActivity form

## 09:49 host renderer system hang diagnosed

- Prior turn made progress: real controller fix passed; foreground regex fixed from actual device evidence; captured startup diagnostics
- Android Dropbox system_app_anr at 09:41 identifies com.android.systemui SCREEN_ON broadcast ANR; main stack blocked in libhwui HardwareRenderer.syncAndDrawFrame / pthread_cond_wait, graphics allocator and SurfaceFlinger high kernel CPU
- Subsequent D1 startup reached resumed MainActivity but never created game surface; keyguard showing remains true. No engine regression inferred from this contaminated Android system state
- Saved full system ANR report in graphics-system-anrs.txt, window policy and logcat; host-renderer test logs and invalid/incomplete result rows preserved in host-renderer-followup-evidence
- Stopped exact owned queue PID 35480 and child test PowerShell scripts to recover Android system, leaving default emulator-5554/5556 untouched
- Restarting dedicated pair with swiftshader GPU renderer and hardware CPU acceleration (DXX_ANDROID_SOFTWARE_EMULATOR remains unset). Primary boot session 75336; secondary boot started separately. All original assertions and test scope retained
- After both boot calls finish, rerun full follow-up queue session against same retained controller-routing.apk and corrected foreground pattern, then required exhaustive repeat
- Both software-GPU boot calls completed exit 0 (primary session 75336, secondary 31516). Live process commands confirm -gpu swiftshader, no SoftwareCpu/accel-off option
- Existing emu_health.ps1 recovery reads and preserves live GpuRenderer settings; no code change needed to maintain renderer across recovery
- Restarted complete dedicated follow-up queue with WaitForProcess -1 after previous queue terminal. Controller/idle/rearm/full graphics/paired/CD scope unchanged

## 09:53 software renderer repeats passed controller and idle

- Current queue session 70315 / PID 30024 live on dedicated SwiftShader pair
- Controller raw repeat passed 37/37 steps again, exit 0
- Idle screen saver D1 and D2 both passed all 69/69 steps, exit 0, including real sleep/background/resume, pause, audio, checkpoint and presentation assertions with corrected foreground guard
- Graphics Rearm owner now running. Full graphics matrix, paired checkpoints/co-op/MP, complete existing-oracle CD workflow, canonical all-category regeneration and exhaustive repeat remain required

## 09:56 rearm return-input diagnosis

- Previous turn made progress: diagnosed SystemUI renderer ANR, booted isolated software-GPU pair, controller and both full idle scripts passed
- Focused Rearm now starts and restores EGL, but native state still shows Game Menu, idle/pending chooser, TexFilt 0
- Logcat confirms BACK closes SetupActivity during transition then MainActivity resumes; native Game Menu appears despite correct Setup foreground guard
- Replaced resume-driver BACK with existing SETUP_COMMAND launch returnToGame action (same return action verified by LAN healthy-process test). Existing game is returned before any new launch is attempted; rearm marker broadcast remains before resume
- Full graphics matrix ongoing; each run_test child loads current helper, so later Rearm cases can verify new return action. Final exhaustive repeat still required to verify a uniform complete run
- Scoped quality session pending / resume-launch-action-quality.log
- Resume launch-action quality passed (session 88286 exit 0), scoped diff check clean
- Full graphics matrix currently passing through D1 Timeout plus opposite-engine handled checks. Queue remains live on PID 30024; wait for background/rearm coverage and paired/CD owners before exhaustive repeat

## 10:01 resume launch action integration evidence

- Previous turn made progress: replaced handoff BACK with existing launcher return action and passed scoped formatting
- D1 Background full graphics scenario passed 31/31 steps with the new return action, including challenge recovery and observed foreground lifecycle. Full owner still running
- Do not reuse the earlier focused Rearm failure as current result; it predates return-action change. Upcoming full matrix Rearm must pass both engines and cross-engine marker handling
- D1 Background and D2 Background-handled passed with new launcher return action; no new post-reboot SystemUI ANRs found in Dropbox dump
- Added syntax-checked dedicated SwiftShader preflight to full repeat scratch driver, preserving accelerated CPU, full scope and fresh APK build. It verifies live renderer or boots missing dedicated AVDs before launch; default devices are untouched
- Full graphics queue remains live; no restart required while current cases continue

## 10:06 D1 rearm fix verified

- Prior turn made progress by adding full-repeat renderer preflight and gathering current full matrix evidence
- D1 Crash and D2 Crash-handled passed
- D1 Rearm now passed all 49/49 steps with launcher return action, exit 0. Native chooser automatically opened after EGL recreation, retained editing without deadline, canceled and stayed handled
- This directly resolves the previous Game Menu/idle-pending failure without relaxing assertions
- Full graphics owner still needs remaining opposite-engine handled case and all D2 scenarios; current queue session 70315 / PID 30024 is live

## 10:08 scope audit against repository testing guide

- Previous turn made progress: D1 Rearm passed actual all-49-step integration; current queue remains live
- Read android/tests/README.md: FullSuite runs every retained unattended scenario and route case; FullExtracts / ExtendedGraphics / ExtendedMultiplayer independently expand required test coverage. These are exactly the exhaustive repeat flags
- Corrected earlier self-imposed regeneration requirement: user requested full test and regression suites, not maintenance refresh of checked-in oracles/fingerprint/metadata datasets
- Canonical all-category regeneration includes CD -RefreshOracle and fingerprint -Force, which are explicit maintenance actions rather than additional regression validation. Do not run it solely to satisfy this test request
- Completion still requires a complete clean exhaustive run including full native/JVM owners, all headless/full-graphics replay runs, full physical routes, all extraction variants and extended multiplayer, plus existing-oracle full CD workflow already queued
- Preserve existing replay/CD expectations; only update normalized test results when authoritative comparison succeeds

## 10:10 verified wait

- Prior turn made progress by auditing scope against authoritative tests README and correcting unrequested maintenance regeneration expansion; full regression validation scope is preserved
- Current queue PID 30024 remains live; full graphics matrix reached 26/44 passes with D2 Accept/Cancel and opposite-engine handled cases passed
- No new failure or recovery required. Do not start competing device work, restart live tests, or treat partial matrix as complete
- On terminal paired/CD queue: inspect all actual owner results, fix any real paired/extraction failures, then launch prepared full exhaustive repeat driver. All-category maintenance regeneration is excluded per scope audit above

## 10:13 verified wait and multiplayer provisioning audit

- Prior turn was a verified wait on live queue PID 30024; it remains live now with additional D2 Back and Timeout passes
- Read multiplayer provisioning source: test_mp.ps1 ensures standard retail game data for both configured targets even with SkipBuild, so previous shareware owner state does not silently become its prerequisite
- Current full graphics matrix continues without failure. Keep original timeout and complete all 44 checks before calling owner clean

## 10:15 verified wait and fix review

- Previous turn was verified wait on active PID 30024; queue remains live with additional D2 graphics passes
- Reviewed current checkpoint-reference C++ diff: new helper is guarded by __ANDROID__; unrelated newdemo/laser includes and level_demo_inventory automation edits belong to other ongoing work and are preserved
- No additional source change justified while full graphics checks continue passing. Paired checkpoint coverage is still pending in the same serial queue

## 10:18 CD current-build selection fix

- Canonical CD workflow requests test_all_extracts -BuildAndInstall; Install-ApkOnDevice prefers inherited DXX_TEST_APK, so it would reinstall retained parent APK despite claiming to build/test current source
- Explicit BuildAndInstall now copies Get-AndroidTestApkPath (fresh Gradle output) into its already-created ReportDir/app-debug.apk and sets child DXX_TEST_APK to that exact retained build before restart/install
- Parent process and ordinary master extraction owner (without BuildAndInstall) retain their selected APK unchanged
- Scoped quality invocation exit 1, inspect before retry; extraction workflow integration session 95479 pending
- Full graphics owner remains live, passed D2 Unsupported and Crash; all-case completion still pending

## 10:20 full graphics matrix clean

- Full first-run graphics owner passed all 44 checks, exit 0, covering 11 scenarios in both games and opposite-engine handled-marker checks
- D2 Rearm passed 49/49 steps at 10:18:50; D1 Rearm-handled passed 19/19 at 10:19:20
- Evidence: followup-graphics-full-fixed.log and android/temp/graphics-first-run-20261013-095324
- Earlier focused Rearm failure in queue results predates final launcher-return fix; preserve it as failure evidence, do not rewrite it into a pass
- Extraction workflow integration passed (session 95479 exit 0). Scoped extraction quality retry session 18574 remains running after external formatter PID 15320 finished; wait for completion before further edits
- Queue session 70315 / PID 30024 continues retained-APK checks and then paired/CD owners. Full exhaustive repeat remains required after remaining failures are resolved

## 10:21 CD APK fix quality complete

- Scoped extraction APK selection quality passed (session 18574 exit 0); extraction workflow integration had passed (session 95479 exit 0); scoped diff check clean
- ReportDir is initialized and retention-guarded before explicit BuildAndInstall, so retained app-debug.apk is within that run's artifacts
- Current device queue still live PID 30024; full graphics owner is terminal clean, focused crash follow-up underway before Mac/pause/paired/CD checks
- Original exhaustive extraction failure at source 19 (Destination Quartzon Europe) was ADB staging/daemon transport failure, not an oracle mismatch: chunk append mismatch, failed server restart on tcp:5037, and later pidof timeout (exit 98)
- Full existing-oracle repeat must finish all 39 sources. Watch global ADB transport as well as isolated emulator health; no extraction behavior fix justified by this original transport evidence alone

## 10:23 current-APK Mac repeat clean

- Previous turn made progress: verified extraction current-build quality and identified original CD failure as global ADB transport/staging failure, not oracle mismatch
- Latest retained controller-routing.apk Mac SAF owner passed both seekable and pipe URI paths, exit 0
- Focused D2 Crash -> D1 handled also passed, exit 0, after full 44-case graphics matrix
- Current queue PID 30024 now running retail pause-state owner against retained current APK; paired co-op/archive/MP and full existing-oracle CD remain next

## 10:25 current-APK pause owner clean

- Prior turn made progress: confirmed both fresh Mac SAF URI paths passed and inspected original transport failure evidence
- Retail pause-state owner passed both games on retained controller-routing.apk, exit 0. D1 fixture 85/85, D2 fixture 86/86 plus live pause/presentation probes completed; results.json records both PASS
- Queue PID 30024 now running co-op pause against configured dedicated pair; independent checkpoint archives, matchmaking MP and full existing-oracle CD follow afterward

## 10:27 paired co-op progress

- Previous turn made progress: current APK retail pause owner passed both games, queue advanced into paired co-op
- D1 co-op host/join synchronized successfully with two players on emulator-5574/5576
- Host held-fire quick-load/tray/music and native menu/save/load checks completed; same probes now running on join device. No new failure reported
- Queue PID 30024 remains live; wait for full D1+D2 owner and independent checkpoint archive owner before claiming paired fix verified
- D1 paired co-op completed host and join native menu/save/load/held-fire probes plus host quick-save/restore and guest rejection; wrapper advanced to D2
- D2 host/join synchronization now succeeded with both players. Owner still pending final D2 pause/menu/save assertions; do not infer both-game completion from D1 pass

## 10:33 paired co-op owner clean

- Previous turn was verified wait on live queue PID 30024 with D1 owner success and D2 progressing
- Full co-op pause/menu owner passed both engines, exit 0, including host and guest menu/input blocking, simulation/network progression, native save/load, host quick-save/load, guest rejection and restore while guest menu open
- D1 evidence android/temp/coop-pause-20261013-102536; D2 evidence android/temp/coop-pause-20261013-102902; followup-coop-pause.log
- This verifies the bounded fresh time_paused snapshot fix without relaxing simulation/network assertions
- Queue now running independent-checkpoints owner to validate synchronous disk reference / co-op archive comparison, then 90s matchmaking soak and full existing-oracle CD workflow

## 10:35 independent checkpoint progress

- Prior turn made progress: full co-op pause/menu owner passed both engines; queue advanced to independent archives
- Paired checkpoint owner verified independent host/client local histories and completed host restore; it advanced into the next D2 scenario
- Current queue PID 30024 remains live. Require complete wrapper success for archive-reference fix validation, then matchmaking soak and existing-oracle CD validation

## 10:36 paired checkpoint owner clean

- Independent checkpoints owner passed both engines, exit 0, including different host/client histories and repeated authoritative restore verified on both peers
- D1 pass at 10:33:35; D2 pass at 10:35:23; evidence followup-independent-checkpoints.log and owned checkpoint artifacts
- This completes paired integration coverage for the corrected synchronous disk reference, alongside previously passing D1/D2 async checkpoint fixtures
- Queue PID 30024 now running matchmaking MP with 90s soak. Full existing-oracle CD validation follows; exhaustive repeat driver remains ready after remaining failure checks finish

## 10:39 multiplayer soak clean, CD workflow active

- Previous turn made progress: independent checkpoint owner passed both engines, verifying paired archive reference change
- Matchmaking MP with 90s sustained connectivity passed, exit 0, two Counterstrike players remained in gameplay and relay packets continued
- Queue PID 30024 now runs complete canonical CD workflow against existing specs, no RefreshOracle or SkipLaunch; build/install step uses new retained current-source APK selection fix
- The original exhaustive extraction failure was infrastructure at source 19; full CD owner must complete all 39 source imports/launches before that failure is considered resolved
- After terminal CD workflow, inspect complete report and artifact changes, then launch run-complete-repeat.ps1 with all exhaustive flags and fresh build

## 10:43 CD host extraction and artifact review

- Previous turn made progress: 90s matchmaking soak passed and canonical existing-oracle CD workflow started
- Canonical CD child PID 35956 remains live under queue PID 30024; host extract-all child PID 24540 is running -Force, with zero extraction errors reported through late D2 sources
- Reviewed current game_data diff: only Descent - Mac macplay and d1 mac 2nd bin+cue track_hashes.json file counts change 8 -> 9; underlying disc SHA1 hashes are unchanged, and no timestamp-only diff appears
- Counts reflect additional extracted Mac resource, consistent with corrected SAF fixture expectations. Preserve semantic output changes for validation; do not regenerate oracle specs to hide differences
- Android import/launch stage still pending; all 39 source coverage remains required, then full exhaustive repeat
- Canonical workflow advanced through host CD extraction, GOG extraction and existing-spec validation successfully, then began current Gradle debug build for all 39 device extraction/launch checks
- Build log shows all three Android ABI native configurations (arm64-v8a, armeabi-v7a, x86_64) proceeding; do not start another build or formatter while active

## 10:45 CD fresh build successful

- Current-source Gradle debug build succeeded in 2m09s across all Android ABI configurations
- Exact fresh APK retained under temp/extract_regression_reports/run_20261013_104213/app-debug.apk; SHA256 matches latest Gradle output, directly verifying inherited older APK is superseded by explicit BuildAndInstall
- Canonical workflow restarting only configured primary for clean device extraction preflight; existing-oracle full 39-source import/launch stage still pending
- No replay/CD oracle refresh performed. Existing Gradle deprecation notice is unchanged toolchain output; no new compiler failure reported

## 10:47 CD device preflight verified wait

- Prior turn made progress: host CD/GOG extraction and spec validation passed; fresh ABI build succeeded and retained APK SHA256 matched current Gradle output
- Dedicated primary reboot completed (sys.boot_completed=1), live process retains -gpu swiftshader; exact fresh APK installed on emulator-5574
- Canonical full extraction child PID 37436 remains live under CD PID 35956 and queue PID 30024, selecting All with BuildAndInstall and RestartDevice
- Original device health and launcher preflight running; do not start competing tests or restart a verified live owner

## 10:53 recurring ADB client connection failure investigation

- Actual 39-source CD import reproduces shared ADB connection failures during 16MB chunk staging; first source passed after recovery, second source repeated failed server connection / daemon startup messages
- Shared adb server PID 25800 remains alive since 10/11 with listening tcp:5037; roughly 962 time-wait sockets, no global port exhaustion indicated (all TCP connections under 1200)
- Local tools all use C:/local/android-sdk/platform-tools/adb.exe 37.0.0 protocol 1.0.41. No evidence yet for binary version mismatch or engine/oracle fault
- Started independent ADB server tcp:5038 via documented adb -P option. Initial device discovery only shows default emulators; investigate registration of dedicated high-port pair before switching active tests
- Original queue/CD/extraction owners remain live; do not restart solely on observation timeout. Any controlled restart must be for concrete transport isolation verification and repeat full 39-source validation

## 10:55 controlled ADB server isolation

- Separate server tcp:5038 started using official local adb -P option; ADB_SERVER_SOCKET=tcp:localhost:5038 verified and discovers all four emulators including dedicated 5574/5576 after startup scan
- Shared-server CD staging repeated transient client-connect/daemon-start failures on first two large Mac images despite persistent shared server; both progressed after retries, no oracle mismatch demonstrated
- Stopped exact owned queue PID 30024 and canonical CD PID 35956 descendants for transport isolation experiment, not because observation expired. Shared adb server PID 25800/default emulators untouched
- Preserved interrupted report and full CD log in temp/overnight_full_suite/shared-adb-cd-evidence; retained fresh build as cd-current-build.apk
- Dedicated primary was stopped with its owned CD descendants; rebooting with swiftshader and isolated-server env (session 65341). Secondary remains running
- Next: verify actual 16MB chunk transfer/read/append through private server, then repeat entire canonical existing-oracle CD workflow and prepared exhaustive full suite using same server env

## 10:58 isolated transport probe passed; full CD repeated

- Actual extraction functions loaded from AST (Invoke-AdbRaw, Quote-ShArg, size readers) copied five real 16MB Mac image chunks through tcp:5038, chmod/stat verified each, appended 80MB into app-private storage and verified every cumulative size; no retry or timeout, probe exit 0 in about 4s
- Temporary device probe files removed exactly; local source chunk/log remain in repo temp
- Transport-isolated canonical full CD workflow now started with all four pair settings, ADB_SERVER_SOCKET=tcp:localhost:5038 and JDK21. No scope reduction, no oracle refresh
- Added same private server env to prepared exhaustive repeat driver; shared server/default devices remain untouched
- Current isolated CD session/PID recorded by tool and private-adb-cd.pid; durable console private-adb-cd-regressions.log and terminal private-adb-cd.exit

## 11:01 private CD verified wait

- Previous turn made progress: exact source transport function probe passed 80MB without retry; complete canonical CD workflow restarted on isolated server
- Current workflow outer PID 7132, canonical child PID 8356, unified exec session 38254 confirmed live; host extraction progressing through late D2 sources without errors
- Shared ADB server PID 25800 remains listening on 5037; isolated server PID 2140 on 5038. Default emulator activity remains untouched
- Require all 39 device source results; passing 16MB probe alone does not resolve full extraction coverage or prove a clean run

## 11:05 private CD build stage verified wait

- Previous turn was verified wait on active private workflow PID 7132 / child 8356, session 38254
- Host CD extraction, GOG extraction and existing-spec validation passed again; private workflow advanced to test_all_extracts -All -BuildAndInstall -RestartDevice, child PID 36420
- Fresh APK Gradle build reached packaging/assemble tasks across all three Android ABIs. No source failure or oracle change reported
- Require full 39 imports/launches through tcp:5038, then launch exhaustive repeat with private transport and dedicated software-GPU pair
- Private workflow fresh Android build succeeded in 2m04s; primary clean restart retains swiftshader. Extraction child PID 36420 remains live, no full import result yet

## 11:07 private full-image staging evidence

- Previous turn was verified wait: private CD fresh build and device preflight completed, full 39-source stage started
- First complete Mac CD image imported and launched successfully through tcp:5038 with zero staging retry/daemon-connect errors; workflow advanced to second large d2 Mac image
- This broadens transport evidence beyond the 80MB probe but does not yet certify all 39 sources. Current owner PIDs 7132 / 8356 / 36420 remain live
- Keep existing oracle expectations, full launch checks and original per-case assertions. No staging code compensation needed while isolated transport succeeds

## 11:09 first two private imports clean

- Prior turn made progress: first complete Mac CD import/launch passed through isolated transport without retries
- Second d2 Mac full image imported and launched expected Ahayweh Gate successfully at 11:08:17, also with no transport retries
- Source 3 (Anniversary Brazil) now active. PIDs 7132/8356/36420 remain live
- Searched harness for explicit shared-server port forcing: none in commands; only recovery regex and fixture text mention 5037. Private env remains effective across workflow descendants
- Keep full 39-source workflow serial; no staging code change justified while private imports pass

## 11:13 private extraction verified wait

- Previous turn made progress: both large Mac sources passed full imports and expected-level launches with no retry
- Current private CD owner PIDs 7132/8356/36420 remain live, now five completed source PASS results and zero daemon/staging failure messages
- Levels of the World source import completed and file verification running; retain all 39 planned sources and each defined launch variant
- No new failure or source fix warranted. Full clean exhaustive repeat still required after complete CD workflow
- Source 8 (Test Flight demo) is explicitly non-launchable per existing spec/runner and passed file-only verification; this is defined coverage, not a new SkipLaunch reduction
- Earlier RESULT:PASS console counts omitted file-only results. Use incremental summary.json Results statuses for authoritative completed-source totals going forward

## 11:16 authoritative CD progress

- Private CD report is temp/extract_regression_reports/run_20261013_110128/summary.json; it is written incrementally after each source
- Latest summary records nine PASS sources and no failures; use results rather than launch-only console RESULT counters
- Live owner PIDs remain 7132/8356/36420, session 38254. Full 39-source completion and exhaustive rerun still pending

## 11:18 full-repeat isolation audit / verified wait

- Previous turn made progress by identifying authoritative incremental extraction summary and confirming 9 PASS sources, all first attempts
- Private CD owner PIDs 7132/8356/36420 remain live and continue adding PASS sources, no failures
- Reviewed prepared repeat script and master cleanup/recovery source: private ADB env preserved, dedicated pair selected, automatic stale cleanup explicitly targets configured primary, shutdown stops only started configured emulators
- No new source change needed. Keep CD tests serial and launch full exhaustive repeat only after full workflow finishes and results are reviewed

## 11:23 private CD verified wait

- Previous turn was verified wait on live private workflow with 11 PASS sources and scoped full-repeat isolation audit
- Incremental report now records 12 PASS sources and no failures; current European Definitive Collection disc import completed file verification and launch automation is running
- PIDs 7132/8356/36420 remain live; session 38254 not terminal. Do not interrupt, narrow scope, or start competing device tests
- Full 39-source result, artifact review and exhaustive full-suite rerun remain outstanding

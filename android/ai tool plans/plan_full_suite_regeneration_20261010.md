# Full test suite and regression regeneration

## Scope

Validate the current working tree, preserving pre-existing changes around mods,
saves, graphics, launcher behavior, and multiplayer

## Plan

1. Run the full unattended suite with FullSuite, FullExtracts, ExtendedGraphics,
   and ExtendedMultiplayer, capturing durable output
2. Investigate every failure using test logs and targeted reproductions; fix
   underlying defects without assuming flakiness or weakening expectations
3. Run all regression regeneration categories: CD extraction, fingerprints,
   mission metadata, and GuideBot engine route simulations
4. Format changed implementation files, rerun affected tests and complete the
   full validation; record exact results and any remaining limitations

## Evidence

- Initial working tree contains existing tracked and untracked changes; do not
  reset or overwrite unrelated work
- Canonical entry points: android/run_all_tests.ps1 and
  android/regenerate_all_regression_data.ps1
- Emulator tests and regeneration must run serially because they share state
- First run: Android build succeeded for all three ABIs; preflight failed with
  INSTALL_FAILED_TEST_ONLY before any tests ran. The captured app APK contains
  android:testOnly=true. Added ADB -t to debug/test installation paths, including
  instrumentation installation. The same captured APK then installed successfully
- Scoped PowerShell formatting passed for all eleven changed installation scripts
- Closed orphaned Nexus5X_Light_1/2 instances on ports 5588/5586, then restarted
  the primary test AVD headlessly with host graphics on the canonical port 5554
- Second run reached instrumentation installation but exposed an incorrect APK
  path under intermediates. Switched to the actual outputs/apk/androidTest path;
  direct installation passed, followed by scoped formatting
- Third run passed complete preflight and began all 302 unattended tests
- Active exhaustive output: temp/full_suite_20261010_3.log
- Active exec session: 89259; report generation timestamp: 20261010_142702
- All four distribution variants passed (Play, GitHub, legacy, CI); suite passed
  its first 34 tests and reached test_fingerprint_manifest_publication
- The CD HOG isolation test passed metadata assertions but warned that a worker
  retained stages/cd_descent-anniversary-usa-extras/descent.hog. Confirmed one
  staging directory remained after the test. D1 load_mission mounts descent.hog
  implicitly outside LevelMetadataRequestMounts' explicit mount vector
- Changed LevelMetadataRequestMounts to snapshot the original PhysFS search path
  and retire every added mount at request completion, including implicit HOGs
- Extended test_cd_mission_hog_isolation to assert all staging directories were
  released. Mixed C++/PowerShell formatting passed. Both Windows engines rebuilt
  through test_flyout_safety, and the focused CD test passed with no cleanup
  warnings and no retained staging directories: temp/cd_hog_isolation_fix_20261010.log
- The exhaustive run has reached test 44 (test_generate_regression_specs), with
  no failures so far and one Linux-only dependency-installer skip
- Closed the Linux-only installer coverage gap through Ubuntu WSL using the
  repository-pinned PowerShell 7.6.6 tarball, verified against the GitHub release
  asset SHA-256. All installer fixtures passed: temp/linux_dependency_tests_20261010.log
  For the next full run set DXX_TEST_LINUX_PWSH to
  /mnt/c/local/dxx-redux/temp/linux-pwsh-7.6.6/pwsh and
  DXX_TEST_WSL_DISTRIBUTION to Ubuntu
- test_github_release timed out at 120 seconds near the last of 79 real-Git
  fixture scenarios. The uncapped standalone run passed all 79 in 127.4 seconds
  (temp/github_release_test_20261010.log). Added a 300-second master timeout;
  scoped formatting passed, and the suite runner rerun passed all scenarios in
  145 seconds: temp/github_release_suite/report_20261010_145353.md
- Initial JVM run: 1169 tests, zero failures/errors, one Anniversary fixture skip
  Enabled DXX_DISC_CONTENT_FIXTURE with the existing verified native extraction
  at C:/local/dxx-redux/game_data/CD images/Descent Anniversary (ISO)/data_tracks
  and reran all JVM/proxy tests with --rerun-tasks. Build and all tests passed in
  3m51s: temp/jvm_with_disc_fixture_20261010.log
- All 87 physical GuideBot route cases passed in 15m46s. Case report:
  android/temp/route_regression_cases/run_20261010_145328_807/summary.json
- Saved-world verification also passed restored pose, key state, difficulty,
  and repeat determinism. Main suite reached test_guidebot_secret_transition
  (test 51/302); session 89259 remains active
- Secret-transition, simulation infrastructure, guided-shot annotations, metadata
  workers, and replay comparison-policy tests passed. Main suite reached
  test_input_demo_determinism_matrix passed all three variants (default render,
  low-resolution render, no render), with unchanged recorded trace and final-state
  expectations, in 13m50s. Evidence:
  temp/input_demo_determinism_matrix/20261010_151500
- Main suite reached test_input_demo_regressions_d1 (67/302), session 89259 active
- Optional GuideBot save-continuity probes also passed: both routing modes,
  Scram/Next/Exit/Hostages and Enhanced Unexplored, twice on D2 levels 1 and 11
  and D1-in-D2 level 2. Commands: test_guidebot_original_navigation.ps1 -NoBuild
  -SaveContinuity, repeated with -D1InD2. Log:
  temp/guidebot_save_continuity_20261010.log
- D1 corpus passed all eight recordings both without rendering and with graphics
  D1-in-D2 corpus passed all eight in both modes, and D2 passed both recordings
  in both modes: all 36 corpus runs passed
- test_managed_dependencies passed but emitted a JSON truncation warning because
  it serialized full Windows CIM process objects for child fixture isolation
  Restricted serialization to the three identity fields actually consumed
  Scoped formatting and a complete standalone rerun passed without warnings:
  temp/managed_dependencies_fix_20261010.log
- After rebuilding the APK for the metadata mount fix, rerun the existing
  test_level_metadata_request_mount_scope and test_level_metadata_hxm_worker_reuse
  Android scenarios (the exhaustive run started with the earlier APK)
- Native CTest passed all 69 D1 and 78 D2 tests. Server integration and all
  automation/catalog validation tests passed
- test_sdk_package_cleanup failed on duplicate hw.cpu.ncore in an existing
  temporary AVD. Inventory unnecessarily parsed/rejected unrelated tuning keys
  Limited SDK INI inventory to package-reference keys and added integration
  coverage accepting duplicate CPU options while rejecting duplicate image and
  AVD path keys. Mixed scoped formatting passed. Both cleanup and inventory
  then passed through the master runner: temp/sdk_inventory_cleanup_suite/
  report_20261010_160255.md (2 passed, zero failures/timeouts/skips)
- Main suite reached test 127/302 (test_vertigo_metadata); session 89259 active
- Android tests passed through save/load dispatch, SDK lifecycle, texture
  bindings/labels, and both-game pause/matcen save restoration scenarios
- Passing save/load dispatch diagnostics exposed repeated MEM_FREE_NOMALLOC
  warnings in D2. Traced concrete ownership mismatches in digi_tsf_music.c:
  bounded HMP input and raw MIDI reads use malloc, but cleanup used d_free
  Changed HMP input release to free; MIDI cleanup now distinguishes tracked HMP
  conversion output from ordinary raw MIDI and resets source kind after release
  Extended existing music_completion fixture to allocate source-appropriate
  MIDI buffers and assert allocator matching/release across all 21 lifecycle
  cases. Scoped formatting passed; compile/device validation still pending
  Rerun test_android_music_completion and unified save/load/music tests after
  rebuilding the APK, checking diagnostics for allocator warnings
- test_android_xmodel_mipmaps failed before rendering: ADB push of libtsf.so
  could not connect to localhost:5037 (10060). Server PID 31612 remained alive
  since Sep 27; adb.log has no server crash or transport reset at the failure
  Next Android test reconnected. Keep this failure open until a full serial
  rerun of the enhanced-model rendering fixture passes; do not weaken assertions
- Main suite is running test_async_checkpoint_unified (151/302), session 89259
- Async checkpoint, all seven measured audio-mix scenarios, automap readiness,
  missing-pilot autosave resume, autoselect crash, axis mapping, boss health bars,
  and compressed-music metadata passed. Main suite reached
  test_controller_compare_unified (160/302), session 89259 still active
- Updated music completion fixture compiles for both Android engines without
  warnings: android/temp/music-ownership-compile/run_20261010_1634/build.log
- test_controller_live_rebind failed in its baseline at step 36/63 after saving
  and reopening Controller Mapping. Cached failure introspection shows
  controller_mapping.active=false, dialog_open=false, bindings={} and the
  launcher still showing Controller Mapping, so persistence loss is not proven
  Baseline evidence: temp/controller-live-rebind-d2-baseline.log
  Added targeted accessibility click node/bounds/result/root diagnostics in
  SetupAutomationApi.kt for a rebuilt-APK rerun before changing behavior
  Possible inconsistency to investigate: collectAccessibleButtons uses
  textBelongsToClickable (contains-or-center), whereas performAccessibilityClick
  uses full rectangle containment. No behavior change made on this hypothesis
- Controller overlay, raw input, response, and both-engine hotplug checks pass
- test_d1_in_d2_standalone failed waiting for forward movement. Evidence in
  temp/d1-launch-runtime-emulator-5554-20261010-164244 shows game active but
  pause.reasons=8 (ANDROID_PAUSE_GRAPHICS), graphics_safety.phase=editing and
  first_run_trial=true. The isolated runner bypassed calibration only for
  shareware; generalized its existing graphics_first_run_offered setup command
  to all gameplay scenarios, matching standard run_test.ps1 onboarding setup
  Full standalone rerun remains pending until the device is available
- Main suite reached test_death (180/302); session 89259 active
- Preserved standalone failure directory and controller baseline log under
  temp/full_suite_failures_20261010 before later isolated demo runs can retire
  their original timestamped artifacts
- Demo corpus import and both seven-level shareware campaign runs passed, as
  did Anniversary disc-content import, distribution launch/info and double-launch
  Main suite reached test_dxa_asset_usage (188/302), session 89259 active
- Full DXA matrix passed all 12 fresh-launch phases: D1 98, D2 183,
  D1-in-D2 98 and Mac D1 98 matching resident sound samples, real playback,
  and off/on/off restoration verified. Evidence: temp/dxa-asset-usage
  Main suite reached test_endgame_singleplayer_d1 (189/302), session 89259 active

## Remaining validation queue

- Finish live exhaustive suite and investigate all further failures
- Rebuild Android APK and native engines with metadata/music fixes and click
  diagnostics; run metadata mount scope/HXM reuse, music completion, and
  save/load/music integration against those final binaries
- Reproduce live controller rebind with click diagnostics; fix the demonstrated
  cause and pass original binding assertions for both engines
- Rerun D1-in-D2 standalone after onboarding setup fix; rerun full enhanced-model
  mipmap case after its ADB staging failure
- Existing release timeout and SDK cleanup failures have passing master-runner
  reruns; preserve original failed report and point to supplementary evidence
- Run all four real regression regeneration categories, review regenerated data
  and rerun affected verification; no corpus expectation weakening
- Regeneration has not started. Keep it serial with the suite, then run
  pwsh -NoProfile -File android/regenerate_all_regression_data.ps1 -Category All
- Enemy Within full-wrapper import and Level 2 both failed from insufficient
  emulator storage, with explicit 440 MB required / 156 MB free and ENOSPC
  The existing AVD is 6 GB while current provisioning config uses 8 GB
  Removed only three confirmed abandoned Oct 6 dxx-zip cache files (225 MB)
  Level 2 runner now releases its 390 MB transfer copy before import and wraps
  staging in try/finally, cleaning its private fixture too. Scoped format passed
  Both cases still require serial passing reruns; storage guards remain intact
- Main suite reached test_fov_cpu_visibility (194/302), session 89259 active
- Building final Android debug/instrumentation APKs without installing over the
  active test: temp/android_fixes_build_20261010.log
- Final debug and instrumentation APK build passed in 3m 7s, including both
  engines for arm64-v8a, armeabi-v7a, and x86_64. No compiler warnings
  Evidence: temp/android_fixes_build_20261010.log. Not installed over live suite
- FOV CPU visibility and both-game FOV recording/playback passed
  Main suite reached test_gamepad_menu_navigation_unified (196/302)
- Serial suite remains live: gamepad menu navigation (197 steps), GLES3 VBO
  arrays, and black-output recovery all passed for both games
  Reached graphics capabilities (199/302), session 89259
  No new failures in this batch. APK build session 60476 exited 0
- Graphics capabilities and confirmation-input passed for both games
- test_graphics_context_loss D1 failed before game startup: launcher ANR
  Main thread waits in HWUI RenderProxy::setStopped; RenderThread blocks in
  qemu_pipe_read -> rcCreateWindowSurface_enc -> eglCreateWindowSurface
  Android system_server restarted (555 -> 27220), and the D2 launch also stalls
  Evidence: temp/full_suite_failures_20261010/context_loss.anr.txt and
  temp/context_loss_live_logcat_20261010.log; bugreport session 15495 pending
- Temporarily suspended OWN master suite PID 26792 with NtSuspendProcess to
  prevent cascading tests on the unhealthy emulator. MUST NtResumeProcess it
  after controlled emulator recovery. Goal stays active; session 89259 persists
  Child run_test PID 33288 is still live, D1 failure already captured
- Preserved full ANR/watchdog bugreport: temp/context_loss_bugreport_20261010.zip
  Both context-loss game variants failed before reaching their graphics assertions
- Emulator original disks/config backed up with SHA-256 verification under
  temp/emulator_recovery_20261010. Online grow exposed existing ext4 reserved-GDT
  inconsistency, so original disk images were restored from that backup
- Testing now uses a separate fresh 8 GB image via emulator -data
  C:/local/dxx-redux/temp/emulator_recovery_20261010/suite-userdata.img
  Same canonical AVD Nexus5X_Light_1 and port 5554, same host graphics, 1080x1920
  Boot confirmed 7.7 GB filesystem, 7.0 GB free. Original image remains separate
- At task completion restore original AVD config.ini and encryption/cache images
  from the backup (fresh boot can modify shared AVD encryption/cache images)
  while emulator is stopped. Keep original data and Downloads intact
- Installing the rebuilt APK and instrumentation and rerunning both context-loss
  variants before resuming held master PID 26792 (NtResumeProcess required)
  Evidence: temp/emulator_recovery_20261010/context_loss_rerun.log
- Debug APK vanished from build/outputs after successful 17:15 build; another
  Gradle daemon and isolated emulator were started outside this suite at 17:21-23
  Asked user asynchronously about concurrent work, without blocking progress
  Rebuilding and immediately copying APKs into the recovery directory before
  installation, to prevent shared output cleanup from removing the test artifact
  Master PID 26792 remains held until context-loss rerun completes
- Repackaging succeeded (all three ABI native targets) using packageDebug and
  packageDebugAndroidTest, leaving native retention candidates intact while
  the held test wrapper prevented destructive cleanup. Logs: recovery/package.log
  Archived both APKs under temp/emulator_recovery_20261010 and installed them
- Original context-loss rerun PASSED both games (54 steps each), exec 68463 exit 0
  D1 PASS 17:54:55; D2 PASS 17:55:44. Android system_server stayed PID 557
  Partial console capture: recovery/context_loss_rerun_tail.log; D2 durable
  device result: recovery/context_loss_d2_result.json. Earlier direct Tee-Object
  did not capture Write-Host output; use external pwsh or *>&1 for future logs
- RESUMED master PID 26792 with NtResumeProcess after successful rerun
  Session 89259 remains the authoritative full-suite process; no longer held
  Tests after this point run on the rebuilt APK and fresh temporary 8 GB data
- Resumed suite passed test_graphics_egl_failure for both games in 38 seconds
  First-run graphics matrix (203/302) remains live; completed D1 acceptance,
  cancel/back/timeout/previous/unchanged/background/stall/unsupported/crash
  cases and corresponding cross-engine handled checks pass so far
  Fresh data filesystem has 6.4 GB free; no new ANR/system restart observed
- Full first-run graphics matrix PASSED in 14m 30s: 11 scenarios from each
  starting engine, plus cross-engine handled-state checks (44 scenario runs)
  Evidence: android/temp/graphics-first-run-20261010-175711 and master test log
  Main suite reached test_graphics_lifecycle (204/302), session 89259 active
- test_graphics_lifecycle D1 lost ADB at 18:12 after foreground lifecycle
  assertions; the QEMU process stayed live but transport disappeared/offlined
  Built-in recovery restarted the canonical AVD with default host graphics,
  headed mode, and ORIGINAL data disk, discarding the custom -data setting
- Held master PID 26792 AGAIN with NtSuspendProcess while child recovery finishes
  MUST resume after renderer recovery/rerun. Existing temp disk remains preserved
- Fixing demonstrated recovery configuration loss: preserve existing renderer,
  headless mode, private data path and read-only mode when restarting a target
  Will verify actual restart preserves a marker in the temporary data disk
- Recovery configuration fix implemented in emu_health.ps1/test_helpers.ps1
  Get-ManagedEmulatorLaunchSettings reads the selected port's process arguments
  before shutdown; recovery preserves data path, GPU, headless, and read-only
  settings unless explicitly overridden. Startup/fallback forward data isolation
- Extended test_managed_emulator_start with quoted Windows paths and Unix argv
  containing spaces; both primary and fallback launches preserve isolation
  All fixtures pass in PowerShell 7 and Windows PowerShell 5.1, scoped formatter
  and diff checks pass. Logs: recovery/managed_start_fix.log and
  recovery/managed_start_powershell51.log
- Live forced restart verified a changed boot ID and unchanged private marker,
  data path, SwiftShader backend, headless mode and writable mode; 6.4 GB free
  Evidence: temp/emulator_recovery_20261010/verify_recovery.log
  Lifecycle rerun still active in session 90971; master PID 26792 remains held
- Original graphics lifecycle rerun PASSED both games, 53 steps each, session
  90971 exit 0. D1 18:22:38 / D2 18:23:29, full log verify_recovery.log
- Using the held recovery window to verify earlier fixes and resolve remaining
  failures serially before resuming master. PID 26792 is STILL HELD and must
  be resumed after this queue; session 89259 remains open, no test child active
- Began controller live rebind D2 with rebuilt accessibility diagnostics
- Controller D2 baseline reproduced the same step-36 failure with diagnostics
  Saved label bounds Rect(74,1754-347,1806) extend 5 px below clickable bounds
  Rect(42,1717-379,1801). Discovery uses text-center membership, but accessibility
  click used full containment, yielding node=null and no reopened dialog
  Reused textBelongsToClickable in the click matcher, matching discovery
  Original binding assertions retained. Evidence: recovery/controller_d2_diagnostics.log
  Formatting/build and both-engine live rebind reruns pending
- Controller fix APK compiled for all 3 ABIs; both D1/D2 baseline (63 steps), launcher rebind (12 steps), and same-process game rebind (13 steps) PASS, session 25791 exit 0. Evidence: recovery/controller_fixed_d1.log and controller_fixed_d2.log. Starting remaining targeted queue while master remains held
- Music completion: 21 actual producer/callback/ownership cases PASS in each engine
- Mipmap transfer reproduced localhost ADB timeout 10060 before any assertions
  Existing server PID 31612 remained listening; attempted second server failed bind
  10048. Server/device remained healthy afterward. No retry/assertion changes made
- Pinned native fixture to main Debug/585cr1v5/x86_64 build and captured ADB traces
  Full mipmap rerun PASS, 16 cases in each engine, session 3969 exit 0
  Evidence: recovery/xmodel_adb_failure.log, xmodel_adb_trace.log, adb_server.log
  Exact intermittent localhost connection cause still undetermined; monitor repeats
- Diagnostic queue was stopped during compilation to scope ADB_TRACE to only the
  native fixture, keeping diagnostic stderr out of subsequent JSON consumers
- Android metadata mount scope PASS (10 steps); HXM worker reuse PASS (11 steps,
  Obsidian 18 levels then KCX-F2), using rebuilt APK
- Unified save/load PASS D1 85 steps and D2 86 steps. Both captured complete
  logcat and final introspection contain zero MEM_FREE_NOMALLOC warnings
  Evidence: recovery/targeted_metadata_*.log and targeted_saveload_d[12]*.log
- Targeted remaining queue session 82965 is active, currently D1-in-D2 standalone
  Master PID 26792 STILL HELD; resume once standalone and Enemy Within reruns pass
- D1-in-D2 standalone PASS with first-run marker fix; interaction and transition
  evidence: temp/d1-launch-runtime-emulator-5554-20261010-184441
- Enemy Within full 936 MB wrapper import PASS; level-2 gameplay/save/briefing
  test PASS 47 steps with transfer-copy cleanup, no ENOSPC
  Evidence: recovery/targeted_enemy_within_import.log and targeted_enemy_within_level2.log
- Remaining targeted queue session 82965 exited 0
- RESUMED master PID 26792 with NtResumeProcess at 18:49 after all targeted
  repair checks passed. Session 89259 active, no longer held, continuing test 205+
- Resumed tests 205 graphics mode restore and 206 MSAA allocation failure PASS
- New test 207 graphics_native_interruption FAIL both engines at step 34:
  Tab during graphics challenge never opens automap. Final graphics state rolled
  back correctly but automap_active stayed false. Original assertions preserved
- ReadControls pause gate in both engines consumes all blocked input except
  Escape/Pause; graphics pause reason is included, so automap mapping is skipped
  Adding focused DLOG_GRAPHICS key/reason logging before choosing a fix
- Added one diagnostic line in each d1/d2 main/gamecntl.c, scoped formatter PASS
  Diagnostic APK package build session 73209 active, logfile
  recovery/graphics_pause_diagnostic_build.log, archive target
  recovery/app-debug-graphics-diagnostic.apk. DO NOT install during active test
- Master session 89259 remains RUNNING (not suspended), now test 209 graphics
  recovery fault matrix, after test 208 graphics preview PASS
- Next: after diagnostic build and a safe test boundary, hold master, run original
  native-interruption D2 with logcat capture, confirm rejected key=15/reasons=8,
  fix shared paused graphics navigation without allowing firing/movement, test
  both engines and relevant pause coverage, then RESUME master
- Diagnostic APK build session 73209 exited 0, all 3 ABIs, 4m19s; archived APK
  recovery/app-debug-graphics-diagnostic.apk. Gradle finish delay traced to Kotlin
  FUS report sorting, not a build hang; no action needed
- HELD master PID 26792 again with NtSuspendProcess while its current child
  31984 finishes the graphics recovery matrix. MUST resume after interruption fix
  Main exec session 89259 remains open; goal remains active
- Graphics recovery matrix has passed all 9 D1 cases and first 3 D2 cases
  It remains active in child 31984, now d2-rebuild_failure; report directory
  android/temp/graphics-recovery-20261010-185433
- Diagnostic orchestration exec session 68300 is ACTIVE, waiting for child 31984
  to exit. Then it automatically installs archived diagnostic APK on 5554, clears
  logcat, runs original test_graphics_native_interruption D2, captures logcat:
  recovery/graphics_pause_diagnostic_test.log / graphics_pause_diagnostic_logcat.log
  Expect test exit 1 before fix; inspect native key=15 pause reasons=8 evidence
- MASTER PID 26792 IS SUSPENDED ONCE; MUST NtResumeProcess after repair validation
  Full suite exec session 89259 persists. No other active build/targeted sessions
- Candidate fix design, NOT APPLIED: shared android_pause_try_open_automap(event)
  called inside blocked ReadControls branch in both engines. Only allow when sole
  reason GRAPHICS, live front gameplay, not dead/endlevel/demo, with normal net
  reactor countdown restriction. Use kconfig_read_controls(event,1) on button/key
  presses, extract automap_count, clear Controls before/after so no movement/fire
  leaks; call do_automap only on mapped request. Keep diagnostics in shared helper
- Extend existing native interruption JSON with lctrl/up presses during challenge
  and assertions fire state/count and pitch/heading/throttle zero before Tab.
  Preserve existing automap/rollback assertions. Then scoped mixed formatter,
  packageDebug all ABIs, archive/install only while master held, rerun both-engine
  interruption and test_android_pause_state before resuming master
- Full graphics recovery matrix PASS 18/18; child 31984 exited. Coordinator held
- Diagnostic D2 rerun reproduced original step-34 failure, session 68300 exit 1
  Native log confirmed key=15 rejected with sole pause reason 8 (GRAPHICS), then
  graphics trial timed out normally. Evidence: recovery/graphics_pause_diagnostic_logcat.log
- Implemented shared android_pause_try_open_automap using configured bindings,
  strict graphics-only pause eligibility, normal dead/demo/endlevel/network limits,
  and zeroing all ship controls before/after reading button events. Both engines
  call it from their blocked ReadControls branch. Removed duplicate probe logs
- Extended native interruption fixture with fire/movement suppression assertions
  Scoped mixed C/header/JSON formatting and diff checks PASS
- Verification session 70300 ACTIVE: builds all-ABI package, archives/installs
  app-debug-graphics-pause-fixed.apk, runs D1/D2 interruption with logcat capture,
  then both-engine Android pause reconciliation. Logs under recovery/graphics_pause_*
  MUST resume master PID 26792 once verification passes. Master STILL SUSPENDED
- First fixed build caught legacy automap.h dependency on MAX_SEGMENTS; included
  segment.h before automap.h and reran scoped formatter, PASS
- Corrected build PASS all 3 ABIs in 3m01s; installed archived
  recovery/app-debug-graphics-pause-fixed.apk
- Expanded graphics native interruption PASS D1/D2, 56 steps each. Logs confirm
  fire/up key events automap=0, controls zero; mapped Tab automap=1 and rollback
  preserves accepted/requested graphics values. Per-game test/logcat saved
- Verification session 31667 now running test_android_pause_state for both engines,
  evidence android/temp/pause-state-20261010-192425. Master PID 26792 STILL HELD
  Earlier verification 70300 and diagnostic 68300 terminal; formatter 75244 terminal
- Pause reconciliation first rerun failed at physical Options tap, before graphics
  challenge. dumpsys input/window proved ImmersiveModeConfirmation system prompt
  owned focus. UI XML identified Viewing full screen / Got it; after acknowledging
  that prompt, identical Options coordinates opened the expected native menu
- Added emulator-only Initialize-EmulatorTestUi to readiness/boot provisioning:
  verify immersive_mode_confirmations=confirmed, write only if missing, fail
  readiness on failed publication, never change a physical device setting
- Extended test_test_helpers_process_wait with fresh/confirmed/unwritable settings
  and physical-device guard. Formatter, PowerShell 7 and Windows PowerShell 5.1 PASS
  Evidence: recovery/fullscreen_provision_host.log and fullscreen_provision_host51.log
- Real validation cleared the setting on isolated primary, cold-restarted through
  emu_health preserving SwiftShader/private data, and verified provisioning. Both
  D1/D2 complete pause reconciliation then PASS (session 89507 exited 0)
  Evidence: recovery/fullscreen_provision_live.log, pause_after_fullscreen_provision.log,
  android/temp/pause-state-20261010-193353/results.json
- RESUMED master PID 26792 with NtResumeProcess at 19:39 after graphics fix and
  provisioning verification. Session 89259 active; NO LONGER SUSPENDED
  Current installed APK: recovery/app-debug-graphics-pause-fixed.apk
- Resumed test 210 graphics_settings_confirmation PASS D1/D2, 74 steps each
  Full suite is now running test 211 graphics_video_overlay in session 89259
  Coordinator 26792 is running normally, not held. All targeted/build sessions
  including real provisioning/pause validation 89507 are terminal
- Still required: finish remaining full-suite tests, investigate any new failures,
  run ALL regression regeneration categories (not started), review resulting data,
  run clean final full verification with WSL/disc fixture environment enabled,
  restore original primary AVD disks/config from verified backup at task completion

- 19:54: Held master PID 26792 once for routing-menu failure investigation; test 218 child may finish. Master now SUSPENDED; resume exactly once after serialized repro/fix
- Test 216 routing menus failed at fixed 800 ms hold, selected row 0. Test 217
  routing modes PASS; test 218 headed/headless semantic parity PASS after hold
- Added selection-change diagnostic, built all ABIs, reproduced original sequence
  failure at row 1: native log proves 13 Down events wrapped all 12 rows
  Evidence recovery/guide_menu_repeated_diagnostic{,_logcat}.log (expected FAIL)
  Individual prior diagnostic passes reached 9-13 moves per frame-driven hold
- Fixed routing-menu fixture to assert tap wraps to zero, then release in the same
  native frame that observes repeat movement (controller_input precondition),
  preserving final selection range. Added failure result/state/logcat capture
  Diagnostic retained as category-gated Android log, not unconditional force
- Scoped mixed formatting PASS. Complete mode/secret/save-restore test PASS:
  recovery/guide_menu_fixed_test.log. Now running no-repeat negative control
  and 30 consecutive original-mode menu cycles, session 65262
- Production diagnostic-gated all-ABI package build session 19015 ACTIVE;
  archive target recovery/app-debug-guide-menu-fixed.apk. Current installed
  diagnostic APK app-debug-guide-menu-diagnostic.apk until build/retests finish
- Master PID 26792 STILL SUSPENDED once, child parity has exited. Resume exactly
  once after this serial verification. Main session 89259 remains active
- No-repeat negative control correctly FAILED precondition at selection 1;
  wrapper verified exact failure and continued. Repeated fixed sequence PASS
  382/382 steps (30 full menu cycles), evidence recovery/guide_menu_repeated_fixed.log
  and recovery/guide_menu_no_repeat_control.log. Session 65262 terminal 0
- Gated-log all-ABI build PASS, installed recovery/app-debug-guide-menu-fixed.apk
  Complete production routing-menu test PASS, session 35042 terminal 0,
  evidence recovery/guide_menu_production_test.log. Formatting/diff checks PASS
- RESUMED master PID 26792 exactly once at 20:10. It is RUNNING NORMALLY again,
  session 89259. No active targeted/build sessions. Current installed production
  APK archive is recovery/app-debug-guide-menu-fixed.apk

- Test 219 unexplored goal FAIL: max_tick_us=12330, tick_overruns=6 across 50 ticks; inspecting actual workload/yield budget. HELD main PID 26792 ONCE at 20:12 while child 220 hires pack release finishes. Master now SUSPENDED again; later resume exactly once
- Added TEMPORARY route overrun diagnostics in shared/secretarea.c:
  thread CPU time (CLOCK_THREAD_CPUTIME_ID), post-plan wall time, endpoint kind,
  forced overrun log only; deadlines, work limits, and test expectations unchanged
  This is diagnosis-only pending measured cause. Must finish/remove force logging
  after investigation. Scoped format PASS, all-ABI build PASS session 95631
- Archived recovery/app-debug-route-budget-diagnostic.apk, NOT installed yet
- Active serialized wrapper session 30317:
  recovery/verify_route_budget_diagnostic.ps1 waits for hires child PID 28876 to
  finish (it is live, no overlapping device test), then installs diagnostic APK
  and runs unchanged test_guidebot_unexplored_goal.jsonc, saves diagnostic test
  and logcat under recovery/route_budget_diagnostic*.log. Inspect terminal exit
  and CPU/wall overrun lines before changing planner or regression expectations
- Hires pack test 220 output android/temp/hires-release/
  run_72dd669ab8e3422db937a2152f925f40. D1 128/256/512 baseline/on/off PASS through
  20:19. D2 and D1-in-D2 remain. Master PID 26792 STILL SUSPENDED once since 20:12
  Do not resume it until diagnostic replay and any needed fix are finished
- Current installed APK remains recovery/app-debug-guide-menu-fixed.apk until
  queued wrapper installs diagnostics. Previous build/test sessions all terminal
  except master 89259, hires child 28876, and queued diagnostic session 30317
- Hires pack release test 220 COMPLETE PASS at 20:30:45: all 21 phases across
  D1, D2, D1-in-D2 and 128/256/512 packs. Native logs checked for crash/allocator
  errors (none). Enabled packs all show mipmapped residency, disabled none loaded
- Queued wrapper session 30317 observed child 28876 exit, installed
  recovery/app-debug-route-budget-diagnostic.apk, and started unchanged D2
  unexplored-goal test at 20:31. Master PID 26792 STILL SUSPENDED once
- Route diagnostic session 30317 TERMINAL expected FAIL: max tick 29752 us.
  Four overrun records show wall/CPU/post-plan times; one pending tick retained
  incumbent and jumped 27 ms between adjacent timing reads. All 37 deferred
  ticks retained incumbents; no full/shadow planner, compiled selectors <400 us
  Evidence recovery/route_budget_diagnostic_test.log and _logcat.log
- Independent NDK native clock_probe compiled warning-free and tested serially:
  active rendering: 147317 samples, 63 EMPTY adjacent-read intervals >4 ms,
  max 22895 us; idle force-stopped app: 181923 samples, 9 empty overruns, max6735us
  evidence recovery/clock_probe_rendering.log and clock_probe_idle.log
- Same route assertions at 640x480 ALSO FAIL; temporary control only under
  recovery/unexplored_640_control{,_logcat}.log, session10250 terminal1
  No production test resolution/threshold changes made
- Native clock_compare: 100000 MONOTONIC calls cost2266995us (~22.7us each),
  direct syscall2065311us; coarse398us but resolution4ms unsuitable2msdeadline
  recovery/clock_compare_idle.log. su 0 sysfs confirms ONLY HPET clock available
  Guest /proc/cmdline includes clocksource=pit; WHPX enabled/usable on host
- Added TEMP diagnostic AVD config kernel.parameters=clocksource=tsc (no forced
  reliable flag, no stability checks disabled), backed up current 8GB config as
  recovery/config-before-clock-diagnostic.ini. Original6GB config backup remains
- ACTIVE session63373 cold restart via emu_health preserves primary-only private
  data/SW/headless; writes clock_restart.log and clock_boot_dmesg.log, prints
  clocksource availability and cmdline. Inspect boot evidence next
- Master PID26792 STILL SUSPENDED ONCE, no active emulator tests besides restart
  current APK route-budget-diagnostic, all other sessions except89259 terminal
- First clocksource=tsc cold restart booted but still HPET; earliest dmesg rolled
  out of buffer. Added temporary log_buf_len=4M to isolated AVD kernel.parameters
  and cold-restarted again (session1893 terminal0). Full boot trace now available
  recovery/clock_boot_full_dmesg.log: calibrated TSC, switched tsc-early -> tsc;
  no reliability/watchdog overrides. TSC remains selected under active rendering
- Clock comparison on TSC: 100000 monotonic reads2085us vs2266995us HPET, ~1000x
  overhead reduction. Original FULL-RES unexplored test PASS46/46, max tick267us,
  tick_overruns0, full_plan_calls0. session38058 terminal0, evidence
  recovery/unexplored_tsc.log and recovery/clock_compare_tsc.log
- 10sec TSC rendering probe:15149680 samples,2 empty gaps>4ms (ordinary scheduling),
  host elapsed10.2237664sec inclADB overhead. Clock stayedTSC, no kernel instability
  evidence recovery/clock_probe_tsc_rendering.log. No test thresholds changed
- Removed ALL temporary secretarea.c instrumentation; git diff for file empty
  Added six-line Windows-only TSC clock preference to newly created lightweight
  AVD defaults in create_light_avds.ps1; kernel stability checks/fallback retained
  Existing AVDs remain untouched by creation script without Force
- Scoped mixed formatter PASS; SDK writer complete host regression PASS session23599
  evidence recovery/avd_clock_sdk_writer_test.log; diff checks PASS
- Production all-ABI package rebuild session77858 ACTIVE, output
  recovery/route_clock_production_build.log, archive target
  recovery/app-debug-clock-verified.apk. Install it and rerun original unexplored
  test once build finishes, then RESUME MASTER PID26792 EXACTLY ONCE
- Master remains SUSPENDED since20:12, active session89259. Hires220 child is DONE
  PASS (all21phases). No current device test. Emulator primary is healthy on TSC,
  still SwiftShader/headless/private8GB data image; AVD TEMP kernel.parameters now
  clocksource=tsc log_buf_len=4M. Restore original config/disks at goal completion
- Production all-ABI rebuild PASS (4m37s), session77858 terminal0. Installed
  recovery/app-debug-clock-verified.apk (NO temporary native timing diagnostics)
- Original unexplored-goal test PASS46/46 on production, session85295 terminal0:
  max_tick_us234, tick_overruns0, refresh_overruns0, full_plan_calls0
  recovery/unexplored_clock_production{,_logcat}.log, TSC still selected
- RESUMED master PID26792 exactly once at20:52. Main session89259 is RUNNING
  NORMALLY again, no suspend outstanding. Hires220 PASS, next test221
  No other active builds/targeted/device verification sessions
- Active test AVD retains clocksource=tsc log_buf_len=4M until final restoration
  Production defaults now prefer TSC on new Windows AVDs. At task completion,
  restore and hash-verify original disks/config, then retain ONLY the intentional
  clocksource=tsc preference in restored primary config for durable test timing;
  remove temporary log_buf_len. Preserve all original data and other settings
- Master recorded hires220 PASS (walltime41m48 inflated by coordinator hold;
  child itself finished20:30). Test221 host_dialog_loading PASS. Now running
  test222 idle_screen_saver, session89259, coordinator26792 NOT SUSPENDED
- All targeted/build sessions (including85295,77858,23599,51640,1893,63373)
  terminal. Current installed APK recovery/app-debug-clock-verified.apk
- Still required unchanged: finish all302 suite cases and new failure repairs,
  ALL4 regression regeneration stages (NOT STARTED), review normalized changes,
  clean finalfull suite with WSL and discfixture env enabled, originalAVDdata
  restoration + only verified clocksource preference retained at completion
- Tests222 idle screen saver (both engines),223 independent trigger axes
  (both engines),224 input demo recording profile,225 intro skip inputs PASS
  Current226 KCXF2 guidebot route next is analyzing imported mission metadata,
  main session89259 / PID26792 RUNNING normally, no suspension
- Prepared but DID NOT LAUNCH temp/run_regression_all_20261010.ps1. This calls
  canonical -Category All, captures before/after SHA256+timestamps for all529
  game-data JSON/JSONC (including ignored) and known asset databases, records
  dirty-tree baselines, writes durable timestamped evidence with retention
  Script parsed successfully. Run ONLY after suite and new failure fixes finish

- Test226 KCXF2 PASS192steps. Test227 LAN active discovery FAIL: host receives QUERY but client never sees ANNOUNCE sent via console UDPredir. Tests228 orphan restart,229 LANQR,230 transportrecovery PASS. HELD masterPID26792 once at21:03, child231 launch_to_automap may finish. Master now SUSPENDED; inspect discovery packet routing then resume once after verified fix
- LAN227 diagnosis: paired replies in recovery/lan_reply_diagnostic.log prove direct
  reply received as DirectQueryOnlyHost while console-redirection reply never
  reached the wildcard-bound app socket. Fixed fixture to reply from listening
  host socket to QUERY source endpoint, exercising actual NAT return mapping
- Fixed original discovery + no-auto-join assertions PASS in
  recovery/lan_discovery_fixed.log; scoped formatter and diff check verified
- Child231 launch_to_automap finished PASS both engines. RESUMED master26792
  exactly once after discovery repair at21:09; NO suspension outstanding
- At21:10 HELD master26792 ONCE for test232 launcher_dpad failure; test234 metadata worker active. One suspension outstanding. Discovery scoped formatter session18562 terminal0.
- Test232 launcher D-pad failure diagnosed: portrait viewport clips controls
  before controller mode enables focus; old fixture waited for visible controls
  BEFORE its R1 input. Paired before/after diagnostics reproduce this exactly
  in recovery/launcher_dpad_diagnostic.log, then all5 navigation checks PASS
- Moved existing EnableLauncherDpadMode before initial visibility check; all
  focus/activation/back/restoration/right assertions retained. Production fixture
  PASS in recovery/launcher_dpad_fixed.log; scoped formatter73322 terminal0,
  verification54500 terminal0, diff check clean
- Test234 metadata HXM worker reuse PASS11/11 Obsidian18levels and KCXF2sameworker
- RESUMED master26792 exactly once after D-pad fix. Main89259 running normally,
  NO suspension outstanding, no independent device tests/builds active
- Master235 metadata interactive preemption PASS9/9;236 Plutonian Shores
  launcher ZIP metadata PASS7/7;237 alternating base/mod request mount scope
  PASS10/10. Now238 result-cache reuse, session89259 / PID26792 RUNNING normally
- Prepared/parsed temp/run_full_suite_final_20261010.ps1 (NOT LAUNCHED): canonical
  exhaustive4flags plus WSL PowerShell and Anniversary disc fixture env. Run after
  remaining cases/fixes and ALL4 regression regeneration (still NOT STARTED)
- Test238 result-cache reuse still analyzing Uneasy4 initial request (started
 21:19:57). Native worker17058/thread17190 actively uses CPU in route planning;
 background queued request timed out120s behind it. No test failure yet
- Captured recovery/result_cache_live.log and result_cache_worker_stack.log
 via shell su0 debuggerd -b17058 (brief read-only stack dump). Stack plan_route
 -> dependency_planner::path_to_position -> search_routes/vector resize.
 Current main89259/PID26792 RUNNING normally, NO hold outstanding
- HELD master26792 ONCE during ongoing Uneasy4 cache238 analysis to preserve evidence before next device test. Child26948 still running; one suspension outstanding. No planner changes yet
- Test238 Uneasy4 completed PASS6/6 in439567ms (~7m20). Second request explicitly
  ASSERT_PASS persisted result-cache hit. No planner/runtime changes made
- Retained completed metadata log and second stack in recovery/result_cache_*
  TSC remains selected; first analysis CPU-bound, within existing600s timeout
- RESUMED master26792 EXACTLY ONCE at21:28; session89259 RUNNING normally,
  no suspension outstanding. Child26948 terminal, no independent device tests
- Tests239 levelcomplete touch skip92steps,240 lobby latency/discovery/savecheck
  concurrency/commit-abort-retry/manual-IP recovery,241 LunarSeriesRevamped
  metadata6steps,242 MacD2demo graphics41steps,243 MacPlay audio actualPCM,
 244 mergedwall snapshot45steps,245 mergedwalltwo-pass41steps all PASS
- Current246 mission_asset_isolation child40368 (with supportscript), master
 89259/PID26792 RUNNING normally, NO suspension, no other device tests active
- Full-suite3 failure-list audit has14 historicalfailedcases, all matched to
 earlier targeted passing verification. Final clean all-flags run STILL REQUIRED
- All4 regression regeneration remains NOT STARTED. Prepared wrappers unchanged:
 temp/run_regression_all_20261010.ps1 and temp/run_full_suite_final_20261010.ps1
- Mission asset isolation246 fully PASS95/95 and wrapper verifies sameprocess21743
  custom -> base -> custom quickload; evidence temp/mission_isolation_logcat.txt
- Current247 mission_launch_cache (both engines) RUNNING main89259/PID26792,
  no suspension outstanding. No new failures this continuation
- Mission launch cache247 PASS16steps each D1/D2, includes repeated launch and
 owned extraction cache deletion. Current248 mission_zip_batch child34332 runs
 canonical Android batch MaxZips3/-NoRegressionJson, progressing Moon missions
  master89259/PID26792 RUNNING, NO suspension or other device work
- ZIP batch248 PASS3/3 no skips/failures (-MOON-,af_d1_beta,af-d2x), output
 android/temp/mission_zip_batch/20261010_213634. Mod loading249 PASS35steps;
 D2movie skip250 PASS29; MSAA251 bothenginesPASS (D2 122steps);
 multiplayerrecovery252 PASS transfer cancel/resume, stalecallbacks,proxyretry,
 hostrollback,probehandoff,launcherretry
- Current253 music_save_source_restore_d2, main89259/PID26792 RUNNING normally,
 no suspension outstanding, no independentdevice work. RegenerationNOTSTARTED
- HELD master26792 ONCE at21:53 for newFAIL259 Obsidianlevel6guidebot_switch_grate. Child260level7 may finish; one suspension outstanding. Inspect259 log before serial targeted work
- Test259 failure reproduced with unchanged assertions + existing profiling and
  guidebot logs: recovery/obsidian6_trace{,_logcat}.log. At21:56:28.183 warp then
 21:56:28.318 trigger14 actually fires (walls85/87 opened), latch logged.
 Guidance correctly advances to redkey467. Fixture posed player at switch shot
 pose then warped companion with same facing, unintentionally activating switch
- Diagnostic control replaces pose+warp with existing guidebot_segment506,
 keeps player away (314) and ALL original retention/path/timing assertions,
 explicitly fires14 afterward and expects redkey467. PASS45steps incl3logging
 steps: recovery/obsidian6_pose_control{,_logcat}.log. No C++ edits
- Production fixture updated same positioning plus explicit14->redkey check,
 scoped formatter13696 terminal0. Final production fixture test15637 RUNNING
 recovery/obsidian6_fixed.log (capturelogcat atfinish), master26792 STILL HELD
 ONCE. Level7 child260 finishedPASS47/47, no other active emulator test
- Production Obsidian6 fixed fixture PASS42/42, session15637 terminal0,
 recovery/obsidian6_fixed{,_logcat}.log, diffcheckclean. No runtimecode changed
- RESUMED master26792 exactly once at22:01. Main89259 RUNNING normally again,
 NO suspension outstanding, no targeteddevice/build/formatter sessions active
- Prior tests253 music-save-source28steps,254 musictrackcontrolsD145/D252,
 255 newmenurenderboth(D290),256 Obsidianobjective204,257 Obsidian13profile25,
 258 Obsidian4postreactorkeys47 allPASS. Current260 completePASS47; next261
- All4regeneration and finalcleanfullsuite still required/NOTSTARTED. Current
 fullsuite3 now15historicalfailures, all diagnosed and targetedverificationPASS
- Tests261 gauge batching bothenginesPASS(D184/D283),262 runtime texture options
 bothPASS(D158/D288 plus probes); SwiftShaderAFunsupported branch covered
 output android/temp/runtime-texture-options-20261010-220408
- PC D2 demo music263 PASS29,264 nativePCMring boundaries/underruns/reset/
 isolation/concurrentstreamsPASS. Current265pilotlongholddeletebothengines
 main89259/PID26792 RUNNING normally, no suspension, no independentdevice work
- Test265pilotlongholddeletebothPASS25steps each;266quickrecordsidecarPASS35;
 267randompreviewPASS seed2013169671 Lostlvls.zip level2 Satellite2PatheusIII,
 camera changed/stayedalive/returnedwithoutmetadatarefresh;268tinyhelpPASS24
- Main89259/PID26792 stillRUNNING normally nohold, currently270robotpreview
 regeneration/finalverification stillNOTSTARTED; no independenttests/builds
- Resolution269PASS;270robotpreviewPASS aspect/navigation/sound/animation/
 rotation/projectileretirement/orientationdependentshots;271backgroundroute
 priorityPASS26steps;272metadataimporthandoffKCXF2PASS24steps
- Current273 route_metadata_large_level_budget, master89259/PID26792RUNNING,
 nohold/independentdevicework. No new failures this continuation
- Largelevelbudget273 PASS18/18 after60sUneasy4gameplay. Current274SAFarchiver, main89259/PID26792RUNNING normally/nohold. Allregeneration+finalcleanfullsuite+originalAVDrestoration stillrequired
- SAFarchiver274PASS D2level1HAM viaSAF/restoredfile;275SAFredbookPASS69steps
 rapidtrackreplacement/latesttrackonly/nojoinwait;276secretrevealautomapPASS28;
 277sliderinstrumentationPASS controllernavigation/adjustment
- Current278soundtrace, main89259/PID26792 RUNNING/nohold. Regeneration and
 finalcleanfullsuite stillNOTSTARTED; no targeted/build work active
- Soundtrace278PASS23steps+gainchecks;279soundfontdownloadcatalogbothPASS;
 280textureindexloadPASS17;281titlemusicintro-skipbothPASS21steps each;
 282touchlayoutformat internal/readable/slotroundtripsPASS
- Current283Trine2D1inD2customtextures, main89259/PID26792RUNNING/nohold.
 Cdrive~80.9GiBfree. Regeneration/finalfullverification remainNOTSTARTED
- Trine2D1inD2textures283PASS49;284unifiedmissionpickerPASS34;
 285filesetcontentPASS17(includingdeletioncleanup);286VertigometadataPASS10,
 metadata23levels+preview. Current287Vertigomergecreation
- Main89259/PID26792RUNNING/nohold; no new failures this continuation
- Vertigomerge287PASS51;288xcrashPASSexpectednativecrashwithsignal/backtrace
- Current289 full extracts39specs withlaunch, report
 temp/extract_regression_reports/run_20261010_222846. Main89259/PID26792RUNNING
- FirstMacD1disc staging ADBhostconnect10060 at22:30:10 (server31612alive,
 listening5037, directget-state immediatelyhealthy;966TIME_WAITconnections
 farbelowdynamicportrange). Existingwhole-stageretry succeeded22:31, import+
 expectedfilesPASS, launchpending. Preserved recovery/adb_server_extract.log
 No newretrycode or speculativeADBserverrestart; connectioncauseunproven
- Extract1/39 MacD1 PASSED import+launch afterexistingADBstage retry. Inspected
 Copy-LocalFileToAppPrivate: failedtmpremoved, wholefile restarted byte0,
 perchunk+cumulativesizeverified thenmv. No partialappend reused
- Currentextract2/39 MacD2 stagedfullBIN at22:33:21 and importing. Main89259/
 PID26792 RUNNING/nohold. ADB31612 remainslive; rootcauseofisolated10060
 remainsunproven, no speculativefix. Report run_20261010_222846
- Full extracts289 progress: first4/39 PASS MacD1,MacD2,BrazilAnniversary,USAAnniversary withlaunch. Current5DestinationSaturn. Main89259/PID26792RUNNING/nohold, no new failure afterfirststagingretry
- Full extracts289 now8/39 PASS: additionallyDestinationSaturn,LevelsWorld,MacPlay,TestFlight. Current9EuropeAlt. Report run_20261010_222846; main89259/PID26792 live/nohold, no new errors sincefirstADBstage retry
- HELD master26792 ONCE at22:47 during extract9EuropeAlt genuinefatal Invalid POF gun index in boss02.pof. Needholdextractcoordinator too once actualchildtreeinspected; currentcase canfinishfailure first. One master suspension outstanding; diagnostic recovery/europe_alt_launch_live.log
- Also HELD extractbatch27572 ONCE; actualextractchild18608 stillrunsEuropeAlt toterminal. BOTH main26792 andbatch27572 eachhaveONE suspension, resumeeachonceafterfix
- EuropeAlt POF root cause confirmed from preserved 7,261,423-byte imported descent.hog: boss02.pof v7 declares four GUNS records, IDs/models (0,4),(3,5),(1,7),(4,8). Pre-refactor D1 accepted sparse IDs into MAX_GUNS arrays; shared decoder incorrectly enforced ID < record count. Keep count and sparse slot semantics, bound ID by D1_POF_GUNS, validate all eight submodel slots. Added constructed four-record regression with holes, duplicate/OOB/truncation/unused-slot submodel rejection.
- Scoped formatter pof_sparse_format.log terminal PASS, diff check clean. Host both-engine build session74884 and Android all-ABI package session73184 active. Main26792 and extractbatch27572 STILL HELD ONCE EACH; original EuropeAlt child18608 exited, batch has not consumed its final output yet. Must resume each once after targeted verification.
- POF sparse fix VERIFIED: both Windows engines build PASS; CTest D1 69/69, D2 78/78 PASS. Android package all 3 ABIs terminal0 (7m15, Kotlin FUS shutdown scan diagnosed without interruption). Archived/installed recovery/app-debug-pof-verified.apk. Full EuropeAlt test_extract with fresh CD import/launch PASS Lunar Outpost, recovery/europe_alt_fixed.log and logcat. Sessions74884/73184/76542/94910/95706 all terminal0.
- Resuming both outstanding coordinator holds now: batch27572 then main26792, each exactly once. Original failed extract remains historical evidence; final clean full suite still required after All regeneration.
- Both NtResumeProcess calls succeeded status0 at23:03:43. NO coordinator holds outstanding. Main89259/PID26792 and batch27572 live. After fix, extract10Europe and11USA PASS; now12/39 Anniversary ISO staging. Report remains run_20261010_222846 with historical extract9 failure plus separate full PASS recovery/europe_alt_fixed.log. Current APK app-debug-pof-verified.apk, actual normal build outputs same. C free78.5GiB. All-regeneration and final clean full suite still NOT STARTED.
- Continued extracts:12AnniversaryISO PASS (745 imported visible files),13DefinitiveEuropeDisc1 PASS on second complete attempt. First attempt ADBhost5037 connect10060 at23:09:36, server31612 alive sinceSep27,303handles/16threads/7MiB,467TIME_WAIT/1established: no observed resource exhaustion. Preserved recovery/adb_server_definitive.log. Read-only ADB_TRACE health probe90calls over3min PASS all, max469ms; recovery/adb_extract_probe.jsonl, session9217 terminal0. Root cause of intermittent hostconnecttimeouts remains unproven; no speculative retry changes/serverrestart.
- Currentextract14/39 DefinitiveEuropeDisc2 imported28files, all5expected present, launch23:14:36. Main89259/PID26792 +batch27572 live, NOholds. Allregen NOTstarted. Prepared regenwrapper now snapshots exact original JSON bytes in before/ (including ignored fixture files) and added-file manifest as well as original hashes/mtimes; syntaxPASS.
- Upcomingdual tests use5554/5556, not unrelated5586. Secondary5556 not yetstarted. ExistingNexus5X_Light_2 at5586 remainsreadonly/SW; its original15GBuserdata intact. Beforedual tier prefer private secondary userdata+SW/TSC, preserve any alteredoriginalAVDfiles. No secondarychangesmade yet.
- Extract14DefinitiveEuropeDisc2 PASS full launch and15Disc3 PASS expected non-launchable Vertigo file-only check (15files,4required). At23:16:48 report has15completed/14PASS/1historicalPOFfailure (separatefixedPASS). Current16USAdefinitiveDisc1. Main89259/PID26792 +batch27572 confirmedlive/noholds. A transient read of summary duringwrite failed JSON once; reread valid15rows, no evidence of persistedcorruption. Probe9217 terminal0. No other independent tests/builds running.
- Extract16USAdefinitiveDisc1 and17Disc2 PASS full launch. Current18USAdefinitiveDisc3 Vertigo file-only. Main89259/PID26792 andextractbatch27572 live/noholds; originalPOFfailure separatetargetedPASS remains only recordedfailure.
- Secondary original AVD backup COMPLETE verified SHA256 source/copy, session96880 terminal0: recovery/secondary-original/{config.ini,hardware-qemu.ini,cache.img,cache.img.qcow2,encryptionkey.img,encryptionkey.img.qcow2,userdata-qemu.img,userdata-qemu.img.qcow2,userdata.img}, manifest.json. Original15,209,660,416-byte qcow2 included. Also original Nexus5X_Light_2.ini index backedup withindex-manifest.json. NO secondary configuration changes or5556launch yet. Unrelated5586readonly remainsuntouched. Originalsecondarymustrestoreanychangesaftertesting; privateDataPath+SW/TSCpreferredwhenstarting5556 beforedual tier. Cfree62.7GiBafterbackup.
- Source18USAdefinitiveDisc3 PASS on second complete attempt. First failure23:22:48 was am start -W -S30stimeout, NOT same as hostTCP10060: preserved recovery/usa_vertigo_start_timeout.logcat.txt showsAndroidforkPID13643 at23:22:18, processneverattached, Androidkilledit23:22:28 starttimeout; noapp/nativeinitializationlogs. OtherOS/appactivitycontinued. Subsequentnormalstart~800ms. Do not lumpthisintoADBconnectfailures.
- Temporaryread-only startupobserver ACTIVE session65971/pythonPID31844, recovery/watch_startup_attach.py. StreamsActivityManager logs on5554; ifmainapphasnotattached6safterfork, captures su0debuggerd -b PID tostartup_attach_<pid>.txt once. Logrecovery/startup_attach_observer.log. Autoexpires90min (started~23:25:55); STOPobserverafterextractdiagnosticsbeforeunrelatedtestsifstilllive. Doesnotdriveapp/changeassertions. NeedinspectanyCAPTUREs. Currentnormalstartupseen~0.8s.
- Extract19DestinationQuartzonEurope PASS, now20USA DiamondOEM at23:26:54. Main89259/26792 +batch27572live/noholds. KernelstillTSC withnoinstability/oom messages in dmesg. Allregeneration/finalcleanfullsuiteNOTstarted.
- Currentextract23Quartzon3DEurope imported10files at23:35:57, launchpending. Extract20DiamondOEM,21LogitechOEM,22QuartzonUSA allPASS firstattempt. Main89259/PID26792 andbatch27572 live/noholds. Startupobserver65971/python31844 live; zeroCAPTUREs through23:35:57. Allregen/finalfullsuite stillnotstarted. Secondarybackupverifiedbut5556notstarted/configunchanged.
- Extract23Quartzon3DEurope PASS; current24/39 VertigoSeriesUSA file-only started23:37:00. Main89259/26792 andbatch27572 live/noholds. Observer65971/python31844 live, noCAPTUREs. Exactreport run_20261010_222846 hashistoricalcase9POFfailure(separatefixedPASS), allothercompletedcasesPASS; cases13and18requiredexistingfullrecoveryreruns withpreservedevidence.
- Extract24VertigoSeriesUSA file-only PASS and25D2Europev1.1 full-launch PASS. Current26D2Europe staging23:41:28. Main89259/PID26792 +batch27572 live/noholds; observer65971/python31844 live/noCAPTUREs. Primarydata3.6GiBfree observed23:38. No newtest/build/formatter processes; Allregen andfinalcleansuite stillnotstarted.
- Extract26D2Europe PASS afterexistingwhole-file stagingretry onTrack12. At23:42:09 clientfailedconnect thenattemptedsecondADBserver40360; log23:42:31 bind10048becauseoriginalserver31612stillowned5037. Preserved recovery/adb_server_europe_d2.log. adb server-status confirmsversion37.0.0build14910828sameSDKbinary. WindowsglobalTCP:937TIME_WAIT,65established,16384dynamicportswith~1560excluded; noSystem4227/4231events last12h. No evidenceofportexhaustion; hostconnectioncauseunproven. Audit session76291 terminal0.
- Current27/39 D2USA3LevelInteractivePreview imported4files andverifiedsize/SHAofHAM/HOG/PIG; launching23:45:43. Main89259/PID26792 +batch27572live/noholds; observer65971/python31844live/noCAPTUREs. Allregen/finalcleansuiteNOTstarted.
- Extract27D2USA3LevelPreview PASS AhaywehGate afterassetSHAchecks. Current28D2USAAlt at23:46:50. Main89259/PID26792 +batch27572 live/noholds; startupobserver65971/python31844live/noCAPTUREs. Allregeneration+finalcleanfullsuite stillrequired, secondary5556stillnotstarted.
- Prepared recovery/start_secondary_suite.ps1 (syntaxPASS, NOTRUN): checks originalLight2configstillmatchesverifiedbackup, setsclocksource=tsc, starts5556SW/headlesswithprivate recovery/secondary-suite-userdata.img, verifiesrunninglaunchsettings/clock. Leaveunrelated5586alive. Suggestedhandoff: holdmain26792ONCEduringlastsingle-deviceextractcase292, letitschildfinish, provision5556serially, resumeoncebeforedual tier. No holdsoutstandingnow. Fourextracttiercases289all_extracts, thenGOGD1/GOGredbook/MacSAF(sortedcatalogorder);10dualtests293..302afterward.
- Extract28D2USAAlt PASS; current29starting23:50:53. Observer65971/python31844live/noCAPTUREs; main89259/26792 +batch27572live/noholds. No secondaryAVDchangesyet. Allregen/finalcleansuiteNOTstarted.
- Prepared finalwrapper now hashes everytracked+unignoredfile before/after canonicalfinalsuite, records input-changes manifest forcurrent-treeaudit. SyntaxPASS; finalsuiteNOTRUN. RegenerationwrapperstillcapturesignoredJSONoriginalbytes/hashes/mtimes+newfiles. Thesearetempverificationhelpers, nottestexpectationchanges.
- Extract29USA rerelease PASS AhaywehGate after~65slaunch; current30USAv1.1 staging23:55:23. Main89259/PID26792 +batch27572live/noholds, observer65971/python31844live/noCAPTUREs. Allregen/finalcleansuiteNOTstarted. Secondary startupwrapperNOTRUN and5556notstarted.
- Extract30D2USAv1.1 PASS; current31D2USA original sanitizing23:59:20. Main89259/PID26792 +batch27572live/noholds; observer65971/python31844live/noCAPTUREs. Tempfinal/regenwrappers corrected empty-change array emission to[] andsyntaxPASS. BothNOTRUN. Sourcehashfinalsnapshot coversalltracked/unignoredfiles (6075observed), notonlycodeextensions. Secondary wrapperNOTRUN.
- NEW real import/introspection timeout: extract31D2USA firstattemptfailed00:01:34. Preserved recovery/d2_usa_import_introspect_timeout.logcat.txt: import_cd starts00:01:02, introspectbroadcastenqueued00:01:04, importfinishes00:01:32, introspectwritten00:01:34.789 after30sADBdeadline. UnlikehostTCPtimeouts, requestreachesAndroidandwaitsbehindimport. Investigatingautomationreceiver/threading beforefix.
- HELD main26792 ONCE andextractbatch27572 ONCE at00:02. EACHHASONEOUTSTANDINGSUSPENSION. Actualextract31retrychild33140stillruns; allowfinishbeforedevicework. ResumeBOTHonceafterfix/verification. Main89259liveheld; observer65971/python31844stillactive.
- Importbudgetrootcauseconfirmed: SetupActivity.commandReceiver.runIo usesgoAsync withDispatchers.IO, soUIisnotperformingimport; Androidserializesbroadcastcompletionbehindpendingcommand. Get-SetupIntrospection synchronousbroadcast default30s contradictedexisting300s CD/ISOimportbudget. Updatedtest_extract.ps1 introspectionTimeoutSeconds param +bothimportloops remainingdeadlinebudget. Exhausteddeadlinebreaksnormalimport_timeoutfailure; file/classification/launch assertionsand300sbudgetunchanged. Scopedformat session27128terminal0; diffcheckPASS. PS5.1compatibilityrunneractive. NoKotlin/nativechangesneeded.
- PlannedresumeBOTHcoordinatorsafterhostchecks, letremainingextractsrunnewfixture. Originalretrychild33140stillalive may beblockedbyheldparentoutputpipe; doNOTwaitforeverwhilebatchheld. FreshfullD2USA targetedrerunrequiredafterbatchtoverify>30simportcase withfix. Bothmain26792andbatch27572 STILLHELDONCEeachuntilexplicitresumecall.
- ImportbudgetPScompatibilityPASS currenthost+WindowsPS5.1,520scriptsparse; session20285terminal0. Resumedbatch27572andmain26792eachONCE00:06:20status0, NOHOLDSoutstanding. Remainingchildspawnsloadupdatedfixture. MustrunfreshD2USA fulltargetedverificationafterbatchandbeforefinalallpassclaim.
- Extract31D2USA oldscriptretryPASS(attempt2), butnewbudgetfixtargetedrerunstillREQUIRED. Extract32InfiniteAbyss newfixturePASS, CDimport26s sooriginal>30swaitconditionnotyetcoveredbynewfixtarget. Current33QuartzonISO-Version CUE+IMGstaging00:11:12. Main89259/26792 +batch27572live/noholds; observer65971/python31844live/noCAPTUREs.
- Avoidwaitingforeveronchildrenwithsuspendedcoordinator: redirectedoutputmayfillpipes. PreferfreshD2USA targetedrunaftermain302terminal (devicefree), beforeAllregen. Secondary5556mustbepreparedbeforedual tier; keepwatchonlast3extractintegrationsafterbatch. Secondarywrapperprepared/notrun.
- Extract33QuartzonCUE/IMG PASS,34DimensionsUSA file-only PASS,35D2+Vertigo combined2sources/21files PASS. Current36goginstallersD2Full (15hostextractedfiles) started00:18:22. Main89259/PID26792 +batch27572 live/noholds; observer65971/python31844live/noCAPTUREs. Test289timeout7200from22:28:46; current~109min incldebugholds,4sourcesleft. FreshD2USAbudgetfixtargetedrerunstillrequiredaftermainterminal. Allregen/finalcleansuiteNOTstarted; secondary5556NOTstarted.

- 2026-10-11 00:33: Extract batch stopped after source 36 GOG full-introspection/switch-set timeouts; first 35 completed with historical sparse-POF failure already repaired. Source 31 import wait fix formatted and PS7/5.1 checked; targeted long import remains. Sources 37-39 remain. Mac SAF now honors DXX_TEST_APK; formatter/integration pending. Recovery reinstalls master retained original APK, so final clean suite must rebuild all fixes. Secondary original AVD was locked by unrelated read-only 5586. Restored its original config and created isolated Nexus5X_Suite_20261010 for 5556 with private data, software GPU and TSC. Main briefly suspended once at dual boundary and resumed once; no holds remain. GOG logs show reconciliation copying/hashing large descent_ii.gog under content lock; capture stacks on fresh reproduction before deciding fix.

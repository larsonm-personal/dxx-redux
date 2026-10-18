# Full test and regression run, 2026-10-16

Requested scope: run every test and regression script, investigate simple test
failures, and leave regenerated JSON for review without a regression-diff audit

1. Read repository instructions, save initial worktree status, and enumerate the
   full test catalog and regression entry points
2. Run `android/run_all_tests.ps1 -FullSuite -FullExtracts -ExtendedGraphics
   -ExtendedMultiplayer` unattended, serializing shared emulator/build state
3. Run `android/regenerate_all_regression_data.ps1 -Category All` to regenerate CD,
   fingerprint, mission metadata, and GuideBot simulation data
4. Identify independently runnable tests outside the canonical catalog, run those
   supported by this Windows host, and document platform/manual exclusions
5. Investigate straightforward failures, verify any fixes with focused checks,
   and preserve complete logs, exit codes, and final coverage/artifact locations

Evidence root: `temp/full_validation/run_20261016/`
Initial worktree already contains modified mission metadata and simulation JSON
Do not discard or restore pre-existing changes

Status: full unattended suite running, all-category regeneration queued after it

Completed supplementary checks:
- Initial worktree status, binary diff, hashes, and full catalog saved
- Python sweep: 78 unittest modules attempted, with per-module logs and JSON
- Four standalone source-guard scripts passed
- Missing developer-shell compiler and spectral Python dependencies resolved for
  focused retries; asset alternatives, co-op messages, D1 briefing, and spectral
  tests passed
- Fixed stale co-op source-contract assertion to cover the expanded negative
  save-version matrix; all 12 regression-tool contract tests passed
- Fixed MSVC lifecycle fixture warnings (shadowed fixture state, exception from
  a C boundary) and initialized the production graphics rollback snapshot to
  resolve C4701; all 11 lifecycle checks, including eight compiled variants,
  passed with MSVC
- Scoped repository formatting/lint checks passed for each changed code file
- Exact-source save-callsign fixture passed all seven checks with a repo-local
  cc wrapper selecting NDK Clang's Windows target and the installed MSVC linker
- Final supplementary Python summary: 77 module passes, one environment-blocked
  module; details in `python_final_summary.json`
- Explicit host/emulator probes and the two locally available private D2 demos
  are queued after all-category regeneration in `run_supplemental.ps1`

Remaining environment limitations in supplementary checks:
- Windows symlink privilege blocks one nightly-sweep fixture
- Unittest success may include declared per-case platform/media skips; preserve
  logs and do not describe these as full leaf coverage

Running process sessions for continued monitoring:
- Full suite: exec session 52436, wrapper writes `full_suite.exit` when finished
- Regression queue: exec session 39175, waits for that sentinel then runs
  `regenerate_all_regression_data.ps1 -Category All`, writes `regeneration.exit`
- Supplement queue: exec session 45937, waits for regeneration then runs the
  explicit probes in `run_supplemental.ps1`, writes `supplemental.exit`
- All paths above are relative to the evidence root
- As of approximately 23:23 PDT, master test 5/329 (Android distributions) is
  running; tests 1-4 passed, first distribution build finished, next is underway
- APK build and emulator/server preflight passed
- Supplementary Python work and scoped quality checks are finished
- Extra WAV parser script passed; FM quality/feasibility and full/public address
  sanitizer runners have been added to the supplemental queue
- Latest distribution logs show progress through the legacy build; no new
  master test failure has been reported yet
- Continued inventory added exact CLI checks for strict fingerprint admission,
  D1/D2 FM playback, classic demo export transactionality, Obsidian liquid and
  worker/route behavior, and Windows replay-window progress
- Verified the Obsidian fixture archive layout before configuring its extraction
- Approximately 23:29 PDT: distribution test has reached the CI APK variant;
  full-suite process session 52436 remains live, disk reserve about 29.9 GiB
- All four distribution variants passed; extractor CTest passed 57/57
- Fixed `test_android_test_target.ps1` to load the new `Install-SuiteTarget`
  dependency alongside the extracted preflight function; focused rerun passed
- Combined C++/Python/PowerShell scoped formatting and lint checks passed
- Castaway level 2 restored-switch route failed with a 30-second simulation
  budget timeout; both run JSONs and aggregate simulation JSON are preserved in
  `android/temp/test_castaway_level2_restored_switch_10ed60a8cc184120862e730d632e6b61`
- The early route tests use pre-existing binaries via `-NoBuild`; the route
  executable did not pass the source-revision freshness check. Queue focused
  reruns after native builds/regeneration before treating this as a confirmed
  regression. Do not audit or rewrite route expectations
- `temp/full_validation/run_20261016/read_progress.ps1` reads the master's
  durable observations and reports counts/current test/failure log paths
- Latest verified wait: session 52436 live, 41 completed (38 PASS, two original
  FAIL including the resolved target fixture, one SKIP), flyout safety running
- DOS MIDI parity passed; both headless and graphics standalone wrappers for
  the installed private D2 recording corpus are queued in addition to its
  strict repeated headless qualifier
- Native D1/D2 rebuilds completed during flyout safety; D2 route freshness now
  passes. Flyout checks passed for D1, D2, and Grand Finale
- Fresh-native Castaway retry reproduces the same simulation-budget timeout;
  see `castaway_fresh_native_retry.log` and new JSON under
  `android/temp/test_castaway_level2_restored_switch_7b7f370865324be09694180175a3835e`
  Leave this route output for review rather than changing engine expectations
- Latest master observation: 43 completed, 40 PASS, two original FAIL, one SKIP;
  fingerprint/AcoustID check running
- JVM checks passed: app 1190 executed/one skipped, metadata core 18 executed,
  proxy three executed/one skipped, zero failures/errors. Copied 199 fresh XML
  reports into `jvm_results/` and recorded `jvm_summary.json` in the evidence root
- Release orchestration fixture passed; latest master is 50 completed
- Full physical route owner selected all 87 cases (`exhaustive=true`); nine
  passed so far, case 10 running. Its summary is at
  `android/temp/route_regression_cases/run_20261016_235132_183/summary.json`
- Route-case failures now include Castaway L9 (metadata precondition; no engine
  run) and Crossfire L15 (engine runs confirm but normalized report records a
  metadata route mismatch). Crossfire fresh JSON is under
  `android/temp/test_crossfire_zero_countdown/20261016_235710/results/`
- Supplemental runner now reads the finished physical-route summary and
  reruns only failed/timed-out cases after metadata regeneration, preserving
  original assertions and recording whether a real refresh resolves them
- `read_progress.ps1` now includes nested route completion/failure details
- Confirmed server NAT coverage is already included by `test_server_integration`
  through its unrestricted `cargo test`; do not duplicate the NAT subset runner
- Latest route progress: 50/87 completed, same two failures, current waypoint
  clearance case. Both queued process sessions 39175 and 45937 remain live
- Full route owner completed all 87 cases: 85 PASS, two FAIL (Castaway L9 and
  Crossfire L15), no skipped cases. Main suite continued to saved-world checks
- Copied the complete route summary and all per-case logs into `route_results/`
  in the evidence root, together with fresh Crossfire L15 and Castaway L2 engine
  JSON. `artifact_scopes.json` distinguishes selected fresh levels from older
  unselected levels retained in aggregate simulation reports
- Saved-world and secret-transition checks passed, including repeated Lost
  Levels secret revisit and native return-save assertions
- Guided-shot annotations passed using fresh Bitesize/Plutonia metadata;
  artifacts are under
  `android/temp/test_guided_shot_annotations/run_20261017_001815_009`
- Latest master progress: 68 completed; input-demo failure controls running
- The one executed master SKIP is `test_dependency_install`: its installer
  transaction fixtures require Linux/flock; portable dependency checks passed
- Additional CMake inventory found two standalone authored CTest registrations
  outside the master/native/extractor suites: ETC2 limit tests and FluidSynth
  rendering contracts. `run_additional_ctest.ps1` is queued first in the
  supplemental runner; it records per-project logs and JUnit XML and continues
  to the other project if one fails
- Latest master observation: 80 completed, archive-variant checks running, no
  new top-level failures
- Native host suites passed: D1 75/75 and D2 86/86 CTest registrations. Copied
  both complete native logs plus the extractor log into `native_results/` in
  the evidence root, with `summary.json` and `media_exclusions.json`
- No known real-media internal-skip messages were detected in those preserved
  CTest logs; extractor registration result remains 57/57
- Latest master observation: 102nd test (primary target grates) running;
  PowerShell 5.1 compatibility passed and no new failures were reported
- Primary-target grate script generated all five requested cases, then failed
  normalized report/metadata agreement for Vignettes L17 after physical engine
  checks passed. Preserved JSON/logs under `route_results/primary_target_grates/`
  in the evidence root; its `scope.json` describes fresh selected levels
- Added a focused `test_primary_target_grates -NoBuild` rerun after metadata
  regeneration, preserving original expected objectives/report assertions
- Latest master progress: 110 completed; route regeneration audit running,
  four original top-level FAIL observations including the repaired target test
- Secret-area baseline test generated complete D1/D2 JSON and failed structural
  comparison with the checked-in baseline. Preserved actual combined baseline,
  per-game JSON, and test log in `regression_baselines/secret_area/` under the
  evidence root. Do not accept/bless the baseline before user review
- Route regeneration audit and catalog validation passed; latest master has
  119 completed, with full Cargo server/NAT integration running

Next: monitor the long-running sessions/logs, investigate simple failures as
they appear, allow every regression stage to finish, and summarize coverage and
fresh JSON locations. Do not mark the goal complete while queues are pending
- Windows Modern Standby paused the live jobs between 00:48 and 03:28 PDT;
  the main suite survived. A temporary process-scoped system-awake helper now
  watches the main/regeneration/supplemental jobs and releases its request when
  they finish. No persistent power settings were changed
- Server Cargo integration/NAT suite passed; complete output saved in
  `server_results/cargo_full.log` and group counts in `results.txt`
- `test_test_execution_evidence` passed on retry after standby; its original
  nested 45-second master invocation had timed out during the pause
- `test_test_runner_result` exposed stale AST extraction: newly used
  Write-SuiteEvidenceJournal and Install-SuiteTarget were missing. Added both
  real helper definitions and the retail provisioning context; focused retry
  passed all result, evidence, timeout and provisioning recovery checks
- Scoped quality retry initially could not launch installed clang-format due
  to sandbox executable access; rerunning that same scoped check with access
- Final scoped mixed-language quality invocation passed for all five changed
  source/test files; git diff --check also passed
- Main suite reached single-emulator tier; first Android automap, diagnostic,
  crash-restart and mod-patch checks passed. Regeneration/supplement queues
  remain live and serially waiting for their predecessors
- Nightly tooling symlink fixture was retried with normal host access and still
  reports WinError 1314. This is a host privilege limitation: 54 cases passed,
  one errored at symlink creation, one declared Linux-only skip. No assertion
  or Windows policy was changed; log `nightly_tooling_privilege_retry.log`
- New Android mod_renderer failure: D1 passed 11 real GLES cases, D2 failed
  higher-priority UUD PNG versus lower-priority KTX2 equality after its door
  checks. Preserved original d1 JSON and both logs in `renderer_failure/`
- Added expected/actual JSON diagnostics at that assertion, with unchanged
  requirements, and queued a serial no-engine-build retry in supplemental
  tasks. Do not run an extra emulator job beside the live master/regeneration
- Android mod_sounds D1 passed; D2 failed semantic door assignment/timing after
  HAM patch rejection. Original logs and D1 JSON copied to `sound_mod_failure/`
- Direct cause: d2/main/robot.c jointlist_read_n clears offset to zero whenever
  n_joints is zero. UUD2SP raw HAM patch test op 580 expects robot 33
  AnimState0_0Offset=685, but loaded canonical value is zero. Validator rejects
  the whole patch, leaving stock door assignments. No patch test or engine
  normalization was weakened; a serial no-engine-build retry is queued
- Master progressed to Android pause-state tests, 153 completed, 9 original
  top-level failures including three fixture/interruption failures now resolved
- Blame attributes empty animation-offset normalization to e193bdb8af from
  2026-10-16 20:00 PDT (mission/mod security fine-tuning), establishing a recent
  change associated with the UUD2SP patch-test mismatch. Keep normalization and
  patch expectations intact pending a deliberate compatible fix
- Focused passing retries are now recorded in `retry_results.json`, separate
  from original master observations, with logs, exit codes and source hashes
- Shared asynchronous checkpoint checks passed; master reached audio mixing,
  162 completed, with original failure count unchanged
- WSL Ubuntu is installed. The original WSL inventory probe required normal
  host access; native Ubuntu includes GCC, Ninja, CMake, SDL 1.2 and SDL_mixer
- Nightly tooling module passed all 56 cases in Ubuntu, including Windows-
  privilege-blocked symlink coverage and Linux receipt discovery. Original
  Windows result remains preserved; Python summary adds separate platform data
- Downloaded repository-pinned portable PowerShell 7.6.6 into run-local
  linux_tools and verified its SHA256 against official release hashes (the
  upstream hash manifest uses UTF-16). No system runtime installation was made
- All six actual Linux installer transaction scripts passed through the
  repository's native-filesystem WSL bridge: temp files, JDK, archives,
  cmakelang, PowerShell and SDK. Log `linux_installers.log`, exit zero
- Audio mix validation passed all seven scenarios. Preserved report JSON/HTML
  in `audio_results/`; full captures remain in android/temp/audio-mix-normalization
- Queued real Linux native Debug all-target builds/CTest for both D1 and D2,
  plus dependency-light extractor CTest, as one serial supplemental task. The
  driver continues to D2/light tests if an earlier suite fails, retains logs,
  exit files and JUnit XML, and uses separate new buildd1-test/buildd2-test dirs
- Removed obsolete blanket Linux exclusion from the supplemental manifest
- Supplemental driver syntax parsed successfully after queue additions
- test_automation_result_evidence reproduced a fixture-only failure: interpreter
  discovery chose the Windows Store python3 app alias instead of installed
  Python. Prefer python on Windows, python3 elsewhere. The full original
  wrapper/snapshot/diagnostic/byte-bound control script now passes; recorded
  alongside other focused retries in `retry_results.json`
- Generic scratch retention rejected root-level native build directories;
  supplemental setup now retains only its planned Linux report directory.
  Canonical native fixed build trees are new/reused, without deletion or --clean
- Renderer expected/actual diagnostics compiled and linked successfully for both
  Android D1 and D2 using the existing per-game NDK fixture build trees; results
  are in `renderer_diagnostic_builds.json` and its log. Runtime retry remains
  queued to preserve single-device serial execution
- Rechecked current source hashes against all four successful focused master
  fixture/interruption retries; all provenance records still match
- Latest master progress: 170 completed, compressed music metadata running;
  ten original top-level failures, including four resolved focused retries
- New live controller rebind failure: the D2 baseline passes 69/69 steps, but
  returning to launcher and holding L2 for 2300 ms leaves selected_control=null
  at launcher fixture step 3/12. Preserve assertion and production behavior;
  no speculative timing/selection change made. Original/baseline logs are in
  `controller_rebind_failure/`
- Queued actual D1 and D2 retries with an isolated wrapper that captures bounded
  ADB logcat, automation result/log and window-focus diagnostics after each
  test. Both supplemental driver and new diagnostic wrapper parse successfully
- Controller raw input, response, and D1/D2 touch-hotplug checks passed; live
  rebind remains the isolated failing controller owner with diagnostic retries
  queued. Latest primary progress reached 186 completed
- test_coop_save_compatibility (the active IDE script) passed the real save,
  discovery warning and Start fresh controls. Its complete log is retained in
  the main tests report directory
- Counterstrike L20 post-boss GuideBot checks passed; L21 pre-reactor checks run
  next. No additional original failure after the live-rebind owner
- Added compact `triage.json` linking seven unresolved main findings to saved
  evidence and pending retry actions. Its referenced evidence directories exist
- D1-in-D2 standalone, D2 level-7 reactor/water, D2X-XL texture animation,
  death/respawn and demo file-set content checks passed
- Demo import owner passed: all 11 original archives passed instrumentation
  import/readiness/failure-preservation controls, and both D1 shareware runtime
  packages traversed seven levels and completed endings under D2
- Latest master is 197 completed; public first-play demo import now running,
  11 original failures (four resolved by focused retry), one Windows skip now
  exercised successfully through Ubuntu. Regeneration and supplemental queues
  remain live and waiting; disk reserve is about 25.7 GiB
- Public first-play demo import passed, including interruption/recovery,
  generated mission selection, save/restore and launcher resume
- Reran the SDK-free mocked-device test_test_helpers_process_wait after its
  original passing run was suspended for over two hours. Fresh unchanged test
  passed in about two seconds; recorded in `retry_results.json`
- Disc-content import passed. Master reached 200 completed and distribution
  launch checks, with original failure count unchanged at 11
- DXA real-pack asset-use owner passed all four modes and all three phases:
  D1, D2, imported D1, Mac D1; off/on/off-again. Final payload/playback verifier
  confirmed 98, 183, 98 and 98 matching resident samples respectively
- Preserved all 36 fresh per-phase JSON/usage/log files plus full owner log in
  `dxa_results/`; `scope.json` records paths/sizes/times, each verified newer
  than this owner execution start. Original bytes were copied unchanged
- Master reached 203 completed, D1 singleplayer endgame running
- Enemy Within full-wrapper/URI/Rebirth import and level-2 checks passed;
  engine preferences and FOV CPU visibility also passed
- Master reached 209 completed, FOV demo compatibility running. No new failure
  during this interval; 11 original failures remain (four resolved focused
  retries), with original Windows-only skip exercised in Ubuntu
- Disk reserve is about 19.5 GiB, down from 25 GiB. Preserved DXA phase copies
  total only 12.6 MiB; do not attribute the larger reserve change to these
  evidence copies. Continue monitoring before larger regression stages
- Current and queued job handles remain live. All-category generation has not
  started; supplemental retries/native/ASan work also remain pending
- Master FOV demo, gamepad menu navigation, GLES3 VBO arrays, black-output,
  graphics capabilities, confirmation input, context-loss and EGL-failure
  controls passed; first-run graphics matrix is now running
- Catalog inspection confirms test_graphics_first_run has no arguments and
  defaults only to native D1/D2. Added its explicit -D1InD2 scenario matrix to
  the serial supplemental queue. Updated supplemental driver parses cleanly
- Latest verified main count is 218 completed, 11 original failures unchanged
- All three process sessions and the temporary awake helper remain live
- First-run graphics matrix is still running at main count 218. Native D1
  Accept, Cancel, Back, Timeout, Previous, Unchanged and Background cases have
  passed so far, alongside opposite-engine handled-state checks. Evidence is
  in android/temp/graphics-first-run-20261017-052608
- Disk reserve stabilized around 19.1-19.5 GiB. No new original main failure
  during the graphics interval; all-category regeneration remains queued
- First-run graphics owner failed at D1 Rearm after all earlier D1 scenarios
  passed. Captured state already shows foreground, EGL recreated, chooser
  editing, TexFilt=2, accepted=0, first_run_pending=false and no deadline
- Source confirms host background helper now returns through launcher launch,
  avoiding BACK/native pause menu. Rearm fixture still required the old pause
  menu plus pending=true and sent Escape. Removed that stale intermediate gate
  and Escape; stable rearmed chooser, foreground, EGL and persistence/cancel
  assertions remain intact with the original wait bound
- Preserved complete original matrix files/logs and failure_state.json in
  `graphics_first_run_failure/`; added finding to triage.json pending verification
- Queued D1 Rearm retry and full D2 first-run matrix (not reached after failfast),
  plus previously queued imported-D1 matrix. Both scripts parse successfully
- All eight changed source/test files passed scoped mixed-language quality;
  git diff --check passed. Graphics lifecycle, mode restoration and MSAA
  allocation-failure owners passed after the stale Rearm failure
- Supplemental driver now writes plan.json with scheduled names, arguments,
  timeout bounds and Android readiness before execution. Failed Android
  startup/provisioning records an explicit BLOCKED result and nonzero exit,
  rather than silently omitting device cases. Driver syntax checked
- Graphics previews passed. Full native graphics recovery owner passed all 18
  cases: nine faults each in D1/D2, including record/publication failures,
  rebuild failure, abandoned menu, normal exit, interrupted repair and replaced
  activity. Preserved all 87 output files (45 JSON) plus owner.log and scope.json
  in `graphics_recovery_results/`; original output is only about 1.8 MiB
- Master reached 225 completed, graphics settings confirmation running; 12
  original failures include the stale Rearm fixture now fixed but awaiting
  serial runtime verification. No new source changes after passing quality
- Verified every one of the 87 preserved graphics recovery files exists with
  exactly the original SHA256; supplemental driver still parses after explicit
  blocked-setup logging and plan-manifest changes
- Master reached 226 completed; Video Info edits passed D1 and D2 side remains
  running. All queues and awake helper are live, reserve about 19.2 GiB
- Current unresolved triage contains eight findings, including the corrected
  Rearm fixture pending actual serial D1/D2/imported-D1 runtime verification
- Reviewed Windows Python skips and ran ten additional Linux/symlink/case-
  sensitivity modules in native Ubuntu temp directories using verified pwsh
- Linux probe confirmed same-tick metadata for 26/30 tiny in-place mutations.
  The retail asset race fixture could miss its own mutation. Made its mtime
  advance explicit; production staging checks are unchanged. Complete module
  retries pass Windows 46 cases/six symlink skips and Linux 46/no skips
- Scoped quality passed for all nine changed source/test files; diff check
  passed. Original failing Linux sweep output remains retained
- Nine other targeted Linux modules passed, covering pidfd/subreaper behavior,
  Linux build CLI, symlinks, case-sensitive names, public sanitizer CLI fixtures,
  special files and advisory-lock retention. One generated-audio case still
  requires FFmpeg; preparing a workspace-local Ubuntu package bundle
- Python summary now includes additional platform rows and records successful
  focused retail retries separately from original outcomes
- Portable FFmpeg bundle prepared by downloading 82 Ubuntu packages (~60.9 MB)
  into run-local linux_tools and unpacking them without system installation
  or maintainer scripts. BLAS/LAPACK alternate library directories are scoped
  to this process. Versions/checksums and setup logs are preserved
- Nightly Android fixtures now pass all 21 Linux cases, including real generated
  audio repeatability/overwrite refusal and KVM ACL helper contracts, no skips
- All ten additional Linux modules now have passing outcomes (retail retry
  corrected the timestamp-dependent fixture). Original attempt logs remain
  separate; aggregate Python summary includes these outcomes and focused retries
- Remaining Linux sweep skips are Windows-specific bounded-extractor fixtures
  already exercised on Windows, plus conditional device-node creation unavailable
  to the ordinary Ubuntu user. Retained-capture-only effect test remains skipped;
  compiled HMP converter cases were already exercised through full CTest
- Main headed/headless GuideBot simulation parity passed; high-resolution pack
  release/import/residency checks now running at 235 completed
- Verified current additional-platform Python aggregate has no failing rows;
  stale FFmpeg skip reasons were removed when the successful retry was merged
- Git diff --check passes. Source quality for all nine changed files is green;
  no code edits followed that final quality run
- Latest main count remains 235, high-resolution real-pack matrix active and
  making new game launches. Main/queued/awake processes remain live, reserve
  about 18 GiB. Regeneration and supplemental tasks remain pending
- Audited full regeneration profile coverage: host metadata includes built-in
  First Strike/Counterstrike whenever no archive filter is supplied; All uses
  no filter. GuideBot work items mark every game=d1 entry D1InD2=true. Thus the
  queued All run already covers those routing-set targets/imported simulations;
  no redundant RoutingSet regeneration was added
- High-resolution pack owner passed all 21 phases across D1, D2 and imported D1
  at 128/256/512 resolutions. Scope is real-pack import/residency/disable;
  optional -VerifyAdditiveSprites extension was not selected by this owner
- Preserved all output files including 21 JSON captures, logs and PNGs in
  `hires_pack_results/`, verified exact SHA256 copies, with scope.json and owner
  log. Master reached 236 completed and host-dialog loading checks
- Queued D2-only optional additive-sprite pack checks after the full 21-phase base
  pack matrix passed. Supplemental driver syntax parsed successfully
- Idle-screen-saver failed both games: D1 background/lock cycle never records a
  park/wake; D2 records a wake but controller A injection times out at step 33
  with idle saver still sleeping. Preserved original owner and four full native
  state captures in idle_screen_saver_failure/. Added finding to triage.json
- Queued unchanged D1/D2 idle-screen-saver retries with logcat, automation result,
  step journal and window dumps. No product change or timeout relaxation made
- Started independent WSL D1/D2 canonical native build/tests and dependency-light
  extractor early (two build jobs, emulator still serial). Session 41374 writes
  linux_native_now.exit and linux_native/ evidence. Supplemental Linux case now
  waits for this exact run and reports its exit; it will not duplicate builds
- Main launch-to-automap passed D1 but D2 recorded 3750 ms parked versus 4000 ms
  required after a fixed five-second background hold. Lifecycle park/wake and
  foreground acknowledgements succeeded. Preserved owner/state JSON in
  launch_to_automap_failure/ and added triage finding
- Extended only the D2 main-menu background stimulus to seven seconds, retaining
  the four-second actual-park assertion. Queued full D2 retry with diagnostics
- Scoped mixed-language quality passed all ten changed files after retrying
  access to the pinned external clang-format executable. Diff check passed
- Compact review_status.json plus update_review_status.py index original main
  outcomes, focused retries, triage and artifacts; execution remains incomplete
- After the JSON timing-fixture edit, automation catalog validation passed: 95 standalone JSON tests, 258 standalone PowerShell tests and 400 support scripts; source diff check also passed
- Corrected failure identification: request-mount-scope metadata test passed;
  generated launcher media controls failed before device execution because its
  FFmpeg/ffprobe commands were absent from the original master PATH
- Downloaded official Gyan portable Windows 8.1.2 essentials ZIP and verified
  published SHA256; both executable version probes pass. Source/hash/bin path
  are in windows_media_tools/provenance.json, no system installation or PATH edit
- Supplemental environment now includes this prepared bin path only for child
  processes and retries the original -GeneratedAudio owner unchanged
- Real portable Windows FFmpeg/ffprobe fixture generation passed, MP3 480653 bytes,
  SHA256 97ed4a9b5d00d72f5950d00568f2522fc6c95c34510a6686371f6f047c05ebdf;
  complete probe/provenance saved in windows_generated_preview/
- Linux D1 canonical native build/tests completed successfully: all 76 CTests
  passed. Copied detailed LastTest log, D2 configure/build now follows serially
- Android Uneasy 4 result-cache reuse passed: first analysis ~365 seconds, whole
  six-step owner ~371 seconds including a fast cached second request
- Started independent standalone ETC2/FluidSynth native CTests early, session
  51160, writes additional_native_now.exit/log. Supplemental case now waits for
  this exact result rather than duplicating its build/tests. Scripts parse cleanly
- Standalone ETC2 CTest passed. FluidSynth compiled but original CTest could not
  launch (0xc0000135): shared helper forced DLL output to main/Release while this
  standalone renderer uses bin/Release. Original logs/XML/summary preserved in
  extra_native/initial and additional_native_now.log (exit 1)
- Fixed only standalone FluidSynth CMake Windows DLL output to match its runtime
  directory. Fresh configure/build/CTest passes both ETC2 and FluidSynth; actual
  FluidSynth renderer contracts executed (0.29 seconds). Focused retry session
  30816 terminal exit 0, additional_native_retry.log/exit retained
- Supplemental standalone waiter now consumes the verified retry rather than
  repeating builds. Required scoped quality pass expanded to eleven source/test
  files and is running; main 260 completed, Linux D2 build still advances
- Scoped mixed-language quality now passes all eleven changed files, including CMake; diff check passed. FluidSynth focused success recorded in retry_results.json and review index while preserving the initial failure
- Mission launch-cache passed D1/D2. Android mission ZIP sample passed all three
  archives (-MOON-, af_d1_beta, af-d2x), no skips/failures. Copied three complete
  metadata JSONs plus summaries/logs into mission_zip_batch_results/, verified
  hashes and recorded explicit sample scope; broad native generation still queued
- Main 265 completed, mod fuzz smoke campaign active. Linux D2 build past 1030
  of 1656, all main/regression/supplemental/awake processes remain live, ~14 GiB free
- Fuzz D1/imported-D1/demo-D1 baselines passed; D2 baseline was recorded timeout
  although its durable automation result is PASS 53/53 in 28359 ms. It recovered
  from a launcher/emulator restart, then completed normally. Exact case evidence
  preserved under mod_fuzz_failure/, active original campaign remains untouched
- Root cause: classifier searches all logcat for 'Timed out', including unrelated
  Google Phenotype/Wellbeing background services. Added a regression fixture
  using the captured message, reproduced old failure, then gated textual timeout
  hints on nonzero runner exit. Explicit watchdog timeout still always fails
- All 14 fuzz controller tests pass after fix; before/after logs preserved. Queued
  one original D2 baseline device replay under a fresh controller after main/reg
  runs. Existing campaign loaded old code and will retain original classifications
- Scoped quality passed all thirteen changed files after the classifier fix,
  including five Python files left unchanged by Ruff. Diff check passed
- Fuzz replay now uses the preserved case.json copy in mod_fuzz_failure/ rather
  than depending on future retention of the original campaign directory
- Fresh classifier regression proof added to retry_results.json; six device
  fuzz cases currently passed, one original false timeout. Linux D2 build at
  1635/1656 and still active, broad regression/supplemental queues still waiting
- Linux original runner finished exit 1: D1 76/76 PASS; D2 85/86 PASS, upstream
  compatibility timed out after 120 seconds during robot-entry/awareness group;
  light extractor 36 PASS/22 declared SKIP, zero failures (58 registered)
- Saved D2 original JUnit and LastTest before isolated bounded 300-second retry,
  session 71370, linux_native/d2_upstream_retry.log/exit; gdb is available if needed
- Fuzz all-on imported D1 automation completes but ownership snapshot reports
  zero loaded high-res textures despite mounted packs. All-on D2 independently
  fails enabled UUD metadata assertion. Exact case captures copied under
  mod_fuzz_failure/all_on_imported and all_on_d2; focused unchanged replays queued
- Added these three findings to triage.json. No texture/metadata assertion was
  weakened; main fuzz campaign remains active, original controller still loaded
- Isolated Linux D2 retry also timed out at 300 seconds. Bounded GDB snapshot
  found it blocked in POSIX read -> PhysFS -> ai_restore_preflight ->
  test_boss_sound_storage, low CPU. No debugger scripts were auto-loaded
- Exact unchanged binary with generated fixtures on native Linux /tmp passed in
  15.83 seconds under the original 120-second bound. Logs/JUnit copied back to
  linux_native/, owned successful native scratch removed after guarded resolution
- Cause is unbuffered per-field save-fixture I/O on /mnt/c. Production state.c
  uses existing buffered PhysFS helpers. Changed only four open calls in the D2
  boss-sound fixture to those same helpers, keeping all 180 value/version/endian
  combinations and truncation assertions. Quality pass expanded to fourteen files
- Fuzz smoke finished all 14 selected baseline/all-on cases before its deadline:
  10 PASS, one classifier false timeout, three actual ownership assertions
  (imported-D1 textures; D2/Vertigo UUD metadata). Copied all selected JSON/log/text
  captures with SHA256 verification to mod_fuzz_results/ and recorded profile scope
- D2 and Vertigo logs both reject UUD2 texture HAM patch op606 at robot33 empty
  joint offset (expected685 actual0), same root cause as UUD2SP op580 sound failure
- New buffered fixture source quality passes all fourteen changed files. Native
  verification sessions: Linux60390 full D1/D2 CTests; Windows68882 D1/D2 upstream
  target rebuild/tests. Both preserve unchanged120-second timeout and separate
  build/test/JUnit logs; original failures remain intact
- Linux D1 verification passed all76 CTests after rebuilding the shared target
- Windows targeted verification wrapper mistakenly loaded x64 LIB paths for the
  existing x86 Ninja trees. Both link attempts failed with machine-mismatch CRT
  warnings. Initial logs preserved under windows_buffered_fixture/initial
- Verified newly compiled test object headers remain x86 (0x014c); cached compiler
  is explicitly Hostx86/x86. Corrected only temporary wrapper vcvars environment
  to x86 for relinking/testing; no native build-tree cleanup or code workaround
- Buffered fixture validation complete: Linux D1 76/76 PASS and D2 86/86 PASS,
  with unchanged120-second per-test bound. Windows D1 upstream PASS2.69 sec;
  D2 upstream PASS10.76 sec (original95.70 sec). Both original failures and corrected
  compiler-environment attempt retained. Both native game targets are restored
- Recorded verified fix in retry_results.json and removed resolved Linux finding
  from unresolved triage. Linux supplemental waiter consumes this successful full
  retry plus explicit original light-extractor exit0. Review index keeps initial
  Linux failures separate from final passing source verification
- All fourteen changed code/test files pass scoped mixed-language quality; diff
  check passes. No subsequent tracked-code edits. Main272 completed, music-track
  controls running; regression and supplemental queues still pending/live
- Bounded snapshot comparison excludes a filter-setting explanation for imported
  texture failure: all three snapshots use filter0; native D1 loads15/mips14,
  imported D1 loads0, public D2 demo loads18/mips18. Exact selected counts saved
  in mod_fuzz_results/texture_comparison.json; no further code/assertion change
- Coverage audit: default determinism matrix references an unavailable May11 D2
  demo/state trace; installed private Oct15/16 demos have terminal oracles only
  (no trace sidecars). Explicitly documented this prerequisite exclusion; no
  NoOracle mode, fabricated reference trace or weakened replay checks selected
- Private graphics case now invokes the actual graphics wrapper and compares
  with the freshly run headless primary, matching canonical suite wiring
- Physical campaign explicitly rejects emulators; retained exclusion. Standalone
  NSD script accepts emulators, so added managed-pair D1/D2 reconnect probes
  instead of assuming physical-only. Wrapper provisions task AVDs, captures routes
  and exact test logs, and stops its own newly started secondary in finally
- Existing internal AAB/APK identity pair is available (Oct16 21:26/21:30), so that
  read-only explicit test will run. Its artifact age is distinct from the fresh
  four-distribution build tests already passed by the master
- Main Obsidian L7 switch-guidance failed on infrastructure: metadata all18
  levels passed, then emulator/ADB disappeared after game launch. Health retries
  and ADB reset failed at122 seconds, no game assertion captured. Next owners
  recovered/restarted as needed; master now284 completed with17 original failures
- Preserved owner in obsidian_switch_failure/. Queued unchanged D2 retry with
  current automation/window/logcat diagnostics plus host launch logs captured
  before the next case can overwrite the helper's fixed stdout/stderr filenames
- Further audit found paired-D1 owner supports caller-selected shareware recordings
  and can run against eight committed public D1 inputs. Queued actual
  test_d1_replay_parity.ps1 for that corpus with native-repeat/imported captures,
  rather than excluding the script for absence of private retail D1 recordings
- This owner explicitly reports fidelity coverage gaps and exits2 when comparisons
  have no hard mismatch; preserve that as INCOMPLETE, not PASS or fabricated
  qualification. Its normal source-owned comparison policy is unchanged
- Existing internal AAB/APK read-only identity test passed: separate application
  IDs, matching version1.1/build26860, valid APK signature, identical engine bytes
  in every ABI, no Play runtime SDKs in APK. SHA256/mtime/validator provenance and
  owner log saved under combined_package_results/, explicitly an existing pair
- Supplemental identity case reuses that result only while both artifact hashes
  and validator hash are unchanged; otherwise reruns the actual inspection. Reuse
  wrapper parsed and executed successfully. No app package or bundle rebuilt
- Paired-D1 exit2 is labeled INCOMPLETE only if its explicit qualification-gap
  message is present; argparse/other exit2 errors remain FAIL. Driver parsed cleanly
- Latest master299 completed (281 PASS,17 original FAIL,one SKIP), startup-restore
  failure handling active. Disk~11.9GiB. Regression/supplemental wrappers and
  temporary host-awake helper remain live; full JSON regeneration has not started
- Added two focused UUD extras gameplay probes for GroupA/level2, one per UUD-vs-
  KTX2 priority order. These address the unresolved D2 standalone renderer priority
  failure through actual gameplay rather than expanding unrelated test profiles
- Main author UUD mission owner continues its default seven-palette run separately
- Confirmed new eighteenth master failure was startup-restore preparation, not UUD
  gameplay. It had already passed resume_offer_enabled=true after returning to
  launcher, then appended a native-only introspect action unsupported there
- Removed only that unused generated diagnostic step. Autosave preparation,
  obsolete-version rejection, no-world-replacement and touch-Quit assertions stay
  intact. Original prepare/log copied to startup_restore_failure/, full D1/D2
  owner retry queued. Combined quality passes all fifteen changed files
- Default author UUD gameplay passed all seven palette levels and spider-monitor
  destruction checks. Copied exact JSON/scripts/log outputs with verified hashes
  to uud_author_results/ and recorded scope. UUD combinations owner still active
- UUD combination verifier fails D2 uud-first with no rendered UUD replacements;
  native D1 five phases passed, D2 baseline/UUD-only passed before assertion
  All15 physical captures preserved in uud_combination_results/. Independently
  ran previously unreached imported verifier on immutable captures: same failure
- Expanded existing renderer diagnostic to report custom-bitmap classification
  plus stored/current flags at the mismatch. Both NDK fixtures compile/link;
  renderer_classification_build/summary.json includes current source hash
- Scoped quality still passes all fifteen changed files; refreshed source manifest
  and diff check passed. No render behavior or ownership assertion changed
- Default UUD surface owner passed actual native/imported D1 patched surfaces;
  copied JSON/scripts/logs and hashes into uud_surface_results/. Master309 complete,
  UUD2SP owner active,25GiB free, broad regeneration/supplemental jobs pending

- 09:12 PDT: UUD2SP original owner failed sound-first after automation 40/40 passed. Combined HAM patch rejects robot33 empty-joint offset expected685 actual0, same field as sound/fuzz failures. Sound-only passed; remaining three phases unreached. Preserved owner and all emitted JSON/logs with SHA verification in uud2sp_results; no patch guards changed. Main moving into extraction tier.
- UUD2SP artifact audit: stable owner directory retained nine Oct15 files. Segregated those copies under preexisting_output; scope names only four Oct17 JSON/log files as fresh. No current-run patch JSON was produced before rejection.

- 09:20 PDT: Added read-only audit_worktree.py to index tracked JSON hashes against the initial snapshot and HEAD. Initial154 JSON files are still byte-identical to the user's starting state; one additional JSONC change is the documented automap timing fixture. json_change_index.json is a hash index only, not a semantic or freshness decision; rerun after generation.

- 09:32 PDT: Full extraction owner has10/39 sources passed and zero failures. Both Mac variants, Anniversary Brazil/USA, Destination Saturn, Levels of World, MacPlay, non-launchable TestFlight file checks and Europe variants passed. Parent main original results292PASS/20FAIL/1SKIP; extraction stillRUNNING. All main/regeneration/supplemental process identities remain alive; free24.5GiB. Native terminal results remain green; no new source edits.

- 09:37 PDT UUD source diagnosis: e193bdb8af canonicalizes empty joint offsets in d2/main/robot.c; UUD producer Get-Uud2spHamFieldValue still reads raw int16 values from the source HAM. Rebuilding the unchanged producer alone cannot resolve the semantic mismatch. Robot33 AnimState0_0 test/replace operations recorded in triage. No source/pack change made; this is retained as a product contract finding rather than relaxing guards.

- 10:05 PDT: Extraction source23/39 DestinationQuartzon3D Europe first attempt hit ADB timeouts (launcher30s, force-stop/run-as5s; separate read-only15s logcat probe returned no bytes). Owner restarted managed primary emulator and reran whole spec; second attempt passed import/file and actual launch assertions. Evidence and raw complete-case log in extraction_setup_delay; no test/source expectations changed. Extract23PASS/0FAIL, main original292PASS/20FAIL/1SKIP still running; Vertigo now executing.

- 10:10 PDT: Extract26/39 PASS, zero final case failures; prior source23 infrastructure retry retained. Made JDK21 explicit in supplemental scratch driver and revalidated its PowerShell AST before execution; no tracked source changed. Full All regeneration still waiting on main exit.

- 10:18 PDT: Extract30/39 PASS, no final case failures; source23 passed on second full attempt after managed emulator/ADB recovery. Main remains292PASS/20originalFAIL/1SKIP with extraction active. Regeneration and supplemental remain queued; free24.3GiB. No tracked source edits since scoped QA proof.

- 10:32 PDT: Full39-source extraction owner PASS (all39 cases; source23 passed complete retry after infrastructure recovery). Copied summary, per-case logs and owner log with SHA verification into extraction_results; scope distinguishes launch-verified and declared non-launchable cases and captures test-time expectation hashes before future regeneration. Main moved to focused GOG installer owners.

- 10:52 PDT: Main now22 original failures. New publicD1 host migration: initial sync/host promotion pass; returning peer removed for important-packet loss. Trace verify_objects remote74/local0 with early SYNC/end injections; final objects74 on both, yet returning peer remains in a menu/in_gamefalse and host connected1. Preserved12 original artifacts in public_migration_failure. New retail LAN preparation refuses a failed positive emulator identity probe before any reset; subsequent public variant passes the exact same guard and native D1 two-peer session1. Preserved originalguardfailure. Queued unchangedmanagedpair retries for both afterregeneration in supplemental, with fresh-only artifactcopy policy, emulator-only guards, and ownedsecondary cleanup; ASTbothPASS. No trackedsource/expectedvaluechanges.

- 10:58 PDT: Main full suite terminal exit1. Catalog361:306PASS/22originalFAIL/33SKIP (includes configured/manual/recorded-demo exclusions). All39 extraction cases and allthreefocusedextractor ownersPASS; finalrelayMPPASS. Two new dual-emulator failures captured and queued for unchanged retries. Original failures are retained even where focused verification passes. Primary/secondary emulators torn down by main. Full All regression job now released; supplemental waits for its exit. Goal remains active until both finish and final evidence audit/cleanup complete.

- 11:20 PDT: Canonical All regeneration active in CD stage. Host CD/GOG extraction and expectation refresh completed without new tracked JSON diffs (initial154 user data files remain byte-identical; one additional JSONC diff is documented automap fixture). Fresh debug APK rebuilt successfully and clean managed emulator import/launch validation now10/39 PASS, zero failures. CD child report temp/extract_regression_reports/run_20261017_110339 is indexed in review_status; main original306PASS/22FAIL/33SKIP is terminal. Supplemental remains queued, with unchanged LAN retries and explicit missing-private-D1-fixture exclusion added; scratch scripts parse clean.

- 11:26-11:48 PDT: Detected concurrent external checkout updates from starting commit445d5dcca to651357958 (8 incoming commits). Changes include matcen introspection/baselines, public basic-use coverage and nightly corpus/tooling helpers. Current catalog362 adds public-basic-use owner; original main361-entry result remains immutable. Initial154-file JSON snapshot now differs in153 files and those paths match current HEAD; original patch remains preserved. Do not attribute all hash changes to this regeneration job or restore over the newer work
- Affected host verification: initial scratch driver incorrectly joined multiple Python executable paths; fixed driver and supplemental Python selection to take first resolved application. Raw first attempt retained. Updated GuideBot runner exposed ordinary shared host Python resolver preferring an unregistered py launcher; changed Windows order to direct python, py, python3. This is tracked source change16. Same test now passes, Linux branch unchanged. Scoped mixed-language quality all16 PASS after normal-host execution (sandbox denied installed clang-format; failed launch logs retained)
- Updated Windows host run has two environment failures: nightly link fixture cannot obtain symlink privilege (same updated module passes in Linux); legacy platform child cannot load scripts under sandbox policy (unchanged normal-host retry PASS). No global policy changed. Updated Linux run passed6 Python modules and first-play/GuideBot controls; catalog and dependency-platform controls hit shared Windows dependency path/interop configuration. Those3 retried in an owned detached local clone with copied16 validation changes, Linux config/PATH and containment-checked cleanup; catalog2 already PASS, platform pending at this checkpoint
- Supplemental driver extended before its execution: rebuild current Windows native outputs and full CTest suites first; rerun updated retail/shareware secret-area fixtures; rebuild fresh Android APK and save immutable copy before device setup; add new PublicBasicUse and changed full PublicFirstPlay owners. Execution HEAD and primary-emulator ownership recorded. Unchanged retail LAN-preparation/public host-migration managed-pair retries queued. All scratch PowerShell drivers parse clean
- 11:48 PDT: Canonical CD regeneration import/launch validation29/39 PASS, no final failures. Later fingerprint/metadata/simulation stages and supplemental still pending. Free15.6GiB, above4GiB reserve. Main original306PASS/22FAIL/33SKIP stays terminal; goal remains active

- 11:50 PDT: All3 configured Linux PowerShell retries PASS; owned source clone removed after containment verification. Updated host coverage now verified across11 owners: original Linux8PASS plus3 configured retries, with normal-host Windows legacy helper PASS and Linux updated nightly symlink module PASS covering the two Windows environment limitations. Raw attempts retained. Public BasicUse/full FirstPlay supplemental owners moved to the end of device coverage because they intentionally replace disposable app data with public profiles. No tracked source edits after16-file scoped quality PASS.

- 12:02 PDT: Canonical CD stage PASS after all39 device cases, host extractions and refreshed expectation validation. Copied40 report files with SHA verification in regeneration_cd_results;33 actual game launches,6 declared non-launchable file cases. Captured APK source/hash recorded without duplicating64MB; current-source APK rebuild remains explicit in supplemental to account for concurrent checkout. Fingerprint stage now running.

- 12:14 PDT: Fingerprint stage PASS, elapsed742s. Full CD stage remainsPASS. Mission metadata stage starting with CLI installation/verification. JSON index refreshed without semantic review. Verified no matching CD-owned emulator remained to release; no device was stopped by this cleanup attempt. Corrected CRLF-launcher empty sibling directories removed nonrecursively after exact containment checks; all original launcher error logs retained.

- 12:28 PDT: Mission metadata stage PASS,140 inputs total,140 passed,0 skipped/failed; elapsed701s including fresh native CLI build. Metadata/log/summary snapshot copied with SHA verification into metadata_results; source producer run android/temp/mission_zip_host_metadata/20261017_121427. GuideBot simulation stage active, all2639 levels selected Headless/WriteRegression;114 results recorded at checkpoint. No semantic JSON review or manual oracle acceptance performed. Source-based JSON hash index refreshed.

- 12:47 PDT: All-category regeneration terminal exit1: CDPASS, fingerprintPASS, metadataPASS, simulationFAIL. All2639 headless work items recorded in1090.6s;140 publication files changed;1 infrastructure-classified native exit1 is an input/edition rejection: af_d1_beta target0 level7 cefispar.rdl Robot30 unavailable in selectedD1 edition. Preserved normalized publication files, summary/engine JSON provenance and exact log in simulation_results; full7646-result-JSON producer tree remains at android/temp/guidebot_simulation_regression/20261017_122605. No edition/limit or oracle bypass made. Supplemental driver released and fresh Android buildPASS in2m30, emulator provisioning begun. Goal remains active for remaining supplemental execution and final audit/cleanup.

- Supplemental progress: fresh native metadata checks PASS for CastawayL2, primary grates, both retail/shareware secret baselines, and the2 original failed route members CastawayL9/Crossfire. Original main failures retained; focused pass mappings recorded and corresponding pending triage rows resolved. Secret baselines are incoming committed fixtures, not manually accepted here
- Fresh rebuilt NDK renderer/sound fixtures still fail D2. Preserved current raw JSON/logs in renderer_current_results/sound_current_results. Renderer diagnostic custom0 with stored/currentflags3 refutes the direct custom guard hypothesis; image-priority failure remains, followed by secondary destroyed-mutex cleanup abort. Sound HAM op580 still expects685 versus actual0 at robot33 empty-joint offset
- Scratch current-native guard incorrectly required a fully qualified compiler path; cache stores cl.exe. Corrected guard uses generated MSVC architecture ID and pointer size; actual both X86/4-byte and PE014c verified. Metadata build already refreshed general/headless/metadata binaries at12:16/12:18, after new source11:06, so active supplemental coverage continues without interruption. Corrected full native build/CTest pass queued after supplemental in session66941; it independently holds an awake request (nonzero confirmed) and writes windows_current_native_retry.exit. Old first guard failure/log remain raw; it never executed native tests
- Simulation publication snapshot corrected: WriteRegression publishes directly beside source metadata, not results/*.simulation.json. Copied all140 exact paths from final producer changed_files into simulation_results/published/game_data with SHA verification; full per-attempt7646-result-JSON tree retained at original producer path. Original all-category stage fail is the single Robot30/edition rejection noted above

- 13:11 PDT: Public compact headless owner passed all34 native runs for17 cases; producer retains INCOMPLETE full fidelity qualification. Real-engine negative controls now executing. Added REVIEW.txt navigation index without touching producer evidence. All16 tracked validation source hashes still match the scoped QA manifest; git diff --check passes. Current HEAD remains651357958. Supplemental and queued full current native follow-up remain live; no source changes made.

- 2026-10-17 13:52 -07:00: Both compact public replay owners PASS (headless and graphics). Both retain incomplete full fidelity qualification; graphics passes only its scoped imported flight parity with other imported scenes unqualified. Headless negative controls PASS for wrong oracle, changed input and loot no-collect; original corpus unchanged. Graphics wrong-oracle control already PASS, remaining controls running. Saved snapshot SHA audit covers685 metadata/simulation/CD/current renderer/sound files with zero mismatches; artifact_hash_audit.json indexed. REVIEW.txt regenerated from current durable status.

- 2026-10-17 14:14 -07:00: Ordered-input full D1/D2 owner PASS (D1 interim47 checks retained, final matrix in supplemental/ordered_input/report.json). Wall blast shareware/retail and all four autoselect profiles PASS. All-weapons owner failed before gameplay on exact-clean-HEAD source qualification guard; no supported dirty diagnostic mode. Preserved original failure and added triage requirement. Ran four separate actual native assertion captures using unchanged fixture commands in isolated directories: D1 scourge/bigred, D2 honestbob and importedD1 allPASS. Native diagnostic scope explicitly excludes qualification, no compact oracles generated, no guard bypass, no source commit. Captures and hashes indexed in all_weapons_capture_diagnostic/scope.json; all_weapons_diagnostic.exit0. Public save/load owner active, main/regeneration terminal, supplemental/current-native follow-up pending.

- 2026-10-17 14:24 -07:00: Public save continuation owner PASS; report preserves full_trajectory UNQUALIFIED and importedD1 diagnostic-only scope. Public matcens, all3 native metadata workers, maximum-secret and strict fingerprints PASS. Benchmark policy self-testPASS but first D1 metadata digest differs; copied6 exact JSON/timing/stdout files plus owner log into metadata_benchmark_failure. No digest baseline/threshold changed; remaining benchmark matrix not reached. D1 FM playback ownerPASS, D2 active.
- Classic supplemental failures were scratch caller setup: mixed shared staging directory passed as both removable game roots, and replay-only headless binary selected for classic-export CLI. Initial external-asset retry was rejected by auto-review for possible fixture mutation. Safer approved retry copied13 assets into run-owned separate D1/D2 dirs; all external source hashes verified unchanged. Native classic asset missing/custom cases thenPASS; full GameExe export subcasesPASS using copied d2x-redux. D1 network admission ownerPASS with copied separate roots. Transaction ownerPASS unchanged from required repository working directory with full export executable. Raw original and intermediate wrong-caller failures retained, three focused resolutions recorded. No tracked source edits; original16-file quality hashes remain scope.

- 2026-10-17 14:30 -07:00: D2 FM playbackPASS. Obsidian liquid owner originally passed L13 metadata/all44 worker requests but route discovery rejected supplied Vertigo parent folder. Retried full unchanged owner with disposable missions/ install roots: both Vertigo/Obsidian native L1 checkpoint save/restore and wrong-level/mission rejectionPASS; obsidian_followup.exit0 and SHA scope indexed via focused resolution.
- Original D1 fast replay-window ownerFAIL: Pause request posted, no dialog, native replayexit0. ImportedD1/D2 progress ownerPASS. Source D1 game.c filters Pause sinceOct12 and accepts Space/Right manual flags. Temporary fixture Space probe also fails actual one-second frozen-frame requirement; source shared fast loop directly calls step without manual flag gating. Probe alone does not prove native receipt of posted key messages, so keep this as replay control/policy gap requiring targeted input logging before product fix. Restored original tracked fixture byte-for-byte, git diff empty for it; all16 source manifest hashes still match scoped quality PASS. Temporary probe source/patch/window report saved in replay_window_d1_retry, not retained as a fixture change. No production fix attempted. Paired public D1 replay owner stillrunning, qualificationincomplete retained.

- 2026-10-17 14:36 -07:00: Paired actual D1 owner terminalFAIL exit1, all8 public recordings completed. Native repeatability8PASS; native-to-imported strict comparisons8FAIL; recording-to-native8INCOMPLETE because compact public inputs lack original full observation/RNG companions. Full qualificationincomplete is retained in addition to actual mismatch failure. Saved concise SHA scope and complete producerreport/per-casecaptures; no policy/oracle/trace acceptance. This broad strict mismatch does not negate earlier scoped compact public ownerPASS. FM quality active; sanitizerAll/Public and laterAndroid cases remain queued.

- 2026-10-17 15:03 -07:00: FM audition fullyPASS after two fixture updates: guarded gain_db rewrite/current FluidSynth adapter plus standaloneCMake pinned dependency/shared contracts/C++17/WindowsDLLcopy. BothCTestPASS, default60s/3tracks/15clips registersequal, repeatedrendersbyteequal,noclipping. Fresh report/WAV/listeningpage in fm_quality_retry2/listening. Additional unchanged verify_shipping.pyPASS onall3tracks againstthis fresh offline mathematicalreference,72-73dBagreement plusseek checks; no priorapprovedaudition available/accepted. Sourcecount18 atthatcheckpoint, scopedmixedqualityPASS.
- SanitizerbuildD1/D2PASS; fullCTest D1 79/79 and D2 90/90PASS; all3configboundsPASS. LoaderfixtureComet enginecompletes1493frameswithnoreportedASanfinding butexpectedtexture-repairlog/notesabsent, causingfixture nullindex; remaining3 fixtureassertionsnotreached. All2639-levelASanroutebatchcontinuesindependently (over300completed), keepnewJSONforreview withoutrelaxingrepairchecks.
- DefaultrecordedASanstageshademptydefaultcorpus and attemptedReadLinesemptyPath. Reproducedactualemptyinventoryfailure, fixed run_input_demo_regressions.ps1 tocollect discoveryasemptyarray. Actual2emptyListOnlyvariantsnowexit0; nonlistD1importedrejectswithclearmissingrecordings(exit1), preservingguard. RealprivateD2inventorylists2Octrecordings. Sourcechange19; combinedquality19_retry2PASS aftertwoliveformatterlockrefusals(noformattersstopped). Broader65-casecomparatorunitrerunhad64pass/1Win32file-sharingcleanup error; exactunchangedcase normalhostretryPASS. No extraunitfixtureedits.
- Queued privateASanD2followup session92196: waitsforpublicsanitizerownerterminal log, verifiesaddress-configuredtree,copiesactualrecordingsandusesdisposableassetcopies,runscanonicallegacyreplaywith300s/caseandstrictASANoptions. Independentlyholdsawakenonzerorequest, writesasan_private_d2.exit. NoD1corpusfabricated. Reviewexecution_complete nowalso waitsfor thisstage. Activeprimary jobs remain supplemental45937, finalcurrentWindowsnative66941 and queuedprivateASan92196; FM/unittest/probejobsclosed.

- 2026-10-17 15:10 -07:00: Finalization reminder: wait for all80 supplemental cases, current Windows-native retry and queued copied-private-D2 ASan replay, plus any newly necessary focused follow-up. Do not treat terminal main/regeneration as completion of the whole request. Preserve original raw counts and separate focused resolutions. Final audit should refresh JSON change index/review index, verify19 source hashes against latest scoped QA, run diff check, and confirm owned emulator/power-helper cleanup. Canonical regenerated game_data JSON is Git-visible; also save compact final REVIEW/status copies beside this plan (non-temporary, Git-visible) so the morning summary survives default scratch cleanup. Keep raw evidence in its current paths; no generic cleanup while jobs run. Do not commit or publish. Full fidelity/clean-source/private-D1/physical/manual exclusions remain explicit.

- 2026-10-17 15:21 -07:00: The source19 empty-corpus fix also passes all3 actual Linux PowerShell cases using the provisioned portable Linux binary. Initial plain WSL PATH launch lacked pwsh and is retained separately; no source or system PATH changed. Windows and Linux both list an empty corpus successfully and reject attempted replay of missing recordings clearly. Nonempty actual private D2 inventory still selects2 recordings. Source19 mixed QA remainsPASS; no further source edits. RouteASan batch passed950 recorded levels with no asan.* report files so far; current full verdict pending.

- 2026-10-17 16:07 -07:00: All-category ASan owner finished with raw FAIL. Both builds passed; full CTest D1 79/79 and D2 90/90 passed; all three config-boundary cases passed. All2,639 route work items recorded in4,446.75s, with one af_d1_beta level7 Robot30 edition rejection matching regular regeneration and one producer-classified expected Uneasy4 timeout after360s. No nonempty asan.* report files captured. Loader repair-note expectation and missing default replay corpora remain separate limitations; do not label full profilePASS. Final scope saved under supplemental/sanitizer_all/scope.json. Public ASan profile started, private copied-D2 waiter still pending that profile's exit. No source changes after19-file scoped QA PASS.

- 2026-10-17 16:33 -07:00: Public ASan profile completed all 18 stages successfully, with no nonempty ASan diagnostic files. Both pinned private D2 recordings separately passed actual ASan replay; original input hashes remain unchanged. Direct private replay owner initially inherited Windows executable-directory player settings despite isolated HOME/CWD; a clean executable-directory probe passed all four strict runs. Updated the owner to copy the executable and DLLs into each repeat sandbox. All four actual strict repeats now pass against the normal build; nine Windows controls pass, Linux controls pass with the one Windows-only skip. Combined scoped QA over all 21 tracked validation code changes passed. No recording oracle changed.
- The Obsidian Android switch retry exited during metadata analysis before producing an automation result; retained setup/logcat/window evidence. Startup restore preparation now passes all 24 steps, but the 45-second cold-resume check expires during engine initialization with null introspection. Warning/recovery behavior remains unverified. These original and retry failures remain visible. External uncommitted game_automate.cpp changes observed after 15:59 are preserved separately; the frozen supplemental APK predates them. The remaining Android queue and final native rebuild are still running.

- 2026-10-17 17:09 -07:00: User confirmed another session is using the shared emulator. ApplicationExitInfo independently proves an external APK update interrupted Obsidian at 16:23:18. Stopped only validation dispatcher PID38064 after 57 recorded supplemental cases, preserving the original summary and intentional exit -1. The in-flight D2 matrix has no terminal verdict. Shared emulators were left running; no further device actions target 5554/5556/5558. The current native follow-up completed successfully: D1 79/79, D2 90/90, both full CMake builds pass.
- Created workspace-owned API34 Google APIs x86_64 AVDs FullValidation_20261017_a/b on checked-unused serials emulator-5580/5582. Fresh configuration: 2 cores, 1536MB RAM, 720x1280 at320dpi, 8GB/6GB userdata. A first 16GB userdata boot was correctly rejected by the host-space guard; its exact log is retained in isolated_android/boot_failure_1. Reclaimed only42 byte-identical large staged inputs from the completed ASan route run; every retained raw copy is hash-verified and listed in duplicate_asan_stage_input_reclamation.json. All JSON/log/trace evidence remains intact; no generic cleanup ran.
- Isolated serial driver session64131 repeats/resumes cases50-80 (31 cases) using the original frozen supplemental APK EC6645712C9C36AAD1D3C04EE819F74CEFF6846F284848B79834EEFCA95AA044. Original shared-device outcomes remain separate; consolidated review replaces these case names only when an isolated verdict exists and requires all80 names plus the isolated terminal marker. Native and original power helpers closed; isolated driver owns a new awake request and cleans up only its positively identified new AVD pair. Shared emulators remain available to the other session.

- 2026-10-17 17:40 -07:00: Isolated startup restore passes both D1/D2, including the unchanged45-second warning deadline and native touch Quit;13 capture files hash-verified. Baseline fuzz and generated media also pass on the exclusive device. All-on fuzz owners reach gameplay then fail ownership checks: imported D1 lower-priority KTX2 pack not rendered, D2 UUD metadata absent. Obsidian cleanly reproduces Android metadata worker request timeouts around6.2seconds before gameplay. Native full metadata140/140 remains a separate PASS; no assertion or baseline relaxed.
- The imported graphics matrix passes all10 non-Rearm scenarios. Its Rearm and focused D1 Rearm failures expose an error in my earlier fixture adjustment: the native pause menu remains open with first_run_pending=true and time_paused=true after the launcher handoff, so the original escape/resume step is required. Do not classify these modified-fixture failures as confirmed product regressions. Pinned original source and expected edited-source hash are in original_graphics_rearm_retry. Follow-up session63554 waits for the exclusive31-case queue to finish, snapshots its closed graphics reports, restores the original fixture under a hash guard, checks source quality read-only, and runs all three unchanged original Rearm variants serially on5580. It owns an awake request and stops only that exclusive AVD afterward. The current D2 matrix must finish before any restoration; no active test source is edited.
- All21 current validation files passed a stable read-only quality audit at17:24. Co-owned test_upstream_compat.cpp gained external UDP/rejoin declarations/assertions after the original Fix snapshot; our three buffered reads and one buffered write remain intact, and current native79/90 suites include that file. Its later SHA is recorded separately instead of silently refreshing the original QA manifest. Once the graphics fixture is restored, remove that abandoned change from the final source manifest (expected20 retained code changes), retain the prior21-file snapshot, and record the final original-scenario outcomes separately from raw failures.
- Disk management remains limited to completed-run duplicate inputs:42 staged copies removed from each completed simulation run with raw originals retained and hash manifests saved. Another23 identical, same-mtime large raw copies across those two completed trees now share hardlinks; both paths, bytes and mtimes are preserved, recovering about1.4GiB. No JSON, log, trace, active input or original game_data removed. The unused secondary AVD was resized to4GB for LAN-only work; primary remains8GB. Current free space is about7.3GiB. Use live_brief.py for bounded active progress and live_status.py only for the full overview.

- 2026-10-17 19:11 -07:00: Resumed after the conflicting work. Fresh OS inventory confirms the prior isolated driver and graphics waiter are absent;8 completed isolated outcomes remain raw, with no fabricated exit code. Recorded legacy_job_interruption.json. Restored test_graphics_first_run.ps1 byte-for-byte to its original source under a hash guard; its Git diff is empty. Archived the prior21-file source manifest and removed the abandoned graphics edit, leaving20 retained validation changes. The earlier modified Rearm failures are not confirmed product regressions.
- 2026-10-17 19:45 -07:00: Current HEAD remains651357958; recent co-op source/test updates are present in the worktree and separately hashed in coop_refresh/source_before.json. Persistent driver10068 has an independent awake request and dynamic serial queue. Initial native dispatch was blocked by a scratch optional-dependency bug before any test ran; the corrected explicit-empty-dependency retry passes full Windows D1 79/79 and D2 90/90, explicitly including pickup-lock, robot-door, rewind-capture and level-restart fixtures. Linux D1 passes80/80; D2 rebuild remains active.
- Current-source queue also runs clean app/metadata/proxy JVM tests, optional historical proxy comparison using an available local Git object, public ASan, read-only quality over84 current modified/new code paths, a fresh frozen three-ABI APK, full140-input metadata and2639-level simulation publication, and actual private D2 replay owners. Prepared device coverage comprises63 tasks: exclusive-device setup,3 unchanged original Rearm variants,6 singleplayer persistence cases,23 missing/interrupted original cases,29 affected/new co-op scenarios, and the20-owner current nonmanual multiplayer master selection. Relay scenarios use a third exclusive workspace AVD5584; shared5554/5556/5558 remain untouched. Dispatch the prepared plan only after host/build/regeneration prerequisites and disk/source checks, keep failures visible and investigate simple ones, then seal the queue and verify owned cleanup. No regression oracle or baseline acceptance is authorized or performed.
- 2026-10-17 20:04 -07:00: Current-source host verification is green: Windows D1 79/79 and D2 90/90; Linux D1 80/80 and D2 90/90; native XML audit explicitly verifies all four new co-op fixtures in all four game suites. Linux-only D1 test_d1_level_load_failure accounts for its extra case. Dependency-light Linux extractor36 executed PASS with22 declared skips. Clean JVM1211 executed PASS,2 declared skips; optional historical proxy comparison then runs all4 cases PASS. Current public ASan completes all18 stages with no nonempty asan.* report files, including both full native suites. Stable read-only quality passes all84 current changed/new source paths and the20-file retained-validation subset; original Fix hashes are preserved, with later co-owned deltas separate. An immutable84-file source snapshot matches the phase-start hashes. Fresh three-ABI APK build active.
- The persistent queue now has80 tasks including2 current catalog checks, exact publication snapshots immediately after each regeneration producer, and the63 prepared device tasks. Device tasks are appended behind all host work and guarded by a passing fresh APK build plus provenance/current-source/free-space validation before device setup. Queue remains unsealed for justified follow-ups; do not treat a terminal host run as full completion.
- 2026-10-17 20:24 -07:00: Fresh three-ABI APK PASS, SHA7CD96AC887202A0B5D7A67E43D9918AF6DC3BCBB8B093E4235F0AFD0A7F86895,28 modified product-source hashes unchanged during build. Full current metadata140/140 PASS; exact140 publication copies hash-verified. Full simulation2639 selected,2549 native jobs active; known af_d1_beta level7 rejection reproduced verbatim (Robot30 unavailable selectedD1 edition), no edition bypass. Added current four-distribution owner before devices to verify updated co-op code in Play/GitHub/Legacy/CI packages, both engines and all3 ABIs, including lower legacy Android API compilation; queue now81 tasks. Artifact SHA audit1115 files has0 mismatches;20 validation files match the current stable read-only quality snapshot, with the separately recorded co-owned upstream delta preserved.
- 2026-10-17 20:39 -07:00: Full current simulation records all2639 selected levels in943.003seconds,140 changed publication files, the same1 Robot30 input rejection,0 expected timeouts. Exact140 simulation publication copies hash-verified; saved-evidence audit now1256 files with0 mismatches. Actual private replay4 strict runsPASS, separate2 headless and2 graphics recording ownersPASS. Both current catalog checksPASS. Fresh Play/GitHub/Legacy/CI distribution ownerPASS, both engines/all3 ABIs and lower legacy API. Current host/build/regeneration work is complete; all63 device-stage tasks remain required.
- Device setup actualFAIL was INSTALL_FAILED_UPDATE_INCOMPATIBLE: original frozen APK certificate3837d616... differs from fresh isolated-Android-user-home debug certificate724cab80... (public fingerprints verified with apksigner37). Android correctly refused the update;62 dependent cases were BLOCKED without execution. Explicit disposable-fixture reset retry then exposed two scratch harness errors before any reset: exact identity comparison did not split the console reply's name/OK lines, and emulators started under a per-test Windows kill-on-close job disappear with that worker. Both original failed attempts and all blocked rows remain raw; no product/device test verdict is fabricated.
- 2026-10-17 20:47 -07:00: Fixed scratch identity parsing to require an exact trimmed name line. Launched independent hidden keeperPID9100 from normal host (identity/start guard in coop_refresh/emulator_keeper); it starts only owned5580/5582 outside per-test jobs and performs no automatic restarts that would interfere with restart tests. Primary booted and positively verified; secondary first boot still active. Keeper readiness gates requeue_after_keeper.py, which appends all63 tasks with keeper_retry2 IDs and explicit ResetSigningFixture/unique recovery evidence path. Queue remains unsealed; after completion stop the keeper via its stop file and verify only positively owned5580/5582/5584 cleanup. Persistent dispatcherPID10068 retains the awake request. Current144 raw rows comprise16PASS,3FAIL (simulation input plus2 setup),125BLOCKED; these are not144 actual test executions. Completion audit follows superseding attempts and requires all63 prepared case names plus all20 master owners and all80 original supplemental names.
- Co-op wrapper now preserves raw end-of-attempt device JSON via adb exec-out plus any changed fixed host JSON/log/text outputs before the next case overwrites them. Host outputs are explicitly weaker provenance if another concurrent producer writes those fixed paths; device-specific raw files, script IDs and owner verdict remain authoritative. No shared5554/5556/5558 actions or global ADB resets were performed.

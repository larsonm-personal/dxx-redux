# Suite reliability and runtime cleanup - 2026-09-12

## Objective and completion requirements

Repair the 12 failures in report_20260911_204903.md; remove development-only and duplicate coverage; consolidate setup and use deliberate sampling so the default unattended suite finishes in 60-90 minutes. Completion requires an actual full default run with no failures/timeouts, explicit accounting for each original failure, and measured runtime. Sampling must not conceal known failures; extended cases remain independently verified when retained

## Prior work consulted

- testing/plan_test_suite_cleanup_20260620.md: absorb unique assertions, remove obsolete debug probes
- testing/plan_test_suite_dedup_runtime_pass_20260806.md: shared setup, deliberate engine coverage, headless corpus plus graphics canaries, smoke versus soak, no state-order dependencies
- testing/plan_added_test_consolidation_20260811.md: data-driven subsystem suites, preserve distinct behavioral assertions

## Evidence and strategy

- Baseline: 265 passed, 12 failed, 7 skipped, 5:05:14; measured per-test time is 155m07s PowerShell and 142m35s JSON automation
- Many mission-specific physical tests repeat native configuration/build with no compiler work; build once and share isolated result generation
- Investigate missing result files despite successful native exits before interpreting those as engine failures
- Replace brittle incidental route-order expectations only after verifying required causal behavior and metadata agreement
- Review costly headed diagnostics and overlapping per-level checks against native integration and explicit Android canaries
- Introduce a transparent default coverage policy with mandatory subsystem canaries and reproducible rotating samples for large mission matrices; retain an explicit exhaustive profile
- Verify remaining UI failures serially with native/introspection evidence; preserve meaningful unique user workflows
- Run scoped quality and focused tests, then the full default suite; iterate until pass and runtime requirements are demonstrated

## Original failure ledger (pending investigation)

- input_demo_regressions_d2: native exit 0 reports writing actual result; wrapper says missing
- castaway_level7_trigger_dependencies: confirmed physical route rejected by exact objective sequence
- counterstrike_level1_route_confirmation_headed: terminal state not observed in 600s
- guidebot_long_path_simulations: inspect runner failure
- guidebot_player_defaults: runner infrastructure failure
- mission_route_corpus: reviewed baseline differs
- newmenu_render_paths_unified: D2 menu flow stalls
- obsidian_level1_objective_markers: bypass expectation differs
- obsidian_level12_gold_key_advance: frontier door flag differs
- random_level_preview: camera input not observed
- saturn_level15_simulation: inspect runner failure
- gog_installer_redbook_unified: launcher and game exited without result

## Worktree protection

Starting HEAD c2667c2d; existing android/benchmarks/level_metadata_analysis_history.json modification belongs to the user. Do not overwrite it

## First implementation tranche

- Replay exit/file polling race repaired; deterministic race check lives in the existing process-wait test; all 11 D2 replay baselines pass unchanged
- Player-default and Saturn scratch roots now use fresh generations; both passed twice. The old long-path probe also passed twice after the same repair, then was retired because it allowed every route to fail and overlaps stronger FirstStrike long-path completion plus repeated simulation/replay coverage
- Castaway L7 expected prerequisites now include trigger 41, present in both checked metadata and confirmed physical output; focused repeat-2 test passes
- Removed the 15,845-line shadow route hash fixture and its 194-line comparator. It never executed the engine and duplicated version-controlled metadata. Independently repeated both changed pharioka variants: both complete at 4918 frames
- Retired duplicate Counterstrike headed JSON (same level and controller covered by Android headed/headless parity) and Obsidian L12 debug-pose JSON (physical switch-approach completion and maintained Android guidance owners preserve useful behavior). Both old JSON tests passed on focused rerun before retirement
- D2 newmenu and the 204-step Obsidian objective-marker owner passed unchanged on focused rerun; suite-order validation remains required
- Preview test now removes the stale snapshot before requesting its post-input observation. Original failing seed 367489867 passes twice with this correction
- 83 physical route scenarios now have one explicit owner. Their distinct assertions remain available as support cases and direct focused commands. The owner builds D2 once, captures per-case logs/results, applies a bounded per-case watchdog, and continues after failures
- Default route selection keeps seven fixed canaries plus one additional case per mission family. A deterministic daily rotation covers all 15 families; -SampleSeed reproduces the run, -FullRouteCorpus runs every retained case, and existing -Filter patterns can target support cases
- Catalog validation covers ownership, deterministic selection, required canaries, family coverage, complete rotation, and exhaustive inclusion
- Removed duplicate D1/D2 demo replays from the funnel test; their full unchanged corpus remains mandatory

## GOG root cause and pending validation

The failing GOG run reproduced. Android ApplicationExitInfo records an uncaught InsufficientStorageException during publication: 478 MB required for a second DESCENT_II.gog copy, only 396 MB available. Changes pending build/device validation:

- Transfer same-volume staging files by rename, preserving transactional backup/rollback and a copy fallback
- Existing disc-content test verifies that published audio retains file identity and consumes the staged copy; existing rollback coverage remains
- Catch setup IO command exceptions, finish the ordered broadcast with an explicit failure, and propagate it through launcher automation instead of crashing the application
- Give the GOG test its own content set and remove it afterward, with cleanup failures counted as failures
- Capture Android crash-buffer output and ApplicationExitInfo in shared failure diagnostics; repair double-shell quoting that made tombstone diagnostics list the device root

Exhaustive physical case validation is running in temp/suite_cleanup_all_route_cases.log. Full default suite timing and final reliability verification remain outstanding

## Storage reliability discovered during exhaustive validation

The first grouped run passed 74 cases, then ran the host volume out of space (about 54 MB remained). Cases 75-77 failed from storage and the owner could not publish its next summary. The process is confirmed terminal, not a pending wait

Reclaimed approximately 32 GB by releasing only the successful cases' reproducible raw/stages payloads and copied engine binaries. Their logs, result JSON, engine hash manifests, and settings remain. Shared payload cleanup now performs this release after each passing route case; failed cases retain diagnostics. Existing workspace tests cover evidence preservation and rejection of source-directory cleanup. Exhaustive validation must be repeated with this lifecycle fix

A broader default scenario portfolio is still needed: routing consolidation alone does not account for the original 142 minutes of JSON automation. Candidate mandatory core (full JVM/CTest and headless replays, lifecycle/save-load/input/render, mission import, route parity/objective markers, external import and multiplayer interfaces) costs roughly 49 minutes using the old report, before routing canaries and rotating additional UI/host scenarios. Keep explicit extended access and avoid masking original failures; implement and measure before claiming the 60-90 minute target

## Default coverage policy

Implemented explicit test_suite_coverage.ps1 policy: fixed integration owners and inexpensive host guards, plus one daily rotating scenario in each of twelve additional families (packaging, host routes, launcher, gameplay, route guidance, input preferences, graphics, metadata lifecycle, content browser, audio preferences, network, and saved routes). Catalog rejects unclassified or stale entries and checks reproducibility, mandatory owners and complete rotation. Reports enumerate omissions and seed. -Filter bypasses portfolio selection; -FullSuite includes all retained unattended scenarios and physical route cases. Existing extraction, graphics and multiplayer extended switches remain independent

The full disc-content JVM tests and APK build passed. GOG happy path passes all 55 steps with rename publication. Cleanup initially failed with only empty .content/mods and entries directories remaining; an immediate later delete succeeds. ModManager directory getters can recreate directories outside the content lock while FileSetManager recursively deletes under that lock. Getters now share the lock; deletion logs bounded leftover paths on failure. This final race repair still needs build and device validation

Additional cleanup: retired three static checked-JSON probes (base_mission_route_status, castaway_level2_route, obsidian_level2_key_preference). Like the shadow route hash comparator, they did not execute changed code; two froze exact intermediate route choices. Actual native route tests, repeated physical Castaway completion, mission metadata integration and route regeneration auditing remain. The Anniversary wrapper now runs its distinct Android import scenario directly; its redundant Gradle subset is owned by the full JVM suite

The core retains lifecycle, save/load dispatch, controller comparison, render settings, input replay corpus, mission import, route parity/objective UI, disc/SAF import, and multiplayer. Longer alternate axis, autosave, menu rendering, network and saved-route scenarios rotate; they remain directly runnable and included in exhaustive mode. Historical estimate for the selected daily portfolio is approximately 80 minutes before route-owner/build/infra changes; actual timing is still required

Validation update: all 83 physical route cases passed with automatic payload release (android/temp/route_regression_cases/run_20260912_102512_081/summary.json). Catalog and existing process/workspace/sampling helpers pass. Mixed scoped quality passes

The first parity rerun exposed an incomplete generated Android dependency list: missing descent2.s22 caused a native fatal exit after intro, then a wait for a result that could never arrive. Added its maintained hash; the repeat is running. The newmenu fixture already declares this file, so do not attribute its original failure to this finding

Missing-installer probe now produces correlated launcher automation failure at step 2/3 while the app remains alive (PID 16219); crash buffer is empty. ModManager manifest updates acquire content then publication locks, matching deletion and avoiding reversed lock order

Final focused validation: rebuilt APK and ModManager/FileSet JVM tests pass; GOG import plus cleanup exits 0 and its temporary set is absent. Newmenu D2 passes all 83 steps immediately after GOG. Anniversary integration passes all 28 steps and releases its staged ISO. Headed/headless parity passes both 21-step repeats and semantic comparison after the missing sound dependency fix

Started the normal unattended portfolio with -SampleSeed 254, including its normal APK build and infrastructure provisioning. Log: temp/suite_cleanup_default_full.log. Do not mark the goal complete until this run finishes successfully and elapsed time is verified

Default run checkpoint: APK build passed, full JVM owner passed (1,033 tests reported, no failures/errors), sampled route owner passed 22/22 in 4:11, and D1, D1-in-D2 and D2 headless corpora plus all graphics canaries passed unchanged. Full run is still active in temp/suite_cleanup_default_full.log

Final review follow-up after the run: run-ktlint.ps1 limits its root to app/src/main/java even for explicit paths, so separately check/format the edited DiscContentImportTest.kt with the pinned ktlint CLI. Do not run formatters while the suite is active. Also classify test_lan_discovery with the explicit/manual entries rather than core; it is already excluded by the runner's manual list, so this does not change the actual default selection

During default parity validation, the first headed repeat was much slower (345 seconds) but passed all 21 steps. A short simpleperf capture (temp/suite_cleanup_parity_stall.perf and *_callgraph.txt) placed most native game-thread CPU in rendering and emulator glBufferData synchronization, not route planning. Its 1080p resolution and cockpit mode match the focused run. No timeout was raised or extended. security.perf_harden was restored to its original value 1 after profiling. The second repeat and remainder of the suite are still running; do not classify the slowdown as a permanent deadlock

Additional final review follow-up: Format-MissionZipBatchDuration in android/helpers/run_mission_zip_batch.ps1 casts fractional hours/minutes directly to [int], which rounds (54 seconds appeared as 1:54). Use Math.Floor before both casts and verify 29/30/54/59/60/3599/3600-second boundaries. This affects per-archive log formatting, not the overall suite stopwatch

The default run found one cleanup-related failure: the PowerShell 5.1 parser enumerated deleted-but-not-yet-staged files through git ls-files --cached. Filter the catalog to files present in the working tree. Focused rerun passes: 354 files parse under Windows PowerShell 5.1 and compatibility helpers/canonical spec parity pass (temp/suite_cleanup_powershell_compat_fixed.log). Preserve the original failed full-run record and record this recheck separately; do not rewrite it as an original pass. Include this file in the final scoped formatting pass

## Completed default run and final rechecks

The measured default run (seed 254) finished all 129 selected owners in 1:46:21: 127 passed, two failed, zero timeouts. Original report is temp/test_reports/report_20260912_105354.md and remains unchanged. This is about 65% less test time than the original 5:05:14, but it does not yet meet the preferred 60-90 minute range. The normal APK build took an additional 1:26. Do not substitute historical estimates for these measurements

All extraction tests passed, including the 55-step GOG import and cleanup and both seekable/pipe Mac SAF paths. Full JVM coverage (1,033 tests), native host coverage (50 tests), all input-demo corpora/canaries, the 204-step Obsidian marker scenario, and the 22-case default route owner passed. All 83 route cases also passed in the independent exhaustive validation

The two default-run failures were the deleted-but-unstaged PowerShell parser inputs (fixed; focused check passed) and LAN address discovery. On the emulator, Wi-Fi status reports a connected address while shell ip address output is empty and ip link reports permission denied. Shared Get-DeviceWlanIp now falls back to cmd wifi status; LAN, host migration, lobby discovery and broadcast use this one lookup. Broadcast passed in the full run with the fix. A serial test_lan* recheck is running in temp/suite_cleanup_lan_recheck.log

Final scoped PowerShell quality and separate pinned ktlint formatting of DiscContentImportTest.kt passed. Duration display boundary checks pass at 29/30/54/59/60/3599/3600 seconds. test_lan_discovery is now correctly classified as explicit/manual (actual default selection unchanged). The user's benchmark history retains its original 727 added lines

Final network recheck passed all three tests: direct LAN gameplay (both peers report two players), UDP broadcast, and lobby discovery. Report: temp/test_reports/report_20260912_124403.md; test time 2:29, zero failures/timeouts. The corrected PowerShell 5.1 parser, process/workspace helpers and automation catalog also pass after final formatting (temp/suite_cleanup_powershell_compat_final.log, suite_cleanup_helpers_final.log, suite_cleanup_catalog_final.log). Both failures in the preserved full-run report therefore have passing focused rechecks

Validation is complete for the selected normal portfolio, all 83 physical route cases, and the original failures retained or consolidated into maintained owners. The 60 rotating scenarios omitted by seed 254 were not all rerun in this default pass; reports distinguish them from passes. No full all-green 129-owner rerun was claimed after the two small harness fixes. Runtime remains 1:46:21 measured, not a demonstrated 60-90 minutes. Further runtime work should target repeated headed-render setup and cold import/metadata preparation without weakening completion or error assertions

## Follow-up: prevent extended coverage from becoming dormant

User requested normal-run sampling for otherwise archived coverage. Audit: 190 policy entries comprise 112 fixed owners, 70 rotating scenarios (12 selected), one explicitly gated graphics probe, and seven manual/utility entries. Physical routing already samples 22/83 with complete rotation. The 60 scenario omissions in the measured run are rotating coverage, not archives

Close the explicit-only variant gap by sampling full graphics replay coverage plus the two-pass probe on 5% of normal daily seeds, and the 90-second multiplayer soak on a separate 5%. Keep explicit overrides. Extraction sampling now follows the suite seed rather than a fixed commit hash. Daily seeds remain reproducible; repeated same-day runs select the same coverage. Extend the existing catalog test with cadence and reproducibility checks and run scoped quality plus catalog validation

Follow-up validation passed: scoped quality, catalog sampling/cadence checks, and Windows PowerShell 5.1 compatibility. Counts above refer to 190 policy entries; runtime reports expand some entries by game/variant, so their denominators differ. No full device-suite rerun was needed for this selection-only change

## Follow-up: keep only broad integration owners and fast checks fixed

Use the measured 20260912_105354 report to move specialized slow checks out of core. Keep four broad emulator owners fixed: D1/D2 launch-to-automap, SDK lifecycle, save/load dispatch, and matchmaking multiplayer. Keep full headless demos and graphics canaries, JVM/native unit owners, and the already-sampled physical route owner. Other fixed checks should be roughly 30 seconds or less in the measured run

Move slow specialist coverage into existing families wherever possible; add one storage/import family so normal runs exercise one external storage path instead of all of them. Keep complete rotation, rare extended coverage, direct filters and exhaustive mode. Validate catalog coverage and required integration anchors; estimate savings from the prior report without claiming a newly measured suite time

Implemented: 19 slow specialized owners moved from fixed coverage to existing scenario families or a seven-case storage/import family. The fixed count is now 93: eight broad owners plus 85 checks measured at <=30 seconds each. Their expanded test executions sum to 25:55 in the previous report, before build/provisioning and sampled scenarios. There are now 89 rotating policy entries in 13 families; the largest family has 16 cases. No tests were removed, and all retained cases remain reachable in normal rotation. Catalog checks explicitly preserve the eight integration anchors in addition to verifying full rotation

Smaller-core validation passed: scoped formatting/lint, automation catalog (required anchors, complete rotation, exhaustive coverage), and PowerShell 5.1 compatibility. Evidence: temp/suite_smaller_core_quality.log, temp/suite_smaller_core_catalog.log, temp/suite_smaller_core_compat.log. Selection changed only; no new full emulator run or end-to-end runtime claim

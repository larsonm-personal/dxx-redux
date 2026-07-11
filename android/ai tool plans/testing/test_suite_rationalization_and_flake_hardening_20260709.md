# Test suite rationalization and flake hardening

Date: 2026-07-09

## Goal

Reduce duplicate coverage, improve failure signal, and remove structural sources of nondeterminism from the automated test suite. The target is a suite whose orchestration, isolation, readiness checks, and diagnostics make failures reproducible and actionable.

## Constraints

- Do not treat longer timeouts as a reliability fix
- Do not alternate persistent menu ordering or other shared state between tests
- Prefer state-based synchronization over sleeps and timing assumptions
- Prefer explicit test isolation over cleanup that depends on the previous test's outcome
- Preserve meaningful integration coverage while consolidating redundant paths
- Keep D1 and D2 coverage deliberate rather than mechanically duplicating every scenario

## Evidence to inspect

- Latest full report: `temp/test_reports/report_20260710_132022.md`
- Prior full reports: `temp/test_reports/report_20260708_232908.md` and `temp/test_reports/report_20260709_225804.md`
- Earlier suite cleanup, runtime, hardening, and report follow-up plans under `android/ai tool plans/testing/`
- Test manifest and orchestration scripts
- Shared setup, cleanup, fixture, automation, and result-reporting helpers
- Failure artifacts for the eight failures in the latest run

## Work plan

1. [complete] Reconstruct prior work and identify repeated fixes, regressions, and deferred structural changes
2. [complete] Classify the latest failures by shared root cause and map them to runner/test architecture
3. [complete] Audit suite duplication, state ownership, synchronization, isolation, and diagnostics directly from code
4. [complete] Design a transformational target architecture with staged, independently verifiable tranches
5. [complete] Implement the highest-leverage safe tranche that fits this pass
6. [complete] Run focused validation, then update this plan with results and remaining work

## Status

Three implementation tranches are complete. The two failures in `report_20260710_132022.md` were both resolved as product/architecture defects: lossy Android continuous-input delivery and an over-broad Guide-Bot exit-selection change. No timeout was extended and no demo was re-recorded.

## Second full-report tranche

Latest full run: 74 passed, 6 failed, 0 timed out, 6 skipped, 0 not run in 1:07:43.

1. [complete] Reconcile all six failures with their complete logs and durable artifacts
2. [complete] Collapse duplicate input-demo reporting so one simulation divergence is one primary failure with backend corroboration
3. [complete] Repair merged-wall diagnostics at the producer/contract boundary rather than weakening assertions
4. [complete] Replace incidental guidebot route-size assertions with a semantic route contract, while preserving real route defects as failures
5. [complete] Replace whole-file secret baseline failure output with a normalized structural diff and classify the current change
6. [complete] Run focused regression tests, builds, code quality, and a representative profile

### Second-tranche outcome

- The normal full profile now runs the complete headless D1, D2, and D1-in-D2 demo corpus as the primary simulation contract, plus one hash-pinned graphics canary per engine path. The complete graphics matrix moved to `-ExtendedGraphics`.
- Graphics replay compares against the archived headless actual result, so one simulation divergence is one primary failure with backend corroboration rather than two nominal failures.
- Host replay builds use a freshness stamp that distinguishes relevant host sources from newer Android-only files.
- Merged-wall probes now assert the producer's actual diagnostic contract, and the duplicate debug-mode top-level scenario was folded into the remaining extended probe.
- Guide-Bot route scenarios assert semantic route status, objective, activation, and reachable guidance instead of incidental selected-index values.
- Secret-area baselines use normalized structural differences and the accepted additive schema was regenerated.
- The resulting full report was `report_20260710_132022.md`: 77 passed, 2 failed, 0 timed out, 7 skipped, 0 not run in 1:13:49.

## Third full-report tranche

The July 10 report predates this remediation. Its two failures were independently reproducible and were not timing-budget problems.

1. [complete] Replace lossy SDL-queued Android axes with a mutex-protected latest-state mailbox drained on the game thread
2. [complete] Preserve discrete threshold-region transitions for axis-bound buttons so a press and release during one stalled frame cannot be coalesced away
3. [complete] Correlate assertions to the exact mailbox generation consumed by `kconfig`, including a production-publisher probe with all 127 usable SDL queue slots deliberately occupied
4. [complete] Merge analog routing, multi-axis hold/release, D-pad, and trigger coverage into one 69-step scenario; move static binding ownership to Kotlin units; delete `test_dpad_triggers`
5. [complete] Remove the obsolete D-pad timeout override and the ineffective axis timeout exception
6. [complete] Diagnose the D2 level 9 demo and restore classic Guide-Bot exit semantics without changing the recording
7. [complete] Validate D1, D2, the trigger-only KCXF2 path, native host suites, Android builds/units, catalog, and code quality

### Android input root cause and design

- The default Advanced layout can publish about 100 axis samples per second from gyro alone. SDL 1.2 exposes 127 usable event slots, so 1.1 to 2.0 second full-suite frame stalls filled the queue.
- `nativeJoystickAxis` ignored `SDL_PushEvent` failure. The reported D2 heading assertion sampled zero because the automation axis event had been silently dropped. An isolated retry passed only because the queue was empty.
- Production JNI, touch, controller, gyro, and automation now publish through one 11-axis mailbox. Continuous values coalesce to the latest complete desired state; the game thread drains them before the discrete SDL queue.
- Axis-button threshold regions are retained as an ordered transition stream. This preserves short trigger/directional pulses while still coalescing high-frequency samples inside the same region.
- `event_flush` does not discard continuous state. This prevents a one-shot zero release from racing with a menu/input flush and leaving an axis stuck.
- Automation owns a complete vector during source-correlated assertions, while production updates continue accumulating and are restored on release. Trigger edges are probe-relative, not process-lifetime counters.
- Kotlin suppresses unchanged mixed-axis JNI calls and releases all active mixed input when the activity stops.
- The merged scenario deterministically fills all 127 SDL slots, then proves the production mailbox path still reaches `kconfig`. It also publishes a trigger press and release before one drain and observes exactly one down and one up edge.

### Guide-Bot demo root cause and fix

- The failing fixture `d2_descent2_level9_20260512_115624.dximdemo` was stable, not flaky. RNG state matched through call 45726; the first divergence was Guide-Bot path creation at call 45727.
- Commit `a2d7f70b` changed `find_exit_segment` from legacy `child == -2` selection to one combined `child == -2 || TT_EXIT` scan. That made the chosen destination depend on segment numbering.
- A survey of all 30 stock Counterstrike levels found the combined loop chose an external endpoint first in 12 levels, a trigger surface first in 14, and an external-only endpoint in four. Firewalker changed from external segment 252 to trigger segment 38.
- `find_exit_segment` now uses two explicit passes: preserve the classic external endpoint when present, then fall back to `TT_EXIT` for trigger-only community levels such as KCXF2. A host policy unit owns external precedence, trigger-only fallback, external-only behavior, and no-exit behavior.
- The original level 9 fixture now passes with its exact recorded final state. KCXF2 still passes all 47 semantic route steps. No replay special case, expectation change, or fixture re-record was needed.

### Third-tranche validation

- `test_axis_mapping.json5`: D1 and D2 both passed 69/69; queue saturation reported 127 slots; final mailbox state had no pending axes; the pre-drain trigger pulse reported one correlated down and one correlated up edge.
- Targeted D2 demo replay passed 1/1 with the original shields 200, score 300950, lives 9, and segment 37 result.
- `test_kcxf2_guidebot_route_next.json5` passed 47/47 after the external-first fallback change.
- Native host CTest passed D1 14/14 and D2 16/16, including mailbox lifecycle/transition and Guide-Bot exit-policy tests.
- JDK 21 `:app:testDebugUnitTest :app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64.
- Scoped code quality and `git diff --check` passed. Catalog validation reports 45 standalone JSON tests, 15 support scripts, and 36 PowerShell entries.

### Batched producer follow-up

- [complete] Controller motion now mixes its six physical axes and all configured half-axis combiners as one producer vector.
- `InputMixer.setAxes` applies every source update before calculating changed mixed outputs, then invokes one batch callback. Unchanged axes are omitted because their desired mixed state is already current.
- `nativeJoystickAxes` validates parallel axis/value/touch arrays and publishes the changed vector through one mailbox lock acquisition and one shared generation. The game thread can no longer observe a partially published controller sample.
- Multi-axis source clearing and activity-stop release also use the batch callback. Individual touch and gyro updates retain the single-axis path.
- Kotlin unit coverage verifies additive touch/controller mixing, deterministic axis order, unchanged-vector suppression, and atomic source clearing. The native mailbox unit verifies three axes receive one generation.
- JDK 21 unit/build validation passed for all Android ABIs; native CTest remained D1 14/14 and D2 16/16; the installed D2 routing scenario passed 69/69 with the 127-slot saturation proof intact.

## Prior work assessment

The earlier work added useful infrastructure, but it stopped between centralization and isolation:

- `plan_test_suite_runtime_survey_20260620.md` proposed collapsing autosave coverage, reducing the duplicated D1/D2 axis matrix, creating one input-demo matrix runner, separating multiplayer smoke from soak, reusing the suite APK, and centralizing launcher/game setup
- `plan_test_suite_cleanup_20260620.md` completed the autosave collapse and removed several debug-era scripts, but the remaining structural recommendations were not implemented
- `test_suite_speed_fixed_wait_audit_20260612.md` added useful introspection-backed waits, but hundreds of per-step delays and fire-and-forget setup commands remain
- `auto-infra-test-suite.md` made the suite self-provisioning, but all single-emulator tests still share one mutable app-data sandbox
- Report-by-report follow-ups repeatedly added timeout floors, optional menu branches, recovery retries, and special ordering without establishing a per-case execution contract

The quick-record sidecar pair is the clearest example. The install script explicitly depends on the stage script, the full runner contains a name-based ordering exception, and the quick runner can budget-skip the producer while still running the consumer. This occurred in `temp/test_reports/quick_report_20260709_100415.md`.

## Current architecture findings

- There is no authoritative test catalog. Full, quick, and interactive runners each discover or enumerate tests differently
- `run_all_tests.ps1` infers policy from names, including infrastructure, timeout overrides, support ownership, and one ordering exception
- One JSON file can run D1 and D2 internally but still produces one aggregate report row, so cases cannot be independently isolated or reported
- The current tree has 64 JSON scripts, 49 standalone JSON entries, 35 PowerShell test entry points, 83 Kotlin test files, and 15 local input-demo fixtures
- Forty standalone launcher scripts call `reset_state`, 38 separately call `clear_save_files`, and the JSON scripts contain hundreds of explicit post-delay fields
- PowerShell and Kotlin reset different subsets of state. Neither establishes a complete canonical baseline, and shared preferences survive most runs
- Setup commands are fire-and-forget broadcasts. Scripts guess completion with `post_delay_ms`
- Automation results use one shared `automation_result.json` and `automation_log.jsonl`, without a case/attempt identity or atomic result publication
- Generic ADB helpers do not require a leased serial, so a leftover second emulator can make a nominally single-emulator run ambiguous
- Missing dependencies can exit zero from `run_test.ps1` and be reported as PASS instead of SKIP
- `test_autoselect_plx.ps1` skips its core D1/D2 assertions when prior tests did not create pilot files, then exits successfully
- `test_saf_basic.json5` is already run by `test_saf_archiver.ps1` but is also discovered as an independent top-level test without the SAF fixture
- The full input-demo matrix runs every local fixture in both headless and graphics modes. The latest report counts one identical D2 divergence twice

## Latest report classification

The eight failures in `report_20260708_232908.md` are not eight independent flakes. They collapse into about five deterministic clusters:

1. Gradle unit drift: two `GyroToggleConfigTest` assertions expected the previous layout version
2. One D2 input-demo simulation divergence, reported once for headless and once for graphics with the same final-state mismatch
3. Two stale merged-wall diagnostic contracts after the underlying diagnostic/route implementation changed
4. Two guidebot route tests that deterministically remained at route index 2 and exposed a real live-key/trigger pathing defect; their exact selected-index assertions are also too incidental
5. Secret-area baseline schema growth, where the whole-file equality failure did not summarize the additive fields

This report is evidence that top-level failure count currently exaggerates correlated variants and mixes product regressions, stale diagnostics, and baseline drift. Timeout changes would not address any of these clusters.

## Target architecture

### Authoritative catalog and case expansion

- One catalog owns adapter, kind, games, parameter cases, profile membership, resources, fixture, mutations, coverage claim, and expected artifacts
- Full, quick, and interactive runners become catalog views instead of separate inventories
- Expand `(test, game, parameter set)` before execution so every case has its own status and artifacts
- Support scripts and diagnostics are not counted as skipped product tests
- A top-level test cannot depend on another top-level test's output. Persistent multi-stage workflows are one scenario

### Test session protocol

- Give every suite, case, and attempt a unique identity
- Publish results atomically and require the watcher to match the case identity, script hash, build, fixture, and emulator serial
- Put logs and artifacts under a run-scoped directory instead of overwriting shared filenames
- Represent PASS, ASSERTION_FAIL, INFRA_ERROR, SKIP, and FLAKY_RETRY separately
- A retry may classify an infrastructure failure, but a retry-pass remains visible and never converts an assertion failure to PASS

### Canonical isolation and readiness

- Add a debug-only `prepare_test(fixture)` transaction that runs after force-stop and returns an acknowledgement and state fingerprint
- Preserve immutable content-addressed game assets while clearing mutable preferences, configs, pilots, saves, mods, manifests, automation residue, and test-created files
- Remove reset and common launch preambles from individual scenarios after they migrate
- Replace setup-command delays, logcat launch strings, and timeout extension with explicit state generations, command acknowledgements, phase progress, stall deadlines, and one hard safety cap
- Gameplay-focused tests should start from named deterministic fixtures. Only launcher-contract tests should traverse pilot/menu/mission UI

### Coverage policy

- Run the complete headless input-demo corpus as the primary simulation contract
- Run a stratified graphics/backend canary set in the normal full profile and the complete graphics matrix in an extended profile
- Run shared Android behavior fully on one game with explicit cross-game smoke cases unless engine-specific behavior requires both
- Separate multiplayer connection/start smoke from sustained soak
- Parse JUnit and other structured child results so aggregate adapters do not hide individual failures

## First implementation tranche

This pass established several invariants without attempting the full catalog migration:

1. [complete] Merge the quick-record stage/install pair into one self-contained scenario and delete the ordering exception
2. [complete] Mark support-owned scripts explicitly, stop running `test_saf_basic` twice, and validate that every non-standalone script has a real owner
3. [complete] Give common-runner automation invocations a unique staged script identity, publish result files atomically, and reject stale/mismatched results
4. [complete] Clear shared test preferences synchronously as part of the common reset path
5. [complete] Fix false PASS paths for unavailable dependencies and missing autoselect fixtures
6. [complete] Run focused host, Gradle/native build, launcher, game, sidecar, and quick-profile validation

## Implemented changes

### Inventory and ownership

- Replaced the quick-record producer/consumer pair with one 37-step record, verify, install, and re-verify scenario. The name-based ordering exception is gone
- Deleted the unused `test_lan_mp.json5` support script
- Added `_owner` metadata to all 15 support scripts and made `test_saf_basic.json5` support-owned by `test_saf_archiver`
- Added a catalog validator that rejects malformed metadata, duplicate top-level names, missing owners, standalone scripts with owners, missing owner entry points, and owner paths that do not reach their support scripts
- Full and interactive discovery now exclude support scripts rather than reporting them as skipped product tests
- The resulting JSON inventory is 47 standalone scenarios and 15 support scripts

### Invocation, isolation, and result protocol

- `run_test.ps1` now stages a uniquely named script for every game/case invocation and supplies a GUID run ID to the launcher or game process
- Launcher and native results are written to a temporary file and atomically renamed, include the run ID, and report a bounded completed-step count
- The watcher accepts terminal results only when their run ID matches. A stale result no longer suppresses health, background, or resume processing
- Native script path, resume step, and run ID are one mutex-protected request that is latched on the game thread. A later request cannot retag an active script
- Launcher recovery persists the independently expected run ID and rejects a `LAUNCHER_CONTINUE` file that cannot prove it belongs to the active run
- Common reset now clears both launcher/game preference files synchronously in addition to config, controller, saves, and staged-demo state
- Full-runner cleanup now stops only emulators that the current suite invocation started or restarted

### Truthful status and profile behavior

- Missing declared game-data dependencies now emit `RESULT: SKIP` and exit 2 instead of exiting 0 as a false PASS
- Exit code 2 is a SKIP only when the child also emits the machine-readable marker. This avoids misclassifying tests that return a failure count of two
- Both full and quick reports preserve runtime skip reasons and explicitly report selected tests that were left `NOT_RUN` after early stop or recovery failure
- `test_autoselect_plx.ps1` is now a non-gating `probe_autoselect_plx.ps1`; missing pre-existing pilots can no longer produce a green coverage row
- The quick profile is ordered by coverage priority, uses build-freshness-aware replay estimates, and no longer includes the observed 74-second D-pad test. D-pad coverage remains in the full profile
- A warm quick profile now includes the self-contained sidecar scenario and completed all 11 selected cases in 2:25

## Validation results

- Scoped code-quality pass completed for all changed C, C++, Kotlin, PowerShell, JSON5, and plan files
- PowerShell AST parsing passed for all changed scripts
- `test_test_helpers_process_wait.ps1` passed run-ID and explicit status-mapping checks
- `test_validate_automation_catalog.ps1` passed: 47 standalone JSON scenarios, 15 support scripts, and 35 PowerShell entries
- Filtered `run_all_tests.ps1` host runs passed and no longer stopped a pre-existing emulator
- JDK 21 `:app:testDebugUnitTest :app:assembleDebug` passed; native debug builds completed for arm64-v8a, armeabi-v7a, and x86_64
- Focused merged sidecar run passed with a matching unique script/result ID, all three installed sidecars, and no temporary result file
- Focused native completion passed with `26/26` steps; focused launcher completion passed with `19/19` steps and cleared the persisted active-run identity
- Final quick report: `temp/test_reports/quick_report_20260709_222122.md`, 11 passed, 0 failed, 0 timed out, 0 skipped, 0 not run, total 2:25

The 58-minute full suite was not repeated in this tranche. The latest report's input-demo divergence, merged-wall contract drift, guidebot live-route defect, and secret-area baseline growth remain product/contract work; this pass intentionally did not hide those signals with retries, optional assertions, or larger timeouts.

## Deferred tranches

- Introduce a full declarative catalog and make full, quick, and menu selection pure catalog views. This tranche enforces ownership metadata but does not yet replace the runners' separate inventories
- Migrate extract, mission ZIP, LAN, SAF, and legacy `Send-AutomationScript` callers onto the correlated invocation helper. They benefit from atomic publication but still use the legacy shared filename without a required run ID
- Move results and logs to run-scoped paths and include script/build/fixture hashes in the completion contract
- Implement canonical fixture preparation and migrate a small pilot set through forward, reverse, and seeded-random repetition gates
- Require explicit emulator serial leases and add cross-process resource locks and guaranteed cleanup
- Split game/parameter cases and ingest JUnit results
- Rationalize input-demo, axis, multiplayer soak, graphics probe, guidebot, and repeated launcher/game bootstrap coverage

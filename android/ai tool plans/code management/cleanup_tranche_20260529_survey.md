# Plan: Cleanup Tranche 2026-05-29 Survey

## Goal
- Refresh the cleanup backlog with current repo signals and pick the safest first tranche

## Scope
- Android-owned code, branch-added shared helpers, test scripts, and branch-added D1/D2 hooks
- Out of scope: behavior changes, broad upstream formatting, and fixing unrelated inherited warnings

## Cheap Check
- Non-mutating status, stale formatter check, focused searches, and current cleanup/diff reports

## Steps
- [x] Review reusable cleanup playbook
- [x] Baseline current diff and formatter state
- [x] Survey cleanup candidates from lint, tests, duplication, diagnostics, and open TODO markers
- [x] Prioritize the smallest safe tranche and record follow-up validation

## Survey Notes
- Created from `android/ai tool plans/reusable/cleanup.md`
- Current local changes before this plan: `android/ai tool plans/testing/plan_report_20260528_201452_failures.md`, `android/game_scripts/test_door45_cover_gpu_regression.json5`, `android/game_scripts/test_door45_pose_repro.json5`, and `android/run_all_tests.ps1`
- Formatter hygiene: `android/helpers/stop-stale-formatters.ps1` reported no stale formatter tasks, and `android/temp/run-code-quality.lock.json` was absent
- Reusable playbook mismatch: `diff_vs_upstream.ps1` and `stop-stale-formatters.ps1` now live under `android/helpers/`, while the playbook still shows older top-level command paths in several examples
- Current D1/D2 diff report from `android/helpers/diff_vs_upstream.ps1 -Top 20`: 314 files, D1 146, D2 168, +35685/-3085 vs `upstream/main`
- Largest D1/D2 churn items: `d2/main/input_demo_hooks.c` 5361, `d2/arch/ogl/ogl.c` 2209, `d1/arch/ogl/ogl.c` 2051, `d2/main/state.c` 1644, `d1/main/state.c` 1333, `d2/main/net_udp.c` 1241, `d1/main/net_udp.c` 1117
- `android/run-code-quality.ps1` check mode passed: clang-format, ktlint, PSScriptAnalyzer, UTF-8 BOM lint, shellcheck, shfmt, cmake-format, and cmake-lint
- `OGL_MERGE` is not dead scaffolding. It is defined by both `d1/CMakeLists.txt` and `d2/CMakeLists.txt`, so OGL cleanup should be diff-shrink/shared-helper work, not conditional deletion
- `ai_probe_*` fields are part of `android/app/src/main/cpp/shared/input_demo_state_trace.h`, serialized in `input_demo_state_trace.cpp`, filtered by `android/tests/compare_input_demo_state_trace.ps1`, and present in existing `.dximdemo` data. Treat as schema cleanup only after an explicit input-demo format decision

## Candidate Tranches

1. Refresh reusable cleanup playbook helper paths
	- Evidence: the playbook examples still use `android/diff_vs_upstream.ps1` and `android/stop-stale-formatters.ps1`, but the live files are under `android/helpers/`
	- Scope: `android/ai tool plans/reusable/cleanup.md`
	- Validation: link/path check plus the non-mutating helper commands from repo root
	- Risk: very low documentation cleanup; no build required

2. Archive or delete stale tracked probe/manual artifacts
	- Completed: removed tracked investigation scripts under `android/game_scripts/probe_merged_wall*.json5`, and removed tracked `android/tests/test_dual_emu.ps1.new`, a superseded copy next to `test_dual_emu.ps1`
	- Scope: five `probe_merged_wall_level1_*.json5` scripts and `android/tests/test_dual_emu.ps1.new`
	- Validation passed: no probe scripts or `.ps1.new` leftovers remain, and the normal `test_*.json5` discovery check reports `probe-discovered-test-count=0`
	- Risk note: historical plan notes still mention the old probe filenames as investigation records; active runner/catalog code does not

3. D1/D2 fireball explosion probe helper consolidation
	- Completed: moved the common exploding-object debug probe formatting and D2 structured replay probe append into `android/app/src/main/cpp/shared/input_demo_debug_logging.cpp`
	- Scope done: `d1/main/fireball.c` and `d2/main/fireball.c` now call `input_demo_debug_log_exploding_object_probe()` at the existing explosion lifecycle points; D2-only debris, secondary explosion, powerup, and blast-damage probes stayed local
	- Behavior note: D1 exploding-object probe output now follows the shared `-inputdemo-debug-log` activity gate, matching D2 and the rest of the shared debug logging API
	- Validation passed: diagnostics clean on touched code, scoped `android/run-code-quality.ps1 -Fix -Paths @(...)` passed, `run-windows-build.ps1 -Target both` passed before and after formatting, Android arm64 debug CMake Gradle tasks passed, direct `ctest --test-dir buildd1/buildd2` found no registered tests, and `android/tests/test_input_demo_runtime_smoke.ps1 -Game both -TimeoutSeconds 60` passed
	- Risk note: D2 smoke still prints existing RNG mismatch diagnostics for the tiny generated fixture before reporting PASS; this is pre-existing smoke-test behavior, not a failure

4. Test-suite blind wait reduction in older PowerShell tests
	- Evidence: many direct `Start-Sleep` calls remain in `android/tests/test_saf_archiver.ps1`, `test_extract.ps1`, `test_lan_broadcast.ps1`, `test_lan_lobby_discovery.ps1`, and dual-emulator helpers
	- Completed: updated `android/tests/test_lan_broadcast.ps1` so the Python UDP receiver prints `READY` after binding, and the PowerShell harness polls receiver output for `READY` and the expected packet result instead of sleeping fixed 2s/3s intervals
	- Completed: added `android/tests/udp_test_tool_android.csrc`, a tiny native UDP helper that is compiled as C with the pinned Android NDK and pushed to both emulators when the image reports `NOPYTHON`
	- Scope done: removed the six blind listener/send waits from the three UDP packet checks; one polling delay remains inside `Wait-FilePattern()`
	- Validation passed: diagnostics clean, scoped `android/run-code-quality.ps1 -Fix -Paths @('android/tests/test_lan_broadcast.ps1')` passed, direct pinned clang-format ran on `android/tests/udp_test_tool_android.csrc` with `--assume-filename=udp_test_tool.c`, `git diff --check` passed, and `android/run_all_tests.ps1 -Filter test_lan_broadcast -StopOnFail -ReportDir temp/test_reports_cleanup_lan_broadcast -TestTimeoutSeconds 120` passed
	- Validation note: the focused test log confirmed `NOPYTHON`, native fallback build/push, and passing UDP unicast, global broadcast, and subnet broadcast checks
	- Remaining scope: choose another script with a durable readiness signal available, not a broad sleep purge
	- Risk: medium; do not replace sleeps that intentionally allow emulator, app, or network state to settle without a better signal

5. Input-demo wrapper/bootstrap simplification
	- Evidence: `run_input_demo_regressions.ps1`, `run_input_demo_replay.ps1`, `run_input_demo_headless.ps1`, and `input_demo_host_build_guard.ps1` form a layered wrapper stack
	- Completed: mapped the active wrapper stack under `android/tests/` and kept the main replay runner untouched
	- Completed: parameterized `android/tests/test_input_demo_regressions.ps1` with `RunMode`, then changed `android/tests/test_input_demo_regressions_graphics.ps1` to delegate to it with `RunMode = graphics`
	- Validation passed: diagnostics clean, scoped `android/run-code-quality.ps1 -Fix -Paths @('android/tests/test_input_demo_regressions.ps1','android/tests/test_input_demo_regressions_graphics.ps1')` passed, `git diff --check -- android/tests/test_input_demo_regressions.ps1 android/tests/test_input_demo_regressions_graphics.ps1` passed, and both wrappers passed against `temp/quick_demo_subset_20260530_130805`
	- Risk: medium-low; replay semantics and host build guard behavior unchanged

6. D1/D2 high-churn shared-helper extraction survey
	- Evidence: diff report still shows paired large deltas in OGL, state, and UDP networking
	- Completed survey only: refreshed `android/helpers/diff_vs_upstream.ps1 -Top 20` into `temp/cleanup_candidate6_diff_vs_upstream_20260530.txt`; paired OGL and UDP files remain near the top of D1/D2 churn
	- Best future extraction candidate: finish moving duplicated Android MSAA FBO lifecycle helpers from `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` into existing `android/app/src/main/cpp/shared/ogl_msaa_android.c` and `ogl_msaa_android.h`
	- Evidence: both games still have near-identical local `ogl_msaa_destroy_fbo()` and `ogl_msaa_create_fbo()` bodies, while shared `android_ogl_msaa_destroy_fbo()` and `android_ogl_msaa_create_fbo()` already exist and are already wired into Android CMake
	- Secondary candidate: a small Android UDP bind-address helper around the duplicated `udp_bind_loopback ? "127.0.0.1" : NULL` block in `udp_open_socket()`, but payoff is smaller because most Android UDP helpers are already centralized
	- Recommended validation for a future implementation: before/after `android/helpers/diff_vs_upstream.ps1 -Top 20`, scoped code quality for touched OGL/shared files, Windows host build for both games, Android native build, and D1/D2 graphics smoke with MSAA enabled
	- Risk: medium-high for MSAA render correctness; do not edit this without an MSAA-specific runtime smoke path

7. Run-quick-tests stale historical estimate cleanup
	- Evidence: `android/run_quick_tests.ps1` still names `report_20260518_223317.md` as a hardcoded historical estimate source
	- Completed: updated `android/run_quick_tests.ps1` to describe the estimates as fixed per-test baselines, with one `$historicalEstimateSource` used by console and markdown report output
	- Validation passed: diagnostics clean, scoped `android/run-code-quality.ps1 -Fix -Paths @('android/run_quick_tests.ps1')` passed, `git diff --check -- android/run_quick_tests.ps1` passed, and `android/run_quick_tests.ps1 -MaxTotalSeconds 1 -ReportDir temp/quick_tests_estimate_dry_run` printed `Historical estimate: 02:28 from fixed per-test baselines` before the expected budget exit
	- Risk: low; behavior unchanged except user-visible estimate provenance

## First Recommended Slice
- Completed candidate 1: updated `android/ai tool plans/reusable/cleanup.md` so the documented `diff_vs_upstream.ps1` and `stop-stale-formatters.ps1` commands use the current `android/helpers/` paths
- Validation passed: both helper files exist, `android/helpers/stop-stale-formatters.ps1` reported no stale formatter tasks, and `android/helpers/diff_vs_upstream.ps1 -Top 5` ran from the repo root and produced the expected D1/D2 diff summary
- Completed candidate 2: removed stale `android/game_scripts/probe_merged_wall_level1_*.json5` forensic scripts and the superseded `android/tests/test_dual_emu.ps1.new` copy; validation confirmed no probe scripts or `.ps1.new` leftovers remain
- Completed candidate 3: consolidated D1/D2 exploding-object probe logging into the shared input-demo debug logger and kept D2-only probe paths local
- Validation passed for candidate 3: `git diff --check`, scoped code quality with `-Fix`, both Windows host builds, Android arm64 debug native build, empty CTest runs for the host build dirs, and the D1/D2 input-demo runtime smoke test
- Completed candidate 4: replaced the fixed listener/send sleeps in `android/tests/test_lan_broadcast.ps1` with receiver-output polling and added the native no-Python UDP fallback in `android/tests/udp_test_tool_android.csrc`; focused suite validation passed through the fallback path on the emulator image
- Completed candidate 7: removed the stale quick-test report provenance and validated the fixed-baseline estimate output with a no-infra budget check
- Completed candidate 5: consolidated the unattended input-demo regression wrappers without touching replay semantics; both headless and graphics one-demo validations passed
- Completed candidate 6 survey: recommended MSAA FBO helper extraction as the best future D1/D2 shared-helper cleanup and left source untouched for now
- Next likely slice: another focused test wait cleanup with a runnable readiness signal, or a dedicated MSAA helper extraction tranche with graphics smoke coverage

## Survey Artifacts
- `temp/cleanup_survey_diff_vs_upstream_20260529.txt`
- `temp/cleanup_survey_code_quality_20260529.txt`
- `temp/cleanup_candidate6_diff_vs_upstream_20260530.txt`

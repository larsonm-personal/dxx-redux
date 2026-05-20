# Plan: run quick tests helper -- 2026-05-19

## Goal
- Add `android/run_quick_tests.ps1` next to `android/run_all_tests.ps1`
- Pick a report-backed quick suite that should fit within a 3 minute wall-clock budget
- Run the quick suite and fix any problems it exposes

## Inputs
- Historical timings from `temp/test_reports/report_20260518_223317.md`
- Existing runner behavior from `android/run_all_tests.ps1`
- Existing single-test entry points from `android/run_test.ps1` and `android/tests/test_*.ps1`

## Hypothesis
- A curated set of fast host and single-emulator smoke tests can stay within a 180 second wall-clock budget while still covering one committed demo replay in both headless and graphics modes, as long as the suite avoids extract, server, and two-emulator flows and keeps launcher prerequisites self-contained

## Steps
- [x] Read the current full runner and the latest report, then choose a minimal quick suite and record its estimated total
- [x] Implement `android/run_quick_tests.ps1` as a thin sequential runner with a small report and the same subprocess isolation pattern as `run_all_tests.ps1`
- [x] Add per-test PowerShell arguments plus a staged quick-demo subset so the quick suite can run one committed regression demo in both headless and graphics modes
- [x] Fix host-build fallout exposed by the new demo coverage
- [x] Make the quick-record install slice self-contained by running the stage prerequisite immediately before install
- [x] Run the quick suite, fix local failures, and rerun until it passes

## Validation
- `android/tests/test_input_demo_regressions.ps1 -DemoRoot android/temp/quick_demo_subset -Game d2 -TimeoutSeconds 120 -StopOnFirstFailure`: pass after fixing the D2 host/headless target wiring
- `android/run_quick_tests.ps1`: initial reruns exposed three local issues and each was fixed in place
- D1 host build fix: `d1/main/net_udp.c` and `d2/main/net_udp.c` now keep `net_udp_android_set_bind_loopback()` as a no-op off Android so host builds do not reference Android-only state
- D2 host/headless build fix: `d2/main/CMakeLists.txt` now compiles `net_udp_p2p_proxy_shared.c` into `dxx-redux-d2-headless` and defines `DXX_BUILD_DESCENT_II` for both host D2 targets so shared sources take the D2 branches
- Final validation: `android/run_quick_tests.ps1` pass, 15 passed, 0 failed, 0 skipped, total time `00:02:37`, report `temp/test_reports/quick_report_20260519_150233.md`

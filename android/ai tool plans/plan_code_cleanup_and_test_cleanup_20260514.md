# Plan: Code Cleanup And Test Cleanup 2026-05-14

## Goal

- Clean up current Android-branch lint and warning issues without broad churn in pre-cmake `d1/` or `d2/` code
- Bring `android/run_all_tests.ps1` back in sync with the current test inventory, including regression demos and unattended-safe coverage

## Local hypothesis

- `android/run_all_tests.ps1` misses newer test coverage because it only auto-discovers standalone `game_scripts/test_*.json5` and `android/tests/test_*.ps1` entries.
- The smallest safe fix is to extend the test catalog with explicit wrapper-script entries and update the unattended skip list for any newly added manual tests.

## Cheap check

- Patch the runner to include the regression demo batch script as a no-infra test and mark `test_manual_lan_coop` as manual-skipped.
- Run a filtered `run_all_tests.ps1` pass that exercises the new catalog entries without rerunning the entire suite.

## Steps

- [x] Audit current `run_all_tests.ps1` coverage against the current Android test scripts
- [x] Patch `run_all_tests.ps1` to include regression demos and newly added unattended exclusions
- [x] Run targeted cleanup tooling and fix any local lint or warning issues in touched Android-branch code
- [x] Validate the updated runner with focused filtered runs
- [x] Update this plan with final outcomes and validation notes

## Outcome

- `android/run_all_tests.ps1` now covers the committed input-demo regression corpus and Android JVM unit tests through two dedicated `test_*.ps1` wrappers: `android/tests/test_input_demo_regressions.ps1` and `android/tests/test_gradle_unit_tests.ps1`.
- The unattended catalog was de-flaked by reclassifying newer host-only tests into the no-infra tier, moving `test_bot_client` into a server-only tier, moving the GOG installer wrappers into the extract tier, and skipping `test_manual_lan_coop` plus `test_lan_discovery` as manual helpers.
- Focused validation exposed an unrelated host-build blocker in the mirrored `test_android_save_meta` CMake targets: both `d1/maths/CMakeLists.txt` and `d2/maths/CMakeLists.txt` were missing PhysFS include/link wiring for `android_save_meta.c`. That wiring is now fixed in both targets.
- A scoped `android/run-code-quality.ps1 -Fix` pass was run on the touched Android PowerShell files and completed cleanly.

## Validation

- `android/run_all_tests.ps1 -Filter test_input_demo_regressions` passed after the final formatting pass with `RESULT: PASS (11 regression demo(s))`.
- `android/run_all_tests.ps1 -Filter test_gradle_unit_tests` passed after the final formatting pass with `BUILD SUCCESSFUL`.
- `android/run_all_tests.ps1 -Filter test_manual_lan_coop` reported `0 runnable, 1 manual-skipped`.
- `android/run_all_tests.ps1 -Filter test_lan_discovery` reported `0 runnable, 1 manual-skipped`.
- `run-windows-build.ps1 -Target d1` completed successfully after the mirrored PhysFS wiring fix.
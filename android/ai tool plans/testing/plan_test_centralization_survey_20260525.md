# Test Centralization Survey

## Goal
- Survey current test files and runners to find tests not covered by `android/run_all_tests.ps1`
- Identify what would be needed to centralize runners by domain while keeping domains separate

## Steps
- [x] Read repository instructions
- [x] Inventory test scripts, source tests, Gradle tests, CMake tests, Rust tests, and automation scripts
- [x] Compare inventory against `android/run_all_tests.ps1`
- [x] Summarize uncovered tests and recommend centralization changes

## Notes
- `android/run_all_tests.ps1` discovers top-level `android/game_scripts/test_*.json5` scripts except `_standalone: false`, plus `android/tests/test_*.ps1`.
- Kotlin JVM unit tests are covered indirectly through `android/tests/test_gradle_unit_tests.ps1`.
- Native extract CTest tests are covered indirectly through `android/tests/test_cue_iso.ps1`.
- D1/D2 native maths/input-demo test executables are built by CMake but are not registered with CTest and are not run by `run_all_tests.ps1`.
- Rust coverage is partial: `test_server_integration.ps1` runs `cargo test --test integration`, but not `nat_sim_tests` or plain `cargo test`.
- `validate_extract_regression_specs.ps1` and root-level `android/test_door45_pose_repro*.json5` are not discovered.

## Implementation Started
- [x] Registered D1/D2 native maths/input-demo executables with CTest.
- [x] Added `android/tests/test_native_host_unit_tests.ps1` as the native host domain runner.
- [x] Changed the server runner to use `cargo test` so Rust integration, NAT simulator, and unit tests are covered together.
- [x] Added `android/tests/test_validate_extract_regression_specs.ps1` so extract spec validation is discovered.
- [x] Made `run_all_tests.ps1` pass `-All` to `test_all_extracts.ps1`.
- [x] Removed stale root-level door45 JSON duplicates in favor of `android/game_scripts`.
- [x] Made `_standalone: false` JSON scripts show as skipped in the run-all report instead of being silently ignored.

## Validation
- [x] PowerShell parser check passed for changed scripts.
- [x] Scoped `android/run-code-quality.ps1 -Fix` passed for changed test infrastructure files.
- [x] `android/tests/test_validate_extract_regression_specs.ps1` passed.
- [x] `server/run_nat_tests.ps1` passed.
- [x] `android/tests/test_server_integration.ps1` passed with full `cargo test`.
- [x] `android/tests/test_native_host_unit_tests.ps1 -Game d1` passed.
- [x] `android/tests/test_native_host_unit_tests.ps1 -Game d2` passed.
- [x] `android/run_all_tests.ps1 -Filter test_validate_extract_regression_specs` passed.
- [x] `android/run_all_tests.ps1 -Filter test_native_host_unit_tests` passed.

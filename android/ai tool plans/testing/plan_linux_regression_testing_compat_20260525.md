# Plan: Linux regression testing compatibility 2026-05-25

## Goal

- Make the Android/regression testing helper scripts usable from Linux in the same style as the existing Linux get_deps/build work
- Keep Windows behavior intact and avoid source changes in d1/ or d2/ unless a later test run proves they are needed

## Survey

- [x] Inventory regression and automation test entrypoints under `android/`
- [x] Identify Windows-only assumptions: PowerShell path handling, adb invocation, emulator control, process cleanup, and output redirection
- [x] Identify scripts that already have usable shell equivalents and scripts that need cross-platform PowerShell fixes

## Survey Findings

- JSON5 automation scripts are mostly portable data; the PowerShell harness was the first choke point
- Shared helpers in `android/test_env.ps1` and `android/test_helpers.ps1` had the highest leverage Windows assumptions: adb/emulator paths, PATH separator, Git Bash usage, firewall setup, and `.exe` server binary names
- Host input-demo replays needed Linux executable names and the Linux build helper instead of `.exe` binaries and `run-windows-build.ps1`
- No-infra helper scripts also had direct Windows assumptions in Gradle, bot-client, fpcalc/acoustid, and extract spec validation paths
- Process-heavy dual-emulator and LAN scripts still need a later pass after the shared helpers are settled

## First Implementation Tranche

- [x] Add or reuse platform helpers for host OS, repo paths, adb checks, and process cleanup where the regression scripts need them
- [x] Patch the main test runners so Linux paths and `pwsh` execution work without Windows drive assumptions
- [x] Patch test data copy/setup helpers so populated `game_data/` is usable on Linux
- [x] Validate with syntax/lint checks first, then one focused test command if the environment supports it

## First Tranche Details

- Added `android/test_host_platform.ps1` for host detection, portable path joining, current `pwsh` resolution, Android SDK tool resolution, and build-tool executable resolution
- Wired host helpers into `android/test_env.ps1`, `android/test_helpers.ps1`, `android/run_test.ps1`, `android/run_all_tests.ps1`, `android/Run-TestMenu.ps1`, `android/Run-Emulator.ps1`, input-demo host replay scripts, Gradle unit tests, extract tests, SAF archiver tests, lightweight LAN helpers, fpcalc/acoustid checks, and bot-client server checks
- Kept Windows defaults intact while allowing Linux tools such as `/home/user/local/android-sdk/platform-tools/adb`, `/home/user/local/android-sdk/emulator/emulator`, `./gradlew`, and Linux host game binaries without `.exe`
- Made `run_input_demo_regressions.ps1 -ListOnly` noninteractive so inventory checks do not prompt for headless/graphics mode
- Extended `game_data/generate_game_data_index.ps1` to include `.pkg` and `game_data/demo installers`
- Hardened `android/tests/validate_extract_regression_specs.ps1` so unreadable CUE files are reported as validation failures instead of terminating the whole scan
- Added shared Gradle wrapper resolution so runner scripts use `gradlew` on Linux and `gradlew.bat` on Windows

## Validation

- Scoped `android/run-code-quality.ps1` passed over 19 touched PowerShell files plus `game_data/generate_game_data_index.ps1`
- Linux helper smoke check resolved adb, emulator, repo root, game-data index, and D1/D2 host executable names correctly
- `android/tests/run_input_demo_regressions.ps1 -ListOnly` now lists demos without prompting
- `android/tests/test_gradle_unit_tests.ps1` exits 0 on Linux with the shared Gradle wrapper selection
- `find game_data -name extract_regression.json5 | wc -l` found 34 extract specs
- `android/tests/validate_extract_regression_specs.ps1` now completes its scan, but reports local media permission failures: 33 unreadable CUE files and 192 unreadable BIN/ISO/IMG files under `game_data/CD images`

## Remaining Work

- Fix or document local permissions for `game_data/CD images` before running extraction tests on Linux
- Do a second pass over dual-emulator, LAN, and multiplayer scripts once the shared helper layer is stable
- Run at least one emulator-backed JSON5 test after adb/emulator health is confirmed and media permissions are readable
- Run or extend Gradle/JVM no-infra tests after deciding whether this tranche should include APK/unit validation

## Results

- First tranche complete and validated with no-emulator checks; emulator-backed and dual-emulator scripts remain for the next tranche

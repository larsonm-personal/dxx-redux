# Project

Cross-platform Descent 1/2 in C/C++ with SDL; current focus is the Android port

## Architecture

- Minimize changes in `d1/` and `d2/` to ease upstream merges; put shared new code in `android/`. Check both games when adding hooks or fixing duplicated behavior, without broadly deduplicating upstream code
- Preserve Windows/Linux/macOS builds. Guard Android-only changes with `__ANDROID__` or separate files; identify Android-related diagnostic additions
- Core edits are appropriate for introspection/automation, touch support, Android filesystem integration, and maintaining a single source of truth. Keep game-format knowledge in the engine (e.g. player-file access through `playsave.c`), not duplicated in Kotlin
- Document constants or interface definitions duplicated across native and JVM code so both copies stay synchronized
- Android launcher/save/config/data formats are currently pre-release and disposable: replace directly without compatibility readers, migrations, or compatibility tests unless requested. Preserve compatibility in `d1/` and `d2/` for upstreaming
- Keep launcher services separate from game-specific code so the launcher can eventually support other games
- Keep dependencies lightweight and cross-platform; pin dependencies and build tools to exact versions or commits

## Code and workflow

- Match existing style, especially in `d1/` and `d2/`, including header-defined string constants. The engine is largely C-style C++; simple RAII and standard containers are welcome in `android/` when they improve safety
- Avoid unnecessary wrappers, abstractions, and duplication. Preserve handmade comments
- Prefer printable ASCII and UTF-8 without BOM. No emoji or em dashes. Omit terminal periods in standalone log messages and comments
- For significant multi-step work, write a plan in `android/ai tool plans/`; skip this for one-step fixes or questions
- Verify code changes with the relevant CMake build and tests; fix new compiler warnings
- For major changes, add or extend a high-level integration test and run it to completion, fixing failures. Prefer meaningful integration coverage over tests for every small function; provide reusable runners for new Android tests
- Format/lint changed code with one scoped, mixed-language invocation:
  `.\android\run-code-quality.ps1 -Fix -Paths path\to\changed-file path\to\changed-dir`
- Reserve unscoped formatting for broad cleanup, formatter/config changes, or release validation. The formatter mutates files: wait for it to exit before editing or rerunning. After interruption or a stale lock, inspect `android/helpers/stop-stale-formatters.ps1` and use `-Kill` if needed

## Build and test entry points

Run commands from the repository root

- Windows host build: `.\run-windows-build.ps1` handles MSVC and CMake/Ninja setup; see `.github/workflows/package-msvc.yml` for CI options
- Before Android Gradle tasks on Windows, set JDK 21:
  `$env:JAVA_HOME='C:\local\jdk-21'; $env:Path="$env:JAVA_HOME\bin;$env:Path"`
- Check devices with `adb devices`; Windows fallback: `C:\local\android-sdk\platform-tools\adb.exe`
- Start/provision an emulator with `.\android\Run-Emulator.ps1` (`-NoBuild` if the APK is current), or `android/run_emulator.sh` on bash hosts. Start one when needed for testing; the PowerShell helper may keep tailing logcat after setup
- Run emulator tests serially because they share device state. Clear logcat first, capture output to a file, and check the exit code. Keep temporary artifacts under repo-local `temp/` or `android/temp/`

```powershell
adb logcat -c
.\android\helpers\run_test.ps1 -ScriptName test_launch_to_automap.jsonc -Game d2 2>&1 | Out-File temp\test_output.txt -Encoding utf8
$testExitCode = $LASTEXITCODE
Get-Content temp\test_output.txt -Tail 30
Write-Output "EXIT: $testExitCode"
```

## Debugging and automation

- For on-device problems, add targeted logging to test hypotheses before attempting speculative fixes
- Android logs use `debug_log()` from `android/app/src/main/cpp/shared/android_log.h`, with categories in `debug_log_categories.h`. `con_printf` routes to `DLOG_GAME`; Android does not write `gamelog.txt`. Read debug files via adb or export them from the launcher's Advanced tab
- Inspect game/setup state through the introspection API instead of screenshot/OCR analysis; extend the API when necessary. Implementation: `android/app/src/main/cpp/shared/game_introspect.cpp`
- Use `android/helpers/introspect.sh` for state dumps; useful modes include `menu`, `player`, `position`, `console`, `setup`, `autolog`, and `autoresult`
- Drive tests through the automation API; save reusable scripts in `android/game_scripts/*.jsonc`. Existing scripts demonstrate player/mission/level selection and skipping briefings. Base missions are named "first strike" (D1) and "counterstrike!" (D2)
- The test runner uses durable `files/automation_result.json` for pass/fail and `files/automation_log.jsonl` for step diagnostics; inspect these on failure
- Generate regression text/JSON in a stable, normalized, pretty-printed form at the source, not through a post-run formatter

## Simulation determinism

- Input replays in `android/regression_demos/*.dximdemo` start from a level or save and compare final game state. Per-frame state and `.rngtrace.jsonl` traces help locate divergence; see `android/regression_demos/README.md`
- A replay mismatch may be a regression or remaining engine nondeterminism. Investigate only far enough to test causes and fix the engine; avoid replay-specific compensation
- Existing desyncing demos are evidence, not compatibility targets. New demos can be recorded after the engine is fixed

## Mission metadata regeneration

- Default full regeneration: `.\android\helpers\regenerate_all_mission_metadata.ps1` builds/installs the APK, runs the emulator ZIP batch, and analyzes configured extracted-CD missions on the host
- Fast host-only refresh: `.\android\helpers\regenerate_all_mission_metadata_host.ps1`. This does not validate Android import, storage, staging, or automation behavior
- For custom filters or options, use `android/helpers/run_mission_zip_batch.ps1`; omit `-NoRegressionJson` when regenerating checked-in metadata. Use the emulator path for Android behavior/parity checks

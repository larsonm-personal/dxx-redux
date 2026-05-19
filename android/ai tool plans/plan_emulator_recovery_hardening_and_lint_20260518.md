# Plan: Emulator recovery hardening and lint pass 2026-05-18

## Goal

- Harden Android emulator recovery so suite helpers can force a clean recycle instead of relying on the current "restart only if unhealthy" path
- Extend recovery beyond the current primary-emulator launcher case so dual-emulator and later-tier degradation can be recovered without manual terminal cleanup
- Follow the recovery changes with a scoped lint / warning reduction pass on the touched scripts

## Local hypothesis

- The current helper stack treats adb responsiveness as sufficient health and uses `emu_health.ps1 -Restart` in places where the emulator is already degraded but still responsive enough to skip the restart
- The broad late-suite collapse is consistent with stale adb / emulator session state and missing forced-recycle hooks for tier transitions and non-primary recovery paths, not with a single test-script regression

## Cheap checks

- Patch `android/emu_health.ps1` to support forced clean restart and parameterized AVD launch instead of one hard-coded primary path
- Patch `android/test_helpers.ps1` / `android/run_all_tests.ps1` to call the stronger restart path for primary and dual-emulator recovery
- Validate the touched PowerShell with `get_errors` and a scoped `PSScriptAnalyzer` run instead of another full suite

## Steps

- [x] Add forced-restart and parameterized launch support to `android/emu_health.ps1`
- [x] Reuse the stronger recovery path from `android/test_helpers.ps1` for primary and second-emulator startup/recovery
- [x] Harden `android/run_all_tests.ps1` tier recovery logic for single-emu, extract, and dual-emu degradation
- [x] Run scoped PowerShell lint / warning checks on the touched scripts and fix actionable findings
- [x] Update this plan with outcomes and remaining risks

## Outcomes

- `android/emu_health.ps1` now supports `-ForceRestart`, parameterized AVD launch, serial-scoped health checks, and safer cleanup of multi-emulator adb state
- `android/test_helpers.ps1` now routes launcher recovery and primary startup through the forced clean restart path and uses a shared managed start path for the second emulator with explicit boot readiness checks
- `android/run_all_tests.ps1` now forces a real recycle during primary recovery, repairs the primary emulator after extract-tier failures, and recycles both emulators plus the matchmaking server after dual-emulator failures
- Scoped lint checks on the touched recovery scripts passed after fixing two string interpolation issues in `android/emu_health.ps1`
- A broader Android PowerShell analyzer pass also passed after fixing one brace-formatting issue in `android/Run-Emulator.ps1` and one null-comparison warning in `android/tests/run_input_demo_replay.ps1`
- Android Kotlin lint is now clean repo-wide after fixing `ConfigImportExport.kt` and `LauncherScriptExecutor.kt`
- Android Kotlin compile-warning collection stayed at zero warnings after the Kotlin lint fixes
- Windows host warning reduction removed the repeated `loadgl.h` / GLEW macro-redefinition warnings in both `d1` and `d2`
- Windows host warning reduction also removed the callback-signature and missing-return MSVC warnings surfaced in `songs.c`, `multi.c`, `gr.c`, and `ogl.c`
- A fresh host-build warning inventory then exposed only one follow-up warning class introduced by the callback cleanup (`songs_play_file` / `jukebox_hook_next` old-style declarations), and that class was removed by aligning the shared declarations in `songs.h` and the jukebox callbacks in both trees
- The last remaining Android-native warnings in branch-owned code came from dead helper functions in `android/app/src/main/cpp/shared/game_introspect.cpp`; removing that unused block brought `android\gather-warnings.ps1 -NativeOnly` back to zero warnings

## Validation

- `get_errors` reported no parser/type errors in `android/emu_health.ps1`, `android/test_helpers.ps1`, and `android/run_all_tests.ps1`
- `android\run-psscriptanalyzer.ps1 -Check -Paths android\emu_health.ps1,android\test_helpers.ps1,android\run_all_tests.ps1` passed
- `android\emu_health.ps1 -Restart -Wait -ForceRestart -TimeoutSeconds 240 -AvdName Nexus5X_Light_1` recovered an actually offline `emulator-5554` to healthy state
- `Start-SecondEmulator` returned `True` once the managed secondary device finished booting
- `android\run-psscriptanalyzer.ps1 -Check` passed for all Android PowerShell scripts
- `android\run-ktlint.ps1 -Check` passed for all Android Kotlin files
- `android\gather-warnings.ps1 -KotlinOnly` reported zero Kotlin compiler warnings after the Kotlin fixes
- Incremental `run-windows-build.ps1 -Target d2` and `run-windows-build.ps1 -Target d1` rebuilds no longer emitted the targeted MSVC `C4005`, `C4113`, or `C4715` warnings after the host warning fixes
- A fresh `run-windows-build.ps1 -Target both` warning scan exposed only `C4028` from the newly-tightened music callback signatures, and clean child-shell `run-windows-build.ps1 -Target d1` / `-Target d2` reruns produced no `warning C`, `warning:`, or `error C` lines after aligning `songs.h` and `jukebox.c`
- `android\run-code-quality.ps1 -Paths @('android\app\src\main\cpp\shared\game_introspect.cpp')` passed after the final shared introspection cleanup
- `android\gather-warnings.ps1 -NativeOnly` reported zero C/C++ warnings after removing the unused `game_introspect.cpp` helpers

## Remaining risks

- The dual-emulator runtime validation here confirmed startup and readiness, but it did not run the full batch harness; the real acceptance surface remains the nightly `run_all_tests.ps1` pass the user requested
- The Windows warning checks above were validated with incremental host rebuilds of the touched targets, not a fresh clean full-warning inventory pass for both games
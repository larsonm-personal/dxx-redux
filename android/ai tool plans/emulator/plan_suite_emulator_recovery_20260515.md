# Plan: Suite Emulator Recovery 2026-05-15

## Goal

- Stop long mid-suite cascades where launcher-backed Android tests fail one after another with `SetupActivity not responding`
- Add a stronger launcher-specific recovery path for the current test and a suite-level emulator recycle strategy for later tests

## Local hypothesis

- The current health check only proves that adb, shell, and package manager respond; it does not prove that `SetupActivity` can actually start and service `SETUP_INTROSPECT`
- Once one single-emulator test poisons the launcher/app state, subsequent launcher-backed tests fail at the same first step with no strong recovery, so the suite accumulates a long streak of non-informative failures

## Cheap checks

- Patch `android/run_test.ps1` / `android/test_helpers.ps1` so launcher startup does one stronger recovery attempt when `Wait-SetupActivityReady` fails
- Patch `android/run_all_tests.ps1` so repeated single-emulator launcher-health failures trigger an emulator restart and reprovision before the next test
- Re-run a focused launcher-backed slice and confirm there is no `SetupActivity not responding` cascade across successive tests

## Steps

- [x] Add shared launcher recovery helper(s) in `android/test_helpers.ps1`
- [x] Use the recovery helper in `android/run_test.ps1` launcher startup flow
- [x] Add mid-tier single-emulator recycle logic in `android/run_all_tests.ps1`
- [x] Run focused validation on a launcher-backed slice
- [x] Update this plan with outcomes and validation notes

## Outcomes

- Added `Restart-AdbServer` and `Invoke-LauncherStartupRecovery` to `android/test_helpers.ps1`
- `Start-GameWithRetry` now escalates to emulator recovery before retrying if `SetupActivity` never becomes ready
- `android/run_test.ps1` launcher flow now does one recovery-and-reprovision pass before failing a launcher-backed test on `SetupActivity` startup
- `android/run_all_tests.ps1` now tracks single-emulator failure streaks, detects launcher-health signatures in per-test logs, and restarts/reprovisions the primary emulator before later tests continue

## Validation notes

- `get_errors` reported no PowerShell errors in `android/test_helpers.ps1`, `android/run_test.ps1`, or `android/run_all_tests.ps1`
- `android/run_all_tests.ps1 -Filter "test_au*"` could not complete because the suite build step is currently blocked by an unrelated native compile error in `android/app/src/main/cpp/android_input.c` (`android_cutscene_tap_suppressed` undeclared / duplicate declaration)
- Direct launcher validation still ran through `android/run_test.ps1` using the existing installed debug app:
	- `test_autosave_resume_missing_pilot_unified.json5 -Game d2` launched `SetupActivity` successfully, then failed later at step 8 waiting for `screen_mode = menu`
	- `test_autosave_resume_unified.json5 -Game d2` launched `SetupActivity` successfully immediately after the first failure, then failed later at step 8 waiting for `screen_mode = menu`
- In that back-to-back validation pair there was no repeat of the report's `SetupActivity not responding` cascade
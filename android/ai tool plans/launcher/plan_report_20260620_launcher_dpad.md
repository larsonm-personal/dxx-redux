# Report 2026-06-20 launcher D-pad timeout

## Goal
- Fix the final non-passing test from `temp/test_reports/report_20260620_133235.md`
- Target `test_launcher_dpad`, which timed out during Test 5
- Avoid broad launcher rewrites unless the focused repro points to a real focus bug
- Run focused verification and scoped quality checks

## Plan
- [x] Read repo instructions and failure report
- [x] Inspect the test helper and launcher D-pad code paths
- [x] Reproduce or add enough diagnostics to locate the timeout boundary
- [x] Patch the narrowest issue
- [x] Run focused verification
- [x] Run scoped quality checks
- [x] Record results and residual risk

## Target
- `test_launcher_dpad` timed out after printing `Test 5: DPAD RIGHT navigation...`.

## Findings
- A focused run passed end-to-end in roughly 44 seconds, so the basic launcher focus behavior is healthy.
- The timeout point sits inside a path with unbounded ADB calls: setup introspection broadcasts in `Wait-SetupCondition`, setup commands, and raw `adb shell input keyevent`.
- Under full-suite emulator/ADB load, any of those can consume the outer 120 second per-test timeout without a normal test failure message.

## Fix
- Changed setup introspection polling to use `Adb-Timeout` for the broadcast, matching the bounded read already used for `setup_introspect.json`.
- Changed `test_launcher_dpad` key events and setup commands to use `Adb-Timeout` and fail with a specific diagnostic if ADB stalls.

## Results
- Cleared logcat and ran `android/tests/test_launcher_dpad.ps1 -TimeoutSeconds 180`; it passed with `EXIT: 0`.
- Ran the same test through the suite wrapper: `android/run_all_tests.ps1 -Filter test_launcher_dpad`; report `temp/test_reports/report_20260620_153325.md` shows `PASS`, `Failed: 0`, `Timeouts: 0`, and `test_launcher_dpad` completed in `00:51`.
- Scoped quality checks passed for `android/helpers/test_helpers.ps1` and `android/tests/test_launcher_dpad.ps1`.

## Residual Risk
- The focused run passed before and after the harness hardening, so this fix addresses the observed suite-timeout failure mode rather than a launcher focus code regression.
- If ADB stalls repeatedly for more than the per-call timeout, the test should now fail with a specific ADB timeout message instead of being killed by the outer runner.

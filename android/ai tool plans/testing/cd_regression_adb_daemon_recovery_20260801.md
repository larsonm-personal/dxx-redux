# CD regression ADB daemon recovery

## Goal

Recover the extraction regression batch from a transient local ADB daemon outage without hiding genuine extraction or device failures.

## Plan

- [x] Trace the failing sanitization command, ADB wrapper, and batch retry policy
- [x] Define a narrow recoverable ADB transport classification and recovery boundary
- [x] Implement bounded daemon/device recovery without retrying semantic failures
- [x] Extend existing recovery-oriented coverage rather than duplicating extraction regressions
- [x] Run scoped formatting and focused existing tests
- [x] Record outcome and remaining emulator validation

## Outcome

- The child extraction runner now maps only its own ADB timeouts and recognized ADB transport failures to infrastructure exit code 98. Other ADB command failures and semantic extraction failures remain hard errors.
- The batch first resets the ADB server and rechecks device health. It escalates to the existing full emulator restart only if that lighter recovery does not restore the device, then reruns the complete spec once.
- `test_extract_regression_workflow.ps1`, scoped code quality, and `git diff --check` pass.
- `test_test_helpers_process_wait.ps1` currently fails in its pre-existing mocked `Reset-DeviceGameState` case because `Publish-DefaultActiveFileSet` cannot succeed under that test's `Adb-Timeout` mock. The failure is outside the extraction recovery changes.
- A real emulator rerun remains the end-to-end validation for an actual daemon outage.

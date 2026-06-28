# Test Failure Robustness Plan - 2026-06-28

## Goal
Review the five failures from `temp/test_reports/report_20260628_101133.md`, choose the two tests most likely to benefit from transformational robustness work, and implement focused fixes without masking problems by extending timeouts.

## Selected Tests
- `test_all_extracts`: failed after an ADB short write because retry cleanup invoked `rm -f` on an unquoted app-private path containing spaces and parentheses. The robustness payoff is high because this is a shared source-staging path for extraction tests.
- `test_dpad_triggers`: failed on a transient input-state assertion after axis injection. The robustness payoff is high because the test was sampling one frame instead of synchronizing on the intended held-input state.

## Tasks
- [x] Inspect failure logs and scripts for the five failed tests
- [x] Select two tests with the best robustness payoff and document why
- [x] Implement fault-tolerance changes in the selected tests or shared helpers
- [x] Run targeted verification for the changed tests where practical
- [x] Run scoped code quality on touched files
- [x] Summarize selected tests, changes, and remaining risks

## Verification
- `android\helpers\run_test.ps1 test_dpad_triggers.json5 -Install -TimeoutSeconds 600`: passed for D1 and D2; the previously failing LT reverse phase observed non-zero `throttle_time` and then returned to neutral.
- `android\tests\test_all_extracts.ps1 -Seed 342861260`: passed the same `Dimensions for Descent (USA)` sample from the failure report, including app-private staging of the `.cue` and `.bin` paths with spaces and parentheses.
- `android\run-code-quality.ps1 -Fix -Paths <touched files>`: passed. PowerShell parse and non-ASCII checks also passed.

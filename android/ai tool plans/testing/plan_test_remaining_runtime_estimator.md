# Test remaining runtime estimator

## Goal

Base suite progress on the historical runtime weights of tests that remain in
the execution queue, rather than subtracting current elapsed time from a
historical total.

## Plan

- [x] Locate the estimator and historical runtime inputs
- [x] Define remaining-time and remaining-percent behavior
- [x] Add focused regression coverage for an overrun
- [x] Implement the remaining-test historical sum
- [x] Run focused tests and scoped code quality
- [x] Record validation results

## Behavior

- Each selected test receives its known historical runtime or the existing
  median fallback when no history is available
- Estimated remaining time is the sum of those weights for the current test
  and every test after it in execution order
- Remaining percent is that sum divided by the sum of all selected test
  weights
- Actual suite elapsed time is displayed but does not reduce historical work
  still present in the queue

## Validation

- `test_test_suite_progress.ps1`
- `test_validate_automation_catalog.ps1`
- Filtered `run_all_tests.ps1 -Filter test_test_suite_progress`, with the new
  test classified in the no-infrastructure tier
- Scoped `run-code-quality.ps1 -Fix`
- `git diff --check`

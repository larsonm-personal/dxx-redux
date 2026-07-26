# Test progress estimate correction

## Goal

Correct the full test runner's elapsed, remaining-time, and percentage progress
calculation.

## Plan

- [x] Locate the progress estimate implementation
- [x] Reproduce the displayed calculation with the reported 38/86 example
- [x] Correct the narrow arithmetic defect
- [x] Add or extend focused regression coverage
- [x] Run scoped quality and tests
- [x] Record the result

## Result

Two defects compounded:

- The remaining time was the sum of historical durations for unstarted tests,
  so it did not subtract the suite's actual elapsed wall time from the
  estimated total.
- The estimate copied only the newest report. That report contained a 34:19
  outlier for `test_kcxf2_guidebot_route_next`, versus 1:17 to 2:21 in the
  preceding four reports.

The runner now subtracts actual elapsed time from the historical total and
uses the median runtime from the four most recent completed reports. The
runtime-reader regression test covers even-sample medians, hour parsing,
missing samples, skipped tests, and rejection of a large newest-run outlier.

Validation:

- `android/tests/test_test_report_runtimes.ps1` passed after formatting.
- Scoped `android/run-code-quality.ps1 -Fix` passed for all three changed
  PowerShell files.
- PowerShell parsed `android/run_all_tests.ps1` without syntax errors.
- `git diff --check` passed.

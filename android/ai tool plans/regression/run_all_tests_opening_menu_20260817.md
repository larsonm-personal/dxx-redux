# Run-all-tests opening menu

## Goal

Expose the approximately 45-minute proportional test profile through a `T` option when `run_all_tests.ps1` is opened interactively without parameters.

## Plan

- [completed] Add an interactive Full, T, and Quit profile selector without disrupting parameterized or redirected runs
- [completed] Add focused coverage for menu choices and unattended behavior
- [completed] Run focused tests, scoped code quality, and required build validation

## Validation

- Menu selection and unattended gating tests passed
- Scoped code quality passed
- Windows D1 and D2 builds passed
- CTest passed 33/33 D1 tests and 40/40 D2 tests

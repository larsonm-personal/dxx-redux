# Regression wrapper argument validation

## Goal

Make `regenerate_all_regression_data.ps1` fail immediately when any unsupported argument is supplied.

## Plan

- [x] Add explicit advanced-script parameter binding with no accepted parameters
- [x] Extend the existing wrapper test to verify unknown arguments fail
- [x] Run scoped formatting and the existing wrapper test
- [x] Record completion

## Result

The wrapper accepts only standard PowerShell common parameters. Unsupported parameters such as `-Force` and `-UnknownArgument` now fail during parameter binding before any regeneration stage runs.

# Pasted regression failure follow-up

## Plan

- [x] Read and classify the pasted failure output
- [x] Trace the failing stage to its implementation and existing tests
- [x] Reproduce the failure with a focused command
- [x] Implement the scoped fix and regression coverage
- [x] Run focused verification and code quality checks
- [x] Record findings and completed work

## Findings

- `Resolve-Selection` writes its results to the pipeline, so a one-item result is assigned as the item itself rather than an array
- In the reported run, the only existing install-sync choice was the PowerShell hashtable
- Selecting all target updates also produced additional install selections, and `+` therefore attempted hashtable addition instead of array concatenation
- Both resolved selection variables are now explicitly array-wrapped, and the later merge wraps both operands defensively

## Verification

- Added source-contract checks for array-wrapped selection resolution and merging
- Added a behavioral one-existing-install plus two-target-installs fixture
- `android/tests/test_get_deps_runtime_updates.ps1`: passed
- PowerShell parser checks passed for the updater and test
- Scoped code quality checks passed
- `git diff --check` passed

The live `a` plus `a` workflow was not rerun because it would apply the listed dependency target updates and invoke installers.

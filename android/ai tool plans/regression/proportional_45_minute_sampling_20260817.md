# Proportional 45-minute sampling

## Goal

Make targeted runs approach 45 minutes by selecting a comparable percentage of items from every contained stage, for both regression-data regeneration and `run_all_tests.ps1`.

## Plan

- [completed] Map each regeneration stage and test group to its item discovery, filtering, and timing interfaces
- [completed] Design a shared proportional sampler that preserves every nonempty group and targets the runtime budget
- [completed] Add item-level targeted modes to the regeneration child stages and wire the master runner to them
- [completed] Update `run_all_tests.ps1` to use proportional group sampling
- [completed] Add integration coverage for distribution, reproducibility, and runtime targeting
- [completed] Run focused workflows, builds, tests, and scoped code quality

## Validation

- Runtime sampler, master regeneration, CD runner, and extraction workflow tests passed
- Scoped code quality passed for all changed scripts
- Windows D1 and D2 host builds passed
- CTest passed 33/33 D1 tests and 40/40 D2 tests

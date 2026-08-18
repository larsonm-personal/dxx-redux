# Runtime-targeted regression sampling

## Goal

Add an optional approximately 45-minute profile to the regression-data and full-test runners. Use historical timings and randomized, type-balanced selection so the profile covers every available test category without treating the duration as a hard cutoff.

## Plan

- [x] Inspect both runner catalogs, timing reports, and existing runner tests
- [x] Implement historical-runtime, randomized type-balanced selection in both runners
- [x] Add or extend focused tests for selection, argument handling, and reporting
- [x] Run scoped code quality and relevant tests, then record results here

## Results

- Both runners accept `-Target45Minutes` and an optional reproducible `-SampleSeed`
- The shared sampler uses median recent report timings, randomized ordering, type coverage, closest-target fill, and graceful handling of stages larger than the target
- `run_all_tests.ps1` balances infrastructure requirements and ps1/json5 types and automatically adds required headless input-demo runs for selected graphics comparisons
- The regeneration menu also exposes the targeted profile as `T`; its estimate notes that the three regeneration stages are coarse
- `test_runtime_targeted_sampling.ps1` and `test_regenerate_all_regression_data.ps1` pass
- All five changed PowerShell files pass the scoped code-quality wrapper, parser validation, and `git diff --check`
- A full runner smoke was intentionally not launched because its standard cleanup would stop the pre-existing emulator

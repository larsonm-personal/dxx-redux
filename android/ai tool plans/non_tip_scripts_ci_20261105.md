# Reduce redundant non-tip script CI

- Audit current cmake workflows, timings and required-check references
- Keep workflow lint and policy/runner checks per PR
- Run full script suites on native tips, manual runs and uncertain metadata
- Skip heavy script and XL suites on all validated native non-tips, including test-changing layers; preserve standalone path selection and combined-tip coverage
- Gate entire jobs so omissions report skipped; admitted jobs finish and report actual test results
- Move XL contracts and Rust compilation to the tip with standalone path selection; Rust uses the live full stack base and runs conservatively when that base is uncertain
- Rename visible workflow and matrix checks to scripts and ci tests
- Add selection and workflow contract regressions, run policy tests and actionlint, publish a draft targeting cmake

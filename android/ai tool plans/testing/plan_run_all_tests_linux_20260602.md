# run_all_tests Linux compatibility pass

## Goal
Get `android/run_all_tests.ps1` and the related quality checks running on Linux without regressing Windows behavior.

## Steps
- [done] Inspect test runner and helper scripts for Windows-only assumptions
- [done] Reproduce the Linux failures with focused invocations
- [done] Patch cross-platform path/process/tool handling
- [done] Run focused checks, then a representative test-suite invocation
- [done] Run code quality for changed scripts where possible

## Results
- `test_env.ps1` now adds `JAVA_HOME/bin` to `PATH`, so Linux shells without a global `java` still run Gradle and ktlint correctly
- `run_all_tests.ps1` now prints Gradle task progress during APK builds, avoiding long silent build phases
- `run-psscriptanalyzer.ps1` now installs and imports pinned `PSScriptAnalyzer` version `1.25.0` from PSGallery when missing
- Verified `run_all_tests.ps1 -Filter test_validate_extract_regression_specs -StopOnFail`
- Verified `run_all_tests.ps1 -Filter test_double_launch -StopOnFail -TestTimeoutSeconds 60`
- Verified scoped `run-code-quality.ps1` on the changed files

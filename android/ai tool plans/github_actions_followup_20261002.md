# GitHub Actions tooling follow-up

## Evidence

- Latest Android tooling run 37098146620 at commit f409688aa19b fails on
  Ubuntu 24.04 and Windows Server 2022 during tooling smoke tests
- The catalog rejects `test_fov_demo_compatibility.jsonc` because its support
  metadata has no owner. The existing runner is `run_fov_demo_tests.ps1`
- `test_fov_cpu_visibility`, `test_mac_d2_demo_graphics`, and the artifact-only
  `test_combined_package_identity` are missing suite coverage families
- Catalog rejection also causes evidence and master catalog smoke failures
- Local source checkout is clean at the start of this repair

## Plan

- [x] Add a discoverable FOV demo owner that invokes the existing integration runner
- [x] Register new scenarios and classify the artifact inspection as an explicit host probe
- [x] Extend catalog integration checks for the new owner and scheduling declarations
- [x] Run the full bounded tooling smoke suite and Windows PowerShell 5.1 release checks
- [x] Run scoped formatting and workflow lint
- [x] Validate the repaired workflow on both hosted runner platforms

## Validation

- Catalog validation passed with 94 standalone JSON tests, 347 support scripts,
  and 183 standalone PowerShell tests
- Scoped mixed code quality passed for all six changed files
- All workflows pass actionlint 1.7.12; scoped `git diff --check` passes
- Windows PowerShell 5.1 passed all 77 release integration scenarios
- Full bounded Windows tooling smoke passed all 24 checks, including the master
  catalog integration, execution evidence, and bounded Python/extraction checks
- Hosted branch run 37099345018 and PR run 37099375894 passed on both Ubuntu
  24.04 and Windows Server 2022 at source commit
  `773a90c81c6f707cb24d42f91599ba80eb9bdf47`
- Both hosted Windows jobs passed the separate PowerShell 5.1 release step
- PR #3 targets `cmake`: https://github.com/larsonm-personal/dxx-redux/pull/3
- Terminal hosted results are recorded on the PR instead of generating another
  tooling run for a report-only commit

The source fix is also present in the original checkout. The hosted `cmake`
branch still requires the PR to be merged before subsequent runs use this fix.
No game engine, APK packaging, signing, or test validation gates were changed.

Evidence: `android/temp/github_actions_followup_20261002/`, with the full local
smoke report in `android/temp/tooling_smoke/run_bd764f3f9db64fe3b16d8a19860cfcaa/`.

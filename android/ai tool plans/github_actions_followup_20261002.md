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
- [ ] Run the full bounded tooling smoke suite and Windows PowerShell 5.1 release checks
- [x] Run scoped formatting and workflow lint
- [ ] Validate the repaired workflow on both hosted runner platforms

## Validation

- Catalog validation passed with 94 standalone JSON tests, 347 support scripts,
  and 183 standalone PowerShell tests
- Scoped mixed code quality passed for all six changed files
- All workflows pass actionlint 1.7.12; scoped `git diff --check` passes
- Windows PowerShell 5.1 passed all 77 release integration scenarios
- Full bounded Windows tooling smoke validation is in progress
- Hosted runner validation is pending. The draft PR will record terminal
  hosted results without generating another tooling run for a report-only commit

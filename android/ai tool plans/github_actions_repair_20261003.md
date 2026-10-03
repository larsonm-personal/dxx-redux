# Controller response catalog repair

## Evidence

- Android tooling run 37106933675 at commit 8a508ee7c92a fails on Ubuntu
  24.04 and Windows Server 2022 in the tooling smoke step
- The controller response JSON script and its PowerShell integration runner
  are both discovered as top-level tests under the same name
- The controller response integration has no suite coverage family
- Master catalog discovery and execution evidence also fail on this catalog error
- The existing 120s master timeout is insufficient for two 180s engine runs
- Use an isolated checkout to preserve concurrent device-network changes

## Plan

- [x] Declare the controller JSON script as support owned by its existing runner
- [x] Add input-preference coverage and a 600s two-engine integration timeout
- [x] Extend catalog integration coverage and expose child stdout in failure diagnostics
- [x] Document required catalog registration and validation for new tests
- [ ] Run scoped code quality, workflow lint, and the full bounded tooling smoke suite
- [ ] Verify hosted Ubuntu/Windows results and prepare the repair for merging

## Validation

- Isolated catalog validation passed: 94 standalone JSON tests, 349 support
  scripts, and 184 standalone PowerShell tests
- Scoped mixed code quality passed for all six changed files in the original
  checkout. Source hashes match the isolated files being tested
- Workflow actionlint 1.7.12 and scoped `git diff --check` passed
- Windows PowerShell 5.1 passed all 77 release integration scenarios
- Full bounded Windows tooling smoke is running in the isolated checkout
- Hosted validation is pending; terminal results will be recorded on the PR
  without triggering another run for a report-only commit

The original checkout contains only this repair's scoped additions alongside
the preserved, unrelated device-network campaign work. No controller gameplay
code or native engine code was changed by this repair.

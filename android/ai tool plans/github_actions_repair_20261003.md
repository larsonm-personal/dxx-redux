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
- [x] Run scoped code quality, workflow lint, and the full bounded tooling smoke suite
- [x] Verify hosted Ubuntu/Windows results and prepare the repair for merging

## Validation

- Isolated catalog validation passed: 94 standalone JSON tests, 349 support
  scripts, and 184 standalone PowerShell tests
- Scoped mixed code quality passed for all six changed files in the original
  checkout. Source hashes match the isolated files being tested
- Workflow actionlint 1.7.12 and scoped `git diff --check` passed
- Windows PowerShell 5.1 passed all 77 release integration scenarios
- Full bounded Windows tooling smoke passed all 24 checks in the isolated
  checkout, including the three catalog/evidence checks that failed in CI
- Hosted branch run 37141674451 and PR run 37141732139 passed on Ubuntu 24.04
  and Windows Server 2022, including the separate Windows PowerShell 5.1 step
- Validated source commit: `1b5b4a118c8caf72907b185af957e4d16ad7bfb5`
- PR #4 targets `cmake`: https://github.com/larsonm-personal/dxx-redux/pull/4
- Terminal hosted results are recorded on the PR without triggering another
  run for a report-only commit. The source fix needs merging into `cmake`

The original checkout contains only this repair's scoped additions alongside
the preserved, unrelated device-network campaign work. No controller gameplay
code or native engine code was changed by this repair.

Evidence is under `android/temp/github_actions_repair_20261003/`. The full
local smoke summary is under
`worktree/android/temp/tooling_smoke/run_a22c83f96c3c4269b49e1fbf0c8daffa/`
within that evidence directory.

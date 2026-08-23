# Mission files and PhysicsFS regression failures

## Plan

- [x] Compare the nine modified regression files with their checked-in versions and identify the shared mismatch
- [x] Trace the mismatch through mission filesystem setup and recent PhysicsFS-related changes
- [x] Correct the implementation or expectations without overwriting unrelated work
- [x] Run focused regression tests and scoped code quality checks
- [x] Record findings and mark completed work

## Findings

- File-set reconciliation moves loose missions into `.content/entries` and publishes them to the game through virtual `missions/` paths in a launch projection
- The extraction runner still verified physical `set_files` or `set_files_recursive`, so reconciliation made valid missions appear missing and introduced a race in direct-import polling
- The runner now verifies the logical file view from base set files plus `content_entries[].files`, while excluding internal content storage and projections
- Legacy direct-import specifications that record a mission basename are accepted when the logical file is now under `missions/`
- The D1 Mac launch-only failure did not reproduce and passed without an implementation change

## Verification

- `android/tests/test_extract_regression_workflow.ps1`: passed
- D1 Mac direct CD full launch: passed, reached `Lunar Outpost`
- Combined D2 plus Vertigo full launch: passed, found all seven expected files and reached `deep kraeg tunnel system`
- Vertigo direct CD file-only test: passed, found all four expected files after reconciliation
- Scoped `android/run-code-quality.ps1 -Fix`: passed
- `git diff --check`: passed

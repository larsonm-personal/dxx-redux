# Mission ZIP batch manual run

## Goal
Make the mission ZIP batch manual test flow continue after per-ZIP failures, report all failed ZIPs at the end, and avoid confusing unresolved template-variable runs.

## Plan
- [x] Inspect the pasted failure log and current batch helper/template flow.
- [x] Fix variable substitution for manual batch runs or add a clear guard for direct template execution.
- [x] Change orchestration so one ZIP failure records a result and continues.
- [x] Print and save a summary listing all failed ZIPs.
- [x] Validate with a dry-run or scoped script test where practical.
- [x] Run scoped code quality checks.

## Notes
- Direct `run_test.ps1` execution of the mission ZIP batch support template now fails before emulator work if `${...}` placeholders remain unresolved.
- `run_mission_zip_batch.ps1` records per-ZIP failures, keeps iterating, prints failed ZIP names with reasons, and writes `failed_zips.txt` under the batch output directory.
- Validated with a direct support-template preflight, PowerShell parser checks, and scoped `android/run-code-quality.ps1 -Fix`.

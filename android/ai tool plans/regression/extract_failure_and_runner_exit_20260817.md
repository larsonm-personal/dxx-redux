# Extraction failure and runner exit

## Goal

Fix the regression-data runner not returning to the prompt after a failed extraction run, diagnose the reported Descent Mac extraction failure, and implement and verify in-scope fixes.

## Plan

- [x] Inspect the active process tree, extraction summary, per-source logs, and runner exit paths
- [x] Reproduce and fix the prompt-return problem with a focused automated test
- [x] Diagnose and fix the Descent Mac extraction regression failure
- [x] Run focused tests and scoped code quality, then record results here

## Results

- The master runner no longer places long-lived stages behind a live `Tee-Object` pipe. It redirects each direct child's output to files, polls those files for live console output, and returns based only on the direct process exit
- A focused test starts a descendant that holds inherited output handles for 30 seconds and confirms the stage runner returns in under 10 seconds with the correct exit code and captured output
- The reported Descent Mac failure was a transient ADB daemon outage while copying its 719 MB BIN, wrapped inside app-private staging errors. The transport classifier now recognizes nested ADB timeout/failure messages so the suite performs its existing full device recovery and one complete-spec retry
- The exact failed Descent Mac source passed a focused end-to-end rerun in 1:58, reaching Lunar Outpost
- Extraction workflow, CD regression runner, and master regeneration runner tests pass
- All changed PowerShell files pass scoped code quality, parser checks, and `git diff --check`

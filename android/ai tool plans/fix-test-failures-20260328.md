# Fix Test Failures from report_20260328_092607

## Failures
1. test_mp (exit 1): orphaned matchmaking server port conflict (os error 10048)
2. test_extract (exit 1): empty log, script produced no output
3. test_saf_archiver (exit 1): descent2.ham missing on device after re-push

## Root Causes & Fixes

### test_mp: port conflict
- run_all_tests Tier 2 starts a matchmaking server on ports 9000/8080/9001
- test_mp also starts its own server
- test_mp only kills existing process on port 9000, missing 8080 and 9001
- Fix: expand port-kill to all 3 ports (9000, 8080, 9001) and also kill by process name

### test_extract: empty log
- With ErrorActionPreference='Stop', any unhandled error terminates silently (stderr only)
- The test ran for 28 seconds, suggesting it got past auto-discovery
- Possible causes: ADB command failure, PowerShell stdout buffering
- Fix: replace Write-Error + exit 1 with Write-Host + exit 1 throughout, ensuring output always goes to stdout. Also add global try/catch to capture unexpected failures

### test_saf_archiver: descent2.ham missing
- A prior test (likely test_extract) cleared file sets
- Resolve-GameDataDeps re-push reports "2 files pushed via deps" but stat still shows missing
- Possible: the pushed file has wrong case or wrong path
- Fix: add explicit verification after Resolve-GameDataDeps, add ls -la diagnostic output

## Plan
- [x] Create plan file
- [x] Fix test_mp: expand port-kill to all 3 server ports
- [x] Fix test_extract: improve error handling for empty-log resilience
- [x] Fix test_saf_archiver: improve file restoration diagnostics and robustness
- [x] Run linter on changed files

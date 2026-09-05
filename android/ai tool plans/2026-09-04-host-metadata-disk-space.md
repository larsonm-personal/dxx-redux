# Host metadata temporary disk use

- [x] Identify retained raw/staged payloads and disk-full diagnostic failures
- [x] Clean per-archive payloads before result publication and on failure, including crashed workers and CD staging
- [x] Keep optional worker log write failures from aborting the process pool
- [x] Test cleanup boundaries, failure recovery, and a real parallel metadata run
- [x] Run scoped code quality checks

Validation: `android/tests/test_host_metadata_workspace.ps1 -Integration` passed with Chromium and d1secret in parallel, a deliberately partially extracted ZIP using the actual failure/finally handler, and an injected disk-full log write

Removed only raw/staged payloads from the failed `20260904_210503` worker run, retaining reports and logs

Scoped code quality and diff checks passed; PowerShell-only changes required no native rebuild

# Test failures and hash-ring runtime sampling

## Goals

- Diagnose and fix the three failures in report `report_20260818_103106.md`
- Replace random 45-minute sampling with resumable hash-ordered batches in both aggregate runners
- Select through the first target whose predicted cumulative runtime reaches or exceeds 45 minutes
- Resume after the last previously selected target, with a reported random fallback when prior state cannot be recovered

## Plan

- [completed] Inspect the report, failure logs, current samplers, runtime history, and existing state files
- [completed] Diagnose and fix the three failed tests with focused reruns
- [completed] Design shared hash-ring ordering and previous-target recovery behavior
- [completed] Implement hash-ring batches for test and regression-data runners
- [completed] Add or update sampler integration tests
- [completed] Run focused tests, scoped quality checks, and required build validation

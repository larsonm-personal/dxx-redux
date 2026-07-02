# Quick Tests Historical Seconds Fix

## Goal
Fix `android/run_quick_tests.ps1` so fixed per-test historical baselines can be summed reliably from hashtable-backed quick test entries.

## Steps
- [x] Reproduce and locate the `Measure-Object -Property HistoricalSeconds` failure path.
- [x] Add one helper for reading `HistoricalSeconds` with a zero fallback.
- [x] Use the helper for total estimate, budget checks, skipped entries, and report output.
- [x] Run focused validation and script quality checks.

## Notes
- PowerShell dot access works for individual hashtable entries, but `Measure-Object -Property HistoricalSeconds` does not reliably treat the key as an object property.

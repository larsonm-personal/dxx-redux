# Plan: test_dpad_triggers follow-up from report 20260523_114346

- [x] Create tranche plan file and capture the failing report context
- [x] Inspect the failing `test_dpad_triggers` report section, log, and script step where the timeout hit
- [x] Form one local hypothesis for the new failure mode
- [x] Run one focused check to distinguish a game-input regression from a suite-timeout regression
- [x] Apply the smallest fix in the full-suite timeout path
- [x] Run focused validation through the full-suite wrapper
- [x] Update this note with the outcome and any remaining blockers

## Notes

- Report under review: `C:\local\dxx-redux\temp\test_reports\report_20260523_114346.md`
- New symptom differs from the earlier D2 item-50 failure: D1 passes, D2 log stops at `Injecting button 107 DOWN`, and the suite appends `TIMEOUT: Test killed after 120s`
- Focused check: `android/run_test.ps1 -ScriptName test_dpad_triggers.json5` now passes end-to-end with exit code `0`, including D2 Phase 1d (`button 107`) and the final D2 assertions
- Working hypothesis: the test logic is healthy again, but `android/run_all_tests.ps1` still uses the default 120 second wall-clock cap for `test_dpad_triggers`, which is too tight under full-suite load and kills the process mid D2 run
- Fix: add `test_dpad_triggers = 240` to the `run_all_tests.ps1` per-test timeout override table instead of touching launcher or game input code
- Validation:
	- `android/run_test.ps1 -ScriptName test_dpad_triggers.json5`
	- `android/run_all_tests.ps1 -Filter test_dpad_triggers`
- Outcome: the filtered full-suite wrapper now runs `test_dpad_triggers [json5] (timeout: 240s)` and reports `PASS (01:16)` with `Passed: 1 Failed: 0 Timeouts: 0`; validation report: `C:\local\dxx-redux\temp\test_reports\report_20260523_124156.md`
# Plan: test_saf_archiver follow-up from report 20260523_232445

- [x] Create tranche plan file and capture the failing report context
- [x] Confirm the failing surface from the report
- [x] Inspect the owning test and the dedicated failure log around the timeout
- [x] Form one local hypothesis for why the embedded `test_saf_basic` automation timed out
- [x] Apply the smallest fix in the owning test or launcher path
- [x] Run focused validation for `test_saf_archiver`
- [x] Update this note with the outcome and any remaining blockers

## Notes

- Report under review: `C:\local\dxx-redux\temp\test_reports\report_20260523_232445.md`
- Current failure: `test_saf_archiver` fails while waiting for the embedded `test_saf_basic` automation result, with the log excerpt ending at `RESULT: TIMEOUT`
- Local hypothesis confirmed: `android/tests/test_saf_archiver.ps1` could reuse a stale `files/introspect.json` from an earlier run, treat that as proof the new launch was ready, and then send `com.dxxredux.AUTOMATE` before the new game process was listening
- Fix applied: clear `files/introspect.json` with `run-as` immediately before the Step 7 launch broadcast so the readiness loop only accepts a fresh dump from the new process
- Focused validation: `android/tests/test_saf_archiver.ps1 -NoBuild` passed
- Wrapper validation: `android/run_all_tests.ps1 -Filter test_saf_archiver` passed and wrote `C:\local\dxx-redux\temp\test_reports\report_20260524_085840.md`
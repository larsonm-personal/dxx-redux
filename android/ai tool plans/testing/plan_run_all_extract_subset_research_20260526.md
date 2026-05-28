# run_all_tests extraction subset research

## goal
- Determine whether run_all_tests.ps1 was intended to run a random subset of CD extraction tests instead of the full extraction suite
- Identify where that behavior was introduced, changed, or lost
- Recommend a path to restore the intended behavior without weakening direct extraction-suite coverage

## steps
- [x] Inspect current run_all_tests.ps1 and extraction test scripts
- [x] Search git history for run_all_tests/test_all_extracts subset selection changes
- [x] Compare current behavior against the original intent and prior implementation
- [x] Summarize findings and restoration options

## findings
- `test_all_extracts.ps1` currently still contains the intended default: without `-All`, `-SpecPaths`, or `-Filter`, it selects one spec with `Get-Random`
- That default was introduced by commit `60d6f3de` on 2026-03-28, changing `test_all_extracts.ps1` from all-specs-by-default to one-random-spec-by-default plus explicit `-All`
- `run_all_tests.ps1` originally called ps1 tests without extra arguments, so after `60d6f3de` it would have used the one-random-spec default
- Commit `a5b6dcd3` on 2026-05-24 added `Arguments = @("-All")` for `test_all_extracts` and increased its timeout from 300s to 7200s while adding more full coverage into the unified runner
- The adjacent plan `plan_test_centralization_survey_20260525.md` explicitly records this as "Made run_all_tests.ps1 pass -All to test_all_extracts.ps1"
- Later reports show the consequence: full `test_all_extracts.ps1 -All` can fill emulator storage or hang on large imports, which matches the original reason to keep run-all cheaper

## restoration options
- Minimal restoration: remove the special `-All` argument from `run_all_tests.ps1` and reduce the timeout back toward the smoke-test range
- Better restoration: add explicit subset controls to `test_all_extracts.ps1`, such as `-SampleCount`, `-Seed`, and `-RandomOrder`, then have `run_all_tests.ps1` pass a small sample count and a deterministic seed derived from `git rev-parse HEAD`
- Preserve full coverage by keeping direct `android/tests/test_all_extracts.ps1 -All` as the full extraction-suite command, or by adding an explicit `run_all_tests.ps1 -FullExtracts` switch for scheduled/manual deep runs

## implementation
- [x] Added `-SampleCount`, `-Seed`, and `-RandomOrder` to `test_all_extracts.ps1`
- [x] Changed `run_all_tests.ps1` to pass `-SampleCount <ExtractSampleCount> -Seed <git HEAD derived seed>` to `test_all_extracts.ps1` by default
- [x] Added `run_all_tests.ps1 -ExtractSampleCount` and scaled the default extraction timeout down unless `-FullExtracts` is requested
- [x] Added `run_all_tests.ps1 -FullExtracts` to preserve an explicit full-corpus run path from the unified runner
- [x] Validate PowerShell syntax and focused runner behavior
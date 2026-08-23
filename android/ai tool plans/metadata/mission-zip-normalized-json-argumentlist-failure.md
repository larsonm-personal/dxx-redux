# Mission ZIP normalized JSON ArgumentList failure

## Plan

- [x] Inspect the failing normalization helper and its callers
- [x] Reproduce the strict-mode `ArgumentList` property failure with a focused test
- [x] Fix the process invocation or result handling and add regression coverage
- [x] Run focused metadata tests and scoped code quality checks
- [x] Record findings and verification results

## Findings

- The selected regression stage was launched with Windows PowerShell 5.1
- Its .NET Framework `ProcessStartInfo` has neither `ArgumentList` nor `StandardInputEncoding`
- The emulator and host metadata scripts contained separate copies of the incompatible formatter launcher
- Process argument setup is now shared by the JSON formatter and host metadata analyzer, selecting `ArgumentList` on PowerShell 7 or a correctly quoted `Arguments` string on Windows PowerShell
- Standard input encoding and process-tree termination use runtime-compatible fallbacks

## Verification

- Mission metadata normalization test passed under PowerShell 7.6.5
- Mission metadata normalization test passed under Windows PowerShell 5.1
- Focused `ewithin-versions.zip` metadata batch passed under Windows PowerShell with one expected skip and zero failures
- Focused Destination Saturn host analyzer batch passed under Windows PowerShell with one pass and zero failures
- The focused batch wrote valid normalized `summary.json` and failure metadata JSON
- Scoped code quality checks passed
- `git diff --check` passed

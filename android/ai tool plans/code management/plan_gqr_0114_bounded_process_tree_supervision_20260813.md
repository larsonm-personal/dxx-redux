# GQR-0114 Bounded Process Tree Supervision Plan

## Scope

- Remediate `GQF-0127` in branch-added bounded extraction scripts and tests
- Retain complete ownership of extractor descendants through every terminal path
- Keep inherited `d1/` and `d2/` files unchanged

## Supervision policy

- Put every POSIX extractor tree in one private process group
- Put every Windows extractor tree in one private kill-on-close Job Object before the extractor command starts
- Fail closed if private process-tree ownership cannot be established
- Terminate and reap the complete owned tree after success, failure, cancellation, or timeout
- Never discover or terminate processes by executable name or host-wide enumeration

## Work phases

- [x] Implement cross-platform process-tree ownership in `run_bounded_extractor.py`
- [x] Add descendant survival and unrelated sentinel regressions
- [x] Run direct Python and PowerShell wrapper tests
- [x] Run scoped code quality and bounded extraction regression
- [x] Record validation and diff metrics

## Validation target

- Descendants cannot outlive successful, failing, timed-out, or cancelled extractor parents
- PID reuse cannot redirect cleanup outside the retained process group or Job Object identity
- Unrelated sentinel processes survive every cleanup path
- Ownership setup failures stop the admitted root and reject extraction
- Existing diagnostic, output, file-count, byte-count, and timeout limits remain effective

## Completed implementation

- POSIX extractors start in an attempt-owned process group and receive group teardown on every terminal path
- Windows extractors start behind a one-byte gate, enter a private kill-on-close Job Object before the command starts, and wait for zero active job processes during teardown
- SIGTERM is an ordinary supervised cancellation on POSIX; abrupt Windows supervisor death closes the Job Object and terminates its tree
- Ownership setup and cleanup failures reject the attempt instead of falling back to PID discovery or name-wide termination

## Completed validation

- Repository-pinned Python 3.12.8 ran all 11 supervisor tests on Windows
- WSL Python 3.12.3 ran the same 11 tests through the POSIX process-group path
- Descendant fixtures passed for successful, failing, timed-out, and externally cancelled parents
- Every descendant test proved an unrelated sentinel process survived
- Ownership failure injection proved the extractor command never starts
- `test_bounded_python_runtime.ps1` passed all runtime identity and wrapper cases
- `test_bounded_extraction.ps1` passed all synthetic archive rejection cases and four verified installer packages
- Scoped code quality, printable ASCII, no-BOM checks, and `git diff --check` passed

## Diff metrics

- `android/helpers/run_bounded_extractor.py`: 212 insertions, 35 deletions
- `android/tests/test_run_bounded_extractor.py`: 109 insertions
- Inherited `d1/` and `d2/` changes: zero

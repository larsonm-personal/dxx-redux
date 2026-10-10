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

## Diagnosis reopening, 2026-10-09

The checked implementation and validation above remain historical evidence. GQF0127/GQR0114 reopened OPEN/TODO for an uncovered POSIX setup cancellation interval. In run_bounded_extractor.py528-547 the live owner is acquired and diagnostic thread started before the cleanup try548. SIGTERM handler611-612 raises ProcessCancelled; main625-630 returns130 and restores its handler without terminating the owned child when this earlier interval is interrupted. No survival fixture was executed. Complete source and maintained fixture review supports the control-flow diagnosis, not a fresh runtime outcome.

Imported GQR-0114 process owner setup cancellation diagnosis reopening 20261009 SHA25629b411a7c8d03ac38276dd0de0758299540cdb7a27ecaefe8c0db626359fc8b2 records current identities, language-contract reference and acceptance scope. Preserve previous private group/gated job, sentinel and terminal-path controls. Existing test waits for parent-ready; ownership-unavailable mock fails before acquisition. Neither controls this setup interval. Do not infer a Windows job-close failure from the POSIX gap.

- [ ] Define cancellation-safe child creation, owner handoff, diagnostic setup and teardown; moving try after owner assignment alone leaves an acquisition gap
- [ ] Prefer nonthrowing cancellation request with finite wakeup and explicit observation; inspect SIGINT/SIGTERM and repeated cancellation during cleanup, restoring prior signal policy
- [ ] Teardown partially initialized owner/thread/pipe states, terminate descendants and reap the root without assuming diagnostic startup succeeded
- [ ] Add a controlled actual POSIX main setup cancellation barrier and ordinary post-ownership setup failure; assert finite exit, root/descendant cleanup and unrelated sentinel survival
- [ ] Retain and run supported Windows/POSIX terminal-path, ownership-gate, output and wrapper controls during implementation; record fresh results separately from historical passes

No code/test/script edit or execution in this diagnosis tranche. Deferred malformed-media/security/allocation/resource-pressure probes remain deferred. This repair requires no inherited D1/D2 changes.

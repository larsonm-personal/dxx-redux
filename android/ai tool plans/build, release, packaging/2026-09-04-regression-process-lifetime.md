# Regression process ownership

- [x] Stop the verified orphaned simulation runner and its children
- [x] Bind regeneration subprocess trees to host lifetime on Windows
- [x] Clean up active pool workers on exceptions and cancellation
- [x] Test abrupt owner termination, descendant cleanup, and normal parallel runs

## Cause and repair

- PID 33508 was a detached GuideBot regeneration PowerShell runner whose parent PID 10744 no longer existed; stopped that verified tree, including eight route workers and their descendants
- The master sidecar used CreateNoWindow and the scripts disposed process handles without owning descendant lifetime, so a closed console could leave the sidecar or worker scheduler alive
- Added a non-inheritable, unnamed Windows kill-on-close job held for the owning PowerShell process lifetime; enroll the owner before launching children so job membership is inherited without a start/assignment race
- Master menu, stage sidecar, direct GuideBot runner, and direct host metadata runner initialize lifetime ownership before subprocess work
- Pool and master-stage finally blocks stop active child trees on script errors/cancellation; pool callback failures now unwind rather than being misclassified as process startup failures
- Worker count, queue size, parallel retirement, and incremental result persistence are unchanged
- Windows job semantics: https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects

## Verification

- New process-lifetime integration test kills only the owner (not its tree), then verifies both child and grandchild stop, for the pool and the master stage runner
- Callback-error test verifies child/grandchild cleanup while the owner process is still alive
- These tests passed on PowerShell 7 and Windows PowerShell 5.1; the C# interop helper compiles through Add-Type in both runtimes
- Existing headless-pool and master-regeneration integration tests passed, including parallelism, timeout, output and exit-code behavior
- Scoped code quality and all 45 existing D2 CTest tests passed; no engine source changes or simulation regeneration were needed
- Verified no remaining orphaned GuideBot runner or headless engine processes after stopping the original tree
- The texture-conversion failure is not addressed by this process-lifetime repair

# Public replay admission repair, November 14, 2026

## Scope and evidence

PR 504 head 510124dfc3f93a62b7b9856af50ed4b960e570ef failed the
four-minute compact public replay admission step in run 43649849268, job 132622648156. The final catalog finished immediately before the job timeout;
the native replay owner never started. Passing individual commands does not
make this timed-out step pass. The full failure log is retained at
temp/xl-readiness-tranche2-20261114/pr504-native-replay-failure-02.log.

Profiling the complete comparator suite identified repeated validation of
homogeneous integer arrays as its largest Python cost. The repair checks every
leaf with its exact native type and range while selecting the homogeneous
schema once per array. Invalid input falls through to the existing indexed
diagnostic. Mixed schemas and non-unit ranges keep their existing path. There
is no cached validation result; mutations are checked again on every call.

The workflow, sequential command order, four-minute deadline, fixtures,
capacities, assertions and catalog membership remain unchanged. Native engine
code and admission guards are outside this checkpoint.

## Validation and publication plan

Run scoped formatting, the full comparator suite on Windows, and all fifteen
commands extracted from the exact public workflow sequentially on Linux under
a 240-second timeout. Preserve failures and record timing and source hashes.
Publish only the tested repair as an immediate-parent child of PR 504 in
larsonm-personal/dxx-redux, linked to stack 471, then inspect its own CI.

Windows comparator: PASS 86 tests in 61.550 seconds. This includes mutation,
first/middle/last index, exact type, signed/64-bit range, non-unit range, mixed
schema and array shape controls. Scoped formatting PASS with pinned Ruff
0.16.10. Initial isolated-worktree formatter attempts failed due to absent
tools; the pinned Ruff installation and successful fourth attempt are retained.
The focused homogeneous-range benchmark checks 400,000 leaves twenty times:
parent 0.634 seconds, candidate 0.339 seconds. This is a diagnostic measurement,
not a replacement for the full admission owner.

Linux attempt 01 failed because the linked Windows worktree's absolute Git
pointer could not resolve in WSL (86 tests, one orchestration error). The pointer
now uses a relative path verified by Git on both platforms. Attempt 02 passed
all 86 comparator tests but timed out at 240 seconds during the later process
controls. A live process inspection found Git diff spending roughly two minutes
on the Windows-mounted checkout. Both failed receipts remain preserved.
Attempt 03 uses a Linux-filesystem checkout of the same parent with the exact
two-file patch, all fifteen original commands, and the same 240-second limit.
Attempt 03 passed the comparator (86 tests, 41.336 seconds) and process failure
integration, then failed because this fresh checkout lacked dependency_base.txt.
The public workflow explicitly prepares that file before admission. Attempt 04
reproduced that setup and PASSed all fifteen commands in 162.679 seconds under
the unchanged 240-second limit, including 99 standalone JSON, 315 standalone
PowerShell, 547 support scripts and 428 master entries. Source hashes, exact
patch and all attempt logs are retained beside ci-admission-linux-04.receipt.json.
Hosted child qualification is still pending; local timing does not prove CI.

The user now prioritizes useful gameplay progress without a specific 8/8
endpoint. The unchanged sampled readiness result remains separately 4/8;
missing external robot definitions and the camera dependency remain open.
Full G2 remains open, including corrupt-trailer rejection changing energy
91 to 100. This checkpoint makes no gameplay completion claim. Native trace
performance work remains unpublished in the main worktree and is not included
in this isolated CI repair.

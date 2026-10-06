# Disk space management

## Request

Pause the overnight tests, improve build/test disk management, and recover approximately 200 GB of free disk space

## Plan

1. Stop only the overnight runner and preserve its partial test evidence
2. Measure disk usage and preview existing cleanup to identify disposable artifacts and policy gaps
3. Improve cleanup and producer behavior based on measured causes, preserving source assets, active work, links, and current evidence
4. Validate changes with the cleanup integration tests and scoped code quality
5. Apply cleanup, verify actual volume free space, and document the resulting policy and usage

## Progress

- Overnight goal paused at user request; owned runner stopped at 22:41 PDT
- Partial suite log preserved in `overnight-tests-20261005.partial.txt`
- Before cleanup, C: has approximately 9 GiB free
- Inventory found 82.53 GiB under `temp`, 91.54 GiB under `android/temp`, and 10.23 GiB of Cargo incremental state
- Original cleanup preview could reclaim only 90.12 GiB because a custom-named emulator directory lock pinned the entire Android scratch tree
- Cleanup now recognizes known stale emulator markers through `config.ini`, isolates held/unknown locks to their owning workspace, and removes Cargo incremental state while retaining warm binaries/dependencies
- Added integration coverage for custom emulator paths, locked siblings, Git-visible files inside locked owners, and incremental-cache preservation under a held lock
- Cleanup integration tests passed in PowerShell 7 and Windows PowerShell 5.1; fixed the test fixture's read-only Git-object timestamp handling and junction deletion for 5.1
- Scoped code quality and `git diff --check` passed
- Real cleanup started from 10,031,931,392 available bytes (9.34 GiB); revised scan found 187.48 GiB eligible
- Cleanup completed with exit 0, removing 7,477 artifacts totaling 187.48 GiB of logical file sizes
- Actual available space after the main cleanup: 209,272,254,464 bytes, or 209.27 GB (194.90 GiB)
- Actual space recovered from the starting measurement: 199.24 GB (185.56 GiB)
- No tracked files were deleted; original game data, emulator source data, and recorded demos retain their pre-cleanup file counts and aggregate sizes
- Nested repositories, links, and the held Gradle lock remained protected; no deletion errors or changed-file conflicts were reported
- Post-cleanup formatting exposed installed Ruff and the saved Node path living under scratch; moved both to ignored `android/tools/code-quality` storage and documented the persistent dependency location
- Restored Node 24.21.0 from its official SHA-256-verified Windows archive under the ignored formatter tools directory, then reinstalled pinned Ruff/Prettier with the existing installer
- Expanded cleanup tests verify ignored formatter files survive cleanup; both PowerShell versions and formatter file-selection/dispatch tests passed again
- A second real temporary cleanup removed the installer download and scratch state; Ruff still ran afterward, and the complete final scoped code-quality invocation passed without restoring temporary tool files
- Final available space after restoring persistent formatter dependencies: 209.13 GB (194.77 GiB)
- The overnight goal remains paused; resumption will require rebuilding any discarded generated outputs and starting the remaining tests/regeneration

# Interactive workspace cleanup

- [x] Inspect existing cleanup policies and generated artifact locations
- [x] Implement an interactive cleanup helper with preview mode, age thresholds, and protected source paths
- [x] Test automatic deletion, prompts, tracked files, recent files, and path/link safety in a temporary fixture
- [x] Run scoped code quality checks and a read-only scan of this workspace

SDK 36 work is paused while this helper is developed

## Implementation and validation

- Entry point: android/clean-workspace.ps1; usage and boundaries: android/CLEANUP.md
- Automatically removes ignored loose log/tmp/temp files in known scratch locations after 7 days
- Prompts for other generated artifacts after 30 days; folder groups require a second confirmation of the listed candidates
- Preview/WhatIf and AutoOnly modes; protects Git-visible work, recent descendants, links, nested repositories, held locks, and active builds/tests
- Rechecks Git state and tree metadata after confirmation, preserving newly changed or staged work
- Existing timestamp-generation cleanup helper remains unchanged
- Synthetic Git fixture test passed, including real held file locks, junctions, default decline, noToAll, quit, folder approval, and changes during prompts
- Scoped run-code-quality.ps1 passed
- run-windows-build.ps1 passed for D1 and D2; D2 CTest passed 45/45
- Read-only workspace scan completed: 426 automatic candidates (about 0.20 GiB) and 1848 review candidates (about 8.51 GiB), using rounded preview sizes
- No real workspace artifacts were deleted; tests delete only their own synthetic fixtures
- External dependency/SDK installations and global caches are deliberately outside this helper's scope

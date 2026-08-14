# GQR-0105 Nested Music Catalog Budget Plan

## Goal

Close `GQF-0118` by carrying one attempt-owned catalog budget through the outer mission archive and every nested music container, so entry, work, and retained-memory limits cannot multiply across containers

## Constraints

- Keep changes in branch-added Android code
- Reuse the existing extraction and catalog policy instead of introducing unrelated limits
- Preserve cancellation, deterministic cleanup, and independent sequential attempts
- Do not edit the canonical quality ledger or parent campaign plan

## Work

- [x] Map outer and nested catalog enumeration, retained objects, and current budget resets
- [x] Add one shared attempt-owned catalog budget covering entries, parsing work, and retained catalog memory
- [x] Thread the budget through every nested DXA/HOG and ordinary archive source path
- [x] Add exact-limit and one-over tests across outer/inner combinations, retained memory/work, cancellation, cleanup, sequential attempts, and ordinary fixtures
- [x] Run focused JVM tests, scoped code quality, and Android compilation where feasible
- [x] Record final changed paths, validation, and remaining limitations here

## Results

Added `MissionMusicCatalogBudget.kt`, threaded one attempt budget through file-backed and extracted outer archives, DXA members, nested HOG members, song references, tracks, sources, and the final catalog in `MissionZipMusic.kt`, and added `MissionMusicCatalogBudgetTest.kt`

The catalog budget reuses `ExtractionBudget` for the 4,096 work-entry and retained-record ceilings and the 64 MiB metadata ceiling. Retained records include conservative object and UTF-8-derived string storage. Budget rejection and thread interruption fail the complete optional catalog closed, while ordinary malformed optional containers keep their prior skip behavior

Focused validation passed:

- `MissionMusicCatalogBudgetTest`: 8 tests, zero failures, covering exact and one-over outer work, combined outer plus inner work, DXA-to-HOG work, retained count and bytes, cancellation with no archive mutation, extracted containers, and independent sequential attempts
- `MissionZipMusicTest`: 13 tests, zero failures, preserving ordinary ZIP, 7z, DXA, HOG, song-list, malformed optional-container, and no-music behavior
- `:app:testDebugUnitTest` compiled debug Kotlin and unit-test Kotlin and completed successfully in 16 seconds
- Scoped `android/run-code-quality.ps1 -Fix` passed for all four owned paths
- Repository `git diff --check` passed and no `d1/` or `d2/` path was changed

Owned production integration is 74 added and 7 removed lines in `MissionZipMusic.kt`, including concurrent bounded-read call-site arguments retained from `GQR-0091`. New files contain 119 production lines, 179 test lines, and this 37-line durable plan. No native source changed, so focused JVM compilation covered the changed implementation

# Large ZIP metadata resume rescan and nested error plan

## Goal
Fix two launcher metadata-viewer problems for oversized mission ZIPs that are durably extracted:

- returning to the app from minimized state should not visibly rescan the same metadata twice
- opening level metadata through a nested `.hog` or `.mn2` constituent link should use the durable extracted files and should not report `zip entry is too large`

## Current hypotheses
- The duplicate rescan is likely caused by a lifecycle refresh plus an active metadata dialog refresh both invalidating the same state on resume.
- The stale nested-entry error likely means top-level mission ZIP metadata resolves through `MissionZipExtractionStore`, while constituent metadata still builds a ZIP target from the original owner archive entry.

## Work phases
1. [x] Trace metadata dialog state and app resume refresh paths.
2. [x] Trace top-level versus constituent `LevelMetadataTarget` creation for extracted mission ZIPs.
3. [x] Add a focused fix so resume coalesces metadata refreshes and constituent links resolve to extracted files when available.
4. [x] Add or extend unit tests for the oversized extracted ZIP constituent path and any refresh policy helper.
5. [x] Run scoped formatting and relevant unit tests.

## Implementation notes
- `ModsSection` no longer keys the open-details reload effect on the whole `mods` list. Resume still refreshes details through `refreshTrigger`, but the subsequent mod-list state update no longer triggers a second details/metadata rebuild.
- `MissionZipExtractionStore` now resolves a durable extracted file from an owner ZIP entry path and can find same-stem extracted siblings such as `.mn2` for `.hog` and `.hog` for `.mn2`.
- `LevelMetadataTargets.zipConstituent()` now prefers durable extracted mission files for oversized mission ZIPs. `.hog` and `.mn2` constituent links build `mission_files` targets rooted at `.extracted_mission_zips/<owner>` instead of staging the original ZIP entry.
- The constituent detail dialog now prefers local extracted metadata summaries when a durable extraction exists, falling back to the owner ZIP for small or non-extracted archives.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\java\com\dxxredux\app\MissionZipExtractionStore.kt','android\app\src\main\java\com\dxxredux\app\LevelMetadata.kt','android\app\src\main\java\com\dxxredux\app\SetupSections.kt','android\app\src\test\java\com\dxxredux\app\ModManagerMissionZipTest.kt')` passed.
- From `android\`: `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ModManagerMissionZipTest --tests com.dxxredux.app.LevelMetadataTargetsTest` passed.

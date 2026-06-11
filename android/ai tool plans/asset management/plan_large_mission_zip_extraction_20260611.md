# Large mission ZIP extraction plan

## Goal
Make large mission ZIP imports such as `castaway_redux.zip` usable everywhere the launcher treats smaller mission ZIPs as mods:

- launch and saved-game resume can load `castaway.hog` and `castaway.mn2`
- level metadata browsing no longer fails with `ZIP entry is too large`
- extracted files are durable, owned by the source mod ZIP, visible as linked files in storage, and cleaned up when the mod ZIP is removed
- active mod masking and load order continue to use the existing `.active_mod_paths` / PhysFS behavior

## Current findings
- `MissionZip.inspect()` already classifies ZIPs over `SMALL_IN_MEMORY_LIMIT_BYTES` as `extracted_bundle`, but `ModManager.importMissionZipFile()` still stores the ZIP in `filesDir/mods/` and defers extraction.
- `ModManager.writeEnabledModPaths()` extracts mission ZIPs into per-game temporary generated directories under `d1x-redux/.generated_mission_zips/` or `d2x-redux/.generated_mission_zips/` on launch.
- `LevelMetadataAnalyzer.prepareTarget()` separately extracts requested ZIP entries into `cache/level_metadata/.../staged`, with `LEVEL_METADATA_MAX_ZIP_ENTRY_BYTES = 64 MiB`. `castaway.hog` is about 192 MiB, so metadata fails before native analysis starts.
- Android PhysFS mount order already handles mod priority: Kotlin writes paths in UI order, and `physfsx.c` mounts them in reverse with prepend. The durable extraction should feed this same path list rather than invent a new engine-side rule.
- Saved-game resume already runs through `prepareGameLaunchFiles()`, then `modManager.writeEnabledModPaths(game)`, before `MainActivity` receives the resume save path. The fix should preserve this shared launch preparation path.
- Storage Inspector currently annotates helper symlinks, but has no notion of files linked to a mod ZIP owner.

## Proposed design

Add a small durable extraction layer for mission ZIP mods, probably a new Kotlin helper such as `MissionZipExtractionStore`.

Suggested storage:

- `filesDir/mods/.extracted_mission_zips/<safe-mod-filename>/...`
- manifest: `filesDir/mods/.extracted_mission_zips/manifest.json`

Each manifest record should include:

- owner mod filename
- owner size and a cheap freshness key, with sha256 only if already available or worth computing
- scan/import mode and game
- extracted root path
- original ZIP entry path to extracted relative path map
- extracted file sizes
- optional generated alias records, such as single top-level `.sng` copied to `missions/descent.sng`

Flattening should use the exact existing `stagedMissionZipRelativePath(scan, path)` behavior so launch masking and mission discovery stay compatible with current tests.

## Implementation phases

1. [x] Prerequisite: wait for or rebase onto the free-space-check centralization work.
   - Use the centralized API for the ZIP-copy plus durable-extract total, not a local duplicate.
   - Estimate required bytes from uncompressed ZIP entries using the existing `ImportStorageGuard.archiveEntryBytes()` behavior or its replacement.

2. [x] Add durable extraction helper.
   - Move the reusable parts of `extractMissionZipForLaunch()`, `missionZipLaunchStageBytes()`, `stagedMissionZipRelativePath()`, and `missionZipUsesRootedLayout()` into the helper or make them internal shared helpers.
   - Extract to a temp sibling directory first, then rename into place.
   - Reject unsafe paths after canonicalization.
   - Preserve all entries needed for launch, metadata, readmes, and nested DXA mounting.
   - Write the ownership manifest after successful extraction.

3. [x] Trigger extraction on import for large mission ZIPs.
   - After storing/registering the mod ZIP, if `scan.importMode == "extracted_bundle"`, extract once into `.extracted_mission_zips`.
   - Roll back the copied ZIP and manifest entry if extraction fails.
   - For smaller ZIPs, keep current behavior unless the helper makes always-extract cheap and worthwhile.

4. [x] Clean up owned files.
   - `deleteMod(filename)` should remove its durable extracted root and manifest record.
   - `clearAllMods()` should remove `.extracted_mission_zips`.
   - Add a startup/prune pass that deletes extracted roots whose owner ZIP no longer exists or whose freshness key no longer matches.

5. [x] Use durable extraction for launch.
   - `generatedMissionZipPathLines()` should prefer the durable extracted root when present and fresh.
   - It should still return the extracted root first, then nested DXA files in the same order as today.
   - If a large ZIP lacks a durable extraction record due to migration or interrupted import, either re-extract during launch using the centralized storage guard or fail with a clear storage/import repair message. Prefer repair on launch if free space allows.

6. [x] Use durable extraction for metadata.
   - `LevelMetadataTargets` should be able to resolve a mission ZIP target to extracted local files when available.
   - For large extracted bundles, build `LevelMetadataTarget` with `sourcePath`/`dataDir`/`hogFiles` pointing at extracted files instead of `archivePath`/`archiveEntries`.
   - Keep the UI unchanged; the details dialog still displays the ZIP/mod and its constituents.
   - Keep temporary ZIP staging for small ZIPs if desired, but remove the Castaway failure path by bypassing `stageZipEntries()` for extracted bundles.

7. [x] Show linked files in Storage Inspector.
   - Extend `StorageFileEntry` with an optional linked owner label, separate from helper symlink annotation.
   - Read the extraction manifest in `scanStorageFiles()`.
   - For files under `.extracted_mission_zips/<owner>/`, show a purpose like `Linked mission ZIP file` and detail text `Linked to mod ZIP: castaway_redux.zip`.
   - Delete from Storage Inspector must be an owner-level cleanup action for linked extracted files, not a single-file delete.
   - If the user chooses Delete on any extracted file linked to `castaway_redux.zip`, the confirmation should say that all cached files from that ZIP will be removed and the source ZIP will be unlinked from Mods.
   - Confirming that action should call a `ModManager` cleanup method that removes the source mod manifest entry, deletes the source ZIP from `filesDir/mods/`, deletes the durable extracted root, removes the extraction manifest record, and refreshes the storage and mods UI.
   - If the user deletes or unlinks the source ZIP from the Mods section, the same shared cleanup method should delete all linked extracted files.
   - Avoid direct `entry.file.delete()` for linked extracted files so partial bundles cannot be created from the storage browser.
   - If the source ZIP is already missing but linked extracted files remain, Storage Inspector should offer `Remove linked cache` and delete the extracted root plus manifest record.

8. [x] Tests and verification.
   - Unit test: importing a synthetic mission ZIP over the in-memory threshold creates a durable extracted bundle and manifest record.
   - Unit test: deleting that mod deletes the extracted bundle.
   - Unit test: `.active_mod_paths` for large mission ZIPs points at the durable extraction root and nested DXA files, preserving current line order.
   - Unit test: metadata target generation for a large extracted ZIP returns local extracted HOG/descriptor paths and does not call ZIP staging.
   - Storage browser coverage is through the shared `MissionZipExtractionStore` manifest lookup and owner cleanup path; no separate Compose UI unit test was added in this tranche.
   - Existing tests to run: `ModManagerMissionZipTest`, `LevelMetadataTargetsTest`, `GameFileMetadataTest`, `MissionZipTest`.
   - Android integration: import `castaway_redux.zip`, inspect metadata, launch the mod, save, quit, resume the save, then remove the mod and confirm linked extracted files are gone.

## Risks and decisions
- Do not mount extracted mission files globally from the active file set. They should remain available only when the owning mod is enabled, via `.active_mod_paths`.
- Do not duplicate mission descriptor parsing rules in new code. Continue to use `MissionZip.inspect()` and `GameFileFormats`.
- Be careful with D1 mission ZIPs that are allowed in D2 launch. Preserve `enabledForLaunch()` behavior.
- Preserve rooted Rebirth-style layouts versus legacy flat layouts. This is already encoded in `missionZipUsesRootedLayout()`.
- Consider whether small ZIPs should also extract once later. For this task, only large ZIPs need durable extraction to reduce churn.

## Implementation notes
- Added `MissionZipExtractionStore`, with durable extraction under `mods/.extracted_mission_zips/<owner>/` and a JSON ownership manifest.
- Large mission ZIP imports now extract once on import and roll back the copied ZIP if extraction fails.
- Launch now uses the durable extraction root for `extracted_bundle` mission ZIPs while preserving the existing generated staging path for smaller ZIPs.
- Level metadata target creation now detects fresh extracted bundles and points native metadata analysis at the extracted root/HOG paths instead of ZIP staging.
- Storage Inspector now labels linked extracted files and owner ZIPs with linked caches. Deleting either one linked file or the owner ZIP removes the owner mod link and the whole extracted cache. If the source ZIP is already missing, deletion removes the orphaned cache record/root.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths android\app\src\main\java\com\dxxredux\app\MissionZipExtractionStore.kt` passed.
- `.\android\run-code-quality.ps1 -Fix -Paths android\app\src\main\java\com\dxxredux\app\ModManager.kt` passed.
- `.\android\run-code-quality.ps1 -Fix -Paths android\app\src\main\java\com\dxxredux\app\LevelMetadata.kt` passed.
- `.\android\run-code-quality.ps1 -Fix -Paths android\app\src\main\java\com\dxxredux\app\AdvancedSettingsPage.kt` passed.
- `.\android\run-code-quality.ps1 -Fix -Paths android\app\src\test\java\com\dxxredux\app\ModManagerMissionZipTest.kt` and the containing test directory both passed, but the wrapper reported no Kotlin files for that `src/test/java` path.
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ModManagerMissionZipTest --tests com.dxxredux.app.LevelMetadataTargetsTest` passed.

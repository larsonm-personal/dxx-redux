# Large mission archive extraction design

## Goal
- Design a durable extracted-file path for mission ZIP/7z/RAR archives larger than the initial 10 MB threshold

## Current Behavior
- Mission packs are imported as mod records with `kind = mission_zip`
- `MissionZip.inspect(file)` opens the archive and returns a `ScanResult`
- `ScanResult.importMode` currently chooses:
  - `stored_zip` only for ZIP archives below the large in-memory limit and without huge nested mission assets
  - `extracted_bundle` for non-ZIP archives and archives that exceed the current larger limits
- `ModManager.writeEnabledModPaths()` deletes and recreates `.generated_mission_zips` on every launch
- For `stored_zip`, launch staging calls `extractZipToRoot(...)` into per-game `.generated_mission_zips`
- For `extracted_bundle`, launch uses `MissionZipExtractionStore.ensureExtracted(...)` under `mods/.extracted_mission_zips`
- `MissionZipExtractionStore` already links extracted files to the owner archive and supports cleanup through `removeOwner(...)`
- Level metadata already prefers extracted files when a fresh extraction record exists
- The metadata details window still opens the original archive for `MissionZip.inspect(...)`, `MissionZipMusic.inspect(...)`, and some constituent summaries
- Advanced storage already labels linked mission ZIP cache files and can delete linked caches with the owner

## Design Summary
- Make durable extraction the normal path for mission archives whose source archive size is greater than `N MB`
- Set `N = 10 MB` initially, via a named constant such as `MISSION_ZIP_DURABLE_EXTRACT_THRESHOLD_BYTES`
- Keep two explicit import/storage paths:
  - `stored_zip`: keep archive only, lazily stage small ZIP contents as today
  - `extracted_bundle`: keep archive plus durable extracted files, and use extracted files for launch, metadata, music browsing, and constituent inspection
- Extend the existing `MissionZipExtractionStore` instead of adding a separate ownership system
- Preserve the original archive as the owner so deletion, reorder, enable/disable, and provenance still work consistently

## Extraction Decision
- A mission archive should use `extracted_bundle` when any of these are true:
  - source archive size is greater than 10 MB
  - archive format is not ZIP, because 7z/RAR random access is slow or extract-backed already
  - archive contains nested mission assets above the existing nested threshold
  - import path is a selected child archive extracted from a parent archive and that child archive is above 10 MB
- A mission archive may use `stored_zip` only when all of these are true:
  - archive format is ZIP
  - source archive size is at or below 10 MB
  - no nested mission asset triggers durable extraction
- The threshold should be based on source archive byte size, because the user-facing problem is repeated archive processing time
- The extraction manifest should also record total extracted bytes so the UI can show real disk cost

## Storage Model
- Keep source archives in `filesDir/mods/<ownerFilename>` as today
- Keep durable extracted files in `filesDir/mods/.extracted_mission_zips/<safe owner filename>/`
- Keep a manifest at `filesDir/mods/.extracted_mission_zips/manifest.json`
- Extend each record with:
  - `schema`: bump from `dxx-mission-zip-extractions-v1` to `v2`
  - `owner_filename`
  - `owner_size_bytes`
  - `owner_sha256` or a cheaper content identity if hashing is too expensive during import
  - `archive_format`
  - `import_mode`
  - `created_at_ms`
  - `root_path`
  - `extracted_size_bytes`
  - `file_count`
  - `source_archive_name`
  - optional `source_parent_archive_name` for nested Rebirth imports
  - serialized scan metadata or enough fields to rebuild a `MissionZip.ScanResult` without opening the archive
  - per-file entries containing `entry_path`, `relative_path`, and `size_bytes`
- Keep `owner_size_bytes` for fast freshness checks, but use `owner_sha256` when available to catch replaced archives with the same size
- Do not store extracted files under per-game `d1x-redux` or `d2x-redux`, because those generated directories are intentionally launch-time scratch

## Directory-Backed Scan
- Add a way to produce a `MissionZip.ScanResult` from an existing extraction record
- Preferred approach:
  - add `MissionZip.inspectExtracted(record)` or `MissionZip.inspectDirectory(rootDir, files)`
  - reuse `GameFileFormats.parseMissionDescriptor(...)` against extracted descriptor files
  - reconstruct constituents from manifest file entries
  - preserve archive-relative paths through each `entry_path`
- Alternate approach:
  - serialize the original `ScanResult` into the extraction manifest
  - read that cached scan for UI and metadata
- Recommendation:
  - store enough scan metadata in the manifest for speed, but also support rebuilding from extracted files as a repair path

## Launch Path
- `writeEnabledModPaths()` should not re-open or re-extract large archives on ordinary launch
- For `extracted_bundle`:
  - read the fresh extraction record
  - mount the extracted root path
  - add any extracted `.dxa` paths needed by the mission pack
  - if the record is missing or stale, rebuild it once with progress before launch or surface a clear import-repair error
- For `stored_zip`:
  - keep the existing generated per-game staging behavior
  - this path remains acceptable because it is limited to <=10 MB ZIPs
- The game engine should see the same `.active_mod_paths` shape it sees today: either a root directory plus nested `.dxa` files, or the generated small-ZIP staging path

## Metadata Browser Path
- The mod details dialog should not open large 7z/RAR/ZIP archives when a fresh extraction record exists
- `getMissionZipDetails(...)` should:
  - find the extraction record first
  - build details from cached scan or directory-backed scan
  - summarize HOG/DXA/descriptor constituents from extracted files
  - read readmes from extracted files
  - fall back to archive-backed inspection only for `stored_zip` or missing/stale caches
- `LevelMetadataTargets` already checks `MissionZipExtractionStore` for extracted files, so the main work is making sure details and constituent summaries use the same record consistently
- `MissionZipMusic.inspect(...)` and `MissionZipMusicStageManager` need an extracted-file path:
  - either inspect music from extracted root files directly
  - or expose a `MissionZipMusicCatalog` source type that points at extracted files rather than the archive
  - avoid reopening the original large archive just to list or preview music

## UI Changes
- Mission import progress:
  - existing progress UI should show durable extraction as part of import
  - progress labels should distinguish `Copying archive`, `Inspecting level pack`, `Extracting level pack`, and `Finalizing level pack`
- Mods list/details:
  - add a storage/details section in mission metadata view when a durable extraction exists
  - show original archive filename
  - show archive path or owner link
  - show extraction root path
  - show archive size
  - show extracted size
  - show extracted file count
  - show extraction freshness, for example `Extracted cache: ready`
  - show parent archive provenance for nested Rebirth imports if captured
- Mission constituent dialog:
  - show whether the constituent is being read from extracted storage or from the archive
  - external readme/document open should use extracted files when available
- Advanced storage page:
  - current linked-cache labels can stay
  - add extracted cache summary to owner entries if easy
  - deleting the owner archive should remove its extracted cache
  - deleting a linked extracted file should offer to remove the whole linked mission pack or cache, not leave a broken partial cache
- Error UI:
  - if extraction is stale/missing, show a repair action or clear error in mod details
  - if storage is insufficient, keep the owner archive but mark extraction failed and do not enable the mod until repair succeeds

## Cleanup And Ownership
- Owner archive and extracted directory must be treated as one unit
- `ModManager.deleteMod(filename)` should continue calling `MissionZipExtractionStore.removeOwner(filename)`
- `clearAllMods()` should remove `mods/.extracted_mission_zips`
- `MissionZipExtractionStore.pruneMissingOwners()` should run from places that scan storage or load mods
- If a source archive is replaced with a different file under the same name:
  - mark the extraction record stale
  - delete and rebuild extracted files on next repair/import
- If extraction fails midway:
  - write into a temp root first
  - never update the record until extraction fully succeeds
  - delete temp root on failure
- If a user disables a mission pack:
  - keep extracted files
  - this avoids re-extraction when re-enabled
- If a user reorders a mission pack:
  - no extraction change

## Enemy Within And Nested Archives
- Current parent ZIP behavior selects the single child ZIP whose name contains `rebirth`
- Preserve that behavior
- Once the child ZIP is selected and copied into `mods/`, apply the same threshold rules to the child archive
- If the selected Rebirth child ZIP is greater than 10 MB:
  - import `ewithin-rebirth.zip` as the owner archive
  - extract it durably under `.extracted_mission_zips/ewithin-rebirth.zip`
  - store optional provenance that it came from the parent archive, if the parent filename is available at import time
- Do not keep the parent ZIP unless a separate future feature wants parent-package provenance or alternate child selection
- If the parent contains multiple Rebirth-like child ZIPs:
  - keep current `singleOrNull` behavior unless a UI chooser is added later
  - record a clear import failure when selection is ambiguous

## Edge Cases
- ZIP self-extractor preambles:
  - keep existing preamble handling for ZIP streams
  - durable extraction should continue using the same archive abstraction
- RAR/7z archive listing:
  - import-time scan is allowed to pay the cost once
  - details and launch should reuse the extracted record afterward
- Archives with path traversal:
  - keep canonical path checks during extraction
  - never write outside the extraction root
- Case-only duplicate names:
  - Android filesystems may be case-sensitive or case-insensitive depending on storage
  - detect collisions before committing the extraction record
- Multiple mission sets in one archive:
  - one extraction record per owner archive
  - `ScanResult.missionSets` still drives multiple metadata buttons
- Mission ZIPs with top-level song lists:
  - continue the existing generated `descent.sng` alias behavior in `extractZipToRoot(...)`
  - record generated files with `entry_path = ""` or a synthetic source tag
- Custom music:
  - music metadata and preview should use extracted files when present
  - fingerprint cache keys should include owner archive identity plus track identity, not the transient extracted root path alone
- External documents:
  - read/open from extracted files for durable archives
  - archive extraction to cache should only be used for `stored_zip`
- Missing source archive but present extracted files:
  - default behavior should mark the cache orphaned
  - metadata can show limited cache info, but launch should probably refuse unless owner exists, because mod enable/order depends on the owner record
- Storage pressure:
  - estimate required space using archive entry sizes before extraction
  - show extracted size in details so the user can understand disk use

## Implementation Phases
- Phase 1: constants and extraction decision
  - lower mission ZIP durable threshold to 10 MB
  - keep non-ZIP formats on extracted path
  - update tests for import mode selection
- Phase 2: manifest v2
  - extend `MissionZipExtractionRecord`
  - preserve compatibility with v1 records
  - record extracted totals and source/archive metadata
- Phase 3: metadata/details use extracted records
  - add directory-backed scan or cached scan read
  - update `getMissionZipDetails(...)`
  - update constituent summary/readme/external-document paths
- Phase 4: music use extracted records
  - add extracted-root music inspection/staging path
  - keep archive-backed music path for `stored_zip`
- Phase 5: launch repair/error handling
  - avoid archive reopen on normal launch
  - rebuild stale/missing records with progress where appropriate
  - surface clear repair errors
- Phase 6: UI polish and cleanup
  - show extraction details in mod details
  - enhance advanced storage labels/actions
  - add cleanup/prune checks

## Verification Strategy
- Unit tests:
  - small ZIP remains `stored_zip`
  - ZIP over 10 MB becomes `extracted_bundle`
  - 7z and RAR stay `extracted_bundle`
  - nested Enemy Within parent imports Rebirth child and durably extracts when child is over threshold
  - deleting mission archive removes extracted cache
  - disabled/re-enabled mission pack reuses fresh extracted cache
  - stale owner size/hash causes cache rebuild
  - metadata target uses extracted files and does not require archive staging
- UI/manual or automation tests:
  - import large 7z and confirm progress
  - launch large 7z once after import, then launch again and verify no long extraction delay
  - open mod details and verify extraction note, owner archive, extracted root, archive size, extracted size, and file count
  - open a readme/constituent from an extracted archive
  - delete the mod and verify linked files are removed from advanced storage

## Open Decisions
- Whether to hash owner archives during import for stronger freshness checks, or rely on size plus last-modified for speed
- Whether stale/missing extraction should auto-repair during launch or require an explicit repair action
- Whether parent archive provenance for Enemy Within is worth preserving if the parent ZIP is not kept
- Whether `N = 10 MB` should be user-configurable later or remain a launcher constant

## Plan
- [x] Map current mission archive import, launch, metadata, and cleanup paths
- [x] Define storage/linking model for extracted archives and nested Rebirth ZIPs
- [x] Identify UI/menu/metadata changes and edge cases
- [x] Summarize implementation phases and verification strategy

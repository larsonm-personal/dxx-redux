# KCXF2 7z Metadata Viewer ZIP Fallback Investigation

## Reported behavior

- Opening KCXF2 in the launcher metadata viewer reports `zip END header not found`
- The failure occurred after the reusable D2 analysis worker crashed while reopening
  Obsidian metadata

## Findings

- [x] KCXF2 is imported as an extracted 7z bundle with a durable v3 manifest
- [x] A valid extraction record produces a directory-backed metadata target and never
  needs to reopen the original 7z as a ZIP
- [x] `freshRecord` rejects a record when the owner timestamp, size, or SHA changes,
  the manifest is invalid, or any extracted file fails its size and SHA validation
- [x] When that record is rejected, mission inspection itself still supports 7z
- [x] Mod detail constituent summaries already use the archive-neutral `ArchiveFiles`
  reader; no change was needed in that path
- [x] Level metadata target construction and staging used `ZipFile` directly after a
  stale extraction record fell back to the original archive
- [x] That staging fallback produced `zip END header not found` for a valid 7z archive
- [x] The D2 native worker crash does not modify the extraction manifest or KCXF2 files,
  so the two failures share timing but not a direct state mutation
- [x] The retained emulator's current KCXF2 record, source SHA, and extracted file SHAs
  all match, and its latest KCXF2 analysis succeeded. The exact record-rejection reason
  on the reporting device cannot be recovered from the exception text alone

## Repair plan

- [x] Make archive-backed metadata discovery, constituent pairing, and staging use
  `ArchiveFiles` for ZIP, 7z, and RAR sources
- [x] Rebuild a stale durable extraction record while opening mission-pack details
  before exposing metadata actions
- [x] Preserve bounded extraction sizes and free-space checks in archive-neutral staging
- [x] Avoid Windows case-insensitive self-copy deletion when publishing mission aliases
- [x] Add regressions for 7z target discovery, staging, and stale-record repair
- [x] Verify reusable worker cleanup does not invalidate later metadata requests
- [x] Run scoped formatting, JVM tests, Android build, and emulator verification

## Validation

- [x] The 7z regression discovers the HOG level through the original archive and stages
  both the descriptor and HOG without using `ZipFile`
- [x] The stale-record regression rebuilds the durable extraction and returns metadata
  details without `zip END header not found`
- [x] `LevelMetadataTargetsTest` and the focused `ModManagerMissionZipTest` pass
- [x] `:app:assembleDebug` and the reusable-worker emulator regression pass

## Constraints

- Preserve ZIP, 7z, and RAR mission imports
- Prefer durable on-disk extraction identity over process-local state
- Do not duplicate archive parsing logic in the metadata viewer

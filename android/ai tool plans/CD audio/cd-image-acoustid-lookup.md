# CD Image AcoustID Lookup Plan

## Goal
Add AcoustID album+track name lookups to CD image `track_fingerprints.json` files,
carry those through to `known_discs.json5`, and git add all metadata files.

## Changes

### 1. `game_data/fingerprint_disc_tracks.ps1`
- Add `-SkipAcoustId` switch
- Add AcoustID config loading (same pattern as fingerprint_music_packs.ps1)
- Add `Invoke-AcoustIdLookup` function (same implementation)
- After Phase 1 (fingerprinting), add Phase 2 (AcoustID):
  - For each folder with track_fingerprints.json, load it
  - For each audio track without `acoustid_name`, do AcoustID lookup
  - Add `acoustid_name` and `acoustid_album` fields
  - Re-save if any tracks were updated
- This handles both fresh runs and adding AcoustID to existing files

### 2. `game_data/update_known_discs_fingerprints.ps1`
- When building `$fpBySha1` lookup, also store `acoustid_name` and `acoustid_album`
- When merging lines into known_discs.json5, also insert/update these fields
- Strip and re-add pattern (same as chromaprint/duration_ms)

### 3. Re-run pipeline
- `fingerprint_disc_tracks.ps1 -SkipBuild` (adds AcoustID to existing fingerprints)
- `update_known_discs_fingerprints.ps1` (merge into known_discs.json5)
- Verify results

### 4. Git add
- `git add game_data/CD images/*/track_fingerprints.json`
- `git add game_data/music/*/chromaprint_info.json5`
- `git add android/app/src/main/assets/known_discs.json5`
- `git add` modified scripts

## Status
- [x] Plan created
- [x] fingerprint_disc_tracks.ps1 modified
- [x] update_known_discs_fingerprints.ps1 modified
- [x] Pipeline re-run (200/203 tracks had AcoustID, 3 unresolvable)
- [x] Git add (32 track_fingerprints.json + scripts + known_discs.json5 + chromaprint_info.json5 files)

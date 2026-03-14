# Plan: Integrate Mac Macplay Descent 1 CD Image

## Disc Overview
- **Folder**: `game_data/CD images/Descent - Mac macplay/`
- **Files**: `Descent - Mac macplay.bin` + `.cue` (bin/cue format, single bin, 686 MB)
- **Track layout**: 1 data track (Mode1/2352) + 13 audio tracks (redbook)
- **Data track filesystem**: Apple Partition Map + HFS (NOT ISO 9660)
  - DDR block size: 512, block count: 319,992
  - 2 partitions: Apple_partition_map (2 blocks) + Apple_HFS "Descent" (319,989 blocks)
- **HFS volume name**: "Descent"
- **Content**: MacPlay installer disc for Descent 1. Game data (.hog/.pig) is inside
  a StuffIt Installer v2 archive ("Install Descent", 8.6 MB data fork). Not raw ISO files.
- **Other disc contents**: Descent Manual (PDF), joystick config files, MacPlay demos
  (Dungeon Master 2, Frankenstein, Mario's Game Gallery), interactive content/multimedia
  guide (CHM-style QuickTime/JPEG content)

## Cue File Fix (done)
The cue FILE directive referenced `"final.bin"` (the intermediate Aaru conversion output).
Changed to `"Descent - Mac macplay.bin"` to match the actual filename.

## What Works Now
1. Track SHA-1 hashing via `extract_cd.exe` -- all 14 tracks hashed successfully
2. HFS volume listing/extraction via Python `machfs` library
3. Full HFS content can be extracted to disk

## What Doesn't Work
1. **ISO extraction fails**: `extract_cd.exe`'s `iso_list_files()` returns "ISO listing failed"
   because the data track uses HFS, not ISO 9660. This is expected and correct behavior.
2. **Game data extraction**: The .hog/.pig/.ham files are compressed inside the StuffIt
   Installer (STi2 format). No available Windows/CLI tool can decompress this:
   - 7-Zip: detects HFS but can't open classic HFS; can't open STi2
   - WSL unar: package not available in Ubuntu 24.04
   - Python: no StuffIt library available
   - The installer contains: `descent.hog`, `descent.pig`, `CHAOS.HOG` (compressed)

## Steps Completed
1. [x] Fixed cue FILE directive to match actual .bin filename
2. [x] Ran extract_cd.exe -- got track hashes (1 data + 13 audio)
3. [x] Extracted raw data track user data to flat image
4. [x] Parsed Apple Partition Map, identified HFS partition
5. [x] Used Python machfs to list all files on the HFS volume
6. [x] Extracted all accessible HFS files to disk
7. [x] Searched StuffIt installer for game data -- files are compressed
8. [x] Saved track_hashes.json in the disc folder
9. [x] Ran hash_disc_tracks.ps1 -- added to known_discs.json5 (id: descent-mac-macplay)
10. [x] Added version mapping "D1 Mac (MacPlay)" to hash_assets.ps1
11. [x] Regenerated SOURCE_FILES.txt (now 223 files, +2 for .bin/.cue)
12. [x] Cross-referenced audio hashes -- unique to this disc (no matches with any other disc)
13. [x] Ran hash_assets.ps1 -- no new game asset entries (data_tracks/ empty, as expected)

## Audio Track Analysis
- 13 redbook audio tracks (tracks 2-14) -- unique to this disc
- PC Descent 1 discs have ZERO audio tracks (music is MIDI-based on PC)
- The Mac port added CD audio, likely replacing the MIDI soundtrack
- No audio track SHA-1 matches with any other disc in known_discs.json5
- This is the only D1 disc in the database with redbook audio

## Remaining Work
- Game asset hashing blocked until StuffIt Installer (STi2) can be decompressed
   - Could be unblocked by: obtaining StuffIt Expander for Mac, or building
     unar/lha from source, or finding a working Python StuffIt library
   - For now, track-level hashes and disc characterization are complete

## New Disc Integration Checklist (general)
For adding any new CD image to the project:
1. Place bin/cue files in `game_data/CD images/<disc name>/`
2. Ensure the cue FILE directive references the correct .bin filename
3. Run `.\extract_all_cds.ps1` (or manually run extract_cd.exe) to:
   - Extract data track files to `data_tracks/`
   - Generate `track_hashes.json`
4. Run `.\hash_disc_tracks.ps1` to add disc to `known_discs.json5`
5. Add a version mapping entry in `hash_assets.ps1` `$VersionMap`
6. Run `.\hash_assets.ps1` to hash extracted game files into `known_versions.json5`
7. Run `.\generate_source_manifest.ps1` to update `SOURCE_FILES.txt`
8. For non-ISO data tracks (HFS, etc.): manual extraction needed before step 6

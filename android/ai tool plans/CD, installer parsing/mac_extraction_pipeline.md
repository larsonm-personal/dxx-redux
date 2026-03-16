# Plan: Mac CD Extraction Pipeline and Dependency Scripts

## Context

The Mac version of Descent 1 (MacPlay) stores game data on an HFS-formatted CD
with the actual game files (.hog/.pig) compressed inside a StuffIt Installer v2
(STi2) archive. The existing extract_cd.exe tool only handles ISO 9660, so we
need additional tools and a new pipeline.

## Deliverables

### 1. get_unar.sh (dependency install script)
- Downloads pre-built `unar` (The Unarchiver CLI) for Windows
- URL: https://cdn.theunarchiver.com/downloads/unarWindows.zip
- Installs to `$LOCAL_DIR/unar/`
- Version pinned in tool_versions.conf
- Follows same pattern as get_dosbox.sh, get_clang_format.sh

### 2. get_stuffit.sh (dependency install script)
- Builds `stuffit` CLI from Rust crate v0.1.4 via `cargo install`
- Installs to `$LOCAL_DIR/stuffit/`
- Version pinned in tool_versions.conf
- Requires Rust toolchain (not managed by this project)

### 3. extract_mac_cd.ps1 (all-in-one extraction script)
Location: game_data/extract_mac_cd.ps1

Pipeline stages:
1. Parse CUE sheet to find data track
2. Strip Mode1/2352 headers to get raw user data (2048 bytes/sector)
3. Parse Apple Partition Map to find HFS partition
4. Extract HFS partition
5. Read HFS volume using Python machfs library
6. Extract "Install Descent" data fork (STi2 archive)
7. Run unar to decompress STi2 into game files
8. Copy game data files to data_tracks/ directory

Input: CD image folder path (containing .bin and .cue)
Output: data_tracks/ directory with extracted game files

### 4. Hash game assets
- Run hash_assets.ps1 to add extracted game files to known_versions.json5
- Version mapping already exists: "Descent - Mac macplay" = "D1 Mac (MacPlay)"

## File entry points in STi2 archive (discovered via binary analysis)
- CHAOS.HOG at offset 0x69399 in the STi2 file
- descent.hog at offset 0x7E76A
- descent.pig at offset 0x456263

## Tools involved
- extract_cd.exe: Track hashing, Mode1 sector parsing (already exists)
- Python machfs: HFS volume reading (pip install machfs)
- unar (The Unarchiver CLI): STi2 decompression (new dependency)
- stuffit CLI (Rust crate): SIT 1.x/5.x decompression (new dependency)
  Note: stuffit does NOT handle STi2 format. unar is the primary tool.
  stuffit is installed per user request as a supplementary tool.

## Completed
- [x] tool_versions.conf entries for unar and stuffit
- [x] get_unar.sh install script
- [x] get_stuffit.sh install script
- [x] extract_mac_cd.ps1 all-in-one pipeline (tested, works end-to-end)
- [x] Game asset extraction: descent.hog, descent.pig, CHAOS.HOG, CHAOS.MSN, 3 demos, Descent exe
- [x] hash_assets.ps1 run: 5 new entries added to known_versions.json5 (187 total)
- [x] SOURCE_FILES.txt regenerated
- [x] track_hashes.json, known_discs.json5 already updated (from previous session)

## Findings
- CHAOS.HOG (Mac) is byte-identical to chaos.hog (D1 v1.0 PC)
- CHAOS.MSN differs between Mac and PC (different hash)
- .msn extension not in hash_assets.ps1 $GameExtensions list (pre-existing gap)
- descent.hog and descent.pig are Mac-specific versions (unique hashes)
- unar handles STi2 format; stuffit CLI only handles SIT 1.x/5.x (not STi2)

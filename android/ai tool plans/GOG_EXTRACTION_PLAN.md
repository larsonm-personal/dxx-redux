# GOG Installer Extraction + CD Re-extraction Plan

## Overview
Two-part plan:
1. Re-extract all 31 CD images with O_BINARY fix + SOW extraction, rehash everything
2. Add GOG installer extraction (.exe via InnoSetup reader, .pkg via XAR+cpio reader)

## GOG Installers (4 files in game_data/gog installers/)
- `setup_descent_1.4a_(16596).exe` — D1 Windows (InnoSetup)
- `setup_descent_2_1.1_(16596).exe` — D2 Windows (InnoSetup)
- `descent_enUS_1_0_35122.pkg` — D1 Mac (XAR+cpio)
- `descent_2_enUS_1_0_51877.pkg` — D2 Mac (XAR+cpio)

---

## Phase 1: CD Re-extraction and Re-hashing — DONE ✓

### Steps
1. ✅ Rebuild extract_cd.exe (O_BINARY fix already applied)
2. ✅ Update extract_all_cds.ps1: delete data_tracks/ on -Force
3. ✅ Run extract_all_cds.ps1 -Force — re-extracted all 29 CDs with SOW decompression (2 CDs have no CUE)
4. ✅ Run hash_disc_tracks.ps1 -Force — regenerated known_discs.json5 (31 entries, 29 CD + 2 hand-crafted)
5. ✅ Run hash_assets.ps1 -Force — regenerated known_versions.json5 (181 entries, 19 version groups)
6. ✅ Version rationalization — D2 v1.0/v1.1/v1.2 distinguished (v1.1 has different descent2.ham/hog from v1.0; v1.2 GOG matches v1.1); D1 v1.4a/v1.5 aliased (same game data); Added version comments to known_discs.json5

### Key findings from rationalization
- D2 v1.0 CDs have unique descent2.ham (7A288B) and descent2.hog (AE0872, 7107354 bytes)
- D2 v1.1 CDs (including Rerelease and Definitive Collection) have descent2.hog (F1ABF5, 7595079 bytes) matching GOG v1.2
- D2 v1.1 CDs mapped correctly: "Descent II (v1.1)", "Rerelease", and Definitive Collection Disc 2
- D1 v1.4a (GOG) and D1 v1.5 (Definitive Collection) have identical descent.hog/pig — both labels in known_versions.json5
- O_BINARY fix changed CD-extracted hashes; shared files now correctly match GOG copies

---

## Phase 2: GOG Installer Extraction — C Library

### Architecture
All readers are general-purpose with full enums and clean error messages for
unsupported cases. Only the formats used by GOG's Descent installers are fully
implemented; other branches return descriptive "unimplemented" errors.

### 2A: InnoSetup Reader (extract/inno_reader.c) — DONE ✓
- Parse InnoSetup header (magic detection, version identification)
- Read file listing from compressed metadata
- LZMA1 + LZMA2 decompression via cmake FetchContent of lzma-sdk
- GOG Galaxy before_install script parsing for real filenames
- GOG Galaxy zlib double-decompression
- Extract individual files with progress callback
- Full enum for InnoSetup compression methods (lzma, lzma2, zlib, bzip2, none)
  with lzma1, lzma2, and zlib implemented
- Supports InnoSetup 5.5.7 (D2) and 5.6.2 (D1)
- Verified: all 7 D1 + 21 D2 game files byte-for-byte identical to reference

### 2B-D: Mac .pkg Reader (extract/pkg_reader.c) — DONE ✓
- Consolidated XAR + cpio into single pkg_reader.h/c (no separate xar_reader/cpio_reader)
- XAR: parse 28-byte BE header, decompress TOC XML with zlib uncompress()
- TOC: minimal strstr-based XML parser finds package.pkg/Scripts entry
- gzip: streaming inflate (MAX_WBITS+16) for the Scripts payload
- cpio odc: parse "070707" ASCII format (76-byte headers, octal fields)
- Streaming two-pass: pkg_open() scans headers, pkg_extract_all() extracts matches
- Game files found at ./payload/Contents/Resources/game/ in the cpio archive
- Verified: D1 7 files + D2 15 files byte-for-byte identical to .exe extraction

### 2E: Standalone Test Tool (extract/extract_gog.c) — DONE ✓
- Detects format from extension (.exe → InnoSetup, .pkg → Mac pkg)
- Extracts game assets to output directory
- Prints JSON output with extracted file list

### 2F: CMake Integration — DONE ✓
- LZMA SDK via FetchContent (not vendored)
- zlib via FetchContent (not vendored)
- New test executables: extract_gog
- All new sources added to both test CMakeLists.txt and Android CMakeLists.txt

### 2G: Test Script (game_data/extract_all_gog.ps1) — DONE ✓
- Builds extract_gog.exe via cmake
- Runs on all 4 GOG installers (.exe and .pkg)
- Extracts to game_data/gog installers/<name>/extracted/
- SHA-256 hashes all extracted files
- Cross-references against known_versions.json5
- Idempotent: skips already-extracted unless -Force
- Results: 50 files total, all matched known versions
- Added chaos.msn to D1 v1.0 version group (was missing)
- Mac .pkg extractions byte-for-byte identical to Windows .exe
- D2 .pkg has 15 files; D2 .exe has 21 (extra missions + demo + .inst)

### New files
```
android/app/src/main/cpp/extract/
  inno_reader.h      — InnoSetup extraction API
  inno_reader.c      — InnoSetup reader implementation
  pkg_reader.h       — Mac .pkg extraction API (XAR + gzip + cpio odc)
  pkg_reader.c       — Mac .pkg reader (consolidated, no separate xar/cpio files)
  extract_gog.c      — Standalone GOG extraction tool (.exe and .pkg)
game_data/
  extract_all_gog.ps1           — Test script for all GOG installers
  generate_source_manifest.ps1  — Generates SOURCE_FILES.txt (SHA-256 manifest of originals)
  SOURCE_FILES.txt              — SHA-256 manifest of all original source files (not in git)
```

### Modified files
```
android/app/src/main/cpp/extract/CMakeLists.txt — new targets + LZMA SDK
android/app/src/main/cpp/CMakeLists.txt         — new sources for Android
game_data/extract_all_cds.ps1                    — clean re-extract on -Force
```

---

## Phase 3: Android Integration (future, not this PR)
- JNI bridge for InnoSetup + pkg extraction
- SetupActivity .exe/.pkg detection dialog
- BIN/CUE import prompt for D2 GOG
- File set creation from extracted assets

## Phase 4: Regression Testing (future, not this PR)
- Introspection extensions for file set contents
- Emulator-based regression script
- Automation scripts for each installer type

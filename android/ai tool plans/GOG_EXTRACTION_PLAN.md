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

## Phase 1: CD Re-extraction and Re-hashing

### Steps
1. Rebuild extract_cd.exe (O_BINARY fix already applied)
2. Update extract_all_cds.ps1: delete data_tracks/ on -Force
3. Run extract_all_cds.ps1 -Force — re-extract all 31 CDs with SOW decompression
4. Run hash_disc_tracks.ps1 -Force — regenerate known_discs.json5
5. Run hash_assets.ps1 -Force — pick up SOW-extracted assets, regenerate known_versions.json5
6. re-rationalize the versions extracted from each CD, and update comments in known_discs.json5. for example, you *should* match the .hog files in the 1.1 CD with the known version 1.1 files

### Expected results
- Track SHA1s unchanged (disc-level hashes are independent of file content fix)
- known_versions.json5 gains entries from assets previously locked inside .sow archives

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

### 2B: XAR Reader (extract/xar_reader.c)
- Parse XAR header (magic `xar!`, header/toc sizes)
- Decompress TOC with zlib inflate
- Minimal XML tag parser for file entries (offset, size, name, compression)
- Extract payloads with progress callback
- Enums for XAR compression methods (none, gzip, bzip2, xz) — only none+gzip implemented

### 2C: cpio Reader (extract/cpio_reader.c)
- Parse cpio "newc" ASCII format headers
- Extract files matching filter criteria
- Enum for cpio magic formats (newc, odc, bin, crc) — only newc implemented

### 2D: Mac .pkg Extractor (extract/pkg_reader.c)
- Combines XAR + cpio: open .pkg → XAR → find Payload → decompress → cpio → files
- API: `pkg_list_files()`, `pkg_extract_files()`

### 2E: Standalone Test Tool (extract/extract_gog.c)
- Detects format from extension (.exe → InnoSetup, .pkg → Mac pkg)
- Extracts game assets + optionally BIN/CUE if found
- Prints JSON output with extracted file list and SHA-256 hashes

### 2F: CMake Integration
- LZMA SDK via FetchContent (not vendored)
- New test executables: extract_gog
- All new sources added to both test CMakeLists.txt and Android CMakeLists.txt

### 2G: Test Script (game_data/extract_all_gog.ps1)
- Builds extract_gog.exe
- Runs on all 4 GOG installers
- Extracts to game_data/gog installers/<name>/extracted/
- Cross-references SHA-256 against known_versions.json5

### New files
```
android/app/src/main/cpp/extract/
  inno_reader.h      — InnoSetup extraction API
  inno_reader.c      — InnoSetup reader implementation
  xar_reader.h       — XAR archive API
  xar_reader.c       — XAR archive reader
  cpio_reader.h      — cpio archive API
  cpio_reader.c      — cpio archive reader
  pkg_reader.h       — Mac .pkg extraction API
  pkg_reader.c       — Mac .pkg reader (combines XAR + cpio)
  extract_gog.c      — Standalone GOG extraction tool
game_data/
  extract_all_gog.ps1 — Test script for all GOG installers
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

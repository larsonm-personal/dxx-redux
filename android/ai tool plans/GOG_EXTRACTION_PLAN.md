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

## Phase 3: Android Integration

### Known bugs to fix first

1. **DiscImportDialog extracts to wrong directory** (SetupActivity.kt L2880)
   - `extractIsoFiles()` passes `filesDir.absolutePath` instead of `setDir`
   - Extracted game files land in root app dir, invisible to readiness checks which look in `setDir`
   - Fix: pass `setDir.absolutePath` instead

2. **`.gog`/`.inst` import path inconsistency**
   - File picker dispatches them through `importFile()` which correctly writes to `setDir` (L1420)
   - But `AudioSourceManager.hasLegacyGog()` (L57-61) checks `filesDir` root for the pair
   - `hasLegacyGog()` needs to also check `setDir`, or the migration needs to handle this
   - Fix: update `hasLegacyGog()` to accept/check setDir

3. **Setup introspection `files_on_disk` reports wrong directory** (SetupActivity.kt L345)
   - Lists `filesDir` (root app dir) not the active set directory
   - Should list set dir contents (or both)
   - Fix: add `set_files` field listing `setDir` contents

### 3.1: Fix DiscImportDialog extraction target — DONE ✓
- Change `filesDir.absolutePath` to `setDir.absolutePath` at L2880
- Thread `setDir` into DiscImportDialog (it currently receives `filesDir`)
- After extraction, trigger manifest rehash for newly extracted files

### 3.2: Fix `.gog`/`.inst` legacy check — DONE ✓
- Update `hasLegacyGog()` to accept optional setDir parameter
- Check both filesDir (legacy) and setDir (new) for the .gog/.inst pair
- This ensures both old installs and new imports work

### 3.3: Fix setup introspection `files_on_disk` — DONE ✓
- Add `set_files` field to setup introspection JSON that lists active set directory contents
- Keep existing `files_on_disk` for backwards compatibility
- Add `active_set_path` field showing the current set directory path

### 3.4: GOG JNI bridge (jni_gog_import.c) — DONE ✓
- New file `extract/jni_gog_import.c` wrapping `inno_reader.h` and `pkg_reader.h`
- Three JNI functions (naming: `Java_com_dxxredux_app_GogImportBridge_native*`):
  - `nativeDetectFormat(path)` → "innosetup" | "pkg" | "unknown" (extension-based)
  - `nativeListFiles(path)` → String[] of "name|size" entries (game extensions only)
  - `nativeExtractFiles(path, outputDir, progress)` → file count or -1
- Progress callback via `gog_extract_ctx_t` struct: `onProgress(String, long, long): int`
- Game extension filter: .hog, .pig, .ham, .s11, .s22, .dem, .mvl, .msn, .mn2, .gog, .inst
- LZMA SDK linked via FetchContent (same git repo/tag as test CMakeLists.txt)
- Android NDK zlib linked as system library `-lz` (no FetchContent needed)

### 3.5: GogImportBridge.kt — DONE ✓
- Kotlin JNI wrapper object (pattern: DiscImportBridge.kt)
- `GogFile(name: String, size: Long)` data class
- `detectFormat(path)`, `listFiles(path)` → List<GogFile>?, `extractFiles(path, outputDir, progress)`
- `ExtractProgress` interface with `onProgress(currentFile, bytesDone, bytesTotal): Int`
- Parses native "name|size" strings into GogFile objects

### 3.6: GOG import dialog in SetupActivity — DONE ✓
- File picker now detects `.exe`/`.pkg` extensions → sets `gogImportUri`/`gogImportName`
- `GogImportDialog` composable (~160 lines):
  - LaunchedEffect copies installer to tmp via content resolver
  - Calls `GogImportBridge.detectFormat()` and `listFiles()` on load
  - Shows format, file listing with sizes
  - "Extract to \"setName\"" button with progress bar (LinearProgressIndicator)
  - Progress via `GogImportBridge.ExtractProgress` callback
  - After extraction checks `AudioSourceManager.hasLegacyGog(setDir)` for GOG audio
  - Done button cleans up tmp and triggers onRefresh()
  - Error handling for unknown format or empty file list
- Help text updated: "Select .hog, .ham, .pig files, a .zip archive, .cue/.bin disc images, or GOG installer."

### 3.7: Enhanced BIN/CUE import dialog — DONE ✓
- After ISO extraction succeeds, scans setDir for .sow files via `DiscImportBridge.scanSowFiles()`
- Decompresses any found .sow archives via `DiscImportBridge.extractSowFiles()`
- Status message shows combined counts: "Extracted N file(s) + M from .sow archives"
- No set choice prompt added (extracts to current active set, matching GOG dialog)

### 3.8: Broadcast commands for headless testing — DONE ✓
- Added 5 new SETUP_COMMAND handlers to `commandReceiver`:
  - `create_set --es name "..."` — creates named file set via FileSetManager
  - `switch_set --es name "..."` — switches active set + writes .active_set_path
  - `clear_set --es name "..."` — deletes all files in named set directory
  - `import_gog --es path "..."` — extracts GOG installer to active set (runs on background thread)
  - `import_files --es path "..."` — copies single file to active set
- Usage examples in comments at declaration site
- Note: `import_disc` not implemented yet (would need fd-based BIN/CUE import from filesystem path)

### 3.9: Enhanced setup introspection — DONE ✓
- Added to `writeIntrospectJson()`:
  - `sets` — JSONArray of {name, file_count, active} for all file sets
  - `audio_sources` — JSONArray of {id, label, cue_path, track_count, audio_track_count}
  - `has_legacy_gog_audio` — boolean, true if .gog/.inst pair found in active set
- Previously added (3.3): `set_files`, `active_set_path`

### New files (Phase 3)
```
android/app/src/main/cpp/extract/jni_gog_import.c  — GOG JNI bridge
android/app/src/main/java/com/dxxredux/app/GogImportBridge.kt — Kotlin wrapper
```

### Modified files (Phase 3)
```
android/app/src/main/cpp/CMakeLists.txt       — added GOG sources + LZMA SDK FetchContent + zlib link
android/app/src/main/java/com/dxxredux/app/SetupActivity.kt — GOG dialog, SOW post-extract, broadcast cmds, introspection
android/app/src/main/java/com/dxxredux/app/AudioSourceManager.kt — hasLegacyGog(setDir) parameter
```

---

## Phase 3a: Verification & Regression Testing

### 3a.0: Manual end-to-end verification — DONE
Verified GOG import pipeline on Android emulator (Pixel_6_API_34, x86_64).

**Test results:**
| Test | Result | Details |
|------|--------|---------|
| D1 GOG import (.exe) | PASS | `setup_descent_1.4a_(16596).exe` → 7 files (InnoSetup 5.6.2 unicode) |
| D2 GOG import (.exe) | PASS | `setup_descent_2_1.1_(16596).exe` → 21 files (InnoSetup 5.5.7 unicode) |
| Set management | PASS | create_set, switch_set, clear_set all work via broadcast |
| Setup introspection | PASS | sets array, set_files, has_legacy_gog_audio, audio_sources all correct |
| D2 game launch (GOG set) | PASS | HOG files found, level "Ahayweh Gate" loaded, player shields=100 |
| Combined D1+D2 GOG set | PASS | Both missions visible in mission select |
| D1-only set launch | **FAIL** | D2 binary requires descent2.hog — crashes with SIGABRT |
| UPPERCASE filenames | PASS | GOG extraction produces UPPERCASE; game handles case-insensitively |

**Extracted file inventories:**
- D1 GOG (7 files): CHAOS.HOG, CHAOS.MSN, DESCENT.DEM, DESCENT.HOG, DESCENT.PIG, LEVEL18.DEM, MINIBOSS.DEM
- D2 GOG (21 files): ALIEN1/2.PIG, DESCENT2.HAM/HOG/S11/S22, DESCENT_II.gog/inst, FIRE/GROUPA/ICE/WATER.PIG, INTRO-H/OTHER-H/ROBOTS-H/ROBOTS-L.MVL, d2-2plyr.hog/mn2, d2chaos.hog/mn2, descent2.dem

**Known issue — D1-only sets:**
The android build only produces d2x-redux binary. D1-only sets crash immediately because
`descent2.hog` is required at startup. This isn't a GOG extraction issue — it's an architectural
constraint. Options: (a) always require D2 files, (b) build d1x-redux too, (c) make D2 binary
gracefully handle D1-only mode. For now, the launcher should prevent launching with D1-only sets
unless a D2 HOG is also present.

**Broadcast command syntax (verified):**
```bash
# Separate --es for each parameter:
adb shell "am broadcast -a com.dxxredux.SETUP_COMMAND --es command switch_set --es name default"
adb shell "am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_gog --es path /data/local/tmp/installer.exe"
```

### 3a.1: Regression spec format (`extract_regression.json5`)
- One file per CD image folder and per GOG installer
- Located next to the source files (creates skeleton dirs in git)
- Fields:
  ```json5
  {
    source_type: "cd" | "gog",
    source_files: [{ name: "foo.cue", sha256: "..." }, ...],
    game: "d1" | "d2" | "d1d2",
    expected_mission: "Descent: First Strike" | "Descent 2: Counterstrike!",
    expected_level1: "Lunar Outpost" | ...,
    expected_files: ["descent2.hog", "descent2.ham", ...],  // minimum required
    audio_tracks: 8,  // expected audio track count, 0 if none
  }
  ```

### 3a.2: Generate regression specs
- Script `game_data/generate_regression_specs.ps1`
- Walks `game_data/CD images/` and `game_data/gog installers/`
- Hashes source files, looks up `known_discs.json5` for game type
- Maps game type → mission name table:
  - d2 full → "Descent 2: Counterstrike!"
  - d1 full → "Descent: First Strike"
  - d2 demo → "Descent 2 Demo"
  - d2 oem → "D2 Destination:Quartzon"
  - d1 demo → "Descent Demo"
  - d1 oem → "Destination Saturn"
- Writes one `extract_regression.json5` per source

### 3a.3: Single-source test script
- `android/run_extract_test.ps1 <path-to-extract_regression.json5>`
- Steps:
  1. Clear active set on emulator (broadcast `clear_set`)
  2. Push source files to `/data/local/tmp/` via adb
  3. Broadcast `import_disc` or `import_gog` (based on `source_type`)
  4. Wait, then introspect setup state
  5. Verify `set_files` contains `expected_files`
  6. Broadcast `launch`
  7. Wait for game main menu
  8. Select mission matching `expected_mission`
  9. Skip briefing, wait for level 1
  10. Introspect: verify `in_game`, `current_level_name` matches
  11. Report PASS/FAIL

### 3a.4: Orchestration script
- `android/run_all_extract_tests.ps1`
- Recursively finds all `extract_regression.json5` in `game_data/`
- Runs each through 3a.3
- Summary table at end: source name, PASS/FAIL, timing
- Exit code = number of failures

### 3a.5: Automation script template
- Parameterized JSON5 automation script for in-game verification
- Template in `android/game_scripts/test_extract_regression.json5`
- Parameters substituted by the test runner: mission name, level name
- Actions: wait for menu → select mission → skip briefing → assert in_game + level

### 3a.6: Full coverage run
- Run 3a.4 against all 29 CDs + 4 GOG installers (33 total)
- Investigate failures, categorize:
  - Demo CDs (different mission names, may need special handling)
  - Expansion-only CDs (Vertigo, Quartzon — no base game files)
  - Multi-game CDs (Definitive Collection — both D1 and D2)
- Document known limitations in results

---

## Phase 4: Polish & Extended Testing (future)
- Multi-set regression (create set, import, switch, verify isolation)
- Audio playback verification (Redbook tracks playing in-game)
- SAF leave-in-place import testing
- Performance profiling of extraction on device

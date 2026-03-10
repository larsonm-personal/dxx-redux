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

### 3a.1: Regression spec format (`extract_regression.json5`) — DONE
- One file per CD image folder and per GOG installer
- Located next to the source files (in CD image dirs) or alongside installers (for GOG)
- Fields:
  ```json5
  {
    source_type: "cd" | "gog",
    disc_id: "descent-ii-usa",          // from known_discs.json5 (CD only)
    source_files: [{ name: "foo.cue", sha256: "..." }, ...],
    game: "d1" | "d2" | "d1d2",
    classification: "d2_full" | "d1_full" | "d2_demo" | "d2_oem" | "d2_vertigo" | "d1_demo" | "d1_expansion" | "d1_levels",
    expected_mission: "Descent 2: Counterstrike!" | null,  // null for levels-only discs
    expected_level1: "Ahayweh Gate" | null,
    expected_files: ["descent2.hog", ...],  // minimum required files
    audio_tracks: 8,  // expected audio track count, 0 if none
    total_extracted: 22  // total file count from extraction
  }
  ```
- Classifications with null mission (d1_levels) indicate add-on-only discs that can't launch standalone

### 3a.2: Generate regression specs — DONE
- Script: `game_data/generate_regression_specs.ps1` (supports `-Force` to regenerate)
- Walks `game_data/CD images/` and `game_data/gog installers/`
- CD identification: reads `track_hashes.json`, matches data track SHA1 → `known_discs.json5`
- GOG identification: hardcoded installer→game mapping (4 known installers)
- Classification logic in `Get-DiscClassification` function, using disc_id + extracted file patterns
- **Results: 33 specs generated (29 CDs + 4 GOG)**
  - 27 launchable (d1_full, d2_full, d1_demo, d1_expansion, d2_demo, d2_oem, d2_vertigo)
  - 2 non-launchable (d1_levels: Levels of the World, Dimensions for Descent)
  - Some disc_id overlap is expected (e.g. USA and Alt match same data track SHA1)

### 3a.3: Single-source test script -- DONE
- Implemented: `android/run_extract_test.ps1`
- Takes a path to an `extract_regression.json5` spec file
- Steps performed:
  1. Sanitize device: force-stop, clear test/default sets (both broadcast & direct rm), clean filesDir root, clear stale test sets
  2. Restart app so Java can see clean state
  3. Canary check: verify `can_launch=false` with empty set
  4. Push extracted game files to `regression_test` set via adb staging
  5. Restart app, introspect to verify all expected files present
  6. Anti-demo-set canary: SHA1 hash signature files vs demo set to detect false positives
  7. Check game readiness (`d2.ready`/`d1.ready`)
  8. Launch game, navigate menus (Enter key presses through briefing)
  9. Introspect in-game state, verify `current_level_name` matches expected
  10. Cleanup: force-stop, clear test set
- Flags: `-SkipLaunch` (file-only), `-KeepFiles` (don't clean up)
- Key discoveries: Java `File.listFiles()` can't see files created via `adb shell cat` redirect until app restart; `sha1sum` via `sh -c` hangs (args split); direct `adb shell run-as sha1sum` works
- Verified: Full PASS on Descent II (USA) CD -- `Ahayweh Gate` level loaded

### 3a.4: Orchestration script ✅ DONE
- `android/run_all_extract_tests.ps1`
- Recursively finds all `*_regression.json5` specs in `game_data/`
- Runs each through `run_extract_test.ps1` (3a.3)
- Parameters: `-Filter` (wildcard on source dir name), `-SkipLaunch`, `-MaxFailures`, `-SpecPaths`
- Uses hashtable splatting to correctly pass `-SkipLaunch` switch to child script
- Summary table: source name, PASS/FAIL, timing, exit code
- Exit code = number of failures
- Resets `$ErrorActionPreference` after each child invocation (child may change it)
- Verified: 2/2 PASS on Descent II (Europe) CDs with full launch in 2:28

### 3a.5: Automation script template ✅ DONE
- `android/game_scripts/test_extract_regression_template.json5`
- Parameterized JSON5 automation script for in-game verification
- Template parameters: `MISSION_NAME`, `LEVEL_NAME` (substituted by test runner)
- Actions: wait for pilot select → new game → select mission → Ok → Rookie → skip briefing → assert in_game + level
- Note: Currently exists as a future enhancement path — `run_extract_test.ps1` uses direct adb keyevent presses for menu navigation, which is already working reliably

### 3a.6: Full coverage run ✅ DONE
- Run 3a.4 against all 29 CDs + 4 GOG installers (33 total)
- Results: **21/33 PASS, 12 FAIL** (35:47 total time)
- Script improvements this phase:
  - Adb function rewritten with ProcessStartInfo (timeout, async stderr drain)
  - Push-FileToSet uses direct ProcessStartInfo for sh -c 'cat > dest' (proper quoting)
  - Introspection-guided navigation (detects input fields, movie/briefing, game screens)
  - Cleanup handlers (Invoke-Cleanup, Register-EngineEvent PowerShell.Exiting)
  - Orchestrator: pre/post force-stop, emulator health check, auto-repair
  - Emulator GPU changed from swiftshader_indirect to auto (fixed loading crashes)

#### Results by category

**D2 full — 10 PASS** (all reach level "Ahayweh Gate"):
- Descent II (USA), Descent II (USA) (Alt), Descent II (USA) (v1.1), Descent II (USA) (Rerelease)
- Descent II (Europe), Descent II (Europe) (v1.1)
- Definitive Collection (Europe) Disc 2, Definitive Collection (USA) Disc 2

**D2 GOG — 2 PASS** (both reach level "Ahayweh Gate"):
- descent_2_enUS_1_0_51877, setup_descent_2_1.1_(16596)

**D2 OEM/Quartzon — 6 SKIP** (can_launch=false, expansion-only):
- Descent II - Destination Quartzon (Europe), (USA), (USA Diamond OEM), (USA Logitech OEM)
- Descent II - Destination Quartzon 3D (Europe)
- Descent-II-Destination-Quartzon_Win_EN_ISO-Version

**D2 Vertigo — 3 SKIP** (can_launch=false, expansion-only):
- Descent II - The Vertigo Series (USA)
- (2 Vertigo entries from Definitive Collection discs)

**D2 Demo — 1 FAIL** (game process alive but never reaches menu):
- Descent II (USA) (3-Level Interactive Preview) — uses d2demo.* files, may need special engine support

**D1 full — 9 FAIL** (crash on startup — no D1 engine on Android):
- Descent (USA), Descent (Europe), Descent (Europe) (Alt)
- Descent - Anniversary Edition (USA), (Brazil)
- Definitive Collection (Europe) Disc 1, Definitive Collection (USA) Disc 1
- descent_enUS_1_0_35122 (GOG D1), setup_descent_1.4a_(16596) (GOG D1)

**D1 expansion — 1 FAIL** (crash — no D1 engine):
- Descent - Destination Saturn (USA)

**D1 demo — 1 FAIL** (crash — no D1 engine):
- Descent - Test Flight (USA)

**D1 levels — 2 PASS** (file-only, no launch needed):
- Descent - Levels of the World (USA) — 191 files verified
- Dimensions for Descent (USA) — 88 files verified

#### Known limitations
- D1-only sources require a D1 engine build (not yet available on Android)
- D2 OEM/Quartzon and Vertigo sets are expansion-only — need base game files to launch
- D2 demo (3-Level Preview) may use incompatible demo-specific data format

---

## Phase 4: Direct .sow Import — DONE

**Goal:** Allow importing a standalone `.sow` file and populating a file set with it, using the existing SOW extraction pipeline but triggered directly (no disc image step).

**Result:** All 3 sub-steps complete. `import_sow` broadcast extracts 16 files from descent2.sow into a file set. Setup introspection confirms `d2.ready: true` with all 9 required files found. Also fixed `create_set` handler to not crash on duplicate set names.

### 4.1: Add `import_sow` broadcast handler — *parallel with 4.2*
- In `SetupActivity.kt` (~line 133), add handler following the `import_gog` pattern
- Background thread, calls `DiscImportBridge.extractSowFiles(path, setDir.absolutePath, null)`
- All JNI/C plumbing already exists: `nativeExtractSowFiles()` in `jni_disc_import.c` (line 363), `sow_extract()` in `sow_extract.h`
- Extension filter already includes hog, ham, pig, s11, s22, mn2, mvl, dxa, cfg, txt, 256

### 4.2: Update file picker for .sow recognition — *parallel with 4.1*
- When user picks a `.sow` file in the SetupActivity UI, detect extension and route to SOW extraction dialog
- Similar to how `.exe`/`.pkg` detection was added for GOG import in phase 3.6
- Show extraction progress + result count, then trigger readiness refresh

### 4.3: End-to-end verification — *depends on 4.1*
- Extract `descent2.sow` from one of the D2 CD data_tracks (e.g., Descent II (USA) has `d2data/descent2.sow`)
- `adb push descent2.sow /data/local/tmp/`
- `adb shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command import_sow --es path /data/local/tmp/descent2.sow`
- Introspect setup → verify files extracted (should include descent2.hog, descent2.ham, all .pig files)
- Launch game → verify it reaches "Ahayweh Gate" (level 1)

#### Relevant files
- `SetupActivity.kt` — add `import_sow` handler at ~line 133
- `DiscImportBridge.kt` — already has `extractSowFiles()`, `scanSowFiles()`
- `jni_disc_import.c` — already has `nativeExtractSowFiles()` at line 363
- `sow_extract.h` — `sow_extract()` signature

#### Verification
- Build APK, deploy to emulator
- Broadcast `import_sow` with a standalone .sow path → introspect → confirm files appear
- Launch game → introspect → confirm `current_level_name == "Ahayweh Gate"`

---

## Phase 5: Deep D2 GOG Regression (Audio + Movies) — DONE

**Goal:** Verify the most common installer (D2 GOG .exe) beyond "can it boot" — confirm audio tracks play and intro movies aren't silently skipped. Requires adding new introspection fields.

### Results
- **5.1 + 5.2**: Redbook audio + movie introspection fields added to `game_introspect.cpp` and `movie.c`
- **5.3**: Build succeeded. Initial SIGSEGV crash due to `nullptr` assignment in nlohmann json — fixed by using conditional `std::string()` instead
- **5.4**: Verified in-game at level 1 ("Ahayweh Gate") with GOG audio files extracted:
  - `redbook.enabled: true`, `num_tracks: 9`, `current_track: 4`, `play_status: "playing"` — CD audio confirmed working
  - `movie.last_name: "pla.mve"`, `movie.last_result: "not_played"` — movies attempted but MVE files inside MVL containers not individually accessible
  - Player: score=0, shields=100, energy=100, lives=3, level=1
  - RBAudio logcat: "Number of tracks: 9", "Playing tracks 4–9", "CD render thread started"
- **5.5**: Deferred — full end-to-end GOG import + deep regression to be done as part of Phase 6
- **Key lesson**: Never assign `nullptr` to nlohmann json via subscript operator — causes SIGSEGV
- **Config timing fix**: `enableRedbookInConfig()` added to Kotlin — sets `MusicType=2` and `OrigTrackOrder=1` in `descent.cfg` after GOG import with audio. The C engine's `android_apply_initial_defaults()` only runs when `descent.cfg` doesn't exist, which is too late since Kotlin creates it on first launch.

### 5.1: Add Redbook audio fields to game introspection — DONE
- In `game_introspect.cpp`, added `extern "C"` declarations for `RBAEnabled()`, `RBAGetTrackNum()`, `RBAGetNumberOfTracks()`, `RBAPeekPlayStatus()`
- Serialized as `"redbook": { "enabled": bool, "current_track": int, "num_tracks": int, "play_status": "playing"|"paused"|"stopped" }`
- Verified: `redbook.enabled: true`, `num_tracks: 9`, `current_track: 4`, `play_status: "playing"`

### 5.2: Add movie playback fields to game introspection — DONE
- In `movie.c`, added globals `g_current_movie_name`, `g_last_movie_name`, `g_last_movie_result`
- In `game_introspect.cpp`, serialized as `"movie": { "current": str, "last_name": str, "last_result": "played"|"aborted"|"not_played"|"none" }`
- Verified: `movie.last_name: "pla.mve"`, `movie.last_result: "not_played"` (MVE inside MVL container not individually accessible)

### 5.3: Audio verification in test script — DONE
- Added redbook introspection checks to `run_extract_test.ps1` after in-game verification
- When `.gog`/`.inst` files are in the set: verifies `redbook.enabled`, `num_tracks > 0`, `play_status == "playing"`
- When no audio files: logs "no audio files in set, skipping redbook check"
- Non-blocking: logs result as info, doesn't fail the test (audio is optional)

### 5.4: Movie verification in test script — DONE
- Added movie introspection check to `run_extract_test.ps1` after in-game verification
- Logs `movie.last_name` and `movie.last_result` for diagnostics
- Non-blocking: movies in MVL containers aren't individually loadable, so `not_played` is expected for GOG

### 5.5: Full deep regression run — DONE (manual)
- Test subject: `setup_descent2.exe` (D2 GOG .exe)
- Import via `import_gog` broadcast with `include_audio=true`
- Verified: 21 files extracted, game boots, level 1 = "Ahayweh Gate"
- Redbook: `enabled=true`, `num_tracks=9`, `current_track=4`, `play_status="playing"`
- Movie: `last_name="pla.mve"`, `last_result="not_played"` (MVE in MVL)
- Config: `MusicType=2`, `OrigTrackOrder=1` auto-set by `enableRedbookInConfig()`

### 5.6: Optional audio extraction during GOG import — DONE
- See `GOG_AUDIO_OPTIONAL_EXTRACT.md` for full plan
- `.gog`/`.inst` extraction is optional via checkbox in GogImportDialog
- `enableRedbookInConfig()` sets `MusicType=2` + `OrigTrackOrder=1` after import with audio
- `hasLegacyGog()` case sensitivity fixed in AudioSourceManager.kt
- Verified both paths: with audio (21 files, redbook works) and without (19 files, no audio)

#### Relevant files (all modified/verified)
- `game_introspect.cpp` — `redbook` + `movie` sections added
- `rbaudio_bin.c` — public getters used via extern C declarations
- `movie.c` — `g_current_movie_name`, `g_last_movie_name`, `g_last_movie_result` globals added
- `run_extract_test.ps1` — redbook + movie introspection checks added after in-game verification
- `SetupActivity.kt` — `enableRedbookInConfig()`, GogImportDialog audio checkbox, broadcast handler updated
- `jni_gog_import.c` — `includeAudio` param, `is_audio_extension()`
- `pkg_reader.h/c` — `skip_audio` param
- `GogImportBridge.kt` — `includeAudio` param, `isAudioFile()`
- `AudioSourceManager.kt` — case-insensitive `hasLegacyGog()`

#### Verification — DONE
- Built with introspection changes, deployed to emulator
- Introspect in-game: `redbook.enabled == true`, `num_tracks == 9`, `play_status == "playing"`, `current_track == 4`
- Movie: `last_name == "pla.mve"`, `last_result == "not_played"` (MVE inside MVL)
- Config auto-update verified: `MusicType=2`, `OrigTrackOrder=1` set after import with audio
- Audio exclusion verified: 19 files without .gog/.inst when `include_audio=false`

---

## Phase 6: Regression Result Tracking in Spec Files

**Goal:** Persist test pass/fail results directly in the json5 spec files so results are git-trackable, diffs show regressions/fixes, and no timestamps or other noisy fields cause spurious diffs.

### 6.1: Git-track the spec files — *no dependencies, do first*
- Currently `game_data/` is gitignored (contains large binary media)
- Add exception to `.gitignore`: `!game_data/**/*_regression.json5`
- This un-ignores just the small json5 spec files while keeping binaries ignored
- `git add game_data/**/*_regression.json5` to stage all 33 specs

### 6.2: Define `last_test_result` schema — *parallel with 6.1*
- Fields (all deterministic, no timestamps):
  - `status`: `"pass"` | `"fail"` | `"skip"`
  - `failure_step`: `null` on pass, one of a defined vocabulary on fail/skip
  - `level_reached`: level name string or `null`
  - `files_verified`: count of expected_files confirmed present on device
  - `classification_confirmed`: `true`/`false` — game type matched expected
  - `test_mode`: `"full"` (with game launch) or `"file_only"`
- Failure step vocabulary:
  - `"file_push_failed"` — couldn't push files to device
  - `"canary_failed"` — empty set canary check failed
  - `"files_missing"` — expected files not found after push
  - `"not_ready"` — `d2.ready`/`d1.ready` false after file push
  - `"launch_timeout"` — game didn't start or reach menu
  - `"menu_timeout"` — stuck in menus
  - `"level_mismatch"` — reached game but wrong level/mission
  - `"crash"` — game process died
  - `"d1_no_engine"` — D1-only set, no D1 engine on Android
  - `"expansion_only"` — expansion set, needs base game (skip status)

### 6.3: Add `Write-TestResult` function to `run_extract_test.ps1` — *depends on 6.2*
- After test completes (pass, fail, or skip), write `last_test_result` to the spec json5 file
- Implementation: Read file text → if `last_test_result` block exists, regex-replace it; otherwise insert before final `}`
- Must handle json5 format (comments, trailing commas) — read with `Read-Json5` for validation, write with targeted text replacement
- Simpler alternative: always strip old `last_test_result` block + trailing `}`, then append new block + `}`
- The result is written at the end of the test (both success and failure paths)

### 6.4: Update orchestrator `run_all_extract_tests.ps1` — *depends on 6.3*
- After all tests, print count of specs updated
- Add note in summary about which specs changed vs stayed same
- No `-DryRun` needed initially — results are deterministic so re-runs without changes produce no diff

### 6.5: Initial population run — *depends on 6.1-6.4, ideally phases 4+5 done*
- Run full `run_all_extract_tests.ps1` to populate all 33 specs
- Expected initial results: 21 pass, 6 skip (expansion-only), 1 skip (D2 demo TBD), ~5 fail (D1 no engine)
- `git add` + `git commit` the updated specs as baseline
- Future re-runs that change results will show meaningful git diffs

#### Relevant files
- `.gitignore` — add `!game_data/**/*_regression.json5`
- `run_extract_test.ps1` — add `Write-TestResult` function
- `run_all_extract_tests.ps1` — summary of result updates
- `generate_regression_specs.ps1` — may need awareness of result fields (preserve on regenerate)
- All 33 `*_regression.json5` files in `game_data/` subdirectories

#### Verification
- Run single test → verify spec file has `last_test_result` with correct status
- Re-run same test → `git diff` shows no changes (deterministic)
- Introduce deliberate failure (e.g., wrong expected_level1) → `git diff` shows status change
- `git status` shows json5 files tracked despite `game_data/` being in gitignore

---

## Identified Gaps / Unfinished Work

### D2 Demo (3-Level Preview) — needs investigation, not a full phase
- Status: FAIL in 3a.6 — game process alive but never reaches menu
- Engine supports `d2demo.hog` via fallback (`inferno.c:359`: tries `descent2.hog` then `d2demo.hog`)
- Demo detected by HOG size (2292566), uses `d2demo.ham`, `d2demo.pig`, levels `d2leva-1.sl2`
- Likely issue: `d2.ready` check in setup introspection may require `descent2.hog` specifically, rejecting `d2demo.hog`. Or test script pushes files that don't include the demo-specific filenames
- Fix: update `d2.files[]` readiness check to accept `d2demo.hog` as alternative, then re-test

### D1 Engine on Android — out of scope
- All D1-only tests fail (no d1x-redux binary on Android). Separate project effort.

### import_disc broadcast command — not planned
- Noted as unimplemented in 3.8. Not needed for current test pipeline.

### Expansion-only sets (Quartzon, Vertigo) — future work
- 6 D2 OEM + 3 D2 Vertigo sets skip because they need base game files merged. Could be tested later by combining sets.

---

## Dependency Map

```
Phase 4 (SOW import)          ─── independent, can start immediately
Phase 5 (deep regression)     ─── independent from phase 4
  5.1 + 5.2 (introspection)  ─── parallel, no deps
  5.3 + 5.4 (test scripts)   ─── depend on 5.1 + 5.2
  5.5 (full run)              ─── depends on 5.3 + 5.4
Phase 6 (result tracking)     ─── 6.1-6.2 independent; 6.3+ depend on 6.2
  6.5 (initial run)           ─── best done after all phases complete
```

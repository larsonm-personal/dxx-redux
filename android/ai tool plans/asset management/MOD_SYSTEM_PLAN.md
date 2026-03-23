# File Sets & Mod System — Implementation Plan

## 1. Overview

This plan adds **SAF leave-in-place file access** (the engine reads game files directly from their original location, no copy), **file sets** (multiple versions of the base game), and eventual **mod layering**. SAF leave-in-place is the foundational layer — file sets and all later features build on top of it.

**Scope:** D2 only for the initial release. D1 engine changes deferred.

### Core Concepts

| Concept | Description |
|---------|-------------|
| **SAF leave-in-place** | Game files stay where the user chose them; the engine reads via fd acquired from SAF |
| **File set** | A named collection of game data files, tracked by the SAF manifest |
| **"default" set** | The initial set — always exists — cannot be deleted |
| **Active set** | The one the engine uses when launched; persisted across restarts |
| **Mod** (future) | An add-on layered *on top of* the active set at higher priority |

### Architecture

The key insight: instead of copying files to app-private storage and mounting that directory in PhysFS, we register a **custom PhysFS archiver** that serves files on demand by calling back to Java via JNI to get file descriptors from Android's `ContentResolver`. Files stay in their original location (Downloads, SD card, etc.). The app stores only a metadata file (`.saf_manifest.json`) mapping filenames to content URIs.

```
User's files (Downloads, SD card, etc.)
    │
    │  SAF content:// URIs (persistent permissions)
    ▼
.saf_manifest.json  ←  filename → URI mapping
    │
    │  Parsed by custom PhysFS archiver at engine init
    ▼
saf_archiver (C)  →  JNI callback  →  ContentResolver.openFileDescriptor()
    │                                        │
    │  PHYSFS_Io (pread-based)               │  native fd (detachFd)
    ▼                                        ▼
PhysFS search path  ←  game engine reads files normally
```

### PhysFS Search Path Order

```
0. files/sets/<active-set>/   ← highest (game data for active set, always present)
1. files/                     ← write dir + configs/saves + music (no game data)
2. [SAF archiver mount]       ← leave-in-place files (primary game data source)
3. APK base dir               ← bundled assets
4. data/                      ← relative subdir
```

All sets (including "default") live under `files/sets/<name>/`. The root `files/` directory holds only configs, saves, music, and metadata — never game data files. This ensures complete isolation between sets: switching to a blank set means the engine sees no game data.

### File Categories

| Category | Scope | Examples |
|----------|-------|---------|
| **D2 Full — Required** | Per-set | `descent2.hog`, `groupa.pig`, 5 level PIGs, etc. |
| **D2 Full — Optional** | Per-set | `intro-h.mvl`, `d2x.hog`, `hoard.ham` |
| **D2 Demo — Required** | Per-set | `d2demo.hog`, `d2demo.ham`, `d2demo.pig` |
| **Music** | Shared (root `files/`) | `.gog`/`.inst` CD images — handled separately, not per-set |
| **Extras** | Shared (root `files/`) | `.dxa` hi-res packs — not per-set |

---

## 2. Design Notes

### 2.1 SAF ↔ PhysFS Bridge: Custom Archiver (Approach D)

PhysFS is path-based (`open(path)`, `opendir()`, `stat()`). SAF is URI/fd-based (`content://` URIs, `ParcelFileDescriptor`). Evaluated bridging approaches:

| # | Approach | Verdict |
|---|----------|---------|
| A | `/proc/self/fd/N` paths | Only per-file, can't `opendir()` an fd |
| B | `MANAGE_EXTERNAL_STORAGE` | Invasive permission, Play Store policy issues |
| C | `PHYSFS_mountIo` per archive | Only for HOG/MVL, not PIG/HAM/S22 |
| **D** | **Custom `PHYSFS_registerArchiver`** | **Adopted.** Full integration, all file types. |
| E | Persistent URI + delta-copy | Still duplicates storage |

Approach D registers a custom archiver via `PHYSFS_registerArchiver()`. The archiver presents `.saf_manifest.json` as a virtual directory. When PhysFS opens a file by name, the archiver calls back to Java to get a native fd, then returns a `pread`-based `PHYSFS_Io`. All file types work — archives (HOG, MVL) and non-archives (PIG, HAM, S22) alike.

### 2.2 The `duplicate()` Solution: `pread()`

PhysFS requires `PHYSFS_Io.duplicate()` to return an independent handle (separate seek position). The `pread(fd, buf, len, offset)` syscall reads at a specific offset without modifying the fd's shared file position. Combined with `dup(fd)` (which creates a new fd to the same file) and per-handle virtual offset tracking, each duplicated `PHYSFS_Io` is fully independent. No JNI round-trip needed for `duplicate()`.

### 2.3 PhysFS Search Paths (Not Symlinks)

Symlinks have API level problems (`Files.createSymbolicLink()` requires API 26; minSdk is 23) and lifecycle complexity. The PhysFS search path approach requires no symlinks.

### 2.4 Streaming ZIP Extraction

ZIP files are stream-extracted using `java.util.zip.ZipInputStream` — one entry at a time to a temp file, never the whole archive in memory. No OOM risk.

### 2.5 D1 Per-Set

D1 files (`descent.hog`, `descent.pig`, `.dxa` extras) are per-set, same as D2. Both `checkFiles()` calls in the UI and introspection use `setDir` + the set's manifests. D1 engine code (`d1/misc/physfsx.c`) is still unchanged — the D2 engine handles D1 files via PhysFS.

### 2.6 Music — Separate Handling

Music files (GOG CD images for Redbook audio) stay in the root `files/` directory. They are shared across all sets — switching sets does not affect music. Migration explicitly skips `.gog`/`.inst` files.

### 2.7 Mods — Future

Mods layer on top of the active base file set at even higher PhysFS priority. Separate plan.

### 2.8 KnownVersions Organization

The SHA-256 hash table in `KnownVersions.kt` is organized **by package/installer set**, not by file type. All files from a given distribution (e.g., "D2 v1.2 GOG", "D2 Demo v1.04a") are grouped together.

Example structure:

```kotlin
// ── D2 v1.2 (GOG) ──────────────────────────────────────
"descent2.hog" to listOf(VersionEntry("...", "D2 v1.2 (GOG)")),
"descent2.ham" to listOf(VersionEntry("...", "D2 v1.2 (GOG)")),
"groupa.pig" to listOf(VersionEntry("...", "D2 v1.2 (GOG)")),

// ── D2 Demo v1.04a ─────────────────────────────────────
"d2demo.hog" to listOf(VersionEntry("...", "D2 Demo v1.04a")),

// ── ZIP archives ───────────────────────────────────────
"d2demo.zip" to listOf(VersionEntry("...", "D2 Demo v1.04a (ZIP)")),
```

### 2.9 Demo-Specific Required Files

The demo versions have a smaller, different set of required files. A "D2 Demo" set only needs `d2demo.hog`, `d2demo.ham`, `d2demo.pig`. These are their own complete game, not alternatives to full-game files.

### 2.10 Set Name Validation

Set names become directory names. Reject: empty strings, `.`/`..`, names with `/` or `\`, names longer than ~50 chars, and names colliding with existing sets (case-insensitive).

### 2.11 Disk Usage Visibility

The set management dialog shows per-set disk usage. Leave-in-place files show as "0 bytes (linked)" since they consume no app storage.

---

## 3. Phased Implementation

### Phase M1: SAF Leave-in-Place Engine Layer ✅ COMPLETE

**Goal:** Build the foundational C archiver + JNI bridge + Kotlin SAF layer so the engine can read game files directly from their SAF source. No file set UI yet -- just a single flat `.saf_manifest.json` in `filesDir`. Testable independently via adb.

**Status:** Complete. All engine-layer components implemented and tested. The full pipeline works end-to-end: manifest parsing, JNI bridge, pread-based I/O, PhysFS integration, and SetupActivity launch-gate integration. Verified via automated test on emulator.

M1.5-M1.7 (persistent SAF permissions, leave-in-place UI, stale handling) are deferred to M3 where they naturally fit with the file set UI work.

#### M1.1 -- SafManifest.kt (82 lines)

**File:** `android/app/src/main/java/com/dxxredux/app/SafManifest.kt`

Kotlin data model and JSON reader/writer for `.saf_manifest.json`. Maps game filenames to content URIs with file sizes.

- `SafFileEntry(filename, contentUri, sizeBytes)` data class
- `SafManifest(manifestFile)` with `read()`, `write()`, `addOrReplace()`, `remove()`, `isEmpty()`
- Companion: `FILENAME = ".saf_manifest.json"`, `forDir(dir)` factory
- Filenames lowercased on `addOrReplace` for case-insensitive matching

#### M1.2 -- JNI bridge: jni_saf.c + MainActivity.openSafFile (91 + 18 lines)

**C file:** `android/app/src/main/cpp/jni_saf.c`
**Kotlin:** `MainActivity.kt` lines 81-98

`saf_open_file(const char *content_uri)` returns a native fd or -1:

- **Test mode:** URI starts with `/` -> direct `open()` in C (no JNI round-trip). This enables adb-based testing with files at `/data/local/tmp/`.
- **Production mode:** JNI callback to `MainActivity.openSafFile()` -> `ContentResolver.openFileDescriptor()` -> `detachFd()` transfers fd ownership to native.
- Robust error handling: `ExceptionCheck`, local ref cleanup, `AttachCurrentThread`/`DetachCurrentThread` for non-main threads.

#### M1.3 -- PhysFS archiver: physfs_archiver_saf.c (~540 lines)

**File:** `d2/arch/android/physfs_archiver_saf.c`

Custom `PHYSFS_Archiver` that presents `.saf_manifest.json` as a virtual directory:

- **JSON parser:** ~120 lines of hand-rolled minimal parser (`skip_ws`, `parse_json_string` with escape handling, `parse_json_int`, `parse_manifest`). Sufficient for the flat array-of-objects format.
- **Structs:** `SafEntry` (filename/uri/size), `SafArchive` (entry table), `SafIoData` (fd/pos/len/uri per-handle)
- **I/O:** `pread()`-based reads with per-handle virtual seek position. `safio_duplicate()` uses `dup(fd)` for independent handles. Read-only (write/append return `PHYSFS_ERR_READ_ONLY`).
- **Lookup:** `find_entry()` uses `strcasecmp` for case-insensitive matching.
- **Archiver vtable:** `const PHYSFS_Archiver SAF_Archiver` -- `.json` extension, read-only, no symlinks.

#### M1.4 -- PhysFS registration in physfsx.c

**File:** `d2/misc/physfsx.c` (in `#ifdef ANDROID` block, ~30 lines)

Search path order at engine init:
1. `filesDir` (pref dir) -- write dir + configs/saves
2. SAF manifest (appended via `PHYSFS_mount`) -- leave-in-place files
3. APK base dir
4. `data/` subdir

Also sets `SndDigiSampleRate = 22050` and skips `ReadCmdArgs` (Android-specific workarounds).

#### M1.5 -- SetupActivity SAF integration

**File:** `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`

`checkFiles()` accepts optional `SafManifest?` parameter. Files not found on disk are checked against the manifest (case-insensitive, including alternatives). Files in SAF manifest count as `found=true` for the launch gate (`can_launch`). Two call sites updated: introspection serializer and Compose UI.

#### M1.6 -- Build integration

**File:** `android/app/src/main/cpp/CMakeLists.txt`

Both `jni_saf.c` and `physfs_archiver_saf.c` added to the `d2x-redux` shared library target. No conditional guards needed (Android-only CMakeLists).

#### M1.7 -- Automated testing

**Test automation script:** `android/game_scripts/test_saf_basic.json` (37 steps)
- Wait for init -> pilot select -> assert main menu -> new game -> accept difficulty -> skip briefings (15x enter) -> wait for `in_game=true` -> assert level 1 + game mode -> introspect

**Test orchestration:** `android/test_saf_archiver.ps1` (~360 lines, PowerShell)
- Params: `-NoBuild` (skip Gradle), `-NoCleanup` (leave artifacts for debugging)
- Flow: Build APK -> stop game -> install -> push `descent2.ham` to `/data/local/tmp/test_saf/` -> remove from app files -> create `.saf_manifest.json` -> launch SetupActivity -> verify `can_launch=true` -> broadcast launch -> run automation script -> monitor logcat for `SCRIPT_RESULT` -> report PASS/FAIL/CRASH/TIMEOUT -> cleanup (restore file, remove manifest)

**Key testing discovery:** `/sdcard/` is not accessible from native `open()` on modern Android due to scoped storage (errno=13 EACCES). Test mode uses `/data/local/tmp/` instead, which is world-readable on emulators. Production mode handles this via ContentResolver which has proper SAF permissions.

**Test results:** SAF archiver test PASS (37 steps), regression test PASS (41 steps).

---

### Phase M2: File Set Infrastructure ✅ COMPLETE

**Goal:** Data model + persistence + C code for switching between multiple sets of game files. Each set has its own `.saf_manifest.json` (or copied files, or a mix).

**Status:** Complete. All components implemented and tested. The file set system supports multiple named sets with per-set SAF manifests, persistent active set selection, KnownVersions organized by distribution package, and demo file list definitions.

**Depends on:** M1 (SAF archiver is the primary way sets provide files to the engine).

#### M2.1 — FileSetManager class (183 lines)

**File:** `android/app/src/main/java/com/dxxredux/app/FileSetManager.kt`

Manages multiple file sets with CRUD operations and persistence:

- `FileSetInfo(name, createdAt, source)` data class
- `listSets()` — reads `file_sets.json`, always includes "default"
- `getActive()` / `setActive(name)` — get/set active set name
- `createSet(name, source)` — creates `filesDir/sets/<name>/` with validation (no path separators, max 50 chars, can't recreate "default")
- `deleteSet(name)` — deletes set dir recursively, switches to "default" if deleted set was active
- `getSetDir(name)` — returns `filesDir/sets/<name>/` for all sets including "default" (changed in M6; originally "default" mapped to `filesDir`)
- `safManifestForSet(name)` — SAF manifest for a specific set
- `diskUsage(name)` — calculates directory size (local files only, not SAF-linked)
- `writeActiveSetPath()` — writes `.active_set_path` for the C engine; always writes since M6 (previously deleted file when "default" was active)

#### M2.2 — Persistence via file_sets.json

Handled within `FileSetManager`. JSON format:

```json
{
  "active": "default",
  "sets": [
    { "name": "default", "createdAt": 0 },
    { "name": "d2-demo", "createdAt": 1710000000000, "source": "SAF linked" }
  ]
}
```

Private `loadConfig()`/`saveConfig()` with error recovery (returns empty JSONObject on parse failure).

#### M2.3 — C code in physfsx.c (file-set support, ~40 lines)

**File:** `d2/misc/physfsx.c` (in `#ifdef ANDROID` block)

Extended the existing M1 block with file-set support:

1. Reads `.active_set_path` — if present and non-empty, prepends that directory to PhysFS search path (so set's copied files take priority)
2. Mounts the SAF manifest from the active set's directory; falls back to `filesDir/.saf_manifest.json` for the "default" set
3. Uses `char[512]` buffers (sufficient for Android internal paths)

Search path order after init:
```
0. sets/<active>/   ← always present (game data for active set)
1. filesDir/        ← write dir + configs/saves + music (no game data after M6 migration)
2. [SAF manifest]   ← leave-in-place files for active set
3. APK base dir     ← bundled assets
4. data/            ← relative subdir
```

#### M2.4 — KnownVersions.kt reorganized by package

**File:** `android/app/src/main/java/com/dxxredux/app/KnownVersions.kt`

Restructured from individual file entries to per-distribution grouping with comments demarcating each package. Added D2 Demo entries. Helper functions:

- `lookup(filename, sha256)` — returns version name for known hash
- `identifyPackage(files: Map<String, String>)` — given filename→sha256 pairs, identifies the distribution package (e.g. "D2 v1.2 (GOG)", "D2 Demo v1.04a")
- `shortHash(sha256)` — last 8 hex chars for unknown files

#### M2.5 — Demo file lists in SetupActivity

**File:** `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`

Added `D2_DEMO_FILES` list alongside existing `D2_FILES`. Added `detectFileList()` helper that checks which files are present and returns the appropriate list:

- If demo files (`d2demo.hog`, `d2demo.ham`, `d2demo.pig`) are detected, returns `D2_DEMO_FILES`
- Otherwise returns `D2_FILES` (full game)
- Used by `checkFiles()` and the Compose UI to display the correct requirements

#### M2.6 — Write .active_set_path before engine launch

**File:** `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` (two call sites)

`FileSetManager(filesDir).writeActiveSetPath()` is called before engine launch at both launch paths:
1. Broadcast receiver (for `com.dxxredux.SETUP_COMMAND --es command launch`)
2. Compose UI `onLaunchGame` callback

This ensures the C engine always reads the current active set path at init.

#### M2.7 — Testing

Verified with existing test suite:
- SAF archiver test: PASS (37 steps) — validates default-set behavior
- Regression test (test_launch_to_automap): PASS (41 steps) — validates no regressions
- Build: `BUILD SUCCESSFUL`

---

### Phase M3: File Set UI ✅ COMPLETE

**Goal:** User can create, switch, and delete file sets. File imports (SAF leave-in-place or copy) route to the active set.

**Status:** Complete. All Compose UI components implemented and tested. Users can see and switch active sets, create new sets, delete named sets, and imported files route to the active set's directory. File detail dialogs show SAF leave-in-place info with Unlink action for linked files.

**Depends on:** M2 (FileSetManager, set persistence).

#### M3.1 — Active set indicator

**File:** `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`

Row at the top of the files pane showing the active set:

```
Files in use: default     [Change]
```

Implemented as a `Row` with the set name in bold and a `TextButton("Change")` that opens the set management dialog. Always visible above the file sections.

#### M3.2 — Set management dialog (SetManagementDialog composable)

**File:** `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` (~130 lines)

`AlertDialog` showing:

- **Current set** name and disk usage (`formatSize()`)
- **Switch list** — clickable rows for each other set with size display
- **Add new set** — inline `OutlinedTextField` with Create/Cancel, validates via `FileSetManager.createSet()` (catches `IllegalArgumentException` for invalid names)
- **Delete current set** — 2-step confirmation for non-default sets, calls `FileSetManager.deleteSet()` then switches to default

Switching sets: calls `fileSetManager.setActive(name)`, updates `activeSetName` state, dismisses dialog, triggers `onRefresh()`. All downstream `remember(activeSetName)` values recompute automatically.

#### M3.3 — Route file selection to active set

**File:** `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`

The import flow (file picker → `importFile()` → hash → `manifest.upsert()`) now targets the active set's directory:

- `File(setDir, canonicalName)` replaces `File(filesDir, canonicalName)` for destination
- `importFile(context, f, setDir)` replaces `importFile(context, f, filesDir)`
- `manifest` is `AssetManifest(setDir)` — per-set manifest

For the default set, `setDir == filesDir`, so behavior is unchanged. For named sets, files are imported to `filesDir/sets/<name>/`.

#### M3.4 — Per-set readiness display

**File:** `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`

D2 file status uses the active set's directory and SAF manifest:

```kotlin
val fileSetManager = remember { FileSetManager(filesDir) }
var activeSetName by remember { mutableStateOf(fileSetManager.getActive()) }
val setDir = remember(activeSetName) { fileSetManager.getSetDir(activeSetName) }
val manifest = remember(activeSetName) { AssetManifest(setDir) }
val safManifest = remember(activeSetName) { fileSetManager.safManifestForSet(activeSetName) }
val d2Statuses = remember(refreshTrigger, activeSetName) { checkFiles(setDir, d2FileList, manifest, safManifest) }
```

D1 files always use `filesDir` with separate `rootManifest` and `rootSafManifest` (D1 doesn't support sets).

**Note:** As of M6, D1 files are per-set — `d1Statuses` now uses `setDir`/`manifest`/`safManifest` like D2. The `rootManifest`/`rootSafManifest` variables were removed.

The `checkFiles()` function was refactored to read SAF entries once (outside the `map`) and capture the matching `SafFileEntry` for each file, enabling `safUri` and `safSizeBytes` fields on `FileStatus`.

Hashing (`LaunchedEffect`) is keyed on `activeSetName` — re-hashes when switching sets.

#### M3.5 — FileDetailDialog: leave-in-place info

**File:** `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`

`FileStatus` extended with `safUri: String?` and `safSizeBytes: Long` fields, populated from the matching `SafFileEntry` in `checkFiles()`.

FileDetailDialog shows:

- **SAF-linked files**: Location as "leave-in-place (linked)" with file size; "Unlink" button removes the SAF manifest entry
- **Data-dir files**: Location as "(in data folder)"; 2-step "Delete" confirmation
- **External imports**: "Forget" button (unchanged)

The delete handler uses `detailIsD2` state to select the correct directory (`setDir` vs `filesDir`) and manifest (`manifest` vs `rootManifest`) for operations.

#### M3.6 — Introspection updates

**File:** `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`

`writeIntrospectJson()` updated to be set-aware:

- Creates `FileSetManager` to determine active set and set directory
- D2 file checks use the active set's dir and SAF manifest
- D1 file checks always use `filesDir`
- Output includes `"active_set"` field

`fileStatusArray()` includes `saf_linked: true` and `saf_uri` for SAF-linked files.

#### M3.7 — Testing

Verified with existing test suite:
- SAF archiver test: PASS (37 steps)
- Regression test (test_launch_to_automap): PASS (41 steps)
- Build: `BUILD SUCCESSFUL`

---

### Phase M4: ZIP Import Support ✅ COMPLETE

**Goal:** User can pick `.zip` files containing game data. Extracted files are hashed, identified via KnownVersions, and imported to the active set.

**Depends on:** M3 (set UI for routing extracted files to sets).

#### M4.1 — Accept ZIP in file picker ✅

File picker now accepts `application/zip` MIME type alongside `application/octet-stream` and `*/*`. Selected files are partitioned: `.zip` files → extraction flow, known game filenames → existing import flow.

**As-built:** `filePickerLauncher` callback in `SetupScreen` splits URIs into `zipUris` and `gameUris`. Button text updated to "Select Game Files or ZIP to Import". Helper text mentions `.zip` archives.

#### M4.2 — Streaming ZIP extraction & hashing ✅

`extractZipContents()` suspend function streams `ZipInputStream` one entry at a time:
1. Filters entries by `ALL_GAME_FILENAMES` (case-insensitive, handles nested paths via `substringAfterLast('/')`)
2. Extracts matching entries to `filesDir/tmp/`
3. Computes SHA-256 during extraction (single pass, no re-read)
4. Returns `List<ExtractedFile>` with name, tmpFile, sha256, sizeBytes

**As-built:** `ExtractedFile` data class added. Extraction runs on `Dispatchers.IO`. Progress callback updates `zipProgressFile` state for UI. After extraction, `KnownVersions.identifyPackage()` is called on the collected file hashes.

#### M4.3 — ZIP recognition card ✅

ZIP results shown in a Card composable:
- **Empty:** "No game files found in ZIP archive" with Dismiss button.
- **Recognized:** "✅ Recognized: {packageName}" header, file list with sizes, "Import to Current Set" and "Dismiss" buttons.
- **Unrecognized:** "Found N game file(s)" header, same buttons.

Extraction progress shown in a separate card with indeterminate LinearProgressIndicator and current filename.

#### M4.4 — Place extracted files ✅

"Import to Current Set" button copies each `ExtractedFile.tmpFile` to `setDir` (active set directory), then calls `manifest.upsert()` with the pre-computed SHA-256 hash. No re-hashing needed since hash was computed during extraction.

**As-built:** Uses `File.copyTo(overwrite = true)`. Shows hashing progress bar during import (reuses `hashingFile`/`hashingProgress` state). Import status shows "Imported N of M files from ZIP"

#### M4.5 — Cleanup ✅

`cleanupTmpDir(filesDir)` deletes `filesDir/tmp/` recursively. Called on:
- Successful import completion
- Dismiss button click
- Empty extraction result dismissal

#### M4.6 — M3 Bug Fix (bonus) ✅

Fixed Import All handler that still referenced `filesDir` instead of `setDir` for import destinations (2 occurrences at the original lines 966/972). This was a silent failure from M3's `multi_replace_string_in_file` operation.

---

### Phase M5: Demo Auto-Download ✅ COMPLETE

**Goal:** One-tap download and install of official demo versions.

**Depends on:** M4 (ZIP extraction flow), M3 (set creation).

#### M5.1 — Download URLs ✅

`DemoPackage` data class and `DEMO_DOWNLOADS` list defined in SetupActivity.kt. Uses `https://dxx-redux.com/dl/` base URL (same domain as existing D1 optional file downloads).

**As-built:**
```kotlin
data class DemoPackage(
    val name: String,
    val url: String,
    val description: String,
    val sizeBytes: Long,
    val files: List<String>,
)
```

Currently configured: D2 Demo (d2demo.zip, ~5.5 MB). KnownVersions demo hashes are placeholder — will be filled in when actual demo files are obtained and verified.

#### M5.2 — Demo section in UI ✅

Demo download cards appear when `!canLaunch && !gameRunning` (no required files found for either D1 or D2), below the existing `MissingFilesHelp` card. Each demo shows:
- Package name with game controller emoji
- Description and download size
- "Download & Install" button (disabled during active download)
- Progress bar with percentage during download
- Error message display with retry capability

#### M5.3 — Download → extract → import ✅

Flow:
1. Create `filesDir/tmp/`, download ZIP via existing `downloadFile()` with progress callback
2. Extract ZIP using M4's `extractZipContents()` (streams entries, hashes during extraction)
3. Copy extracted files to active `setDir`, register in `AssetManifest` via `manifest.upsert()`
4. Cleanup `filesDir/tmp/` via `cleanupTmpDir()`
5. Show import status message, refresh UI

Error handling: download failure and empty extraction both show error message and clean up tmp dir.

#### M5.4 — Hosting (deferred)

Demo ZIP files need to be hosted at `https://dxx-redux.com/dl/d2demo.zip`. Not yet deployed — download will show "Download failed" until the file is hosted. KnownVersions hashes need to be computed from actual demo files and uncommented.

#### M5.5 — Label bug fix (bonus) ✅

Fixed `GameSectionHeader` to use context-appropriate "not ready" labels:
- **D1/D2 sections:** "✗ Missing" (was incorrectly showing "✗ Missing, will use MIDI")
- **Music section:** "✗ Missing, will use MIDI" only when D1 or D2 is ready (MIDI comes from game files); "✗ Missing" when neither game is installed

---

### Phase M6: Set Isolation & Default Migration ✅ COMPLETE

**Goal:** Fully isolate file sets so that switching to a blank set means the engine sees no game data. Move all game data (including "default" set) out of `filesDir` root into `sets/<name>/`.

**Problem:** Previously, `getSetDir("default")` returned `filesDir` directly. Since `filesDir` is always in the PhysFS search path (for configs/saves/music), game data files in `filesDir` leaked into every set. A "blank" set could still launch the game because D1/D2 files were visible via `filesDir`.

**Depends on:** M5 (all prior set infrastructure).

#### M6.1 — `getSetDir` always uses `sets/` ✅

**File:** `FileSetManager.kt`

`getSetDir(name)` now always returns `File(setsDir, name)` with `.also { it.mkdirs() }`. The "default" set lives at `filesDir/sets/default/`, same as named sets. No special case.

#### M6.2 — `writeActiveSetPath` always writes ✅

`.active_set_path` is always written (even for "default"), since `sets/default/` ≠ `filesDir`. The C engine always sees a set directory prepended to the search path.

#### M6.3 — One-time migration ✅

**File:** `FileSetManager.kt` — `migrateDefaultSetIfNeeded()`

Idempotent migration on first launch after upgrade:
1. Creates `sets/default/`
2. Scans `filesDir` for files with game data extensions (case-insensitive): `.pig`, `.hog`, `.ham`, `.mvl`, `.s11`, `.s22`, `.mn2`, `.msn`, `.dxa`, `.pog`, `.rl2`, `.dtx`
3. Scans for known game data subdirectories: `missions/`
4. Moves matches to `sets/default/` via `File.renameTo()` (atomic, O(1) on same filesystem)
5. Moves `.asset_manifest.json` and `.saf_manifest.json` if present
6. Skips music files (`.gog`, `.inst`) — they stay in `filesDir` (shared)
7. Writes `migration_version: 1` in `file_sets.json`

Extension-based scanning handles arbitrary mod filenames (e.g., `panic.hog`, `vertigo.mn2`) not just files in `ALL_GAME_FILENAMES`.

#### M6.4 — D1 files per-set ✅

**File:** `SetupActivity.kt`

- `d1Statuses` now uses `setDir`/`manifest`/`safManifest` (same as D2), keyed on `activeSetName`
- Removed `rootManifest` and `rootSafManifest` — no longer needed
- D1 optional file downloads (`.dxa`) now target `setDir` instead of `filesDir`
- File detail popup uses `setDir`/`manifest`/`safManifest` for both D1 and D2
- Introspection D1 checks use `setDir`

#### M6.5 — What stays in `filesDir` (shared)

| Pattern | Purpose |
|---------|---------|
| `*.ini`, `*.plr`, `*.sg*`, `*.cfg` | Configs & saves |
| `*.gog`, `*.inst` | Music (Redbook CD images) |
| `file_sets.json`, `.active_set_path` | Set metadata |
| `introspect.json`, `setup_introspect.json` | Debug |
| `sets/` | All game data (per-set) |
| `tmp/` | Temporary extraction |

#### M6.6 — Testing ✅

Verified on emulator:
- Blank set "ff": D1 not ready, D2 not ready, `can_launch: false`
- Default set: D1 ready, D2 ready, `can_launch: true`
- Game launches and runs with files in `sets/default/`
- Music files (`.gog`, `.inst`) correctly stayed in `filesDir`
- Migration marker `migration_version: 1` written to `file_sets.json`

---

### Phase M7: Mod Layering (Future — separate plan)

Mods layer on top of the active base file set at even higher PhysFS priority. Deferred until file sets are stable. Will be specified in a separate plan.

---

## 4. Phase Dependency Graph

```
M1 (SAF Leave-in-Place Engine Layer)  ← FOUNDATION
 ├── M1.1-M1.3: SafManifest + JNI bridge + C archiver
 ├── M1.4: Register + mount in PHYSFSX_init
 ├── M1.5-M1.7: Kotlin SAF permissions + UI choice + stale handling
 └── M1.8: adb test suite (independent validation)
      │
      ▼
M2 (File Set Infrastructure)
 ├── M2.1-M2.2: FileSetManager + file_sets.json
 ├── M2.3: C code for set switching + per-set SAF manifest
 └── M2.4-M2.5: KnownVersions reorg + demo file lists
      │
      ▼
M3 (File Set UI)
 ├── M3.1-M3.2: Set indicator + management dialog
 ├── M3.3: Route imports (SAF or copy) to active set
 └── M3.4-M3.5: Per-set readiness + file detail popup
      │
      ▼
M4 (ZIP Import)
 ├── M4.1-M4.2: ZIP detection + streaming extraction
 └── M4.3-M4.5: Recognition dialog + place files + cleanup
      │
      ▼
M5 (Demo Auto-Download)
 └── M5.1-M5.4: Download URLs + UI + extraction + hosting
      │
      ▼
M6 (Set Isolation & Default Migration)
 ├── M6.1-M6.2: getSetDir always uses sets/, writeActiveSetPath always writes
 ├── M6.3: One-time migration (extension-based scan + renameTo)
 └── M6.4: D1 files per-set
      │
      ▼
M7 (Mod Layering — future, separate plan)
```

Each phase depends on the one above it. M1 can be built and tested independently via adb — no UI, no file sets, just the raw archiver+JNI+PhysFS integration.

---

## 5. Risk Assessment

| Risk | Mitigation |
|------|-----------|
| `pread`/`dup` behavior varies across devices | `pread` is POSIX standard, supported on all Android versions since API 1. Test on emulator + physical device. |
| JNI callback from PhysFS worker thread | Use `AttachCurrentThread()`/`DetachCurrentThread()`. Cache `JavaVM*` (already done in `jni_main.c`). |
| SAF permission revocation breaks leave-in-place | Detect stale URIs on launch, show warning, offer to re-link or import (copy) as fallback. |
| SAF ContentResolver slow for frequent opens | Files are opened once and held open (PIG paging). Not a hot path — JNI call happens at file-open time, not per-read. |
| JSON parsing in C is fragile | Minimal parser for a known flat format. Alternatively, use `cJSON` (single-file library, MIT, ~30KB). |
| PhysFS archiver registration order matters | Register SAF archiver before any game code runs. The archiver's `.json` extension won't conflict with game files (none are `.json`). |
| PhysFS search-path priority breaks engine assumptions | Test D2 full and D2 demo. Priority only matters when same filename exists in multiple paths. |
| Set directory names collide with engine expectations | Use `files/sets/` — engine never looks there. Configs/saves in `files/` root. |
| Demo zips have unknown contents | Pre-package from known-good sources. Hash everything. |
| Large sets consume storage | Show per-set disk usage. Leave-in-place eliminates duplication for most users. |
| Concurrent set switch while engine running | Only allow switching from SetupActivity. `.active_set_path` read once at init. |

---

## 6. What Doesn't Change

- **Engine game logic** — no changes to how files are read once opened via PhysFS
- **Windows/Linux/Mac builds** — all changes are `#ifdef ANDROID`
- **Config & save files** — always in `files/` root, shared across sets
- **Music handling** — stays in `files/` root, unaffected by set switching
- **Hashing/manifest system** — same `AssetManifest` class, extended with `leaveInPlace` flag
- **D1 engine code** — `d1/misc/physfsx.c` unchanged; D1 file set support is handled by the D2 engine's PhysFS search path
- **File copy option** — importing (copying) files still works as before; leave-in-place is the new default but copy remains available

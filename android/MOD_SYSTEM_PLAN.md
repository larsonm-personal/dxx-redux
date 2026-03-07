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
0. files/sets/<active-set>/   ← highest (file set override, if non-default)
1. files/                     ← write dir + configs/saves + music
2. [SAF archiver mount]       ← leave-in-place files (primary game data source)
3. APK base dir               ← bundled assets
4. data/                      ← relative subdir
```

Copied files (in `files/` or a set subdirectory) shadow SAF files with the same name. This is the correct priority: if the user later copies a file that was previously left in place, the copy wins.

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

### 2.5 D1 Deferred

`d1/misc/physfsx.c` has no `#ifdef ANDROID` block. D1 file set support is out of scope for the initial release.

### 2.6 Music & Extras — Separate Handling

Music files (GOG CD images for Redbook audio) and extras (`.dxa` hi-res packs) stay in the root `files/` directory. They are shared across all sets — switching sets does not affect music.

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

### Phase M1: SAF Leave-in-Place Engine Layer

**Goal:** Build the foundational C archiver + JNI bridge + Kotlin SAF layer so the engine can read game files directly from their SAF source. No file set UI yet — just a single flat `.saf_manifest.json` in `filesDir`. Testable independently via adb.

This is the most technically novel piece. Getting it solid first de-risks everything else.

#### M1.1 — `.saf_manifest.json` format & writer

The metadata file maps game filenames to SAF content URIs:

```json
{
  "files": [
    {
      "filename": "descent2.hog",
      "content_uri": "content://com.android.providers.downloads.documents/document/123",
      "size_bytes": 5038817
    },
    {
      "filename": "groupa.pig",
      "content_uri": "content://com.android.externalstorage.documents/document/primary%3ADownload%2Fgroupa.pig",
      "size_bytes": 153287936
    }
  ]
}
```

This file lives in `filesDir` (for the default set). Later phases put per-set copies in `filesDir/sets/<name>/`.

Kotlin class `SafManifest`:

```kotlin
data class SafFileEntry(
    val filename: String,
    val contentUri: String,
    val sizeBytes: Long
)

class SafManifest(private val manifestFile: File) {
    fun read(): List<SafFileEntry>
    fun write(entries: List<SafFileEntry>)
    fun addOrReplace(entry: SafFileEntry)
    fun remove(filename: String)
    fun isEmpty(): Boolean
}
```

#### M1.2 — JNI bridge: `saf_open_file()`

New C file: `android/app/src/main/cpp/jni_saf.c`

```c
#include <jni.h>
#include <unistd.h>

extern JavaVM *g_jvm;       // from jni_main.c
extern jobject g_activity;  // from jni_main.c

// Opens a SAF content URI and returns a native fd, or -1 on failure.
// The caller owns the fd and must close() it.
int saf_open_file(const char *content_uri)
{
    JNIEnv *env = NULL;
    int need_detach = 0;
    if ((*g_jvm)->GetEnv(g_jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
        need_detach = 1;
    }

    jclass cls = (*env)->GetObjectClass(env, g_activity);
    jmethodID mid = (*env)->GetMethodID(env, cls, "openSafFile",
                                        "(Ljava/lang/String;)I");
    jstring juri = (*env)->NewStringUTF(env, content_uri);
    int fd = (*env)->CallIntMethod(env, g_activity, mid, juri);

    (*env)->DeleteLocalRef(env, juri);
    (*env)->DeleteLocalRef(env, cls);

    if (need_detach)
        (*g_jvm)->DetachCurrentThread(g_jvm);

    return fd;
}
```

Kotlin side (in `MainActivity.kt` since the engine runs in that Activity's process):

```kotlin
// Called from native via JNI
@Keep
fun openSafFile(contentUri: String): Int {
    return try {
        val uri = Uri.parse(contentUri)
        val pfd = contentResolver.openFileDescriptor(uri, "r") ?: return -1
        pfd.detachFd()  // transfers fd ownership to native
    } catch (e: Exception) {
        -1
    }
}
```

#### M1.3 — Custom PhysFS archiver: `physfs_archiver_saf.c`

New file: `d2/arch/android/physfs_archiver_saf.c` (~300–400 lines)

Register at engine init via `PHYSFS_registerArchiver()`, then mount `.saf_manifest.json` into the search path.

**Data structures:**

```c
typedef struct {
    char *filename;
    char *content_uri;
    PHYSFS_sint64 size_bytes;
} SafFileEntry;

typedef struct {
    SafFileEntry *entries;
    int count;
} SafArchiveData;

typedef struct {
    int fd;
    PHYSFS_sint64 pos;   // virtual seek position (per-handle)
    PHYSFS_sint64 len;
    char *uri;           // for re-open fallback
} SafIoData;
```

**Archiver vtable:**

```c
static PHYSFS_Archiver SAF_Archiver = {
    .version = 0,
    .info = {
        .extension = "json",
        .description = "SAF leave-in-place virtual directory",
        .author = "dxx-redux",
        .url = "",
        .supportsSymlinks = 0
    },
    .openArchive  = SAF_openArchive,   // parse .saf_manifest.json
    .enumerate     = SAF_enumerate,     // walk in-memory table
    .openRead      = SAF_openRead,      // JNI → fd → pread Io
    .openWrite     = SAF_openWrite,     // returns NULL (read-only)
    .openAppend    = SAF_openAppend,    // returns NULL
    .remove        = SAF_remove,        // returns 0
    .mkdir         = SAF_mkdir,         // returns 0
    .stat          = SAF_stat,          // cached size/type
    .closeArchive  = SAF_closeArchive   // free table
};
```

**`openArchive`:** Reads the `.saf_manifest.json` file using standard C `fopen`/`fread` + a minimal JSON parser (just enough for the flat array-of-objects format). Builds the in-memory `SafArchiveData` table.

**`openRead`:** Calls `saf_open_file(entry->content_uri)` to get a native fd, then wraps it:

```c
static PHYSFS_Io *SAF_openRead(void *opaque, const char *name) {
    SafArchiveData *arch = (SafArchiveData *)opaque;
    SafFileEntry *entry = find_entry(arch, name);
    if (!entry) return NULL;

    int fd = saf_open_file(entry->content_uri);
    if (fd < 0) return NULL;

    SafIoData *data = malloc(sizeof(SafIoData));
    data->fd = fd;
    data->pos = 0;
    data->len = entry->size_bytes;
    data->uri = strdup(entry->content_uri);

    PHYSFS_Io *io = malloc(sizeof(PHYSFS_Io));
    io->version = 0;
    io->opaque = data;
    io->read = safio_read;
    io->write = NULL;
    io->seek = safio_seek;
    io->tell = safio_tell;
    io->length = safio_length;
    io->duplicate = safio_duplicate;
    io->flush = NULL;
    io->destroy = safio_destroy;
    return io;
}
```

**`pread`-based I/O functions:**

```c
static PHYSFS_sint64 safio_read(PHYSFS_Io *io, void *buf, PHYSFS_uint64 n) {
    SafIoData *d = (SafIoData *)io->opaque;
    PHYSFS_uint64 remaining = d->len - d->pos;
    if (n > remaining) n = remaining;
    if (n == 0) return 0;
    ssize_t got = pread(d->fd, buf, (size_t)n, (off_t)d->pos);
    if (got > 0) d->pos += got;
    return (PHYSFS_sint64)got;
}

static int safio_seek(PHYSFS_Io *io, PHYSFS_uint64 offset) {
    SafIoData *d = (SafIoData *)io->opaque;
    if ((PHYSFS_sint64)offset > d->len) return 0;
    d->pos = (PHYSFS_sint64)offset;
    return 1;
}

static PHYSFS_sint64 safio_tell(PHYSFS_Io *io) {
    return ((SafIoData *)io->opaque)->pos;
}

static PHYSFS_sint64 safio_length(PHYSFS_Io *io) {
    return ((SafIoData *)io->opaque)->len;
}

static PHYSFS_Io *safio_duplicate(PHYSFS_Io *io) {
    SafIoData *orig = (SafIoData *)io->opaque;
    int newfd = dup(orig->fd);
    if (newfd < 0) return NULL;

    SafIoData *data = malloc(sizeof(SafIoData));
    data->fd = newfd;
    data->pos = 0;
    data->len = orig->len;
    data->uri = strdup(orig->uri);

    PHYSFS_Io *newio = malloc(sizeof(PHYSFS_Io));
    memcpy(newio, io, sizeof(PHYSFS_Io));
    newio->opaque = data;
    return newio;
}

static void safio_destroy(PHYSFS_Io *io) {
    SafIoData *d = (SafIoData *)io->opaque;
    close(d->fd);
    free(d->uri);
    free(d);
    free(io);
}
```

#### M1.4 — Register archiver & mount in `PHYSFSX_init`

In `d2/misc/physfsx.c`, inside the `#ifdef ANDROID` block:

```c
#ifdef ANDROID
{
    const char *pref = PHYSFS_getPrefDir("com.dxxredux", "d2x-redux");
    if (pref)
    {
        PHYSFS_setWriteDir(pref);
        PHYSFS_addToSearchPath(pref, 1);

        /* Register the SAF archiver so .saf_manifest.json can be mounted */
        extern PHYSFS_Archiver SAF_Archiver;
        PHYSFS_registerArchiver(&SAF_Archiver);

        /* SAF leave-in-place: mount .saf_manifest.json if it exists */
        char safpath[PATH_MAX];
        snprintf(safpath, sizeof(safpath), "%s.saf_manifest.json", pref);
        FILE *sf = fopen(safpath, "r");
        if (sf) {
            fclose(sf);
            PHYSFS_mount(safpath, NULL, 1);  // append after filesDir
        }
    }
    PHYSFS_addToSearchPath(PHYSFS_getBaseDir(), 1);
    PHYSFSX_addRelToSearchPath("data", 1);
}
```

#### M1.5 — Persistent SAF permissions (Kotlin)

When the user selects files with the file picker:

```kotlin
for (uri in selectedUris) {
    contentResolver.takePersistableUriPermission(
        uri, Intent.FLAG_GRANT_READ_URI_PERMISSION
    )
}
```

Update the existing `importFile` flow to support a "leave in place" path that skips the copy and instead writes to `SafManifest`.

#### M1.6 — UI: "Leave in Place" vs "Import" choice

After the SAF picker returns files, present a choice dialog:

```
┌─────────────────────────────────────────────┐
│  Found 10 game files                        │
│                                             │
│  ▸ Leave in place (read from source)        │
│    No extra storage used                    │
│    Files must stay accessible               │
│                                             │
│  ▸ Import (copy to app storage)             │
│    Uses ~580 MB of app storage              │
│                                             │
└─────────────────────────────────────────────┘
```

"Leave in place" is the first/default option. When chosen:
1. `takePersistableUriPermission` for each URI
2. Hash each file (streamed via `contentResolver.openInputStream`)
3. Write `.saf_manifest.json` with URI + size
4. Update `AssetManifest` with `leaveInPlace = true`, `sourceUri = content://...`
5. **Do not copy**
6. Refresh file status — files show as found

"Import" follows the existing copy flow (unchanged).

#### M1.7 — Stale permission handling

On app launch, verify each URI in `.saf_manifest.json` is still accessible:

```kotlin
fun verifySafPermissions(): List<StaleFile> {
    val manifest = safManifest.read()
    val persisted = contentResolver.persistedUriPermissions.map { it.uri }.toSet()
    return manifest.filter { Uri.parse(it.contentUri) !in persisted }
}
```

Stale files show a warning icon and offer to re-link or import (copy as fallback).

#### M1.8 — adb-based testing (no UI required)

The SAF archiver can be tested entirely via adb, bypassing the UI. This lets us validate the C archiver + JNI bridge + PhysFS integration before building any launcher UI on top.

**Test strategy:** Push game files to a shared location on the device, manually construct a `.saf_manifest.json` pointing at them, then launch the game engine and verify it loads correctly.

##### Test script: `android/test_saf_archiver.sh`

```bash
#!/bin/bash
# test_saf_archiver.sh — Test the SAF PhysFS archiver via adb
# Pushes game data to /sdcard/Download/, writes .saf_manifest.json,
# and launches the game to verify files load via the archiver.

PACKAGE="com.dxxredux.app"
GAME_DATA_DIR="../game_data"
ADB="${ANDROID_HOME}/platform-tools/adb"

# 1. Push game files to a shared location (NOT app-private storage)
echo "=== Pushing game files to /sdcard/Download/ ==="
for f in "$GAME_DATA_DIR"/*.{hog,pig,ham,s11,s22,mvl,HOG,PIG,HAM,S11,S22,MVL}; do
    [ -f "$f" ] || continue
    name=$(basename "$f" | tr 'A-Z' 'a-z')
    echo "  Pushing $name..."
    $ADB push "$f" "/sdcard/Download/$name"
done

# 2. Clear any existing copied game files from app storage
#    (so the ONLY way engine can find them is via SAF archiver)
echo ""
echo "=== Clearing app-private game files ==="
$ADB shell run-as $PACKAGE sh -c '
    cd files
    for f in *.hog *.pig *.ham *.s11 *.s22 *.mvl 2>/dev/null; do
        [ -f "$f" ] && rm "$f" && echo "  Removed $f"
    done
'

# 3. Build .saf_manifest.json from files in /sdcard/Download/
#    For testing, we use file:// URIs which ContentResolver can open.
#    In production, these would be content:// URIs from SAF.
echo ""
echo "=== Building .saf_manifest.json ==="
MANIFEST='{"files":['
FIRST=1
for f in "$GAME_DATA_DIR"/*.{hog,pig,ham,s11,s22,mvl,HOG,PIG,HAM,S11,S22,MVL}; do
    [ -f "$f" ] || continue
    name=$(basename "$f" | tr 'A-Z' 'a-z')
    size=$(wc -c < "$f" | tr -d ' ')

    # For test purposes: use the /sdcard/Download path directly.
    # The test JNI bridge opens these via standard open().
    # Production uses content:// URIs via ContentResolver.
    uri="/sdcard/Download/$name"

    [ $FIRST -eq 0 ] && MANIFEST="$MANIFEST,"
    MANIFEST="$MANIFEST{\"filename\":\"$name\",\"content_uri\":\"$uri\",\"size_bytes\":$size}"
    FIRST=0
done
MANIFEST="$MANIFEST]}"

echo "$MANIFEST" | $ADB shell run-as $PACKAGE sh -c 'cat > files/.saf_manifest.json'
echo "  Written .saf_manifest.json"

# 4. Verify the manifest was written
echo ""
echo "=== Manifest contents ==="
$ADB shell run-as $PACKAGE cat files/.saf_manifest.json

# 5. Launch the game
echo ""
echo "=== Launching game ==="
$ADB shell am start -n $PACKAGE/.MainActivity

# 6. Wait and inspect via introspection API
echo ""
echo "=== Waiting 10s for game to load... ==="
sleep 10
$ADB shell am broadcast -a com.dxxredux.INTROSPECT
sleep 1
echo ""
echo "=== Introspection result ==="
$ADB shell run-as $PACKAGE cat files/introspect.json
```

##### What makes this testable without SAF

The key: `saf_open_file()` in the C code receives a URI string and calls back to Java. For testing, we add a fallback path in the JNI bridge:

```kotlin
@Keep
fun openSafFile(contentUri: String): Int {
    return try {
        if (contentUri.startsWith("/")) {
            // Test mode: direct filesystem path (for adb testing)
            val pfd = ParcelFileDescriptor.open(
                File(contentUri), ParcelFileDescriptor.MODE_READ_ONLY
            )
            return pfd.detachFd()
        }
        // Production mode: SAF content URI
        val uri = Uri.parse(contentUri)
        val pfd = contentResolver.openFileDescriptor(uri, "r") ?: return -1
        pfd.detachFd()
    } catch (e: Exception) {
        -1
    }
}
```

When the `.saf_manifest.json` contains plain filesystem paths (like `/sdcard/Download/descent2.hog`), the JNI bridge opens them directly. When it contains `content://` URIs (production), it goes through ContentResolver. Same archiver, same code path, different fd source.

##### Test matrix

| Test | Method | Validates |
|------|--------|-----------|
| Engine loads with SAF archiver only | Push to /sdcard, clear app files, write manifest, launch | Archiver parses JSON, JNI bridge works, pread I/O works |
| Introspection shows game state | `introspect.sh` after launch | Files loaded correctly, level data parsed |
| Duplicate/seek stress | Run game into level (PIG paging) | `safio_duplicate()` + concurrent `pread()` |
| Mixed mode (some SAF, some copied) | Leave some files in app storage, others only in SAF manifest | PhysFS priority order correct |
| Missing file graceful failure | Remove one file from /sdcard, launch | Archiver returns NULL, engine shows error |
| Permission revocation (production) | Revoke URI permission, relaunch | Stale detection, UI warning |

##### Automated test via game scripts

Use the existing automation infrastructure to verify the game actually runs:

```bash
# After launching, run automation to navigate to gameplay
./run_automation.sh game_scripts/start_new_game.json

# Then check introspection for in-game state
./introspect.sh player   # should show shields, score, etc.
```

#### M1.9 — Build & compile test

- Add `physfs_archiver_saf.c` to CMakeLists.txt (Android builds only)
- Add `jni_saf.c` to the JNI sources
- Build D2 — verify C compiles
- Run `test_saf_archiver.sh` on emulator — verify game loads from /sdcard files
- Run `introspect.sh` — verify `in_game: true`, correct level name

---

### Phase M2: File Set Infrastructure

**Goal:** Data model + persistence + C code for switching between multiple sets of game files. Each set has its own `.saf_manifest.json` (or copied files, or a mix).

**Depends on:** M1 (SAF archiver is the primary way sets provide files to the engine).

#### M2.1 — `FileSetManager` class

New file: `FileSetManager.kt`

```kotlin
data class FileSetInfo(
    val name: String,
    val createdAt: Long,
    val source: String? = null  // e.g. "extracted from d2demo.zip", "SAF linked"
)

class FileSetManager(private val filesDir: File) {
    fun listSets(): List<FileSetInfo>
    fun getActive(): String
    fun setActive(name: String)
    fun createSet(name: String, source: String? = null): File
    fun deleteSet(name: String)
    fun getSetDir(name: String): File
    fun safManifestForSet(name: String): SafManifest  // set's .saf_manifest.json
    fun assetManifestForSet(name: String): AssetManifest
    fun diskUsage(name: String): Long
}
```

Default set = `filesDir` itself (no migration). Named sets = `filesDir/sets/<name>/`.

Each set can have its own `.saf_manifest.json` — the SAF archiver for the active set's manifest is what gets mounted. This means different sets can reference different source files in different locations.

#### M2.2 — Persist active set in `file_sets.json`

```json
{
  "active": "default",
  "sets": [
    { "name": "default", "createdAt": 0 },
    { "name": "d2-demo", "createdAt": 1710000000000, "source": "SAF linked" }
  ]
}
```

#### M2.3 — C code: read `.active_set_path` + mount set's SAF manifest

In `d2/misc/physfsx.c`, extend the `#ifdef ANDROID` block to:
1. Read `.active_set_path` — if non-empty, prepend the set directory (for any copied files)
2. Mount the set's `.saf_manifest.json` (either from the set dir or from `filesDir` for default)

```c
#ifdef ANDROID
{
    const char *pref = PHYSFS_getPrefDir("com.dxxredux", "d2x-redux");
    if (pref)
    {
        PHYSFS_setWriteDir(pref);
        PHYSFS_addToSearchPath(pref, 1);

        /* Register SAF archiver */
        extern PHYSFS_Archiver SAF_Archiver;
        PHYSFS_registerArchiver(&SAF_Archiver);

        /* File-set support: prepend active set directory */
        char asp[PATH_MAX];
        snprintf(asp, sizeof(asp), "%s.active_set_path", pref);
        FILE *f = fopen(asp, "r");
        char setdir[PATH_MAX] = "";
        if (f) {
            if (fgets(setdir, sizeof(setdir), f)) {
                char *nl = strchr(setdir, '\n');
                if (nl) *nl = '\0';
                if (strlen(setdir) > 0)
                    PHYSFS_addToSearchPath(setdir, 0);  // prepend
            }
            fclose(f);
        }

        /* Mount SAF manifest (from set dir if active, else from pref dir) */
        char safpath[PATH_MAX];
        if (strlen(setdir) > 0)
            snprintf(safpath, sizeof(safpath), "%s/.saf_manifest.json", setdir);
        else
            snprintf(safpath, sizeof(safpath), "%s.saf_manifest.json", pref);

        FILE *sf = fopen(safpath, "r");
        if (sf) {
            fclose(sf);
            PHYSFS_mount(safpath, NULL, 1);  // append after set dir
        }
    }
    PHYSFS_addToSearchPath(PHYSFS_getBaseDir(), 1);
    PHYSFSX_addRelToSearchPath("data", 1);
}
```

#### M2.4 — Reorganize `KnownVersions.kt` by package

Restructure from per-file-type to per-package grouping. Add placeholder sections for demo files and ZIP archives.

#### M2.5 — Define demo file lists

Add `D2_DEMO_FILES` alongside `D2_FILES`. Add helper to determine which list applies based on which files are present.

#### M2.6 — Write `.active_set_path` + set's `.saf_manifest.json` before engine launch

```kotlin
val activeSetDir = fileSetManager.getSetDir(fileSetManager.getActive())
val aspFile = File(filesDir, ".active_set_path")
if (activeSetDir.absolutePath != filesDir.absolutePath) {
    aspFile.writeText(activeSetDir.absolutePath)
} else {
    aspFile.delete()
}
```

#### M2.7 — Build & test

- Test with default set (SAF manifest in `filesDir`) — same as M1 behavior
- Test with a named set that has its own `.saf_manifest.json` pointing to different files
- Verify engine picks up the correct set's files

---

### Phase M3: File Set UI

**Goal:** User can create, switch, and delete file sets. File imports (SAF leave-in-place or copy) route to the active set.

**Depends on:** M2 (FileSetManager, set persistence).

#### M3.1 — Active set indicator

Above the file sections in the files pane:

```
Files in use: default     [Change]
```

#### M3.2 — Set management dialog

```
┌─────────────────────────────────┐
│  Choose File Set                │
│                                 │
│  Current: D2 Demo               │
│  Size: 12.3 MB                  │
│                                 │
│  ─────────────────────────────  │
│  ▸ Switch to set "default"      │
│  ▸ Switch to set "Steam v1.1"   │
│                                 │
│  ─────────────────────────────  │
│  ▸ Add new set...               │
│  ▸ Delete "D2 Demo"             │
│                                 │
│              [Close]            │
└─────────────────────────────────┘
```

Switching sets: update `file_sets.json`, write `.active_set_path`, refresh UI.
Delete: recursively delete `files/sets/<name>/`, release persisted SAF permissions for that set's URIs, switch to default if active was deleted.

#### M3.3 — Route file selection to active set

When files are selected via SAF picker, the "leave in place" or "import" flow targets the active set:
- **Leave in place:** writes to `fileSetManager.getSetDir(active)/.saf_manifest.json`
- **Import (copy):** copies to `fileSetManager.getSetDir(active)/`

Both update the active set's `AssetManifest`.

#### M3.4 — Per-set readiness display

Readiness checks both the set's copied files AND its `.saf_manifest.json` entries. A file is "found" if it exists on disk in the set dir, in `filesDir` (fallback), or in the SAF manifest:

```kotlin
fun checkFilesForSet(
    setDir: File,
    safManifest: SafManifest,
    fallbackDir: File,
    fileList: List<GameFileInfo>,
    assetManifest: AssetManifest?
): List<FileStatus>
```

#### M3.5 — FileDetailDialog: leave-in-place info

The file detail popup shows additional info for SAF-linked files:

- **Location:** `content://... (leave-in-place)` or `(in app storage)`
- **Action button:** "Unlink" (removes from SAF manifest, releases URI permission) or "Delete" (removes copied file)
- **Status indicator:** checkmark for accessible, warning for stale permission

---

### Phase M4: ZIP Import Support

**Goal:** User can pick `.zip` files containing game data. Extracted files are registered via SAF manifest or copied, depending on user choice.

**Depends on:** M3 (set UI for routing extracted files to sets).

#### M4.1 — Accept ZIP in file picker

Partition selected files into zips and regular game files. Regular files go through the M1 leave-in-place/import flow. Zips go through extraction.

#### M4.2 — Streaming ZIP extraction & hashing

For each zip (≤50MB):
1. Hash the zip itself (streamed)
2. Extract entries matching known game filenames to `filesDir/tmp/`
3. Hash extracted files, check against `KnownVersions`
4. Collect results

#### M4.3 — ZIP recognition dialog

If recognized, suggest creating a named set or adding to existing set.

#### M4.4 — Place extracted files

Extracted temp files are moved to the target set directory as copied files (zips can't be left in place — the individual files inside need to be on disk for PhysFS). Update the set's `AssetManifest`.

#### M4.5 — Cleanup

Delete `filesDir/tmp/` after extraction regardless of outcome.

---

### Phase M5: Demo Auto-Download

**Goal:** One-tap download and install of official demo versions.

**Depends on:** M4 (ZIP extraction flow), M3 (set creation).

#### M5.1 — Download URLs

```kotlin
val DEMO_DOWNLOADS = listOf(
    DemoPackage(
        name = "D2 Demo",
        url = "https://github.com/<org>/<repo>/releases/download/demos/d2demo.zip",
        zipSha256 = "<hash>",
        sizeBytes = 5_000_000L,
        description = "Official Descent 2 Demo (3 levels)"
    ),
)
```

#### M5.2 — Demo section in UI

When no required files are found, offer download.

#### M5.3 — Download → extract → create set

1. Fetch zip to `filesDir/tmp/`
2. Verify SHA-256
3. Feed into M4 extraction flow
4. Auto-create set, set as active, refresh UI

#### M5.4 — GitHub Releases hosting

Package demo files as .zip, compute hashes, upload to GitHub release tagged `demos`.

---

### Phase M6: Mod Layering (Future — separate plan)

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
M6 (Mod Layering — future, separate plan)
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
- **D1 engine code** — no changes until D1 support is added
- **File copy option** — importing (copying) files still works as before; leave-in-place is the new default but copy remains available

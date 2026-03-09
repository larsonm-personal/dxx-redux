# Asset Management System — Implementation Plan

## Background

The launcher currently copies game files into `filesDir` via SAF and checks readiness by filename.  
There is no hashing, no version detection, no asset manifest, and no mod support.

### Key Constraint: PhysFS Requires Real Files

The C/C++ engine reads all game data through **PhysFS**, which operates on real filesystem paths.  
It cannot read Android SAF `content://` URIs. Files **must** be copied to `filesDir`.  
The original "no copy" design is therefore not feasible without a full engine I/O rewrite.

### Revised Approach

Keep copying files to `filesDir` (engine requires it), but add a metadata layer:

- **`assets.json`** — manifest tracking every imported file with SHA-256, size, version
- **`KnownVersions`** — hard-coded table of SHA-256 → human-readable version name
- **Progress UI** — hashing progress bar with current-file text
- **Enhanced readiness view** — show version name or short hash per file
- **Mod system** (future) — mod files in subdirectories, toggled on/off, mounted via PhysFS priority

---

## Data Model

### AssetEntry (stored in `assets.json`)

```
filename: String       // canonical lowercase name in filesDir
sha256: String         // hex, computed after copy
sizeBytes: Long
importedAt: Long       // epoch millis
versionName: String?   // matched from KnownVersions, or null
```

### `assets.json` Example

```json
[
  {
    "filename": "descent2.hog",
    "sha256": "abcdef1234567890...",
    "sizeBytes": 20145678,
    "importedAt": 1710000000000,
    "versionName": "Descent II v1.2 (GOG)"
  },
  {
    "filename": "groupa.pig",
    "sha256": "fedcba0987654321...",
    "sizeBytes": 98765432,
    "importedAt": 1710000000000,
    "versionName": null
  }
]
```

### KnownVersion (hard-coded in source)

```
filename: String       // e.g. "descent2.hog"
sha256: String         // full hex hash
versionName: String    // e.g. "Descent II v1.2 (GOG)"
```

### Mod (future, stored in `mods.json`)

```
id: String
name: String
files: List<{filename, sha256, sizeBytes}>
enabled: Boolean
```

---

## Phase 1: Asset Manifest + Hashing (core infrastructure)

### Step 1.1 — Define `KnownVersions` table

- Kotlin object `KnownVersions` in its own file
- `Map<String, List<Pair<String, String>>>`: filename → list of (sha256, versionName)
- Populate initially from hashing files in `game_data/`
- Start with D2+D1 required files; expand as community reports hashes

### Step 1.2 — Add `AssetManifest` helper class

- Reads/writes `assets.json` in `filesDir`
- `load(): List<AssetEntry>` — parse JSON array
- `save(entries: List<AssetEntry>)` — write JSON array
- `upsert(filename, sha256, sizeBytes)` — insert or update, auto-lookup version
- `getEntry(filename): AssetEntry?` — lookup by filename

### Step 1.3 — Add SHA-256 hashing helper

- `suspend fun computeSha256(file: File, onProgress: (bytesRead: Long, totalBytes: Long) -> Unit): String`
- Uses `MessageDigest.getInstance("SHA-256")` with 8KB buffer
- Reports progress per buffer read for UI binding

### Step 1.4 — Integrate hashing into import flow

- After `importFile()` copies a file, hash it immediately
- Call `AssetManifest.upsert()` with the result
- Wire into the existing import loop in `SetupScreen`

---

## Phase 2: Hashing Progress UI

### Step 2.1 — Add hashing progress state to Compose

- `data class HashingProgress(currentFile, fileIndex, totalFiles, progress)`
- `LinearProgressIndicator` with text overlay: `"descent2.hog (1/9)"`
- Shown during import and on startup if manifest is stale

### Step 2.2 — Startup re-hash check

- On `SetupScreen` composition, compare `filesDir` contents against `assets.json`
- If file exists on disk but no manifest entry, or size changed → queue for hashing
- Show progress UI while processing queue
- After completion, update manifest

---

## Phase 3: File Readiness View Enhancement

### Step 3.1 — Update `FileStatusRow` to show version or short hash

- Read `AssetManifest` entries for each file
- Version known → `"✓ descent2.hog — v1.2 (GOG)"`
- Version unknown → `"✓ descent2.hog — #abcdef12"`
- No manifest entry → current behavior (found/missing)

### Step 3.2 — Update introspection JSON

- Add `sha256` and `version` fields to each file in `setup_introspect.json`
- Enables automated test verification of file identity

---

## Phase 4: Mod System (future)

### Step 4.1 — Mod storage + manifest

- Directory: `filesDir/mods/<mod-id>/`
- `mods.json` tracks mod metadata
- One mod enabled at a time

### Step 4.2 — Mod import UI

- Separate "Import Mod" button
- User names the mod, picks files → copied to mod directory
- Files hashed + version-detected like base assets

### Step 4.3 — Mod activation via PhysFS

- On game launch, if mod enabled:
  - Pass active mod path from Kotlin → JNI → C
  - In `physfsx.c`, mount mod directory at **position 0** (higher priority)
  - PhysFS finds mod files before base files → automatic override

### Step 4.4 — Mod toggle UI

- Radio-button list of mods (plus "No mod")
- Write selection to `mods.json`
- Game restart required for changes

---

## What Doesn't Change

- SAF file picker (`OpenMultipleDocuments`) — works fine
- File copy to `filesDir` — engine requires it
- PhysFS initialization — unchanged for base game
- `checkFiles()` readiness logic — same disk scanning
- D1X optional downloads — same
- `descent.cfg`, resolution picker, controller config — untouched

---

## Risk Assessment

| Risk | Mitigation |
|---|---|
| Large file hashing is slow (540MB GOG CD image) | Progress UI; Dispatchers.IO; cache in manifest (done once) |
| Known hash table starts incomplete | Ship what's available; show short hash for unknowns; expand via community |
| `assets.json` out of sync with disk | Startup verification; re-hash if size mismatch |
| Mod PhysFS mounting needs JNI change | Minimal: one string param, one `PHYSFS_addToSearchPath()` call |

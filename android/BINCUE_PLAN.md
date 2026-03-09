# Plan: BIN/CUE Data Track Extraction + Multi-Disc Audio Management

## TL;DR
Add support for importing game files from BIN/CUE CD image data tracks (ISO 9660), managing multiple redbook audio sources with user-configurable disc ordering, identifying discs via per-track SHA1 hashes matched against a JSON database, and providing in-game overlay controls for track selection/browsing.

## Architecture Overview

The system adds three major capabilities on top of the existing BIN/CUE redbook audio code:

1. **ISO 9660 data track reader** — reads Mode 1 sectors from BIN files, parses the ISO filesystem, and extracts game files
2. **Multi-disc audio source management** — replaces the single `.gog`/`.inst` pair with a list of audio sources, user-orderable, combined into a unified track sequence at game launch
3. **Disc identification via SHA1** — per-track SHA1 hashing during import, matched against a known-discs JSON database (replacing the hardcoded disc ID system)

---

## Phase 1: CUE Parser Enhancement + Multi-File BIN Support ✅

**Goal:** Extend the CUE parser to handle arbitrary CUE/BIN files (not just `descent_ii.inst`/`.gog`), including multi-FILE CUE sheets.

**Steps:**
1. ✅ Refactor `parse_cue_file()` in `d2/arch/sdl/rbaudio_bin.c` to accept a CUE filename + BIN base path as parameters instead of hardcoding `descent_ii.inst`/`descent_ii.gog`
2. ✅ Add support for `FILE "name.bin" BINARY` directives — track which BIN file each track belongs to (for multi-BIN CUE sheets, each FILE gets its own handle)
3. ✅ Create a reusable CUE parser module (new file `android/app/src/main/cpp/cue_parser.c/.h`) that can be called from both the C engine and from JNI for the Kotlin import flow
4. ✅ The parser should return a structured list: `{track_num, type (data/audio), bin_filename, start_sector, num_sectors}`

**Data structures:**
```
cue_bin_file_t { char filename[256]; int file_index; }
cue_track_t    { int type; int start_sector; int num_sectors; int file_index; }  // file_index maps to which BIN
cue_disc_t     { cue_bin_file_t files[MAX_FILES]; int num_files; cue_track_t tracks[MAX_TRACKS]; int num_tracks; }
```

**Relevant files:**
- `d2/arch/sdl/rbaudio_bin.c` — current CUE parser to refactor (lines 166-270)
- New: `android/app/src/main/cpp/cue_parser.c` + `.h`

---

## Phase 2: ISO 9660 Data Track Reader ✅

**Goal:** Read the ISO 9660 filesystem from Mode 1 data tracks in a BIN file and extract game files.

**Steps:**
1. ✅ Create `android/app/src/main/cpp/iso9660_reader.c/.h` — standalone ISO 9660 reader operating on raw BIN files
2. ✅ Implement Mode 1 sector reading: extract 2048 bytes of user data from each 2352-byte raw sector (skip 16-byte sync+header, ignore 288-byte ECC/EDC tail)
3. ✅ Parse the Primary Volume Descriptor at logical sector 16 (byte offset = (track_start_sector + 16) * 2352 + 16 for user data within raw sector)
4. ✅ Walk the ISO directory tree recursively, building a file listing with paths
5. ✅ Extract files by reading sequential logical sectors, writing user data to output files
6. ✅ Handle multi-session overlay semantics: when multiple data tracks exist, read them sequentially; files from later tracks override files from earlier tracks (later = higher priority, like layered directories)
7. ✅ Preserve directory structure (e.g., `MISSIONS/D2X.HOG`)
8. ✅ Filter extraction to known game file extensions: `.hog`, `.ham`, `.pig`, `.s11`, `.s22`, `.mn2`, `.mvl`, `.dxa`, `.cfg`, `.txt`
9. Expose via JNI: `native_list_iso_files(binPath, cueTrackInfo)` → returns file list; `native_extract_iso_files(binPath, cueTrackInfo, outputDir)` → extracts with progress callback

**Key ISO 9660 details for Mode 1:**
- Raw sector: 12 sync + 4 header + 2048 data + 288 ECC = 2352
- PVD at logical sector 16 (offset = track_start + 16 sectors)
- Root directory record in PVD at offset 156 (34 bytes)
- Directory records: variable length, filename at offset 33, data extent LBA at offset 2 (LE uint32), data length at offset 10 (LE uint32)

**Relevant files:**
- New: `android/app/src/main/cpp/iso9660_reader.c` + `.h`
- Reference pattern: `d2/arch/android/physfs_archiver_saf.c` (file I/O patterns)

---

## Phase 3: Per-Track SHA1 Hashing + Known Disc Database ✅

**Goal:** Hash each track in a CUE/BIN set by SHA1, match against a known-discs JSON database for identification.

**Steps:**
1. ✅ SHA1 hashing done in Kotlin via `java.security.MessageDigest("SHA-1")` — no C SHA1 module needed
2. ✅ For each track: SHA1 = hash of all raw 2352-byte sectors from start_sector to start_sector+num_sectors (hash the full raw sector data, matching redump convention)
3. ✅ Create `android/app/src/main/assets/known_discs.json` — JSON database of known disc definitions
4. ✅ Move existing hardcoded disc IDs (0x7d0ff809 etc.) and track names into this JSON database
5. ✅ Implement matching algorithm: for each known disc, compare SHA1s in order starting from track 1. The disc with the most consecutive matching SHA1s from the beginning wins. Ties broken by total match count.
6. SHA1 hashing done in Kotlin, no JNI needed for hashing
7. ✅ Create Kotlin disc matcher class `DiscIdentifier.kt` that loads `known_discs.json`, runs matching, returns `{discId, label, matchedTracks, totalTracks, allMatched}`
8. "About" dialog shows: disc label, "12/15 track hashes matched" or "all track hashes matched"

**Relevant files:**
- ✅ New: `android/app/src/main/assets/known_discs.json`
- ✅ New: `android/app/src/main/java/com/dxxredux/app/DiscIdentifier.kt`
- Migrate from: `d2/main/songs.c` (disc ID constants, lines 233-258), `d2/main/track_names.c` (hardcoded track names, lines 67-85)

---

## Phase 4: BIN/CUE Import Flow in SetupActivity ✅

**Goal:** When user loads a BIN+CUE, scan it and present import options.

**Steps:**
1. ✅ Add BIN/CUE file detection to the existing file import flow in `SetupActivity.kt` — when a `.cue` file is selected (or a `.bin` alongside a `.cue`), trigger the CD import flow instead of the normal file copy
2. ✅ Parse the CUE via JNI (`cue_parser`) to identify data tracks and audio tracks
3. ✅ Hash tracks via Kotlin (`DiscIdentifier.sha1Hash()`) and run disc identification (`DiscIdentifier`)
4. ✅ Present an import dialog showing:
   - Disc identification result (e.g., "Descent II (GOG) — all track hashes matched")
   - Track listing with data/audio type and size
   - "Extract Game Files" button for data track extraction
   - "Add as Audio Source" button for audio registration
5. ✅ Data track extraction: call `DiscImportBridge.extractIsoFiles()` via JNI with BIN file descriptor directly (no copy needed for extraction)
6. ✅ Audio source registration: copy BIN+CUE to filesDir, register via `AudioSourceManager`
7. ✅ JNI bridges created: `jni_disc_import.c` (CUE parsing + ISO extraction) and `jni_music_control.c` (track controls)
8. ✅ Kotlin bridge: `DiscImportBridge.kt` with `CueTrack`, `IsoFile` data classes
9. ✅ After import, refresh file statuses via `onRefresh()`

**Relevant files:**
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` — import flow (~line 800+), `MusicInfoSection` (~line 1833)
- `android/app/src/main/java/com/dxxredux/app/FileSetManager.kt` — set selection
- `android/app/src/main/java/com/dxxredux/app/AssetManifest.kt` — post-extraction hashing

---

## Phase 5: Multiple Audio Source Management (mostly complete)

**Goal:** Support multiple CUE/BIN audio sources with user-configurable ordering and combined track sequences.

**Steps:**
1. ✅ Create `android/app/src/main/java/com/dxxredux/app/AudioSourceManager.kt` — manages a list of audio sources (CUE/BIN pairs)
2. ✅ Each audio source entry: `{id, cuePath, binPath(s), discLabel, trackCount, audioTrackCount, enabled, order}`
3. ✅ Storage: `audio_sources.json` in `filesDir` (shared across sets, like current music files)
4. ✅ Music section in SetupActivity: shows registered audio sources with disc label + track count
5. Add drag-to-reorder for disc priority — combined track numbering: disc 1 audio tracks 1..N, disc 2 audio tracks N+1..M, etc.
6. At game launch, write an `audio_playlist.json` that the C engine reads to know which BIN/CUE files to use and in what order
7. ✅ Modify `RBAInit()` in `rbaudio_bin.c` to support multi-source — `parse_cue_file()` populates `audio_source_t` and `combined_track_t` arrays (currently falls back to legacy `descent_ii.inst`/`.gog` path; `audio_playlist.json` reading not yet implemented)
8. ✅ Each track in the combined table references: `{file_handle, start_sector, num_sectors, source_disc_index}`
9. ✅ Track advancement in `refill_pcm()` uses `get_track_file()` for multi-source sector I/O
10. ✅ `RBAGetNumberOfTracks()` returns total across all enabled sources
11. ✅ `RBAGetDiscID()` returns the legacy disc ID from the source that owns the current track

**Relevant files:**
- New: `android/app/src/main/java/com/dxxredux/app/AudioSourceManager.kt`
- ✅ `d2/arch/sdl/rbaudio_bin.c` — multi-source playback structs and I/O done
- `d2/main/songs.c` — disc ID backward compat (lines 250-280)

---

## Phase 6: In-Game Track Controls + Overlay (C-side partially complete)

**Goal:** Add overlay controls for track navigation, disc browsing, and track labeling.

**Steps:**

### Quick Controls (overlay top area)
1. Add prev/next track buttons to `TouchOverlayView.kt` — small left/right arrows near the track name display area
2. Wire buttons via JNI: `nativeNextTrack()`, `nativePrevTrack()` → new functions in `rbaudio_bin.c` that jump to the next/prev audio track
3. Show current track info in overlay: "Disc: Descent II (GOG) | Track 5: Ratzez"
4. Add a tap-to-expand gesture on the track name to open the detail panel

### Detail Panel (drawer)
5. Create `MusicControlPanel.kt` — a bottom drawer or side panel showing:
   - Current disc name + match quality
   - Full track list for current disc (scrollable), highlight current track
   - Disc switcher (tabs or dropdown) to browse other loaded discs
   - Tap any track to play it
6. JNI bridge: `nativePlayTrack(discIndex, trackNum)`, `nativeGetCurrentTrack()` → returns `{discIndex, trackNum, trackName}`
7. Track labeling: in the detail panel, allow the user to assign custom names to tracks. Store in `audio_sources.json` alongside the disc entry. Custom names override database names.

### C-side track control functions
8. ✅ Add to `rbaudio_bin.c`:
   - ✅ `RBAPlaySpecificTrack(int combined_track)` — jump to a specific track in the combined sequence
   - ✅ `RBAGetCurrentTrackInfo(int *disc_idx, int *track_num, char *name, int name_len)` — query current state
   - ✅ `RBANextTrack()`, `RBAPrevTrack()` — skip forward/backward
   - ✅ `RBAGetNumAudioTracks()`, `RBAGetTrackName()` — query functions
   - ✅ Declarations added to `d2/include/rbaudio.h`
9. ✅ Expose these via JNI in `jni_music_control.c`, with `external fun` declarations in `MainActivity.kt`

**Relevant files:**
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` — overlay buttons
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` — existing `showTrackName()` callback
- New: `android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt`
- New: `android/app/src/main/cpp/jni_music_control.c`
- ✅ `d2/arch/sdl/rbaudio_bin.c` — track control functions done
- `d2/main/track_names.c` — overlay notification updates
- ✅ `d2/main/track_names.h` / `d2/include/rbaudio.h` — new API declarations

---

## Phase 7: Migrate Disc IDs + Track Names to JSON Database

**Goal:** Move all hardcoded disc identification and track naming into the known_discs.json, making the system data-driven.

**Steps:**
1. Populate `known_discs.json` with SHA1 hashes for all known D2 disc variants (GOG, Definitive, Vertigo, OEM, Mac, etc.) — these need to be computed from actual disc images
2. Include per-track names in the database entries (replacing the hardcoded `d2_track_names[]` in `track_names.c`)
3. Include D1 disc variants
4. Modify `track_names.c` to load names from the matched disc entry at init time (passed from Kotlin via JNI, or loaded from a file the Kotlin layer writes before game launch)
5. Keep `songs_haved2_cd()` working by mapping SHA1-identified discs to the appropriate behavior (which tracks are level tracks vs. title/credits)
6. Add a field to each disc entry: `track_mapping: { title: 2, credits: 3, first_level: 4 }` so the song system can read this instead of using hardcoded constants

**Relevant files:**
- `android/app/src/main/assets/known_discs.json` — database
- `d2/main/track_names.c` — load from JSON instead of hardcoded table
- `d2/main/songs.c` — read track mapping from disc entry

---

## Verification

1. **Unit test CUE parser**: feed it single-BIN and multi-BIN CUE sheets, verify track table output — test file: `android/app/src/test/.../CueParserTest.kt` or C-level test
2. **Unit test ISO 9660 reader**: create a small test ISO image (or use the Definitive Collection disc 1 BIN in `game_data/extracted/`), verify file listing and extraction
3. **Integration test SHA1 hashing**: hash the GOG `descent_ii.gog` tracks, verify against redump database values
4. **Integration test data extraction**: extract game files from the Definitive Collection disc 1, verify extracted files match known SHA256 hashes in `KnownVersions.kt`
5. **Integration test multi-disc audio**: load two audio sources, verify combined track numbering works correctly and playback crosses disc boundaries
6. **Manual test overlay controls**: verify next/prev track buttons work, detail panel shows correct info, track names display correctly
7. **Regression test**: existing GOG `descent_ii.gog`/`.inst` pair continues to work unchanged through the refactored path

---

## Decisions

- **ISO 9660 Mode 1 only** — no Mode 2/XA support needed (PC CD-ROMs are Mode 1)
- **SHA1 for track hashing** (redump convention), SHA256 continues for game asset hashing
- **Known disc database is JSON** (`known_discs.json` in APK assets) — existing integer disc IDs and hardcoded track names will be migrated to it
- **User chooses target file set** for data track extraction
- **User-configurable disc ordering** for multi-disc audio
- **SHA1 hashing only during import** — no re-verification at engine load time
- **Both D1 and D2 supported** — but only D2 engine code changes needed (no d1/ folder changes)
- **Scope exclusions**: no Mode 2/XA tracks, no multi-session CD-ROM (just multi-data-track BIN files), no CHD/MDS/NRG image formats

## Further Considerations

1. **SHA1 implementation**: SHA1 hashing is done in Kotlin via `java.security.MessageDigest("SHA-1")` at import time only. No C-side SHA1 code needed.
2. **Large file I/O on Android**: BIN files can be 500-700MB. For SAF leave-in-place sources, the SHA1 hashing and ISO reading need to work via file descriptors from ContentResolver, not just filesystem paths. The existing SAF infrastructure (`jni_saf.c`) provides a model for this.
3. **Backward compatibility for `songs_haved2_cd()`**: The existing disc ID switch statement needs to keep working during the transition. Recommend: Kotlin writes a `disc_config.json` before launch that includes the legacy disc ID field (looked up from `known_discs.json`), then the C code reads it. This avoids changing the songs.c logic immediately.

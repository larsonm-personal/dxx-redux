# Chromaprint-Based Automatic Song Recognition

## Goal
Add Chromaprint audio fingerprinting to automatically identify and name music tracks
from any source (BIN/CUE disc images, MP3/OGG/FLAC files). Pre-computed fingerprints
in known_discs.json5 serve as the reference database. On-device, fingerprint new
sources and match locally; optionally query AcoustID for unknowns. Replace all
hardcoded/by-hand track naming with fingerprint-based lookup. Add single-file PCM
decoders (minimp3, stb_vorbis, dr_flac) for fingerprinting and future custom audio
playback.

## Key Design Decisions
- Fingerprinting happens once per track at import time, cached in audio_sources.json
  alongside existing disc metadata. Re-fingerprinting only if file size changes (same
  invalidation as SHA-256 asset hashing).
- Fingerprinting progress shown in the same Card/LinearProgressIndicator style as the
  existing hashing progress in SetupActivity.kt.
- AcoustID API key loaded from `android/acoustid_config.json5` (gitignored), with
  `android/acoustid_config.json5.example` as template. Key is loaded at build time
  and baked into BuildConfig or loaded at runtime from app files dir.
- Local matching first (XOR-popcount brute-force against bundled DB), AcoustID web
  fallback off by default, toggled from music picker settings.
- Chromaprint v1.5.1 with bundled KissFFT (no external deps), ~150KB stripped.
- Single-file decoders: minimp3, stb_vorbis, dr_flac (all public domain).
- PC-side C tool shares same Chromaprint lib and cue_parser.c, outputs JSON.

---

## Phase 1: Add Libraries to Build System [COMPLETE]

### 1a. Chromaprint (Android)
- FetchContent in CMakeLists.txt from acoustid/chromaprint pinned to v1.5.1
- Build as shared lib; BUILD_TOOLS=OFF, BUILD_TESTS=OFF, FFT_LIB=kissfft
- Link into both dxx-redux-d1 and dxx-redux-d2
- Pin version in tool_versions.conf

### 1b. Single-file PCM decoders
- minimp3 (lieff/minimp3) -- single header, public domain
- stb_vorbis (nothings/stb) -- single file, public domain
- dr_flac (mackron/dr_libs) -- single header, public domain
- Download via file(DOWNLOAD) with SHA-256 verify (TinySoundFont pattern)
- Create pcm_decoders.c/.h wrapper with unified decode API
- Pin commit hashes in tool_versions.conf

### 1c. AcoustID key setup
- Create android/acoustid_config.json5.example with comments
- Add android/acoustid_config.json5 to .gitignore
- Load key at runtime from app assets or files dir

Files to modify:
- android/app/src/main/cpp/CMakeLists.txt
- android/get_deps/tool_versions.conf
- .gitignore

Files to create:
- android/acoustid_config.json5.example
- android/app/src/main/cpp/shared/pcm_decoders.c
- android/app/src/main/cpp/shared/pcm_decoders.h

---

## Phase 2: Fingerprint Database Schema & Loader [COMPLETE - C module done]

### 2a. Extend known_discs.json5 schema
Add `chromaprint` (base64) and `duration_ms` to each audio track entry:
```json5
{"track": 2, "type": "audio", "sha1": "...", "name": "Title",
 "chromaprint": "AQAA...", "duration_ms": 187000}
```

### 2b. Runtime fingerprint database (C module)
- chromaprint_db.c/.h: loads flattened fingerprint array from JSON at startup
- Matching: XOR-popcount with offset alignment, duration filter (+/- 5%)
- API: chromaprint_db_load(), chromaprint_db_match(), chromaprint_db_free()
- Called via JNI; Kotlin passes known_discs.json5 blob

Files to create:
- android/app/src/main/cpp/shared/chromaprint_db.c
- android/app/src/main/cpp/shared/chromaprint_db.h

Files to modify:
- android/app/src/main/assets/known_discs.json5 (schema extension, data later)

---

## Phase 3: PC-Side Fingerprint Tool (C/C++) [COMPLETE]

### 3a. Standalone C tool
- android/app/src/main/cpp/extract/fingerprint_cd.c
- Reuses cue_parser.c for BIN/CUE; Chromaprint v1.5.1 (static, KissFFT)
- Outputs JSON lines per track: {track, type, sha1, chromaprint?, duration_ms?}
- SHA-1 matches redump convention (full raw 2352-byte sectors)
- Built via extract/CMakeLists.txt as fingerprint_cd target
- MSVC compat: NOMINMAX, _USE_MATH_DEFINES, HAVE_LRINTF, CHROMAPRINT_NODLL

### 3b. Integration scripts
- game_data/fingerprint_disc_tracks.ps1: builds tool, runs on all CD image folders,
  writes track_fingerprints.json per folder
- game_data/update_known_discs_fingerprints.ps1: merges chromaprint+duration_ms into
  known_discs.json5 by matching audio track SHA-1s
- 203 audio tracks across 34 discs now have chromaprint fingerprints

Files created:
- android/app/src/main/cpp/extract/fingerprint_cd.c
- game_data/fingerprint_disc_tracks.ps1
- game_data/update_known_discs_fingerprints.ps1

Files modified:
- android/app/src/main/cpp/extract/CMakeLists.txt (Chromaprint, decoders, fingerprint_cd target)
- android/app/src/main/cpp/shared/fingerprint_gen.c (#ifdef ANDROID for log.h)
- android/app/src/main/cpp/shared/pcm_decoders.c (#ifdef _WIN32 compat)
- android/app/src/main/assets/known_discs.json5 (203 tracks with chromaprint+duration_ms)

---

## Phase 4: On-Device Fingerprinting [COMPLETE]

### 4a. Fingerprint generation (C) [COMPLETE]
- fingerprint_gen.c/.h created
- fingerprint_from_sectors(): raw 2352-byte CD-DA -> 16-bit 44.1kHz PCM -> Chromaprint
- fingerprint_from_audio_file(): decode via pcm_decoders -> Chromaprint
- Feeds PCM to Chromaprint in chunks (low memory)

### 4b. JNI bridge [COMPLETE]
- jni_fingerprint.c created with:
  - nativeFingerprintDiscTrack(binFd, startSector, numSectors) -> {fp, duration}
  - nativeFingerprintAudioFile(path) -> {fp, duration}
  - nativeMatchFingerprint(fp, duration) -> {name, confidence}
  - nativeLoadFingerprintDb(json) -> void

### 4c. Kotlin orchestration (one-time caching) [COMPLETE]
- FingerprintBridge.kt created: JNI bridge object with external fun declarations, DB
  loading from known_discs.json5, fingerprint+match helpers, lookupTrackNames() for
  known discs (avoids fingerprinting when disc ID already known)
- AudioSource.trackNames added: Map<Int, String> persisted in audio_sources.json
- audio_playlist.json extended: track_names object written per source
- DiscImportDialog: after disc identification, looks up track names for known discs
  or fingerprints+matches for unknown discs; stores in AudioSource
- registerGogAudioSource: passes context for known_discs.json5 track name lookup

### 4d. AcoustID web client [SKIPPED - no API key]

Files created:
- android/app/src/main/java/com/dxxredux/app/FingerprintBridge.kt

Files modified:
- android/app/src/main/java/com/dxxredux/app/AudioSourceManager.kt (trackNames field, persistence, writePlaylist)
- android/app/src/main/java/com/dxxredux/app/SetupActivity.kt (fingerprint orchestration, GOG context)

---

## Phase 5: Replace Hardcoded Track Names [COMPLETE]

Track names now flow through:
  AudioSource.trackNames -> audio_playlist.json -> rbaudio_bin.c parse_audio_playlist() -> s_tracks[].name -> track_names_set_cue_title()

- Removed d1_track_names[], d2_track_names[] arrays from track_names.c
- Removed D1_GOG_DISCID, D2_GOG_DISCID constants
- Removed lookup_redbook_name() multi-tier lookup
- track_names_lookup() now delegates directly to track_names_get_cue_title()
- rbaudio_bin.c parse_audio_playlist() extended to parse "track_names" JSON object
  from each source entry, applies names after CUE parsing (overrides CUE TITLE fields)

Files modified:
- android/app/src/main/cpp/shared/track_names.c (removed hardcoded tables)
- android/app/src/main/cpp/shared/rbaudio_bin.c (parse track_names, updated comments)

---

## Phase 6: Music Picker UI [COMPLETE]

- TrackPreviewDialog shows fingerprint-matched names from AudioSource.trackNames
- Falls back to "Track N" when names unavailable

Files modified:
- android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt (TrackPreviewDialog track name display)

---

## Phase 7: Testing [COMPLETE]

- assembleDebug BUILD SUCCESSFUL
- Code quality: all checks passed (clang-format, ktlint, PSScriptAnalyzer, shellcheck, shfmt)
- No new compiler warnings

---

## Phase 8: Global Confidence Threshold + fingerprint_audio.exe [COMPLETE]

### 8a. Global confidence threshold
- Create `android/app/src/main/assets/fingerprint_config.json5` with:
  `{ "match_threshold": 0.4, "duration_tolerance": 0.10 }`
- C side: chromaprint_db.c reads threshold from a setter function instead of #define
  (chromaprint_db_set_threshold). Default stays 0.4 if not called
- Kotlin side: FingerprintBridge loads config and calls setter at DB load time
- PC scripts: read same config file for dedup threshold
- All matching code uses the single source of truth (the json5 asset file)

### 8b. fingerprint_audio.exe (new PC tool for loose audio files)
- Create `android/app/src/main/cpp/extract/fingerprint_audio.c`
  - Accept directory path, enumerate .mp3/.ogg/.flac files
  - Call fingerprint_from_audio_file() for each
  - Output JSON array to stdout (sorted by filename)
- Add `fingerprint_audio` target to extract/CMakeLists.txt
  - Same deps as fingerprint_cd minus cue_parser

Files to create:
- android/app/src/main/assets/fingerprint_config.json5
- android/app/src/main/cpp/extract/fingerprint_audio.c

Files to modify:
- android/app/src/main/cpp/shared/chromaprint_db.h (add set_threshold/set_duration_tolerance)
- android/app/src/main/cpp/shared/chromaprint_db.c (use configurable threshold)
- android/app/src/main/cpp/extract/CMakeLists.txt (add fingerprint_audio target)
- android/app/src/main/java/com/dxxredux/app/FingerprintBridge.kt (load config, call setter)

---

## Phase 9: Archive Extraction + Fingerprinting + AcoustID Lookup [COMPLETE]

### 9a. 7-Zip dependency
- Add SEVENZIP_VERSION/URL/SHA256 to tool_versions.conf
- Create game_data/get_7zip.ps1 to download 7za.exe to $DEP_BASE

### 9b. Main script: game_data/fingerprint_music_packs.ps1
- Extract: for each archive in game_data/music/, parse album name (text before
  first " - "), extract flattened to game_data/music/<album_name>/
  - .zip/.DXA: Expand-Archive; .7z: 7za.exe
  - Idempotent: skip if dir exists with files
- Fingerprint: build fingerprint_audio.exe if needed, run per album dir
- AcoustID lookup: read key from android/acoustid_config.json5, POST to
  api.acoustid.org/v2/lookup with 350ms min delay between requests,
  exponential backoff on 429/errors (1s/2s/4s, max 3 retries)
  - Skip tracks already in existing chromaprint_info.json5
- Output: per-album chromaprint_info.json5 with album name, tracks array
  (filename, chromaprint, duration_ms, acoustid_name if matched)
- Flags: -Force, -SkipAcoustId, -Album <name>

Files to create:
- game_data/get_7zip.ps1
- game_data/fingerprint_music_packs.ps1

Files to modify:
- android/get_deps/tool_versions.conf (7-Zip version/URL)

---

## Phase 10: Database Consolidation [COMPLETE - Fixed]

### 10a. Consolidation script: game_data/update_known_discs_albums.ps1
- Read all game_data/music/*/chromaprint_info.json5 files
- Create album entries in the discs array with type: "album"
  - id: slugified album name; label: album name; tracks: 1..N audio
  - track name: filename without extension; acoustid_name if available
- Dedup: for each album track, match chromaprint against existing CD tracks
  using the global match_threshold from fingerprint_config.json5
  - Uses fingerprint_match.exe (XOR-popcount with offset alignment)
  - Original string prefix comparison was fundamentally broken for cross-format
    fingerprints (CD BIN vs MP3 produce completely different base64 encodings)
  - Results: 337 of 362 album tracks matched CD tracks (93% duplicate rate)
  - 25 unique album tracks remain (MIDI renditions, unique arrangements)
  - Duplicates: comment out in output, note CD source
- Ordering: CD entries first (unchanged), then albums alphabetically
- -DryRun flag

### 10b. fingerprint_match.exe (new PC tool for bulk duplicate detection)
- android/app/src/main/cpp/extract/fingerprint_match.c
  - Reads flat JSON array of {name, disc_id, track, duration_ms, chromaprint}
  - XOR-popcount similarity with offset alignment (-15..+15 frames)
  - Duration pre-filter (10% tolerance)
  - Handles JSON null values (parse_str_or_null)
  - Outputs JSON array of all pairs above threshold with scores
  - Stderr: progress messages (loaded count, pair count)

### 10c. Run consolidation to produce extended known_discs.json5

Files to create:
- game_data/update_known_discs_albums.ps1

Files to modify:
- android/app/src/main/assets/known_discs.json5 (album entries appended)

---

## Phase 11: Android DB Loader Updates [COMPLETE]

- Verify FingerprintBridge.flattenFingerprintDb() handles album entries
  (should work since it iterates all discs[].tracks[] for audio+chromaprint)
- Handle acoustid_name: when flattening, prefer acoustid_name over name for
  the name field passed to C DB (gives AcoustID title priority over filenames)
- Verify MAX_DB_ENTRIES (1024) sufficient for ~203 CD + ~375 album tracks

Files to modify (if needed):
- android/app/src/main/java/com/dxxredux/app/FingerprintBridge.kt

---

## Phase 12: Android AcoustID Client [COMPLETE]

- Create AcoustIdClient.kt: OkHttp POST to acoustid.org/v2/lookup
  - Rate limiter: 350ms coroutine delay between calls
  - Exponential backoff on 429: 1s/2s/4s, max 3 retries
  - Read API key from acoustid_config.json5 (optional, off by default)
- Hook into FingerprintBridge as fallback after local DB miss
- Configuration toggle in music picker settings

Files to create:
- android/app/src/main/java/com/dxxredux/app/AcoustIdClient.kt

Files to modify:
- android/app/src/main/java/com/dxxredux/app/FingerprintBridge.kt

---

## Phase 13: Build + Quality + Integration Test [COMPLETE]

- cmake build of fingerprint_audio target
- Run fingerprint_music_packs.ps1 on all 25 archives
- Run update_known_discs_albums.ps1 consolidation
- assembleDebug -- verify Android build with extended DB
- run-code-quality.ps1 --fix on all new/modified files
- No new compiler warnings
- Existing test regression check

---

## Dependency Graph
```
Phase 1 (libs) --+-> Phase 2 (DB schema) -+-> Phase 4 (on-device) -> Phase 5 (names)
                  |                        |                           Phase 6 (UI)
                  +-> Phase 3 (PC tool) ---+                           Phase 7 (test)

Phase 8a (global threshold) --> Phase 8b (fingerprint_audio.exe)
                                    |
                                    v
                                Phase 9 (extract+fingerprint+AcoustID)
                                    |
                                    v
                                Phase 10 (DB consolidation) --> Phase 11 (loader updates)
                                                                    |
                                                                    v
                                                                Phase 12 (AcoustID client)
                                                                    |
                                                                    v
                                                                Phase 13 (build + test)
```

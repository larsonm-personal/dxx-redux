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

## Phase 4: On-Device Fingerprinting [PARTIAL - C/JNI done, Kotlin not started]

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

### 4c. Kotlin orchestration (one-time caching) [NOT STARTED]
- On source import in SetupActivity, after CUE parsing:
  1. For each audio track, call nativeFingerprintDiscTrack()
  2. Call nativeMatchFingerprint() against loaded DB
  3. If no match and AcoustID enabled, query web service
  4. Store {fingerprint_b64, duration_ms, matched_name} per-track in AudioSource
- Cache in audio_sources.json -- never re-fingerprint unless file size changes
  (same invalidation as SHA-256 hashing uses via AssetManifest.findStaleFiles)
- Show progress: per-track LinearProgressIndicator in same Card style as hashing

### 4d. AcoustID web client
- AcoustIdClient.kt: OkHttp POST to acoustid.org/v2/lookup
- Off by default; toggled from music picker
- API key from acoustid_config.json5

Files to create:
- android/app/src/main/cpp/shared/fingerprint_gen.c
- android/app/src/main/cpp/shared/fingerprint_gen.h
- android/app/src/main/cpp/jni_fingerprint.c
- android/app/src/main/java/com/dxxredux/app/AcoustIdClient.kt

Files to modify:
- android/app/src/main/cpp/CMakeLists.txt (link new sources)
- android/app/src/main/java/com/dxxredux/app/AudioSourceManager.kt (track metadata)
- android/app/src/main/java/com/dxxredux/app/SetupActivity.kt (fingerprint on import)

---

## Phase 5: Replace Hardcoded Track Names [NOT STARTED]

New lookup hierarchy (replaces current 3-tier):
1. Fingerprint match name (highest) -- from chromaprint_db or cached in AudioSource
2. CUE-parsed title -- from CUE TITLE field
3. Import-time disc match name -- from known_discs.json5 name field
4. Generic "Track N" (lowest)

- Remove d1_track_names[] and d2_track_names[] from track_names.c
- Remove D1_GOG_DISCID / D2_GOG_DISCID hardcoded constants
- Track names flow: AudioSource.trackNames -> audio_playlist.json -> C engine

Files to modify:
- android/app/src/main/cpp/shared/track_names.c
- android/app/src/main/cpp/shared/track_names.h
- android/app/src/main/cpp/shared/rbaudio_bin.c

---

## Phase 6: Music Picker UI [NOT STARTED]

- Show fingerprint-matched names in CD Audio and Audio Files tabs
- AcoustID toggle at bottom of MusicPickerPage (below tabs, not in a tab)
- Per-track status: matched name, unmatched "Track N", spinner during fingerprinting

Files to modify:
- android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt

---

## Phase 7: Testing [NOT STARTED]

- C unit tests for XOR-popcount matching with mock data
- PC tool test against known disc image
- Android integration: import disc, verify names, verify music picker
- Regression: CUE TITLE fallback, "Track N" fallback
- Code quality: android\run-code-quality.ps1 --fix
- No new compiler warnings

---

## Dependency Graph
```
Phase 1 (libs) --+-> Phase 2 (DB schema) -+-> Phase 4 (on-device) -> Phase 5 (names)
                  |                        |                           Phase 6 (UI)
                  +-> Phase 3 (PC tool) ---+                           Phase 7 (test)
```

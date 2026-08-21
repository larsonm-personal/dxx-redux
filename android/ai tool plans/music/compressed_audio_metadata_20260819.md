# MP3, OGG, and FLAC Metadata Plan

Date: 2026-08-19

## Goal

- Show embedded MP3, OGG, and FLAC tags in the mission music metadata browser, alongside the existing MIDI metadata experience
- Use a conservative embedded `Title (Composer)` summary as the last runtime naming fallback when curated names, bundled Chromaprint matches, and AcoustID matches are unavailable
- Keep one parser and one naming policy across the launcher, Android game runtime, and host/emulator mission metadata generators
- Preserve existing D1 and D2 desktop behavior by keeping game integration in shared Android sources and Android build wiring

## Investigation completed

- [x] Read `.github/copilot-instructions.md`
- [x] Trace mission music discovery, staging, preview, fingerprint caching, AcoustID lookup, sidecar generation, and runtime lookup
- [x] Trace the existing MIDI metadata parser, JNI bridge, preview dialog, runtime cache, and `music_tracks` regression serializer
- [x] Trace custom audio playlist naming and the jukebox runtime sidecar
- [x] Compare Android `MediaMetadataRetriever`, the existing single-file audio decoders, a handwritten tag reader, and TagLib
- [x] Select the shared library and define the name precedence
- [x] Implementation started

## Current behavior and seams

### Mission metadata browser

`MissionZipMusic` catalogs loose, nested archive, DXA, and HOG-contained MP3, OGG, and FLAC tracks. `MissionZipMusicStageManager` already materializes one selected compressed track into a bounded cache file. `MissionZipMusicDialog` then uses that file for preview and Chromaprint.

MIDI tracks already have a native metadata model, JNI bridge, lazy metadata load, detail printout, active-song-list serialization, and an in-game fallback name. Compressed tracks currently show filename, container details, fingerprint state, and AcoustID state, but no embedded tags.

### Runtime mission names

`MissionZipMusicNames` writes `mission_music_names.json`. Native `track_names.c` loads that sidecar before `songs_get_track_list`, `songs_get_track_info`, and the transient track overlay ask `mission_music_names_lookup` for a display name. This is the correct runtime handoff. The game should not run a tag parser on the render thread.

The current sidecar only writes a record when the fingerprint cache contains a useful local or AcoustID name. If fingerprint generation fails or succeeds without a match, no fallback record is written.

### Custom audio and jukebox names

`CustomAudioSetManager` persists fingerprint names separately from filenames and writes `custom_music_names.json` for enabled playlist entries. Native jukebox list and overlay code already honor that sidecar before using a stripped filename. Embedded fallback names can use the same sidecar without changing D1 or D2 jukebox playback.

### Regression metadata

`midi_metadata_json.hpp` currently emits only active MIDI/HMP rows into `music_tracks`. The Android and headless analyzers share it. Extending this shared serializer is the clean way to keep host and emulator output identical for active compressed mission tracks.

## Library choice

### Use TagLib 2.3.1

Pin TagLib 2.3.1 and its required utf8cpp dependency in `android/get_deps/tool_versions.conf` by immutable archive URL and reviewed SHA-256. Build TagLib statically with examples, tests, bindings, zlib, and unrelated formats disabled. Keep MPEG/ID3 and `WITH_VORBIS=ON` for MP3, OGG Vorbis, and FLAC. Compressed ID3v2 frames are deliberately omitted because the Android, extraction-test, and host builds expose zlib through different target arrangements; ordinary ID3v1/v2 metadata does not require it.

TagLib is the preferred choice because:

- It supports ID3v1, ID3v2, Vorbis comments, MP3, OGG Vorbis, and FLAC in one Unicode-aware API
- Its `PropertyMap` exposes both normalized common fields and additional textual tags for the browser
- Its `IOStream` API permits a bounded PhysFS adapter, so host generation and Android runtime resolve the same bytes without extracting full archives
- Format support can be compiled down to the formats needed here
- It has no codec dependency and does not decode audio samples merely to read tags
- Version 2.3.1 includes the current ID3v2 parser fix and is the current upstream release as of this plan

Primary references:

- https://taglib.org/
- https://taglib.org/api/
- https://github.com/taglib/taglib/releases/tag/v2.3.1
- https://github.com/taglib/taglib/blob/v2.3.1/INSTALL.md

Pin utf8cpp separately because a GitHub tag archive does not populate TagLib's git submodule. Configure an `utf8::cpp` interface target before making TagLib available.

### Rejected alternatives

- Android `MediaMetadataRetriever`: it exposes title, artist, album, composer, and related fields, but Android framework extractors can vary by API level and it cannot support Windows/Linux host regression generation or PhysFS sources
- Existing `minimp3`, `stb_vorbis`, and `dr_flac`: they are excellent decode dependencies and OGG/FLAC expose some comments, but they do not provide one complete, encoding-safe ID3v1/v2 metadata implementation
- Handwritten parsers: Vorbis comments are small, but robust ID3v2.2, v2.3, v2.4, unsynchronization, compressed frames, extended headers, and text encodings are not a lightweight or low-risk reimplementation
- jaudiotagger: it would keep parsing in Kotlin/Java, duplicate native and host paths, and complicate PhysFS-backed active-song metadata

## Shared metadata contract

Add `audio_tag_metadata.cpp` and `audio_tag_metadata.h` under `android/app/src/main/cpp/shared`. The header exposes a plain C ownership API so Kotlin JNI, C game integration, and C++ serializers do not know about TagLib types.

The bounded result should contain:

- `parse_status`: `ok`, `no_tags`, `unsupported`, `truncated`, `invalid`, or `io_error`
- `format`: `mp3`, `ogg`, or `flac`
- normalized `title`, `composer`, `artist`, `album_artist`, `album`, `date`, `genre`, `comment`, `copyright`, `track_number`, and `disc_number`
- `display_name`
- an ordered `properties` list of textual key and value-list records from TagLib `PropertyMap`
- `metadata_truncated`

Do not expose artwork or other binary properties in JSON or UI. Preserve all useful textual fields in the metadata printout, including tags not used for the summary. Sort property keys and preserve each key's value order so checked-in JSON is deterministic.

Apply independent limits for source length, property count, values per property, bytes per value, total retained text, JSON output, and recursion/allocation. Parsing an invalid file must return a status, not abort or throw across the C boundary. Use `readAudioProperties=false` because duration already comes from the fingerprint path and scanning audio frames adds avoidable work.

### Input adapters

Provide two entry points that feed the same core parser:

1. A filesystem path entry point for already staged mission audio and copied custom audio
2. A read-only PhysFS `IOStream` adapter for active mission songs inside mounted HOGs/directories

The PhysFS adapter must implement bounded seek, tell, length, read, and clear/error semantics without loading the complete compressed track into memory. It must reject negative, overflowing, or out-of-file seeks. Explicitly choose the TagLib file class from the trusted lowercased extension rather than allowing an unknown file to fall through to MPEG detection.

## Naming policy

Use one native helper to derive `display_name`:

1. Trim and normalize embedded `TITLE`
2. Read `COMPOSER` without treating `ARTIST`, `ALBUMARTIST`, or `PERFORMER` as a composer
3. Produce `Title (Composer)` when both are useful
4. Produce `Title` when there is no useful composer
5. Produce no embedded fallback when title is absent; retain the existing filename fallback

Reject control characters, blank values, placeholder values such as `unknown` or `untitled`, and values that collapse to separators. Join multiple composer values with `; ` within the existing display byte budget. Truncate on a valid UTF-8 boundary with `...`, reserving room for the composer suffix.

Do not guess a composer from artist tags or filenames. The browser still shows those raw fields. This keeps the runtime label factual and matches the existing MIDI `Title (Composer)` presentation.

The runtime precedence is:

1. Maintained/curated mission or custom-audio label already selected by the existing sidecar path
2. Bundled local Chromaprint database match
3. Successful AcoustID name
4. Embedded audio `Title (Composer)` or `Title`
5. Existing clean filename fallback

For current mission cache behavior, local Chromaprint remains ahead of AcoustID. The key requirement is that embedded tags are only used after both recognition sources have no useful name.

AcoustID web lookup remains disabled by default. When the consent preference is disabled, do not configure the client, offer or run lookup work, update AcoustID status, or count AcoustID as a pending analysis stage. In that state the effective precedence skips AcoustID and proceeds directly from a missing bundled Chromaprint match to embedded metadata.

## Data flow

### Mission browser and cache

Refactor compressed analysis into one staged-file operation:

1. Stage the selected MP3, OGG, or FLAC once
2. Parse embedded tags from the staged path
3. Attempt fingerprint generation and local matching independently
4. Persist both outcomes even when either operation fails
5. Reuse the same cache entry for preview, bulk local analysis, AcoustID lookup, and sidecar generation

Bump `MissionZipAudioFingerprintCache` directly to a new prerelease schema. Make `chromaprint` optional and add `fingerprint_status` plus the normalized embedded metadata fields and display name. Keep content SHA-256 and source identity as the invalidation keys. AcoustID actions must require a nonblank fingerprint; embedded-only cache entries remain valid and must not be repeatedly analyzed on every dialog open.

Rename UI helpers that currently imply every useful label is fingerprint-decoded. A row should distinguish `Matched`, `AcoustID`, and `Embedded metadata`. The compressed preview dialog should show the same normalized header and textual tag printout as the MIDI preview, followed by fingerprint details and the player. Parsing and staging stay on `Dispatchers.IO` and results are cached for the dialog lifetime and persistent analysis cache.

### Mission runtime sidecar

Change `MissionZipMusicNames.bestName` to use the full precedence above and write an embedded fallback record when recognition has no name. `mission_music_names.json` remains the single native handoff, so `songs_get_track_list`, `songs_get_track_info`, the overlay, and introspection gain the fallback without new JNI calls during play.

Update `track_overlay_notify_mission_music` so its final no-sidecar fallback is a clean basename for compressed audio, not `MIDI Track N`. Keep the numbered label for actual MIDI/HMP entries.

### Custom audio and jukebox runtime

Store embedded metadata separately from fingerprint matches in `CustomAudioSetManager.AudioSet`; do not put fallback values into `trackNames`, because that map and its confidence/count UI mean fingerprint identification. Add embedded summary fields to `TrackDetail` and show the full lazy metadata printout in `AudioFileTrackDetailDialog`.

During import or playlist regeneration, parse copied files directly and parse referenced SAF files after their existing staging step. Write `custom_music_names.json` with fingerprint name first, embedded display name second. Native `jukebox_names_lookup`, track list, and overlay require no precedence change because they already consume the resolved sidecar.

If implementation scope must be split, mission browser/runtime support is the first complete tranche. Do not merge a partial custom-audio model that shows tags but omits the jukebox fallback.

### Checked-in mission metadata

Rename the shared active-song serializer from MIDI-specific to music-specific and emit active MP3/OGG/FLAC rows as well as MIDI/HMP rows. Common row fields remain `slot_index`, `slot_kind`, `filename`, `format`, `parse_status`, `title`, `composer`, `display_name`, and `metadata_truncated`.

Compressed rows add artist/album and the normalized property list. MIDI rows retain SMF fields, inheritance fields, and text events. Keep only tracks selected by the active SNG, as today. Android and headless analyzers must call the same serializer.

## Implementation phases

### Phase 1: Dependency and native parser

- [x] Pin TagLib 2.3.1 and utf8cpp with SHA-256 values in `tool_versions.conf`
- [x] Add a shared CMake helper that configures a static, read-only-oriented TagLib build with only MPEG/ID3 and Vorbis/FLAC enabled
- [x] Add the bounded path and PhysFS parser plus plain C result ownership API
- [x] Add normalized property extraction and the shared `Title (Composer)` policy
- [ ] Add native fixtures for ID3v1, ID3v2.3/v2.4 UTF-8/UTF-16, Vorbis comments, FLAC comments, multiple composers, no tags, malformed lengths, oversized values, and invalid UTF-8

Exit gate: native tests pass under MSVC and the Android toolchain, malformed fixtures do not crash, and parser output is byte-stable

### Phase 2: JNI and reusable compressed metadata UI

- [x] Add `AudioTagMetadataBridge.kt` and JNI path parsing to the existing D2 launcher library
- [x] Add serializable Kotlin models matching the native JSON contract
- [ ] Extract a small reusable metadata-lines/printout component shared in presentation with MIDI preview
- [x] Load compressed tags lazily from the already staged preview file on IO
- [x] Show normalized fields and ordered raw textual properties in the mission music dialog/preview

Exit gate: MP3, OGG, and FLAC fixtures show the same normalized values in native tests and the Android dialog

### Phase 3: Mission analysis cache and runtime fallback

- [x] Replace the fingerprint-only cache entry with a combined compressed-audio analysis entry and bump its schema
- [x] Preserve embedded results when fingerprint generation or matching fails
- [x] Guard AcoustID lookup paths against missing fingerprints
- [x] Update progress and status copy to distinguish analyzed, fingerprinted, locally matched, web matched, and embedded-only outcomes
- [x] Extend `MissionZipMusicNames` precedence and sidecar tests
- [x] Fix the compressed no-sidecar overlay fallback label

Exit gate: a tagged fixture with forced fingerprint failure appears as `Title (Composer)` in list, current-track info, and overlay, while a local match still wins

### Phase 4: Active-song regression metadata

- [x] Generalize `midi_metadata_json.hpp` to serialize active MIDI and compressed audio metadata from one shared implementation
- [x] Update Android JNI and headless analyzer call sites
- [ ] Extend host/emulator parity tests and normalized checked-in JSON expectations
- [ ] Regenerate affected mission metadata through the standard generator only after schema and ordering tests pass

Exit gate: host and emulator `music_tracks` output is identical and stable for mixed MIDI/OGG mission song lists

### Phase 5: Custom audio and jukebox fallback

- [x] Persist embedded metadata separately from fingerprint fields in `CustomAudioSetManager`
- [x] Parse copied and SAF-staged custom tracks without duplicate staging
- [x] Show full tags in custom track info
- [x] Resolve `custom_music_names.json` as fingerprint, then embedded summary
- [ ] Add import, persistence, playlist regeneration, SAF staging, and name-precedence tests

Exit gate: copied and SAF-referenced files use embedded fallback in picker and in-game jukebox list/overlay when fingerprint matching is unavailable

### Phase 6: Integration verification

- [ ] Add or extend a launcher automation script that opens a real mission music list, previews tagged compressed audio, and verifies stable metadata state without screenshot analysis
- [ ] Extend music introspection assertions for recognized-name precedence and embedded fallback
- [x] Run focused native CTest and JVM tests
- [x] Run scoped code quality across every touched C, C++, CMake, Kotlin, test, and plan path
- [x] Run D1 and D2 Windows host builds with `run-windows-build.ps1`
- [x] Set JDK 21 and assemble the Android debug APK
- [ ] Run the emulator integration test and inspect automation/introspection JSON
- [x] Run `git diff --check`

Exit gate: tests, D1/D2 host builds, Android build, lint, and emulator verification pass

## Expected files

Dependency and build wiring:

- `android/get_deps/tool_versions.conf`
- `cmake/dxx-verified-dependencies.cmake`, only if the existing helpers cannot expose both pinned sources cleanly
- New shared TagLib dependency helper under `cmake/`
- `android/app/src/main/cpp/CMakeLists.txt`
- `android/app/src/main/cpp/extract/CMakeLists.txt`
- `cmake/dxx-headless-targets.cmake`

Native metadata and runtime:

- New `android/app/src/main/cpp/shared/audio_tag_metadata.cpp`
- New `android/app/src/main/cpp/shared/audio_tag_metadata.h`
- New native parser tests and compact generated fixtures under `android/app/src/main/cpp/shared` or `android/test_fixtures`
- New or renamed shared active music metadata serializer replacing the MIDI-only scope of `midi_metadata_json.hpp`
- `android/app/src/main/cpp/shared/track_names.c`
- `android/app/src/main/cpp/shared/track_names.h`
- `android/app/src/main/cpp/jni_midi_preview.c`, or a narrowly named new JNI translation unit
- `android/app/src/main/cpp/jni_level_metadata.cpp`
- `android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp`

Launcher and caches:

- New `android/app/src/main/java/com/dxxredux/app/AudioTagMetadataBridge.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupSections.kt`
- `android/app/src/main/java/com/dxxredux/app/AudioFilePreviewDialog.kt`
- `android/app/src/main/java/com/dxxredux/app/MissionZipAudioFingerprintCache.kt`, preferably renamed to reflect combined analysis if call-site churn remains small
- `android/app/src/main/java/com/dxxredux/app/MissionZipMusicNames.kt`
- `android/app/src/main/java/com/dxxredux/app/CustomAudioSetManager.kt`
- `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt`

Tests and data:

- `android/app/src/test/java/com/dxxredux/app/MissionZipAudioFingerprintCacheTest.kt`
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicNamesTest.kt`
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicDisplayTest.kt`
- New custom audio embedded-name tests
- Host/emulator mission metadata parity tests
- A maintained launcher automation script under `android/game_scripts/`
- Affected normalized files under `game_data/mission_files/`

## Risks and controls

- Audio files are untrusted archive content. Keep TagLib current, parse read-only, use explicit formats, retain hard bounds, and add malformed corpus tests
- ID3 tags can be large because of artwork. Do not request or serialize pictures; cap retained text independently of file size
- TagLib may seek throughout a file. Use its fast/no-audio-properties mode and a bounded PhysFS adapter instead of copying full tracks into RAM
- UTF-16 ID3 and UTF-8 Vorbis values can produce long Unicode labels. Convert once, validate output, and truncate only at UTF-8 boundaries
- `TITLE` is common but `COMPOSER` is not. Do not silently relabel artist as composer; title-only fallback is acceptable
- Existing cache names imply fingerprints. Rename types or add explicit status fields so embedded-only entries are not counted as Chromaprint matches
- Cache schema changes can leave stale sidecars. Include the parser/name-policy version in source identity or sidecar version and rebuild atomically
- Parsing every archive track while opening a dialog can be slow. Reuse current stage-on-demand/background analysis and never parse on the Compose or game frame thread
- TagLib requires C++17 and utf8cpp. Isolate it behind the C ABI and verify all D1/D2 Android and host targets rather than raising unrelated source requirements globally

## Non-goals

- Do not edit or write tags
- Do not extract or display cover art
- Do not infer composer from artist, album artist, filename, online knowledge, or musical content
- Do not change Chromaprint thresholds, AcoustID consent, or web lookup policy
- Do not eagerly extract every large audio track merely to open the metadata browser
- Do not change playback codecs, SNG ordering, mission soundtrack selection, or desktop D1/D2 behavior
- Do not add compatibility readers for the prerelease Android cache schema

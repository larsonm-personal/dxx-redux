# Plan: Build 1025 - Music Import, Chromaprint, Gyro, Touch Enhancements

## TL;DR
Eight features across music playback pipeline, archive import, chromaprint integration, gyro UI, and touch controls. The critical-path item is fixing OGG/MP3/FLAC playback on Android (currently broken - digi_tsf_music.c is MIDI-only). The pcm_decoders (minimp3/stb_vorbis/dr_flac) already exist for fingerprinting and can be reused for playback.

---

## Phase 1: Fix OGG/MP3/FLAC Playback on Android

**Problem**: digi_tsf_music.c only handles HMP/MID. When songs.c passes an OGG/MP3/FLAC filename, `tml_load_memory()` fails because it expects MIDI.

**Approach**: Add a PCM playback path in digi_tsf_music.c alongside MIDI. Reuse the existing ring buffer + render thread + Mix_HookMusic architecture. The pcm_decoders library (minimp3/stb_vorbis/dr_flac) is already compiled into dxx_fingerprint.so.

**Steps**:
1. In `mix_play_file()` (digi_tsf_music.c ~L438), add extension detection for .ogg/.mp3/.flac
2. For those extensions: use `pcm_decode_file()` to decode entire file to PCM buffer (same API used by fingerprinting)
3. Store decoded PCM in a new static buffer (parallel to g_midi_buf), with sample rate + channel count
4. Add a PCM render path in the render thread that reads from the decoded buffer and writes to the ring buffer (resampling if needed from source rate to output rate)
5. Modify `tsf_music_callback()` to handle both MIDI-rendered and PCM-sourced audio seamlessly
6. Link pcm_decoders into the game shared library (currently only in dxx_fingerprint.so) - or restructure so both share the code

**Files to modify**:
- `android/app/src/main/cpp/shared/digi_tsf_music.c` - add PCM playback path
- `android/app/src/main/cpp/CMakeLists.txt` - link pcm_decoders into game library
- `android/app/src/main/cpp/shared/pcm_decoders.h` / `.c` - possibly no changes, just link

**Verification**: Build game, configure a custom music M3U with OGG files, play a level - music should play

---

## Phase 2: Archive Import (.dxa, .7z, .zip)

**Context**: DXA is a renamed ZIP (PhysFS treats it as standard ZIP). LZMA SDK is already vendored for Inno extraction. The "Add Set" UI in MusicPickerPage.kt currently accepts individual audio files.

**Approach**: Extend the import flow to accept archive files. Extract to temp dir, scan for music files, copy them to `custom_music/<setId>/`, register as audio set with metadata that records source info.

**Steps**:
1. Add archive MIME types to the file picker in MusicPickerPage.kt (application/zip, application/x-7z-compressed, .dxa extension)
2. Create `ArchiveExtractor.kt` utility:
   - ZIP/.dxa: Use Java's `java.util.zip.ZipInputStream` (standard library, no new deps)
   - 7z: Use existing LZMA SDK via JNI, or add a lightweight 7z extraction in C using the already-vendored LZMA SDK
3. Import flow:
   a. User selects archive -> extract to temp dir
   b. Scan extracted tree for audio files (.ogg, .mp3, .flac, .wav)
   c. Show preview dialog with found tracks and proposed set name (from archive filename)
   d. Copy audio files to `custom_music/<setId>/`
   e. Write metadata to `custom_audio_sets.json` including `sourceArchive` field for refresh
4. Add metadata fields to `AudioSet` in CustomAudioSetManager.kt:
   - `sourceType`: "files" | "archive"
   - `sourceArchiveName`: original filename
   - `importTimestamp`: when imported
   - Per-file chromaprint match info (name, album, confidence) - see Phase 3

**Files to modify**:
- `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt` - archive picker + import flow
- `android/app/src/main/java/com/dxxredux/app/CustomAudioSetManager.kt` - new metadata fields, archive-aware import
- New file: `android/app/src/main/java/com/dxxredux/app/ArchiveExtractor.kt` (or inline if small)
- Possibly `android/app/src/main/cpp/jni_fingerprint.c` - JNI for 7z extraction using LZMA SDK

**Decision**: For 7z, evaluate whether the LZMA SDK's already-vendored code can extract 7z archives (it's used for Inno currently). If not straightforward, consider Apache Commons Compress (pure Java) as a simpler path for 7z, or just a JNI wrapper around the LZMA SDK's 7z decoder.

**Verification**: Import a .zip containing OGGs, import a .dxa with music, import a .7z - all should appear as sets

---

## Phase 3: Chromaprint Auto-Matching on Import

**Context**: FingerprintBridge.kt already has `fingerprintAudioFile(path)` and `matchFingerprint(encoded, duration)`. The known_discs.json5 database has chromaprint fingerprints for known tracks. This machinery exists but needs to be triggered automatically.

**Steps**:
1. After importing files (Phase 2 or existing individual import), trigger fingerprinting in background:
   - For each audio file: call `FingerprintBridge.fingerprintAudioFile()` then `matchFingerprint()`
   - Store results in per-track metadata: `{chromaprintName, chromaprintAlbum, confidence}`
2. For CD Audio sources (AudioSourceManager): trigger matching when a new source is added
   - Use `FingerprintBridge.fingerprintDiscTrack()` for each audio track
   - Store in `audio_sources.json` per-source track_names map (already exists as `trackNames: Map<Int, String>`)
3. Persist chromaprint results so matching only happens once:
   - Custom sets: add `trackMetadata: Map<String, TrackMeta>` to AudioSet in custom_audio_sets.json
   - CD Audio: already has trackNames in audio_sources.json
4. Show progress indicator during matching (can be slow for many tracks)
5. Write chromaprint-matched names to `audio_playlist.json` and `custom_music.m3u` (or a sidecar metadata file) so the C engine can read them

**Files to modify**:
- `android/app/src/main/java/com/dxxredux/app/CustomAudioSetManager.kt` - add per-track metadata, trigger matching
- `android/app/src/main/java/com/dxxredux/app/AudioSourceManager.kt` - trigger matching for CD sources
- `android/app/src/main/java/com/dxxredux/app/FingerprintBridge.kt` - possibly batch API
- `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt` - progress UI

**Verification**: Import a known Descent music set -> chromaprint names should auto-populate and persist across app restarts

---

## Phase 4: Track Info Dialog in Music Picker

**Status**: DONE (build 1025)

Track info dialogs already exist (AudioFileDetailDialog, CdTrackDetailDialog). Enhanced with:
- CdTrackDetailDialog: added "Track N" display line
- AudioFileDetailDialog: added file path, match confidence (%), CD track number from fingerprint
- Persisted confidence + trackNum in AudioSet (trackConfidences, trackNumbers maps)
- TrackDetail data class expanded with confidence/trackNum fields
- Import flow captures full MatchResult data (name, confidence, trackNum)

---

## Phase 5: Track Names in Overlay and Picker

### 5A: Track Names in Music Picker
1. Update track display format in MusicPickerPage.kt:
   - CD Audio: "Track N - [chromaprint name]" or "Track N" if no match
   - Custom files: "Track N - [chromaprint name]" or "Track N" (filename-based fallback)
2. Read names from persisted metadata (Phase 3)

### 5B: Track Names in Game Overlay
1. For custom music: the jukebox/songs system needs access to chromaprint names
   - Write a `custom_music_names.json` that track_names.c reads (cleanest approach)
2. Modify `track_overlay_notify()` in track_names.c to check custom music name map
3. JNI: add custom music name loading from the JSON written by Kotlin

**Files to modify**:
- `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt` - display format
- `android/app/src/main/java/com/dxxredux/app/CustomAudioSetManager.kt` - write names JSON before launch
- `android/app/src/main/cpp/shared/track_names.c` / `.h` - load custom music names
- `android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt` - update in-game panel display

**Verification**: Play custom music in-game -> overlay shows track name; check picker shows names

---

## Phase 6: Gyro Deadzone UI Changes

**Current state**: Deadzone slider shows raw value 0-0.1 (radians). Default is 0.02.

**Steps**:
- [x] Change deadzone slider to display as % of maxAngle: range 0% to 30%, default 10%
- [x] Internal storage: fraction (0.0-0.3) converted to radians at application time
- [x] Update GyroConfig default: deadzone = 0.1f (10% of maxAngle)
- [x] Update GyroInputManager.applyDeadzone() for fraction-based value
- [x] Update bundled presets to use new default
- [x] Change default invert: pitch=true, roll=true, yaw stays false
- [x] Migration for old configs

**Files to modify**:
- `android/app/src/main/java/com/dxxredux/app/TouchControl.kt` - GyroConfig defaults
- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` - slider display
- `android/app/src/main/java/com/dxxredux/app/GyroInputManager.kt` - deadzone application
- `android/app/src/main/java/com/dxxredux/app/HumanReadableConfig.kt` - migration
- `android/app/src/main/assets/configs/touch/simple.json` - update preset
- `android/app/src/main/assets/configs/touch/claw.json` - update preset
- `android/app/src/main/assets/configs/touch/advanced.json` - update preset

---

## Phase 7: Double-Tap Mode Selection

**Current behavior**: Double-tap fires binding for 50ms (DOUBLE_TAP_RELEASE_DELAY_MS), with repeated taps each triggering another fire.

**Steps**:
- [ ] Define DoubleTapMode enum: REPEAT_FIRE, SINGLE_FIRE, LATCH_DOUBLE, LATCH_SINGLE
- [ ] Add doubleTapMode to StickControl (default: REPEAT_FIRE)
- [ ] Modify fireDoubleTapBinding() for mode-aware logic
- [ ] Visual indicator for latched state
- [ ] Mode picker in editor stick settings dialog
- [ ] Persist to touch_layout.json

**Files to modify**:
- `android/app/src/main/java/com/dxxredux/app/TouchControl.kt` - DoubleTapMode enum, StickControl field
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` - mode-aware firing logic
- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` - mode picker UI
- `android/app/src/main/java/com/dxxredux/app/HumanReadableConfig.kt` - serialization

---

## Phase 8: Latching Regular Buttons

**Steps**:
- [ ] Evaluate existing `toggle` field vs new `latch` field
- [ ] Material Green #4CAF50 at 10% opacity for latched state
- [ ] drawButton() logic for latched visual
- [ ] Editor UI for latch toggle (default: off)
- [ ] Persist to touch_layout.json

**Files to modify**:
- `android/app/src/main/java/com/dxxredux/app/TouchControl.kt` - latch field
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` - green latched drawing
- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` - latch toggle in editor
- `android/app/src/main/java/com/dxxredux/app/HumanReadableConfig.kt` - serialization

---

## Phase Dependencies

```
Phase 6 (gyro)     --+
Phase 7 (dbl-tap)  --+-- all independent, do first
Phase 8 (latch)    --+
Phase 1 (playback) <- Phase 2 (archive import) <- Phase 3 (chromaprint)
                                                      |          |
                                                  Phase 4    Phase 5
```

**Execution order**: 6 -> 7 -> 8 (quick wins), then 1 -> 2 -> 3 -> 4+5 (pipeline)

## Decisions
- DXA = renamed ZIP, use Java ZipInputStream for extraction
- 7z extraction via LZMA SDK (already vendored) or Apache Commons Compress (TBD)
- Deadzone stored as fraction of maxAngle (0.0-0.3), not raw radians
- Default gyro invert: pitch=true, roll=true, yaw=false
- Latching button uses Material Green #4CAF50 at 10% opacity
- PCM playback reuses existing ring buffer architecture in digi_tsf_music.c
- Chromaprint matching applies to both CD Audio and custom audio files
- Track names for custom music via sidecar `custom_music_names.json`

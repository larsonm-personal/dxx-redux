# GOG Audio Optional Extraction Plan

## Goal
Make .gog/.inst (CD audio) extraction optional during GOG import, since DESCENT_II.gog is ~448MB.
When extracted, the engine's legacy fallback in `RBAInit()` → `parse_cue_file()` will
automatically pick them up (case-insensitive PhysFS lookup).

## Changes

### 1. C layer: jni_gog_import.c
- Add `is_audio_extension()` for `.gog` / `.inst`
- Add `jboolean includeAudio` to `nativeExtractFiles()` JNI signature
- .exe path: skip audio files when false
- .pkg path: pass `skip_audio` flag to `pkg_extract_all`

### 2. C layer: pkg_reader.h/c
- Add `int skip_audio` parameter to `pkg_extract_all()`
- Skip `.gog`/`.inst` entries during streaming extraction when set

### 3. Kotlin: GogImportBridge.kt
- Add `includeAudio: Boolean` to `extractFiles()` and JNI declaration
- Add `isAudioFile(name)` companion helper

### 4. Kotlin: AudioSourceManager.kt
- Fix `hasLegacyGog()` case sensitivity — use case-insensitive file matching

### 5. Kotlin: SetupActivity.kt GogImportDialog
- Partition file list into game files and audio files by extension
- Show audio files separately with checkbox "Include CD audio (XXX MB)"
- Checkbox default: true (but user can opt out)
- Pass checkbox state to `extractFiles()`
- Post-extraction status reflects audio state

## Notes
- Mac D2 .pkg has DESCENT_II.gog but NOT DESCENT_II.inst — audio won't work.
  The checkbox still appears but audio extraction from .pkg is incomplete.
- Engine uses legacy path automatically: no AudioSourceManager registration needed.
- D1 GOG installers don't include .gog/.inst (D1 has no redbook audio in GOG release).

### 6. Config timing fix: enableRedbookInConfig()
- Added `enableRedbookInConfig(filesDir)` in SetupActivity.kt
- Sets `MusicType=2` and `OrigTrackOrder=1` in descent.cfg after GOG import with audio
- Called from both the GogImportDialog and the `import_gog` broadcast handler
- Needed because the C engine's `android_apply_initial_defaults()` only runs when descent.cfg doesn't exist, but Kotlin creates it on first launch via `writeInitialGameConfig()`
- Shared constant: `MusicType=2` corresponds to `MUSIC_TYPE_REDBOOK` (defined in `d2/main/digi.h`)

## Verification Results
- **include_audio=true**: 21 files extracted including DESCENT_II.gog (448MB) + DESCENT_II.inst (404B). Config updated to MusicType=2, OrigTrackOrder=1. Redbook audio verified: 9 tracks, playing.
- **include_audio=false**: 19 game files extracted, no .gog/.inst. Config unchanged.
- **hasLegacyGog case fix**: `has_legacy_gog_audio: true` in setup introspection with UPPERCASE files on disk.

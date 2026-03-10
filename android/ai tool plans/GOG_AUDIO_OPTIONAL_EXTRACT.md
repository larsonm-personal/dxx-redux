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

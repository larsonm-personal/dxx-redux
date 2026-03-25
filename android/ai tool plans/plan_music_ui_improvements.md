# Plan: Music System UI Improvements and Bug Fixes

## Overview
12 items covering bug fixes, new UI composables, and UX improvements to the
Android launcher's music management system.

## Phase 1: Bug Fixes (this session)

### 1A. Filter "[unknown] - [untitled]" track names
- **Problem**: AcoustID returns placeholder names like "[unknown] - [untitled]" which
  display verbatim in the track list and C engine overlay
- **Approach**: Filter at two levels:
  1. `FingerprintBridge.kt` -- in `lookupTrackNames()` and `flattenFingerprintDb()`,
     skip entries where BOTH artist="[unknown]" AND title="[untitled]"
  2. `track_names.c` -- in `track_overlay_notify()`, if the resolved name matches
     the pattern "[unknown] - [untitled]", fall back to "Track N"
  3. `MusicPickerPage.kt` -- TrackPreviewDialog CD track list, apply same filter
- **Keep partial matches**: "Mark Morgan - [untitled]" (d2-mac track 9) has useful info
- **Files**: FingerprintBridge.kt, track_names.c, MusicPickerPage.kt

### 1B. Import warning toasts (BIN without CUE, CUE without BIN, unrecognized files)
- **Problem**: SetupActivity.kt file picker silently drops:
  - .bin files without a matching .cue
  - .cue files without a matching .bin (NEW - user request)
  - any file that doesn't match known extensions
- **Fix**: After categorization, collect "not imported" files:
  - BIN without CUE: "X.bin requires a matching CUE file"
  - CUE without BIN: "X.cue requires a matching BIN file"
  - Catch-all: "X.xyz was not imported"
  - Show Toast(s) for each
- **Files**: SetupActivity.kt ~L1720-1790

### 1C. Auto-set music mode after CD import
- **Problem**: `enableRedbookInConfig()` sets `descent.cfg MusicType=2` but doesn't set
  SharedPreferences `music_mode` pref to "cd", so the launcher music status stays on
  whatever mode was previously selected
- **Fix**: In `enableRedbookInConfig()`, also write SharedPreferences music_mode="cd"
  Need to pass Context to `enableRedbookInConfig()` or handle it at the callsite
- **Files**: SetupActivity.kt -- enableRedbookInConfig() (L3389) and callers (L201, L4396)

## Phase 2: Feature Additions (future session)

### 2A. Track info screen with mini player
- Mini player should reuse existing C player code via JNI (not Android MediaPlayer)
  so bugs show up in both game and launcher, maintaining quality
- Need JNI bridge: start_track_preview(bin_path, track_num), stop_preview(), get_position()
- Show: AcoustID album, track length, track name, play/pause + progress bar

### 2B. Unify CD audio and audio file track lists
- Make CD audio tracks clickable (currently only audio file tracks are)
- Shared TrackListItem composable for both

### 2C. Scroll indicators for track lists in dialogs

## Phase 3: UI/UX Polish (future session)

### 3A. "Files" view redesign (show mode-appropriate content)
### 3B. CD audio source info view
### 3C. Audio files source info view
### 3D. Delete confirmation for audio files (currently missing)
### 3E. Delete note for CD audio (add context about file retention)

## Status
- [x] 1A. Filter unknown/untitled track names -- DONE
- [x] 1B. Import warning toasts (BIN w/o CUE, CUE w/o BIN, unrecognized files) -- DONE
- [x] 1C. Auto-set music mode after CD import -- DONE
- [x] Code quality checks pass (ktlint, clang-format)
- [x] Full APK build passes (arm64, armv7, x86_64 + Kotlin)
- [x] 2A. CD track mini player with native C playback -- DONE
  - cd_preview.c: standalone player (fopen + OpenSL ES, shares PCM decode + resample with rbaudio_bin.c)
  - jni_cd_preview.c: JNI bridge
  - CdPreviewBridge.kt: Kotlin bindings
  - CdTrackDetailDialog: play/pause, stop, seekable progress bar, duration display
- [x] 2B. CD tracks clickable in TrackPreviewDialog -- DONE
- [x] 2C. Scroll indicators in TrackPreviewDialog -- DONE
- [ ] 3A-3E: future work

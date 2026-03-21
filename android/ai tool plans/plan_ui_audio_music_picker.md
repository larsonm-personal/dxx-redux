# Plan: UI/Audio Fixes and Music Picker

## Phase 1: Quick Fixes -- DONE

### 1a: "NEXT" button on level complete screen -- DONE
- Added `volatile int g_levelcomplete_active` flag in android_input.c
- Set/clear around `newmenu_do2()` in `DoEndLevelScoreGlitz()` (d1 + d2)
- Added JNI getter `nativeIsLevelCompleteActive()`
- Overlay poller in MainActivity.kt shows skipButton with "NEXT" label
- Injects ESC key (same as skip button; both ENTER and ESC dismiss the newmenu)
- Scoped to solo score screen only; multiplayer kmatrix deferred

### 1b: Scroll indicators on Advanced Settings -- DONE
- Wrapped scrollable Column in Box, added `ScrollArrows(scrollState)`
- Added private `BoxScope.ScrollArrows()` function (same pattern as SetupActivity)
- Added imports: ScrollState, CircleShape, Icons, KeyboardArrowUp/Down

### 1c: Reduce log/crash file limits -- DONE
- NetLog.kt: MAX_FILES 10 -> 5
- CrashLog.kt: MAX_FILES 20 -> 5

## Phase 2: Audio Latency Fix -- DONE

- SDL_androidaudio.h: NUM_BUFFERS 4 -> 2
- d1/d2 arch/sdl/digi_mixer.c: SOUND_BUFFER_SIZE 4096 -> 2048 (Android ifdef)
- Previous: 4 x 4096/48000 = ~340ms latency
- New: 2 x 2048/48000 = ~85ms latency
- Needs device testing to confirm no crackling

## Phase 3: GOG .gog/.inst Direct Import Fix -- DONE

- File picker categorization: .gog and .inst now detected as separate types
- When both are present, routed to DiscImportDialog as CUE+BIN pair
- .inst treated as CUE sheet, .gog as BIN (which they are)
- If only one is present, falls back to generic game file import
- DiscImportDialog already handles CUE parsing, track listing, and AudioSourceManager
  registration, so no additional dialog code needed

## Phase 4: Music Picker -- NOT STARTED

### 4a: MusicPickerPage.kt (new file)
- Mode selector: MIDI | CD Audio | Audio Files
- Maps to MUSIC_TYPE_BUILTIN (1), MUSIC_TYPE_REDBOOK (2), MUSIC_TYPE_CUSTOM (3)
- Also offer MUSIC_TYPE_NONE (0) as "Off"

### 4b: MIDI mode
- Show count of HMP files in game data (informational)
- Need JNI helper to count HMP in HOG archive
- No user config beyond mode selection

### 4c: CD Audio (Redbook) mode
- List registered sources from AudioSourceManager
- Reorder, enable/disable, add via file picker, remove (with delete option)
- "Preview track list" button for merged view

### 4d: Audio Files (Jukebox) mode
- "Import audio set" from MP3/OGG/FLAC with name prompt
- List audio sets, per-set track list (alpha sorted)
- Remove sets (with delete option)
- Generate M3U playlist at launch time

### 4e: Track list preview dialog
- Scrollable list: track name + source indicator
- Click row to show source details

### 4f: Config writing before launch
- MusicType=N in descent.cfg
- REDBOOK: audio_playlist.json (existing)
- CUSTOM: M3U + CMLevelMusicPath in descent.cfg

### 4g: Persistence
- Redbook: audio_sources.json (existing)
- Custom sets: new JSON file (custom_audio_sets.json or extend audio_sources.json)
- Selected mode: prefs or JSON

### 4h: Navigation
- Entry point from launcher settings or main screen

## Phase 5: Integration Testing -- NOT STARTED
- Automation script for level complete NEXT button
- Device test for audio latency
- Import test for .gog+.inst
- Music picker end-to-end per mode

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

## Phase 4: Music Picker -- DONE (initial implementation)

Implemented files:
- `CustomAudioSetManager.kt`: manages custom audio file sets (MP3/OGG/FLAC) with JSON persistence
- `MusicPickerPage.kt`: full-screen Composable page with 3 modes (MIDI, CD Audio, Audio Files)
- SetupActivity.kt: navigation wiring, ControllerSection "Music" button, config writing at launch

Implementation sub-phases all completed:
- Phase 4a: MusicPickerPage scaffold + navigation wiring -- DONE
- Phase 4b: Music mode selector (3 FilterChips, SharedPreferences) -- DONE
- Phase 4c: Redbook section (AudioSourceManager list, enable/disable, reorder, remove) -- DONE
- Phase 4d: Custom audio section (list sets, add via SAF picker + name dialog, remove w/ delete) -- DONE
- Phase 4e: Track list preview dialog (merged track list per mode) -- DONE
- Phase 4f: MIDI section (informational) -- DONE
- Phase 4g: Config writing before launch (MusicType + CMLevelMusicPath + M3U) -- DONE
- Phase 4h: GOG import enhancement (register as AudioSource) -- DEFERRED (existing flow works)

Build verified: assembleDebug SUCCESS, no new warnings.

## Phase 5: Integration Testing -- NOT STARTED
- Automation script for level complete NEXT button
- Device test for audio latency
- Import test for .gog+.inst
- Music picker end-to-end per mode

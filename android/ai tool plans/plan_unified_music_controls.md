# Plan: Unified Music Controls & Info Category in Touch Editor

## TL;DR
Extend in-game music controls to cover MIDI and Redbook (not just jukebox), make music controls layout-driven via the touch editor as an "Info" category alongside gyro diagnostics, and eliminate the current non-deterministic visibility behavior.

---

## Phase 1: C Engine -- Unified Music Query/Control API (d1/ and d2/) [DONE]

### Problem
- `jni_music_control.c` only calls RBA* functions (Redbook). MIDI mode has no next/prev/list API
- BUILTIN mode's `songs_play_level_song()` rejects offset!=0 (`if (offset) return Song_playing`)
- No API to query current music type at runtime from JNI

### Steps

1. **Add `songs_next_track()` / `songs_prev_track()` to d2/main/songs.c and d1/main/songs.c** (*new functions, guarded with `#ifdef ANDROID`*)
   - For BUILTIN: advance `Song_playing` within the level-songs range (`SONG_FIRST_LEVEL_SONG` to `Num_bim_songs-1`), wrapping, call `songs_play_file()` + `track_overlay_notify()`
   - For REDBOOK: delegate to existing `RBANextTrack()` / `RBAPrevTrack()`
   - For CUSTOM: delegate to existing jukebox track advancement

2. **Add `songs_play_specific_track(int track)` to d2/main/songs.c and d1/main/songs.c** (*`#ifdef ANDROID`*)
   - BUILTIN: validate range, play `BIMSongs[track].filename`, update `Song_playing`
   - REDBOOK: delegate to `RBAPlaySpecificTrack(track)`
   - CUSTOM: set `GameCfg.CMLevelMusicTrack[0]` and call `jukebox_play()`

3. **Add `songs_get_track_info()` to d2/main/songs.c and d1/main/songs.c** (*`#ifdef ANDROID`*)
   - Returns: current music type, current track index, total track count, track name
   - BUILTIN: track=`Song_playing`, total=`Num_bim_songs - SONG_FIRST_LEVEL_SONG`, name=`BIMSongs[Song_playing].filename`
   - REDBOOK: delegates to `RBAGetCurrentTrackInfo()` etc
   - CUSTOM: track=`GameCfg.CMLevelMusicTrack[0]`, total=`CMLevelMusicTrack[1]`, name=`jukebox_current()`

4. **Add `songs_get_track_list()` for track picker** (*`#ifdef ANDROID`*)
   - BUILTIN: iterate `BIMSongs[SONG_FIRST_LEVEL_SONG..Num_bim_songs-1]`, return filenames
   - REDBOOK: iterate `RBAGetNumberOfTracks()`, filter audio, return names via `RBAGetTrackName()`
   - CUSTOM: iterate jukebox list
   - Returns JSON string (matches existing pattern in midi_enumeration.c)

5. **Mirror all changes in d1/main/songs.c** (same hooks, same function signatures)

### Key files
- `d2/main/songs.c` -- add functions (~80 lines)
- `d1/main/songs.c` -- mirror additions
- `d2/main/songs.h`, `d1/main/songs.h` -- declare new functions
- `d2/arch/sdl/jukebox.c`, `d1/arch/sdl/jukebox.c` -- may need to expose `jukebox_play_track(int)` helper

---

## Phase 2: JNI Bridge -- Unified Music Control [DONE]

### Steps

6. **Rewrite `jni_music_control.c` to call `songs_*` instead of `RBA*`**
   - `nativeNextTrack` -> `songs_next_track()`
   - `nativePrevTrack` -> `songs_prev_track()`
   - `nativePlaySpecificTrack` -> `songs_play_specific_track()`
   - `nativeGetCurrentTrackInfo` -> `songs_get_track_info()`
   - Keep existing `nativeGetTotalTracks`, `nativeIsAudioTrack`, `nativeGetTrackName` but route through unified API

7. **Add `nativeGetMusicType()` JNI function** -- returns `GameCfg.MusicType` (0-3)

8. **Add `nativeGetTrackList()` JNI function** -- returns JSON track list from `songs_get_track_list()`

### Key files
- `android/app/src/main/cpp/jni_music_control.c` -- rewrite to unified API
- `android/app/src/main/cpp/CMakeLists.txt` -- may need to add songs.h include path

---

## Phase 3: Touch Editor -- Rename Diagnostic to "Info" Category [DONE]

### Steps

9. **Add `MUSIC` to `DiagnosticType` enum** in `TouchControl.kt`
   - `enum class DiagnosticType { GYRO, MUSIC }`

10. **Rename "Diagnostic Display" to "Info" in AddControlDialog** in `TouchEditorPage.kt`
    - When user taps "Info", show a 2nd-level dialog: "Gyro Display" or "Music Controls"
    - Gyro creates `DiagnosticControl(type=GYRO)` (current behavior)
    - Music creates `DiagnosticControl(type=MUSIC)`

11. **Add type selector to DiagnosticPropertiesPanel** in `TouchEditorPage.kt`
    - Add radio buttons or dropdown: Gyro / Music Controls
    - Allow changing type of existing info control via the bottom property panel

### Key files
- `android/app/src/main/java/com/dxxredux/app/TouchControl.kt` -- DiagnosticType enum
- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` -- AddControlDialog, DiagnosticPropertiesPanel

---

## Phase 4: Touch Overlay -- Layout-Driven Music Controls [DONE]

### Steps

12. **Remove hardcoded music buttons from TouchOverlayView.kt**
    - Remove `prevBtnCenterX`, `nextBtnCenterX`, `musicBtnY`, `musicBtnRadius`, `musicLabelX`
    - Remove `prevBtnPointerId`, `nextBtnPointerId`, `musicLabelPointerId`
    - Remove hardcoded rendering in `onDraw()` (lines ~688-708)
    - Remove hardcoded touch detection (lines ~1425-1449)
    - Remove `releasePrevButton()`, `releaseNextButton()`, `releaseMusicLabel()`

13. **Add music control rendering for `DiagnosticType.MUSIC` controls**
    - When `type == MUSIC`: render prev/next buttons + track label at the control's position
    - Use the control's `xPct`/`yPct` for positioning, `sizeMult` for scaling, `opacity` for alpha
    - Render: [<<] [track name] [>>] horizontally around the control center
    - Track label tap opens MusicControlPanel (same as current behavior)

14. **Add music control touch handling**
    - Detect touches on prev/next/label sub-regions within MUSIC diagnostic controls
    - Fire same callbacks: `prevTrackCallback`, `nextTrackCallback`, `musicPanelCallback`

15. **Visibility: always show if in layout, remove trackLabel-based visibility**
    - Music controls render whenever a MUSIC diagnostic exists in the layout
    - `trackLabel` still updated by polling but only affects the displayed text, not visibility
    - If no music is playing, show "No music" or similar instead of hiding

### Key files
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` -- major refactor of music rendering

---

## Phase 5: MusicControlPanel -- Unified Track Picker [DONE]

### Steps

16. **Update MusicControlPanel to use unified JNI calls**
    - Replace direct RBA* JNI calls with unified `nativeGetTrackList()`
    - Display tracks from any music type (MIDI shows filenames, Redbook shows track names, jukebox shows filenames)
    - Highlight current track using `nativeGetCurrentTrackInfo()`

17. **Handle MIDI track names**
    - Strip `.hmp`/`.mid` extension for display
    - Capitalize/clean up filenames: "game01.hmp" -> "Game 01"

### Key files
- `android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt`

---

## Phase 6: Update Bundled Presets & Testing [DONE]

### Steps

18. **Update bundled touch presets** to include a MUSIC info control
    - Add to `simple.json`, `claw.json`, `advanced.json` in `assets/configs/touch/`
    - Default position: top-left area (similar to current hardcoded position)

19. **Add layout migration** in `TouchLayoutRepository.migrate()` to auto-add a MUSIC info control for existing users

20. **Build and test**
    - cmake build for d1 and d2 (verify new songs.c functions compile)
    - Android build
    - Manual test: MIDI mode -- verify next/prev/picker work
    - Manual test: Redbook mode -- verify next/prev/picker still work
    - Manual test: Touch editor -- add/remove/edit Info controls
    - Verify gyro diagnostic still works unchanged

21. **Integration test**
    - Extend or create a test script that verifies music controls appear in overlay when configured
    - Use introspection API to check music state

22. **Run code quality**: `android\run-code-quality.ps1 --fix`

---

## Decisions

- **MIDI "tracks" = level songs only** (indices SONG_FIRST_LEVEL_SONG through Num_bim_songs-1). Title/briefing/credits songs are not included in the skip list because the player can't meaningfully navigate to them during gameplay
- **Jukebox (CUSTOM) structurally supported** but won't work on Android until MP3/OGG/FLAC decoders are added. Acceptable
- **"Info" not "Diagnostic"** -- rename throughout the UI but keep `DiagnosticControl`/`DiagnosticType` class names in code for now (avoiding a churn-heavy rename of internals)
- **Music controls always visible when in layout** -- replaces the flaky `trackLabel.isNotEmpty()` gate. When no music plays, show "No track" rather than hiding
- **d1/ and d2/ changes are minimal** -- new functions are `#ifdef ANDROID` guarded. Existing behavior unchanged on desktop
- **Single composite control** -- prev + label + next rendered as one unit (not three separate controls in the editor)
- **Preset migration** -- auto-add default MUSIC diagnostic in `TouchLayoutRepository.migrate()` with a version bump
- **Track name quality for MIDI**: strip extension and title-case for now ("game01.hmp" -> "Game 01"), no separate name database

## Status
- [ ] Phase 1: C engine unified API
- [ ] Phase 2: JNI bridge
- [ ] Phase 3: Touch editor info category
- [ ] Phase 4: Touch overlay layout-driven music
- [ ] Phase 5: Unified track picker
- [ ] Phase 6: Presets, migration, build, test

# Plan: Touch Controls, Music Overlays, and Radial Menu Improvements

Eight issues across music controls, popup overlays, and radial menus.

## Phase 1: Music Controls Preview in Touch Editor

**Problem:** The touch editor diagnostic control preview always renders Yaw/Roll/Pitch gyro text regardless of type. MUSIC diagnostics should show a mock music control preview.

**Root cause:** `TouchEditorPage.kt` diagnostic drawing block (~L1045-1082) unconditionally draws gyro text. No type check.

**Fix:** Add type check: if `d.type == DiagnosticType.MUSIC`, render `[< Track Name >]` placeholder instead of gyro values. Adjust box dimensions to match the music control shape.

**Files:** `TouchEditorPage.kt` -- diagnostic drawing block

---

## Phase 2: Jukebox Track Name Display (Chromaprint Integration)

**Problem:** Custom music track label shows full filesystem path like `data/user/0/com.dxxredux.app/files/custom_music/asdf.../track.mp3`. Should show chromaprint-decoded name if available, falling back to stripped filename.

**Current state of chromaprint for jukebox:**
- Launcher fingerprints jukebox files at import time (`MusicPickerPage.kt importAudioFiles()` calls `FingerprintBridge.fingerprintAndMatch()`)
- Matched names stored in `custom_audio_sets.json` in `AudioSet.trackNames` map (filename -> decoded name)
- But `writeM3U()` writes only bare paths -- names are NOT passed to the C engine
- C engine's `jukebox_current()` returns raw path; `songs_get_track_info()` copies it verbatim
- `track_overlay_notify_jukebox()` strips path/extension but has no access to decoded names

**Fix (end-to-end pipeline):**

### 2a. Kotlin: Write track names sidecar JSON
In `CustomAudioSetManager.writeM3U()`, after writing the M3U file, also write `custom_music_names.json` containing a mapping from full absolute path to decoded track name:
```json
{
  "/data/.../custom_music/set1/track01.mp3": "Crawl",
  "/data/.../custom_music/set1/track02.ogg": "Fire in the Hole"
}
```
Only include entries where a chromaprint match exists. This is a path -> name map (not index-based, since jukebox order can change).

### 2b. C: Load sidecar at jukebox init
Add `jukebox_names.c` (or extend `track_names.c`) with:
- A static table `s_jukebox_names[MAX_JUKEBOX][{path_hash, name}]` (or simple linear array)
- `jukebox_names_load(filesDir)` -- parse `custom_music_names.json`, store entries
- `jukebox_names_lookup(path)` -- return decoded name for a given path, or NULL

Call `jukebox_names_load()` during `jukebox_load()`.

### 2c. C: Use decoded names in songs_get_track_info
In `songs_get_track_info()` CUSTOM case (d2/songs.c, d1/songs.c): after getting `jukebox_current()`, call `jukebox_names_lookup(cur)`. If found, use the decoded name. Otherwise, strip path/extension as fallback (same logic as `track_overlay_notify_jukebox`).

### 2d. C: Use decoded names in track_overlay_notify_jukebox
Same lookup: try `jukebox_names_lookup(filename)` first, then fall back to basename stripping.

### 2e. Kotlin: Defensive basename extraction
In `MainActivity.kt updateTrackLabel()`: strip path and extension from `parts[3]` as a fallback safety net, in case the C-side name still leaks a full path.

**Files:**
- `CustomAudioSetManager.kt` -- `writeM3U()`: write sidecar JSON
- `track_names.c` -- add jukebox name table and lookup
- `track_names.h` -- declare new functions
- `d2/main/songs.c` and `d1/main/songs.c` -- `songs_get_track_info()` CUSTOM case
- `MainActivity.kt` -- `updateTrackLabel()` defensive fallback

---

## Phase 3: Overlay Popup Fade-Out Flash

**Problem:** At the end of fade-out animation, the overlay text briefly shows full brightness for one frame.

**Root cause:** In `showOverlayLine()` (MainActivity.kt ~L1776), the fade-out `onAnimationEnd` calls `removeView()`. Between animation end and removal, a layout pass can render the view with non-zero alpha.

**Fix:** In fade-out `onAnimationEnd`, set `tv.alpha = 0f` before `removeView()`. Applies to both track name and level name popups (shared code path).

**Files:** `MainActivity.kt` -- `showOverlayLine()` onAnimationEnd

---

## Phase 4: Missing "New Track" Popup for Redbook Next/Prev

**Problem:** Popup shows for the first track but not when skipping via next/prev.

**Root cause:** `songs_next_track()`/`songs_prev_track()` REDBOOK cases call `RBANextTrack()`/`RBAPrevTrack()`, which call `RBAPlayTrack()`. None of these call `track_overlay_notify()`.

**Fix:** After `RBANextTrack()`/`RBAPrevTrack()` return a valid track number, call `track_overlay_notify(track, 0, RBAGetDiscID())`.

Also add notification after jukebox next/prev (CUSTOM case) -- currently those call `jukebox_play()` which may call `track_overlay_notify_jukebox`, need to verify. If not, add it.

**Files:**
- `d2/main/songs.c` -- REDBOOK case in next/prev (~L558, ~L597)
- `d1/main/songs.c` -- same
- Verify jukebox next/prev calls trigger overlay notification

---

## Phase 5: MIDI Track Skip Causes Music Controls Disappearance

**Problem:** After "next track" in MIDI mode, the music controls vanish entirely.

**Root cause:**
1. `songs_next_track()` BUILTIN: sets `Song_playing` to new track
2. Calls `songs_play_file()` -> `songs_stop_all()` -> `Song_playing = -1`
3. `songs_get_track_info()` returns -1 because `Song_playing < SONG_FIRST_LEVEL_SONG`
4. `nativeGetCurrentTrackInfo()` returns empty -> `trackLabel = ""` -> controls hidden

**Fix:** Save computed track index before `songs_play_file()`, restore `Song_playing` after success (same pattern as `songs_play_specific_track()`).

In `songs_next_track()` and `songs_prev_track()` BUILTIN cases, change from:
```c
Song_playing = <new_index>;
if (songs_play_file(BIMSongs[Song_playing].filename, 1, NULL))
{
    track_overlay_notify(Song_playing, 1, 0);
    return 1;
}
```
to:
```c
int track = <new_index>;
if (songs_play_file(BIMSongs[track].filename, 1, NULL))
{
    Song_playing = track;
    track_overlay_notify(track, 1, 0);
    return 1;
}
```

**Files:**
- `d2/main/songs.c` -- BUILTIN case in next/prev
- `d1/main/songs.c` -- same

---

## Phase 6: Radial Menu Overlap Detection + Two Size Sliders

**Problem:** Overlap warnings use expanded wheel radius (`0.14f * sizeMult`) for radial menus when only the trigger button radius (`0.05f * sizeMult`) is the tap target.

**Additionally needed:** Two separate size sliders -- one for the inner trigger button, one for the outer wheel ring.

**Fix:**
1. Add `ringSizeMult: Float = 1f` to `RadialMenuControl` data class, with JSON serialization
2. Overlap detection: use trigger radius `0.05f * sizeMult` for radial menus (not `0.14f`)
3. Editor preview: use `ringSizeMult` for ghost wheel extent
4. `RadialPropertiesPanel`: rename "Size" to "Button" and add "Ring" slider for `ringSizeMult`
5. `TouchOverlayView.kt`: use `ringSizeMult` for wheel radius in geometry and rendering; keep `sizeMult` for trigger button

**Files:**
- `TouchControl.kt` -- `RadialMenuControl` data class
- `TouchEditorPage.kt` -- overlap bounds, preview, properties panel
- `TouchOverlayView.kt` -- geometry and rendering

---

## Phase 7: Bold + 2x Text for Active Radial Segment

**Problem:** All radial segment labels use same font size/weight during drag. Active segment text should be bold and 2x the size of inactive segments.

**Fix:** In `drawRadialMenu()` and `drawWeaponWheel()`: for active segment labels, set `paintBtnLabel.typeface = Typeface.DEFAULT_BOLD` and `textSize = r * 0.22f` (vs `r * 0.11f`). Reset typeface after.

**Files:** `TouchOverlayView.kt` -- `drawRadialMenu()`, `drawWeaponWheel()`

---

## Phase 8: Radial Menu Quiescent Text = Current Selection

**Problem:** Closed radial buttons show 4-char ID ("PriW", "SecW", "Guid"). Should show current weapon name.

**Fix:**
1. Extend `nativeGetWeaponState()` (jni_main.c) to include `primary_weapon` and `secondary_weapon` indices (append to array as indices 43-44)
2. Update `WeaponState.kt` to parse new fields
3. In quiescent radial drawing: for PriWpn/SecWpn, find the segment whose weaponIndex matches the current weapon index and display its label. Scale text to fit trigger circle. Fall back to ID if no match
4. Need periodic weapon state polling for quiescent text updates -- check if `weaponStateProvider` is already called per-frame or only on open
5. Guidebot: keep "Guide" or centerLabel (no persistent command state)

**Files:**
- `jni_main.c` -- extend array
- `WeaponState.kt` -- new fields
- `TouchOverlayView.kt` -- quiescent drawing, polling

---

## Implementation Order

All phases complete. Build verified (gradle assembleDebug) and linters run (clang-format, ktlint).

Phases 1, 3, 5 are quick independent fixes (one or two files each).
Phase 4 is small but touches d1/d2 songs.c.
Phase 2 is the largest (new JSON sidecar, C loader, multi-file integration).
Phases 6-8 are radial menu related and should go sequentially.

Order: 1 -> 3 -> 5 -> 4 -> 7 -> 6 -> 8 -> 2

---

## Verification
- Build APK, no new warnings
- Touch editor: MUSIC diagnostic shows music preview, not gyro
- Jukebox: track label shows decoded name or stripped basename
- Overlay popup: fade-out has no brightness flash
- Redbook/MIDI next/prev: popup appears and controls stay visible
- Radial: overlap uses trigger radius; two size sliders work
- Radial: active segment text is bold + 2x
- Radial: quiescent button shows current weapon name
- Run `android\run-code-quality.ps1 --fix`

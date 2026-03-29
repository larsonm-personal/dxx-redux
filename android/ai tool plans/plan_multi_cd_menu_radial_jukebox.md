# Plan: Multi-CD Redbook, Menu Button, Radial Presets, Jukebox Names, Audio Import

Seven issues to address in this pass.

## Issue 1: Jukebox track list shows full file paths in-game [DONE]

**Problem:** `jukebox_get_track_name(index)` returns the raw M3U path (absolute filesystem path). This is used by `songs_get_track_list()` CUSTOM case which feeds the in-game track picker.

**Fix:** In `jukebox_get_track_name()` (d1/d2 jukebox.c), try `jukebox_names_lookup()` first for chromaprint name. If not found, strip path+extension for a clean filename. Don't return raw path.

**Files:** d1/arch/sdl/jukebox.c, d2/arch/sdl/jukebox.c

## Issue 2: Audio sets interleaved instead of sequential [DONE]

**Problem:** `writeM3U()` in CustomAudioSetManager does `allFiles.sortBy { it.first }` for a global alphabetical sort across all sets, causing interleaving.

**Fix:** Remove global sort. Instead, iterate sets in order, and within each set sort alphabetically or by track number if chromaprint data exists. The order in the M3U reflects set order -> track order within set.

**Files:** CustomAudioSetManager.kt

## Issue 3: Move "menu" into info button options [DONE]

**Problem:** User wants the game menu (ESC) to be an option within the "info" diagnostic type rather than a separate button type.

**Fix:** Add `MENU` to `DiagnosticType` enum. Implement menu rendering in the overlay (a simple "MENU" button). When tapped, send ESC keypress via JNI.

**Files:** TouchControl.kt, TouchEditorPage.kt, TouchOverlayView.kt

## Issue 4: Prefix info sub-menu labels in "add control" [DONE]

**Problem:** Info sub-menu items show as "Gyro Display", "Music Controls" -- should show "info: Gyro Display", "info: Music Controls", "info: Menu".

**Fix:** In the add control dialog, prefix each `DiagnosticType.label` with "info: ".

**Files:** TouchEditorPage.kt

## Issue 5: Add radial menu preset selector in editor [DONE]

**Problem:** No way to add a preset radial menu (guide bot, primary, secondary) from the editor. The presets exist in layouts but can't be created from the add control menu.

**Fix:** When a radial menu is selected in the editor properties panel, add a "Preset" dropdown at the top with options: Custom, Primary Weapons, Secondary Weapons, Guide Bot. Selecting a preset sets the id and segments accordingly.

**Files:** TouchEditorPage.kt

## Issue 6: Fix music set import from file picker [DONE]

**Problem:** Adding a music set from "select game files..." shows the correct dialog but the set doesn't appear after import.

**Fix:** Move `customMgr` creation outside the conditional block in SetupActivity.kt. This ensures the manager persists across recomposition when the dialog closes.

**Files:** SetupActivity.kt

## Issue 7: Two-CD redbook track list [DONE]

**Problem:** When two CDs are loaded, only the first CD's tracks appear in the in-game track list.

**Root Cause:** CUE filename collision in SAF disc import. Both CDs' CUE files are saved as `filesDir/descent.cue` (using `cueName.lowercase()`), so the second import overwrites the first. Both sources then reference the same CUE file, which contains only the second CD's data.

**Fix:** After disc identification determines the unique `id`, rename the CUE file to `${id}.cue`. This ensures each source has a unique CUE file on disk.

**Files:** SetupActivity.kt (disc import registration block ~line 5511)

## Round 2 Fixes

### Fix 2A: Launcher "Preview Track List" still interleaved [DONE]

**Problem:** `getDetailedTrackList()` in CustomAudioSetManager had a global `tracks.sortBy { it.filename.lowercase() }` that interleaved tracks across sets. The `writeM3U()` function was already correct (no global sort), so the in-game ordering was right but the launcher preview was wrong.

**Fix:** Removed global sort from `getDetailedTrackList()`. Also removed unused `getMergedTrackList()`. Now `getDetailedTrackList()` is the single source of truth for track ordering in the launcher.

### Fix 2B: "info: menu" showing gyro info in editor [DONE]

**Problem:** The editor diagnostic preview only had two branches: MUSIC (with prev/next buttons) and everything-else (with gyro Yaw/Roll/Pitch text). MENU fell into the gyro branch.

**Fix:** Added a `DiagnosticType.MENU` branch to the editor preview that draws a 3x3 dot grid icon (matching the admin tray's settings grid appearance).

### Fix 2C: MENU diagnostic should open admin tray, not game menu [DONE]

**Problem:** `releaseMenuDiag()` dispatched `META_GAME_MENU` (SDLK_ESCAPE) which opens the game menu, not the settings admin tray.

**Fix:** Changed `releaseMenuDiag()` to set `adminTrayOpen = true` and call `animateAdminTray(true)` instead. Also changed the overlay icon from hamburger (3 bars) to 3x3 dot grid. When a MENU diagnostic is configured, the default admin tray tab at the bottom edge is hidden (both visually and for touch input), since the MENU diagnostic replaces it.

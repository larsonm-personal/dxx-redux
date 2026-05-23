# Work Plan: 11-Item Batch (Guidebot, Music, MSAA, Gyro, Config, Stale Refs)

## Item 1: Coop guidebot line not showing at all
### Status: NOT STARTED
### Analysis
The guidebot indicator line (`coop_indicator_lines.c`) is gated by `coop_qol_active()`, which requires `GM_MULTI_COOP` AND `NETGAME_FLAG_COOP_QOL`. In single player, neither condition is met, so the line never shows.

The user said "coop guidebot lines were turned off until guidebot is released" -- this was the `Buddy_allowed_to_talk` gate that was added. The line DOES check `Buddy_allowed_to_talk` (line 159). So in coop, the line should show after the guidebot cage walls are destroyed.

**Likely root cause**: The user may be testing in single player where the feature doesn't apply, OR in coop where `Buddy_allowed_to_talk` never gets set to 1. Need to verify: does the guidebot line work in coop after the guidebot is released? If the bug is specifically "the line doesn't show even after releasing the guidebot in coop", then the issue is likely that `Buddy_allowed_to_talk` isn't being synchronized to non-host players, or the render call is being skipped.

**Another possibility**: The user might want the guidebot line to work in **single player** too, not just coop. Currently it's coop-only by design.

### Plan
1. Add diagnostic logging in `coop_indicator_lines.c`:
   - Log when `coop_qol_active()` is false and why (no coop? no QoL flag?)
   - Log `Buddy_allowed_to_talk` value, `Buddy_objnum` value on each update
   - Log whether path computation succeeds
2. If single-player support is desired, add a separate code path that shows the buddy line in single player without requiring coop QoL flags:
   - In `coop_indicator_lines_render()`, allow rendering when not in coop IF `Buddy_allowed_to_talk` and `Buddy_objnum` are valid
   - This means the buddy path line shows in both single player and coop once the guidebot is released

### Files to modify
- `android/app/src/main/cpp/shared/coop_indicator_lines.c` - add single-player support for buddy line

---

## Item 2: Music controls touch menu renders to left of configured position
### Status: NOT STARTED
### Analysis
The music diagnostic control in `TouchOverlayView.kt` has a positioning issue:
- Geometry calculation (line ~650): Computes `d.musicPrevCX = d.centerX - d.width / 2 + r + diagTextSize * 0.5f`
- The width is calculated AFTER first use, then positions are recalculated -- this part actually looks correct since it recalculates
- The real issue is likely the **editor preview** using a different width calculation than the runtime overlay:
  - Runtime: `d.width = (d.musicLabelX - d.musicPrevCX + r) + paintDiagText.measureText("Track 00/00: xxxxxxxx") + diagTextSize`
  - Editor: `boxW = baseScale * 0.12f * d.sizeMult` (fixed approximation)
- When the user positions the control in the editor, they see it at one position, but at runtime the actual element renders at a different position because the width calculations differ

### Plan
The music control should be **left-aligned** to the configured position (xPct, yPct). Currently the code centers around centerX which causes the visual content to shift left as width grows.

1. In `TouchOverlayView.kt` geometry calculation: use `d.centerX` as the LEFT edge instead of subtracting `d.width / 2`
   - `d.musicPrevCX = d.centerX + r + diagTextSize * 0.5f` (no `- d.width / 2`)
2. In `TouchEditorPage.kt` editor preview: match the left-aligned behavior
3. Touch hit-test should already work since it uses the computed positions

### Files to modify
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` - left-align music control
- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` - match left-align in editor preview

---

## Item 3: MSAA causes automap to render without background
### Status: NOT STARTED
### Analysis
**Root cause identified**: When MSAA is enabled, the rendering pipeline uses an MSAA FBO (framebuffer object):
1. `show_fullscr()` renders automap background to framebuffer 0 (default)
2. `gr_clear_canvas()` clears to framebuffer 0
3. `g3_start_frame()` calls `ogl_start_frame()` which binds the MSAA FBO
4. Automap lines are drawn to the MSAA FBO (which was never cleared -- contains stale data)
5. `gr_flip()` resolves MSAA FBO to screen via `glBlitFramebuffer` -- copies garbage + lines

The MSAA FBO color buffer is never cleared with `glClear(GL_COLOR_BUFFER_BIT)`. Only `GL_DEPTH_BUFFER_BIT` is cleared in `ogl_start_frame()`.

Without MSAA, everything renders to framebuffer 0, so background + clear + lines all go to the same target and work correctly.

### Plan
1. In `ogl_start_frame()`, after binding the MSAA FBO, clear BOTH color and depth buffers:
   ```c
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   ```
   This is the simplest fix. The color buffer needs to start clean each frame when using MSAA.
2. Apply the same fix to both d1 and d2 `ogl.c`
3. Test: enable MSAA, open automap, verify background renders correctly

### Files to modify
- `d2/arch/ogl/ogl.c` (~line 1790) - add GL_COLOR_BUFFER_BIT to glClear after MSAA FBO bind
- `d1/arch/ogl/ogl.c` (equivalent location) - same fix

---

## Item 4: Map button needs to be ~2x its current size
### Status: NOT STARTED
### Analysis
The MAP button has `"size": 0.7` in all three preset configs (simple, advanced, claw). The user wants ~2x larger.

The user's exported config shows `"size": 1.1149...` for the map button. They want it bigger still.

### Plan
1. Change MAP button size in all three bundled presets from 0.7 to 1.4
2. Also update the user's exported config to use 1.4 for the map button (item 9 handles new defaults)

### Files to modify
- `android/app/src/main/assets/configs/touch/simple.json`
- `android/app/src/main/assets/configs/touch/advanced.json`
- `android/app/src/main/assets/configs/touch/claw.json`

---

## Item 5: Guidebot wheel touch menu shows "Locked" even when released
### Status: NOT STARTED
### Analysis
The guidebot wheel menu logic in `TouchOverlayView.kt`:
- Line 728: If `isEscortOwnerProvider?.invoke() == false`, sets label to owner callsign or "Locked"
- Line 1516-1520: If not escort owner OR D1, skips the guide wheel entirely (won't process touch)
- **Problem**: Only checks ownership (`Escort_owner_player`), never checks `Buddy_allowed_to_talk` (release state)
- **No JNI function** exists to expose `Buddy_allowed_to_talk` to Kotlin
- In D2 single player, the player IS the escort owner, so the wheel should work. But in coop, if another player owns the guidebot but the guidebot IS released, the non-owner sees "Locked" even though the guidebot is available

### Plan
1. Add JNI function `nativeIsBuddyReleased()` in `jni_main.c` that returns `Buddy_allowed_to_talk`
2. Add provider in `MainActivity.kt` like existing escort providers
3. Update `TouchOverlayView.kt`:
   - If buddy is NOT released: show "Locked" (correct)
   - If buddy IS released but owned by someone else: show owner callsign but ALLOW interaction (it's usable)
   - If buddy IS released and owned by local player: show normal guide wheel

### Files to modify
- `android/app/src/main/cpp/jni_main.c` - add `nativeIsBuddyReleased()`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` - wire up provider
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` - fix locked logic

---

## Item 6: Gyro editor needs per-axis deadzone
### Status: NOT STARTED
### Analysis
Currently `GyroConfig` (TouchControl.kt line ~434) has a single `deadzone: Float = 0.1f` applied to all axes. The user wants separate deadzones for pitch, yaw, and roll because pitch typically needs a much larger deadzone than roll.

### Plan
1. Add `deadzoneX`, `deadzoneY`, `deadzoneZ` to `GyroConfig`, deprecating the single `deadzone`
2. Migration: if individual deadzones are null/absent, fall back to the single `deadzone` value
3. Update `GyroInputManager.kt` to use per-axis deadzones
4. Update `TouchEditorPage.kt` GyroSettingsDialog to show three separate deadzone sliders (Pitch, Yaw, Roll)
5. Update serialization/deserialization to handle both old (single) and new (per-axis) formats

### Files to modify
- `android/app/src/main/java/com/dxxredux/app/TouchControl.kt` - add per-axis deadzone fields
- `android/app/src/main/java/com/dxxredux/app/GyroInputManager.kt` - use per-axis deadzones
- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` - three deadzone sliders

---

## Item 7: Max deadzone needs to be ~60% (currently 30%)
### Status: NOT STARTED
### Analysis
`TouchEditorPage.kt` line ~2859: `LabeledSlider("Deadzone ($dzPct%)", deadzone, 0f, 0.3f)` -- max is 0.3f (30%).

### Plan
1. Change max from 0.3f to 0.6f
2. Apply to all three per-axis deadzone sliders (from item 6)

### Files to modify
- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` - change deadzone slider max

---

## Item 8: Reorder gyro axis selections, remove triggers
### Status: NOT STARTED
### Analysis
Current axis options include L Trigger and R Trigger which are nonsensical for gyro. User wants this order: disabled, slide l/r, slide u/d, bank l/r, turn l/r, fwd/back.

The gyro axis picker already uses `GyroAxisPicker` in `TouchEditorPage.kt` (line ~2711). The axis labels come from `TouchBindings.AXIS_LABELS`.

Need to check if the regular (non-gyro) axis picker also uses the same labels -- if so, the trigger removal should only apply to the gyro picker, not to all axis pickers.

### Plan
1. Create `GYRO_AXIS_LABELS` map in `TouchBindings.kt` with just the 6 desired options in order
2. Update `GyroAxisPicker` to use `GYRO_AXIS_LABELS` instead of `AXIS_LABELS`
3. Keep `AXIS_LABELS` unchanged for non-gyro axis pickers (they might legitimately need trigger options)
4. The user said "apply this to other axis selections, if it's a shared thing, no need to customize for gyro" -- check if other pickers also have trigger options that should be removed. For non-gyro controls (sticks, sliders), triggers make sense as they map to actual controller axes, so probably leave them

### Files to modify
- `android/app/src/main/java/com/dxxredux/app/TouchBindings.kt` - add GYRO_AXIS_LABELS
- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` - use GYRO_AXIS_LABELS in GyroAxisPicker

---

## Item 9: Use exported config JSON as new build default
### Status: NOT STARTED
### Analysis
The exported config at `android/temp_game_logs/dxx_redux_config.json` should become the new default touch layout for fresh installs. The default is loaded from `assets/configs/touch/advanced.json` (or simple.json, whichever is first alphabetically).

The exported config is a "combined_config" with touch_layout + controller_config + autoselect ordering. The touch_layout portion should replace `advanced.json`.

### Plan
1. Extract the `touch_layout` section from the exported config
2. Replace `android/app/src/main/assets/configs/touch/advanced.json` with the extracted touch layout
3. Apply the MAP button size increase from item 4 to the new default (1.4 instead of 1.11)
4. Apply per-axis deadzone changes from item 6
5. The controller_config and autoselect sections: check if these differ from current defaults and update accordingly

### Files to modify
- `android/app/src/main/assets/configs/touch/advanced.json` - replace with user's config

---

## Item 10: Stale references popup ("alien1.pig" etc.) on every app start
### Status: NOT STARTED
### Analysis
Two `LaunchedEffect` blocks in `SetupActivity.kt` (lines ~2525 and ~2556) both keyed on `activeSetName`:

**Effect 1** (hashing): Calls `manifest.findStaleFiles()` which finds files on disk without manifest entries, then calls `manifest.upsert()` to add them.

**Effect 2** (pruning): Calls `manifest.pruneStaleEntries()` which removes entries where the file doesn't exist on disk.

These run **concurrently** as separate coroutines. If there's no actual bug in the logic (files exist = no prune, files don't exist = no re-add), then the issue might be:

1. **SAF manifest**: The `safManifest.pruneStaleEntries(context)` checks content URIs. If the user imported via SAF and the content URI permissions were lost, the SAF entries get pruned. But SAF entries aren't re-added by `findStaleFiles()` (which only checks physical files). So this wouldn't cause a cycle -- it'd be a one-time prune.

2. **Race condition**: Both effects call `load()` and `save()` on the same manifest without synchronization. If Effect 1 saves after Effect 2 reads but before Effect 2 saves, Effect 2's save would overwrite Effect 1's changes. But this still doesn't explain repeated display.

3. **Re-running**: If `activeSetName` changes (recomposition), both effects restart. If there's frequent recomposition, both effects trigger repeatedly.

4. **Migration**: `migrateDefaultSetIfNeeded()` copies entries from old manifest location. If the old manifest persists after migration, entries keep getting re-copied.

**Most likely**: Need to add diagnostic logging to determine exactly which manifest (asset vs SAF) is reporting the stale entries, and what the manifest state looks like before and after pruning.

### Plan
1. Add logging to diagnose:
   - In `pruneStaleEntries()` for both AssetManifest and SafManifest: log entries before and after prune, and what filesDir is
   - In `findStaleFiles()`: log what files are found and whether they have entries
   - In the LaunchedEffect: log which effect runs first and what it produces
2. Check if `migrateDefaultSetIfNeeded()` runs on every start and re-copies old entries
3. Fix: ensure migration is idempotent (doesn't re-copy if destination already has entries)
4. Fix: if race condition between the two LaunchedEffects, combine them into a single effect with proper ordering (prune first, then hash)

### Files to modify
- `android/app/src/main/java/com/dxxredux/app/AssetManifest.kt` - add logging
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` - fix LaunchedEffect ordering, check migration
- `android/app/src/main/java/com/dxxredux/app/SafManifest.kt` - add logging

---

## Item 11: Config export missing app settings
### Status: NOT STARTED
### Analysis
Current `exportAll()` exports: touch_layout, controller_config, autoselect_d1/d2. Missing: MSAA, AF, texture filtering, resolution, music mode, debug logging toggles, and all other SharedPreferences and descent.cfg settings.

User wants:
- A third button "Export All (including app settings)"
- Include app settings as `"app_settings"` sub-key in the exported JSON
- Import should restore those settings too

App settings are in two places:
1. **SharedPreferences ("dxx_prefs")**: render_resolution, music_mode, game_orientation, selected_game, mp_callsign, host_* settings, dlog_* toggles, etc.
2. **descent.cfg files**: TexFilt, MsaaLevel, AnisoLevel, ColorDepth, MenuTexFilt, HudTexFilt, ResolutionX, ResolutionY

### Plan
1. Add `exportAllWithAppSettings()` function to `ConfigImportExport.kt`:
   - Call existing `exportAll()` logic
   - Read all SharedPreferences from "dxx_prefs" and serialize as JSON object
   - Read graphics settings from descent.cfg via `readConfigValue()`
   - Add both as `"app_settings"` sub-key with `"shared_prefs"` and `"descent_cfg"` children
2. Add `importAppSettings()` to handle the `"app_settings"` section during import:
   - Write SharedPreferences values back
   - Write descent.cfg values via `updateAllConfigFiles()`
   - Skip `installation_id` and other identity keys during import
3. Update `importCombined()` to check for and import `"app_settings"`
4. Add third button in `AdvancedSettingsPage.kt` next to existing export/import buttons

### Files to modify
- `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt` - add export/import for app settings
- `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt` - add third button UI

---

## Implementation Order

Recommended order to minimize conflicts and allow incremental testing:

### Phase 1: Quick fixes (items 3, 4, 7, 8)
- Item 3: MSAA automap fix (d1/d2 ogl.c)
- Item 4: Map button size (JSON presets)
- Item 7: Max deadzone 60% (TouchEditorPage.kt)
- Item 8: Gyro axis reorder (TouchBindings.kt, TouchEditorPage.kt)

### Phase 2: Guidebot fixes (items 1, 5)
- Item 5: Guidebot wheel locked state (JNI + Kotlin)
- Item 1: Guidebot line in single player (coop_indicator_lines.c)

### Phase 3: Gyro deadzone (item 6)
- Item 6: Per-axis deadzone (TouchControl.kt, GyroInputManager.kt, TouchEditorPage.kt)

### Phase 4: Music position (item 2)
- Item 2: Music control positioning fix (TouchOverlayView.kt, TouchEditorPage.kt)

### Phase 5: Default config (item 9)
- Item 9: Update default config (advanced.json) -- do this AFTER items 4, 6, 8 since those change the config format

### Phase 6: Stale references (item 10)
- Item 10: Diagnostic logging + fix for stale references popup

### Phase 7: Config export (item 11)
- Item 11: App settings export/import (ConfigImportExport.kt, AdvancedSettingsPage.kt)

### Phase 8: Build + test
- Full build and lint pass
- Run existing integration tests
- Add/extend tests for new features where feasible

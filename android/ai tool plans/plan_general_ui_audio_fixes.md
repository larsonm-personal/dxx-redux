# Plan: General UI/Audio Fixes (5 Items)

## 1. Delay Keyboard Popup Until Drag Completes

**Problem**: During vertical drag-scrolling in menus, as the selection passes over
`NM_TYPE_INPUT` items, the keyboard opens mid-drag, shifting the blit offset,
altering drag distance, and causing oscillation.

**Root cause**: `newmenu_draw()` checks `menu->citem` type every frame and calls
`android_show_keyboard()` regardless of whether a drag is in progress. The
existing `drag_happened` flag is only used to suppress tap activation -- not
the keyboard show.

**Fix**: Gate the keyboard-show call on `!menu->mouse_state` (finger not
pressed). This means the keyboard only evaluates after touch-up. No new state
needed -- `mouse_state` is already tracked.

**Files**:
- `d2/main/newmenu.c` ~line 1562 -- `newmenu_draw()` keyboard auto-show block
- `d1/main/newmenu.c` -- same block, mirrored change

---

## 2. Admin Tray: Slide-Up Handle + Higher Transparency

**Problem**: The "Settings" tab at the bottom is too opaque and the panel pops
open instantly. User wants more transparency on the tab (matching the touch
overlay's default opacity) and a slide-up/down animation for the panel.

**Fix**:
- Reduce tab alpha values (bg 0x88->0x33, ring 0x66->0x44, text 0xBB->0x66)
- Bottom-anchor the panel instead of centering it
- Add slide-up animation on open, slide-down-to-close (replaces Close button)
- Track drag-to-dismiss gesture (downward drag past 30% threshold)

**Files**:
- `TouchOverlayView.kt` -- `drawAdminTrayTab()`, `drawAdminTrayPanel()`,
  `handleAdminTrayTouch()`

---

## 3. Settings Panel: Edge-to-Edge Grid

**Problem**: In landscape mode, grid buttons are too small with excessive gaps.

**Fix**: Remove inter-cell padding. Cells fill the panel edge-to-edge with
shared borders (1px divider lines). Keep rounded-rect style but zero gap.

**Files**: Same as item 2 -- intertwined with the panel redesign.

---

## 4. Controller Trigger Axis Config Fix

**Problem**: (a) Trigger threshold bar doesn't update live in the picker
dialog. (b) In D2, triggers don't move the ship because they can't be
bound to axis functions.

**Root causes**:
- (a) `ButtonFunctionPickerDialog` receives `lt`/`rt` as captured values at
  dialog open time; the dialog doesn't recompose on `axisGeneration` changes.
- (b) `AXIS_CONTROLS` map only has 4 axes (LS_X/Y, RS_X/Y). LT (axis 4) and
  RT (axis 5) are missing, so `buildJoyPairs()` never writes a trigger-to-
  Throttle binding into the game config.

**Fix**:
- Add `"LT" to 4, "RT" to 5` to `AXIS_CONTROLS`
- Update `buildJoyPairs()` for single-axis (trigger) bindings
- Enable trigger controls to offer axis functions (Pitch, Turn, Slide, Bank,
  Throttle) in addition to button functions
- Fix Compose recomposition by observing `axisGeneration` in the dialog

**Files**:
- `ControllerConfigPage.kt` -- `AXIS_CONTROLS`, `buildJoyPairs()`,
  `ButtonFunctionPickerDialog`, trigger control tap handlers

---

## 5. D1 Redbook Audio Static Fix

**Problem**: D1 Redbook audio is staticky. D2 is fine.

**Root cause**: D1's Android fallback rate is 44100 Hz; D2's is 48000 Hz.
The shared `rbaudio_bin.c` reads CD audio at 44100 Hz. When the mixer is
also at 44100 Hz (ratio 1.0), buffer alignment issues cause pops/static.
D2's 48000 Hz forces proper resampling with linear interpolation.

**Fix**: Add `SAMPLE_RATE_48K` to D1's `digi.h` and use it as the Android
fallback, matching D2 exactly.

**Files**:
- `d1/main/digi.h` -- add `#define SAMPLE_RATE_48K 48000`
- `d1/arch/sdl/digi_mixer.c` -- change Android fallback to `SAMPLE_RATE_48K`

---

## Phase Ordering

| Phase | Items | Notes |
|-------|-------|-------|
| A | 1, 5 | Small C changes in d1/d2 dirs |
| B | 4 | Kotlin controller config fixes |
| C | 2 + 3 | Admin tray UI redesign (intertwined) |
| D | all | Integration testing + code quality linters |

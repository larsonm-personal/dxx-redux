# Plan: Android Keyboard Viewport Offset

## Problem
When the Android soft keyboard appears for text input (e.g. new pilot name),
it covers about 50% of the screen in landscape mode, often hiding the actual
text input field. Users cannot see what they are typing.

## Approach
When the keyboard is visible, shift the entire rendered game canvas upward
in the blit function so the active text input field is centered in the
non-occluded visible area. The top of the canvas slides off-screen above
the top edge. No animation for v1. No scale-blit center adjustment -- blit
offset alone handles centering.

---

## Phase 1: Prevent Android from resizing the SurfaceView

**File**: `android/app/src/main/AndroidManifest.xml`

- Add `android:windowSoftInputMode="adjustNothing"` to `.MainActivity`
- Prevents Android from resizing or panning the SurfaceView when the
  keyboard opens. We handle the offset ourselves.

## Phase 2: Detect keyboard height (Kotlin)

**File**: `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`

- Register `ViewCompat.setOnApplyWindowInsetsListener` on the root
  `decorView` to receive IME insets. When the keyboard's bottom inset
  changes, call a new JNI function `nativeSetKeyboardHeight(imeHeightPx,
  screenHeightPx)`.
- Fallback: if `adjustNothing` suppresses IME insets from the compat
  listener, use `ViewTreeObserver.OnGlobalLayoutListener` with
  `getWindowVisibleDisplayFrame()` to compute keyboard height.
- Add JNI declaration:
  `private external fun nativeSetKeyboardHeight(keyboardHeightPx: Int, screenHeightPx: Int)`

## Phase 3: Pass input field Y from d1/d2 (tiny change)

**Files**: `d2/main/newmenu.c`, `d1/main/newmenu.c`

- Change `android_show_keyboard(numeric)` to
  `android_show_keyboard(numeric, menu->y + menu->items[i].y)`.
- This passes the absolute canvas Y of the active text input field.
- Signature change is inside `#ifdef ANDROID` so desktop builds are
  unaffected.
- About 2 lines changed per file.

## Phase 4: JNI bridge and offset computation (android/ dir)

**File**: `android/app/src/main/cpp/android_input.c`

- Add globals: `g_keyboard_height_native`, `g_screen_height_native`,
  `g_active_input_field_y`.
- Add JNI function `nativeSetKeyboardHeight()` that stores the heights.
- Update `android_show_keyboard(int numeric, int field_y)` to store
  `g_active_input_field_y = field_y`.
- Add `android_get_keyboard_y_offset(int canvas_h)`:
  - Convert keyboard height from native px to game px:
    `kb_game = keyboard_native * canvas_h / screen_native`
  - Visible height = `canvas_h - kb_game`
  - If scale-blit active, remap `field_y` through scale transform
  - `offset = field_y - visible_h / 2`, clamped to `[0, kb_game]`

## Phase 5: Apply offset in blit

**File**: `android/app/src/main/cpp/android_surface.c`

- In `android_surface_blit()`, call `android_get_keyboard_y_offset(src_h)`
  to get the current offset.
- Modify the row blit loop: read from `src_row[y + offset]` instead of
  `src_row[y]`. Fill remaining destination rows with black.
- Export `g_blit_y_offset` global for touch remapping.

## Phase 6: Adjust touch coordinates

**File**: `android/app/src/main/cpp/android_input.c`

- In `nativeTouchEvent()`, after computing `gameY`, add `g_blit_y_offset`
  so touches map back to correct canvas coordinates.

## Phase 7: Clear offset on keyboard hide

- In `android_hide_keyboard()` (android_input.c), clear
  `g_keyboard_height_native = 0`.
- The Kotlin insets listener also reports 0 when the keyboard hides.

## Phase 8: Integration test script

**File**: `android/game_scripts/test_keyboard_viewport.json5`

- Test script that navigates to the "new pilot" name entry screen where a
  text input field is active and the soft keyboard is open.
- Steps:
  1. Wait for menu, accept default pilot prompt
  2. Navigate to "New game" and verify menu is up
  3. At this point keyboard is open on the pilot name entry screen
  4. Wait briefly with keyboard open (blit offset should be active)
  5. Type a few characters to confirm text input works
  6. Accept the pilot name
  7. Assert screen_mode is still "menu" (no crash)
- Parameterized for both D1 and D2.
- Primary purpose: verify no crash and that the blit offset code runs
  without breaking text input. Visual centering is verified manually.

---

## Relevant Files (all changes)

- `android/app/src/main/AndroidManifest.xml`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `android/app/src/main/cpp/android_input.c`
- `android/app/src/main/cpp/android_surface.c`
- `d2/main/newmenu.c` (~2 lines, `#ifdef ANDROID`)
- `d1/main/newmenu.c` (~2 lines, `#ifdef ANDROID`)
- `android/game_scripts/test_keyboard_viewport.json5`

## Decisions

- Blit offset only for v1 (no scale-blit center adjustment)
- Instant jump when keyboard appears (no smooth animation)
- `adjustNothing` mode prevents Android from interfering
- Offset recomputed every frame from globals

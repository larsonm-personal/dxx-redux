# Plan: D1/D2 Pilot Separation, D1 Touch Features, Black Screen Fix

## Issues

1. D1 and D2 pilot configs cross-contaminate -- launcher patches both with D2 binary format
2. D1 lacks Android keyboard popup for text input menus (player name, etc.)
3. D1 lacks OK buttons on input menus (player name, network message)
4. Both games black-screen after backgrounding for ~60s (EGL surface goes stale)

---

## Phase 1: Separate D1/D2 Pilot Patching

**Root cause**: `NativePilotPatcher.kt` hardcodes `System.loadLibrary("dxx-redux-d2")`
and `patch_all_plr_files()` in `android_gamepad_config.cpp` scans both `d1x-redux/`
and `d2x-redux/` directories, applying D2 binary format to D1 `.plr` files.
Both use `DPLR` magic but completely different struct layouts.

**Fix**: Add a `game` string parameter ("d1"/"d2") to JNI patch methods.
Filter directory scanning so each game only touches its own dirs.

| Step | File | Change |
|------|------|--------|
| 1a | `NativePilotPatcher.kt` | Add `game: String` param to `nativePatchPilotFiles` and `nativeResetToDefaults` |
| 1b | `android_gamepad_config.cpp` | Accept `jstring game`, filter dirs: d1 only scans `d1x-redux/`, d2 only scans `d2x-redux/` |
| 1c | SetupActivity.kt callers | Pass correct game identifier to patch calls |

---

## Phase 2: D1 Keyboard Popup for Text Input Menus

**Root cause**: D2's `newmenu.c` has `#ifdef ANDROID` blocks calling
`android_show_keyboard()` on `EVENT_WINDOW_ACTIVATED` for `NM_TYPE_INPUT` menus
and `android_hide_keyboard()` on `EVENT_WINDOW_CLOSE`. D1 lacks these.

| Step | File | Change |
|------|------|--------|
| 2a | `d1/main/newmenu.c` EVENT_WINDOW_ACTIVATED | Add keyboard-show block (same as D2's pattern) |
| 2b | `d1/main/newmenu.c` EVENT_WINDOW_CLOSE | Add keyboard-hide block |

Shared code (`android_show_keyboard`/`android_hide_keyboard` in `android_input.c`
and Kotlin side in `MainActivity.kt`) already exists.

---

## Phase 3: D1 OK Buttons on Input Menus

**Root cause**: D2 adds a tappable "OK" NM_TYPE_MENU item below text inputs.
D1 is missing these in two places.

| Step | File | Change |
|------|------|--------|
| 3a | `d1/main/menu.c` MakeNewPlayerFile() | 1 item -> 2 items, add OK button |
| 3b | `d1/main/multi.c` multi_send_message_dialog() | 1 item -> 2 items, add OK button |

---

## Phase 4: Black Screen After Backgrounding

**Root cause**: `ogl_init_window()` creates an EGL surface from the ANativeWindow
at startup. On background, `surfaceDestroyed()` releases the ANativeWindow.
On resume, `surfaceCreated()` acquires a NEW ANativeWindow, but the EGL surface
still references the dead window. `eglSwapBuffers()` silently fails -- black screen.
Music keeps playing because the game loop continues.

Likely regressed with the home-button-crash fix that added pause/resume guards.

| Step | File | Change |
|------|------|--------|
| 4a | `android_surface.c` | Add `g_egl_surface_stale` flag, set on re-acquire. Add `android_surface_egl_needs_recreate()` and `android_surface_is_paused()` accessors |
| 4b | `d1/arch/ogl/gr.c` | Add `ogl_android_recreate_surface()`: destroy old EGL surface, create new one from current ANativeWindow, preserve context. Full re-init fallback if context also lost |
| 4c | `d2/arch/ogl/gr.c` | Same as 4b |
| 4d | `d1/arch/ogl/gr.c` ogl_swap_buffers_internal | Check paused + stale before eglSwapBuffers |
| 4e | `d2/arch/ogl/gr.c` ogl_swap_buffers_internal | Same as 4d |

---

## Verification

1. Pilot separation: create pilots in both games, apply gamepad config, verify
   files only in correct game dir
2. Keyboard (D1): New Pilot dialog -- keyboard auto-appears
3. OK buttons (D1): New Pilot dialog -- OK button visible
4. Black screen: background either game 60+ seconds, return -- rendering resumes
5. Regression: D2 keyboard/OK/patching still work

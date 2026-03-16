# OpenGL Render Resolution Adjustments

## Problem

Resolution changes in the launcher have no effect on OpenGL rendering.
The game always renders at 640x480 regardless of the selected resolution.

### Root cause

In `d2/arch/ogl/gr.c` (and `d1/arch/ogl/gr.c`), `gr_set_mode()` calls
`gr_check_mode()` which calls `SDL_VideoModeOK()`.  On Android, SDL uses the
dummy video driver, which returns 0 ("mode not available") for every
resolution.  `gr_set_mode` treats this as failure and falls back to 640x480:

```c
if (!gr_check_mode(mode))
{
    con_printf(CON_URGENT,"Cannot set %ix%i. Fallback to 640x480\n",w,h);
    w=640; h=480;
    Game_screen_mode=mode=SM(w,h);
}
```

This same bug exists in both `d1/arch/ogl/gr.c` and `d2/arch/ogl/gr.c`.

The SDL software renderer path (`arch/sdl/gr.c`) already has an Android
workaround that skips the `SDL_VideoModeOK` check, which is why the user
saw resolution changes work under software rendering.

### Fix

Add `#ifdef ANDROID` to `gr_check_mode()` in both `d1/arch/ogl/gr.c` and
`d2/arch/ogl/gr.c` to skip the dummy-driver mode check, accepting any
resolution.  This matches what `arch/sdl/gr.c` already does:

```c
int gr_check_mode(u_int32_t mode)
{
#ifdef ANDROID
    (void)mode;
    return 32; /* dummy driver accepts any resolution */
#else
    /* ... existing code ... */
#endif
}
```

## Dynamic resolution picker

Replace the hardcoded 640/960/1280 options in `AdvancedSettingsPage.kt` with
fractions of the device's actual screen resolution:

- Full: screenW x screenH
- 1/2: screenW/2 x screenH/2  (if >= 640x480)
- 1/3: screenW/3 x screenH/3  (if >= 640x480)
- 1/4: screenW/4 x screenH/4  (if >= 640x480)

Dimensions rounded to nearest even number.  Native aspect ratio is preserved
at every level.  The existing `updateDescentCfgResolution()` function and
the config.c parsing pipeline need no changes.

Also update `writeInitialGameConfig()` to write a default resolution
(1/2 screen) on first launch instead of relying on the 640x480 default.

## Introspection additions

### Game introspection (game_introspect.cpp)

Add a `"resolution"` section:
- `render_width`, `render_height` -- from `grd_curscreen->sc_w/sc_h`
- `display_width`, `display_height` -- from `ANativeWindow_getWidth/Height`

### Native accessors (android_surface.c)

Add accessor functions:
- `android_surface_get_display_width()`
- `android_surface_get_display_height()`

## Files to modify

| File | Change |
|------|--------|
| `d2/arch/ogl/gr.c` | Fix `gr_check_mode` for Android |
| `d1/arch/ogl/gr.c` | Same fix |
| `android/app/src/main/cpp/android_surface.c` | Add display dimension accessors |
| `android/app/src/main/cpp/shared/game_introspect.cpp` | Add resolution section |
| `android/app/src/main/java/.../AdvancedSettingsPage.kt` | Dynamic resolution picker |
| `android/app/src/main/java/.../SetupActivity.kt` | Default resolution on first launch |

## Build and test

1. Build APK, deploy to emulator
2. Launch game and introspect -- verify render resolution matches config
3. Change resolution in Advanced settings, relaunch, introspect again
4. Verify all fractional options appear correctly in the picker
5. Visual check for aspect ratio correctness at each setting

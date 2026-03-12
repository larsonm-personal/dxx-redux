# Fix: Home button crash while in-game

## Problem
The game crashes when the home button is pressed while in-game. Affects both D1 and D2.

## Root causes identified

### 1. Thread-unsafe window_get_front() call (primary cause)
`nativeOnPause()` is called from the Android UI thread but calls `window_get_front()`,
which traverses the game engine's window linked list. The game thread concurrently
mutates this list (creating/closing windows, freeing memory). If the game thread is
mid-modification when the UI thread traverses, it can dereference freed/invalid memory.

### 2. No rendering pause during surface teardown
When the home button is pressed, there's a timing gap between `onStop()` and
`surfaceDestroyed()`. During this gap, the game thread continues rendering via
`android_surface_blit()`. On some devices/Android versions, the underlying surface
backing may be invalidated before `surfaceDestroyed` fires, causing
`ANativeWindow_lock()` or `ANativeWindow_setBuffersGeometry()` to crash.

## Fix applied

### android_surface.c
- Added `volatile int g_app_paused` flag
- Added `android_surface_pause()` / `android_surface_resume()` (mutex-protected)
- Added `g_app_paused` check in `android_surface_blit()` guard clause

### android_input.c (nativeOnPause)
- Call `android_surface_pause()` first, before anything else -- this waits for any
  in-progress blit to finish (via mutex), then prevents new blits
- Replaced unsafe `Game_wind != window_get_front()` with a thread-safe check:
  `!Game_wind || Screen_mode != SCREEN_GAME` (both are simple atomic reads of
  global ints/pointers -- no linked list traversal)

### android_input.c (nativeOnResume)
- Call `android_surface_resume()` to re-enable rendering

## Files changed
- android/app/src/main/cpp/android_surface.c
- android/app/src/main/cpp/android_input.c

## Lifecycle sequence after fix

### Home button pressed:
1. `onStop()` -> `nativeOnPause()`
2. `android_surface_pause()` -- acquires mutex (waits for any blit), sets paused flag
3. Music paused, Escape injected (if in-game)
4. `surfaceDestroyed()` -> `nativeSetSurface(null)` -- releases ANativeWindow safely
5. Game thread blits return early (paused flag set)

### Returning from background:
1. `onResume()` -> `nativeOnResume()`
2. `android_surface_resume()` -- clears paused flag
3. `surfaceCreated()` -> `nativeSetSurface(surface)` -- acquires new ANativeWindow
4. Game thread resumes blitting

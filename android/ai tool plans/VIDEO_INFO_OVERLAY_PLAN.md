# Video Info Overlay Plan

## Goal
Add a togglable in-game overlay showing video/rendering diagnostics:
- FPS (frames per second)
- Hi-res texture count / total loaded (with percentage)
- Max hi-res texture resolution
- GL max texture size (engine cap)

## Architecture

### C Engine (d1/main/game.c, d2/main/game.c)
- Add `int g_current_fps` global, updated unconditionally in `calc_frame_time()` using a 1-second window counter (same technique as `show_framerate()` in gamerend.c but always-on)
- Guarded by `#ifdef __ANDROID__` to avoid affecting desktop builds

### JNI (android/app/src/main/cpp/jni_main.c)
- Add `nativeGetVideoStats()` returning IntArray:
  [0] fps, [1] total_loaded, [2] hires_count, [3] max_hires_w, [4] max_hires_h, [5] ogl_max_texture_size
- Uses extern declarations to access engine globals directly (same pattern as nativeGetMultiplayerPings)

### Kotlin Overlay (VideoInfoOverlay.kt)
- Custom View drawing with Canvas (same pattern as MultiplayerStatsOverlay)
- Polls nativeGetVideoStats() every 500ms
- Semi-transparent background, monospace text, positioned top-right
- Shows/hides via toggle()

### Admin Tray (TouchOverlayView.kt)
- Add ADMIN_VIDEO_INFO = 9
- Bump itemCount to 10 (4 rows x 3 cols, last row has 1 button)
- Label: "Video Info"

### MainActivity.kt
- Declare external fun nativeGetVideoStats(): IntArray
- Create VideoInfoOverlay, add to frame
- Handle ADMIN_VIDEO_INFO toggle in adminTrayCallback

## Status
- [x] FPS global in d1/d2 -- g_current_fps in calc_frame_time(), #ifdef __ANDROID__
- [x] JNI function -- nativeGetVideoStats() in jni_main.c, returns 6-int array
- [x] VideoInfoOverlay.kt -- polls every 500ms, shows FPS/hires/maxres/glcap
- [x] Admin tray button -- ADMIN_VIDEO_INFO = 9, itemCount 9->10
- [x] MainActivity integration -- overlay created, wired to admin tray, hidden on menu return
- [x] Build and test -- BUILD SUCCESSFUL, test_launch_to_automap PASS

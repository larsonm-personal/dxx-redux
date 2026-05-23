# Debug Overlays: Texture Labels + Video Stats

## Status: COMPLETE

## What was built

### 1. Debug Texture Label Overlay
- Green text labels on hires PNG textures, yellow on base game textures
- Labels positioned at 3D-projected centroid of each textured polygon
- Only drawn for visible/rendered textures (accumulated during g3_draw_tmap)
- Max 256 labels per frame to avoid overflow

### 2. Video Stats Overlay
- Top-left HUD showing:
  - Texture memory: total MB, texture count, GL max texture size
  - Render resolution vs display resolution
  - Hires textures: loaded count vs found count

### 3. Toggle Mechanism
- **Automation**: `{"action": "set_debug", "field": "tex_overlay", "value": "1"}`
- **Automation**: `{"action": "set_debug", "field": "video_stats", "value": "1"}`
- **ADB broadcast**: `adb shell am broadcast -a com.dxxredux.GAME_COMMAND --es command debug --es field tex_overlay --ei value 1`
- **ADB broadcast**: `adb shell am broadcast -a com.dxxredux.GAME_COMMAND --es command debug --es field video_stats --ei value 1`
- **Introspection**: flags visible in `debug_flags.tex_overlay` and `debug_flags.video_stats`

## Files changed

### New files
- `android/app/src/main/cpp/shared/debug_tex_overlay.h` -- struct + extern declarations

### D2 changes
- `d2/arch/ogl/ogl.c` -- globals, label accumulation in g3_draw_tmap, ogl_get_texture_bytes()
- `d2/main/gamerend.c` -- label rendering + video stats rendering in game_render_frame_mono

### D1 changes (mirrored)
- `d1/arch/ogl/ogl.c` -- same as d2
- `d1/main/gamerend.c` -- same as d2

### Shared/JNI
- `android/app/src/main/cpp/shared/game_automate.cpp` -- STEP_SET_DEBUG action type
- `android/app/src/main/cpp/shared/game_introspect.cpp` -- debug_flags in JSON output
- `android/app/src/main/cpp/jni_main.c` -- nativeSetDebugFlag JNI function
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` -- external fun + GAME_COMMAND handler

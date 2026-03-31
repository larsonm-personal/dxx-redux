# Plan: Cockpit fix, overlay consolidation, build step

## Issues
1. Cockpit HUD looks worse -- BM_FLAG_RLE_BIG exclusion unreliable
2. Two competing video stats overlays -- remove C engine overlay, move label toggle to Kotlin VideoInfoOverlay
3. Run-TestMenu.ps1 doesn't trigger a fresh build

## Changes

### 1. Fix cockpit ETC2 exclusion
- In `ogl_loadbmtexture_f` (d1+d2 ogl.c): before calling ogl_loadtexture in PNG path, check `bm->bm_w > 128 || bm->bm_h > 128` (original bitmap dimensions; level textures <= 128, cockpits >= 320). Set bit 0x100 on data_format
- In `ogl_loadtexture` (d1+d2 ogl.c): extract 0x100 bit as `skip_etc2`, mask it off for ogl_filltexbuf. Add `!skip_etc2` to ETC2 guard
- Keep BM_FLAG_RLE_BIG check as belt-and-suspenders

### 2. Remove C video stats overlay (8 locations)
- d1/arch/ogl/ogl.c, d2/arch/ogl/ogl.c: remove `volatile int g_debug_video_stats_active = 0;`
- debug_tex_overlay.h: remove extern, update header comment
- d1/main/gamerend.c, d2/main/gamerend.c: remove entire `if (g_debug_video_stats_active)` block and `#include "mouse.h"`
- jni_main.c: remove "video_stats" branch from nativeSetDebugFlag
- game_automate.cpp: remove "video_stats" branch from STEP_SET_DEBUG
- game_introspect.cpp: remove video_stats from debug_flags JSON

### 3. Add label toggle to Kotlin VideoInfoOverlay
- Add `debugFlagSetter` lambda property
- Add "Labels: ON/OFF" row with touch handling
- Wire up in MainActivity.kt where videoInfoOverlay is created

### 4. Add build step to Run-TestMenu.ps1
- Add gradlew assembleDebug call before delegating to run_test.ps1
- Add -Install to run_test.ps1 invocation so fresh APK is installed
- Add -NoBuild switch for quick re-runs

## Status
- [x] Plan
- [x] Cockpit ETC2 fix
- [x] C video stats removal
- [x] Kotlin label toggle
- [x] Build step in test menu
- [x] Build + lint

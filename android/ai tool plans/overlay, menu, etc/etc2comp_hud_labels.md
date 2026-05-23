# Plan: etc2comp integration, HUD exclusion, label toggle

## Task 1: Replace hand-rolled ETC2 with Google's etc2comp
- Add etc2comp as FetchContent in android CMakeLists.txt (pin to master commit 39422c1)
- Build EtcLib source files as static library
- Rewrite etc2_compress.c -> etc2_compress.cpp keeping same C API
- etc2comp needs float RGBA [0..1] input, wrapper converts from uint8
- Uses Etc::Encode() C-style API with effort 40 (default), ErrorMetric::RGBA
- Caller (ogl.c) unchanged -- same etc2_compress_rgba() signature

## Task 2: Exclude HUD/cockpit from ETC2 compression
- BM_FLAG_RLE_BIG (32) marks bitmaps with wide rows, primarily cockpits
- Add `!(bm_flags & BM_FLAG_RLE_BIG)` to the ETC2 guard in ogl_loadtexture
- Both d1 and d2 ogl.c need the same change
- Minimal 1-condition change to existing if statement

## Task 3: Add texture label toggle button in video stats overlay
- Add a 4th line to video stats: "labels: ON/OFF"
- Detect mouse/touch click on that line to toggle g_debug_tex_overlay_active
- Both d1 and d2 gamerend.c need the same change
- Use SDL_GetMouseState + simple debounce

## Status
- [x] Research complete
- [x] etc2comp cmake integration
- [x] etc2_compress.cpp rewrite
- [x] HUD exclusion in ogl.c (d1 + d2)
- [x] Label toggle in gamerend.c (d1 + d2)
- [x] Build + test + lint

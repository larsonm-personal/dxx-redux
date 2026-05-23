# Plan: Fix HUD transparency, texture strips, overlay button

## Status: MOSTLY COMPLETE (pending emulator test)

## Issue 1: HUD transparency with non-square textures
- Root cause: etc2tool pads non-square textures to pow2 with transparent black.
  The ETC2 loading path uses u=v=1.0, so padding is visible.
  Cockpit textures are non-square (640x480, 1280x960, etc.)
- Fix: Store original (pre-padded) dimensions in .etc2 header reserved bytes.
  Engine uses them to compute u/v = orig/padded.
- Files: etc2tool.cpp, pngfile.h (d1+d2), pngfile_stb.c, ogl.c (d1+d2)
- [x] etc2tool: store orig w/h in reserved bytes 10-13
- [x] pngfile.h: add orig_width/orig_height to etc2_file_data
- [x] pngfile_stb.c: read orig dims, fallback if 0
- [x] ogl.c (d1+d2): compute u/v from orig/padded
- [x] rebuild etc2tool
- [x] regenerate non-square .etc2 files in all DXA packs

## Issue 2: Animated texture strips
- Root cause: d2x-xl packs store animation frames as vertical strips (name#0.tga).
  Engine expects separate files per frame (name#0.etc2, name#1.etc2, ...).
  Currently only name#0.etc2 exists, showing all frames on one polygon.
- Fix: Detect and split strips during conversion. No engine changes needed.
- Files: fix_nonsquare_textures.ps1 (one-time fix), convert_d2xxl_textures.ps1 + convert_all.ps1 (future builds)
- [x] D1 packs: all done (843 entries each, 0 non-square)
- [x] D2 packs: done (D2-512: 1755 entries, D2-256: 1756, D2-128: 1756)
- [x] update convert_d2xxl_textures.ps1: added Split-StripTextures pre-processing
- [x] update convert_all.ps1: added strip splitting in merge pass, safe # handling
- Note: remaining non-square entries are legitimately non-square (cockpit, status bars,
  targ02b, targ03b) with correct orig dimensions in headers

## Issue 3: Video overlay label toggle button
- Add a proper button with press feedback that consumes touch events
- Files: VideoInfoOverlay.kt
- [x] draw button with rounded rect background + pressed state
- [x] consume ACTION_DOWN to prevent passthrough to touch overlay
- [x] consume panel touches (prevent accidental game input through panel)
- [x] APK builds clean (no warnings)

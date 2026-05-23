# Overlay rendering: support all four texture resolution combinations

## Analysis

The `DbgAltTexMerge=1` code path sets bm and bm2 from GameBitmaps, then calls
`g3_draw_tmap_2()` which draws bottom first (via `g3_draw_tmap()`) then overlay
on top. This is the ORIGINAL desktop behavior for all overlays, including
super-transparent ones. The `ogl_bindbmtex()` call inside each draw function
loads textures on-demand: KTX2 hires if available, otherwise base game 64x64.

For super-transparent overlays, the base game 64x64 path uses `ogl_filltexbuf()`
which sets palette index 254 to RGBA=(255,255,255,0). For KTX2 hires, the RGBA
ETC2 texture has alpha=0 for key color pixels. Both work with the existing GL
state: GL_BLEND(SRC_ALPHA, ONE_MINUS_SRC_ALPHA) + GL_ALPHA_TEST(GEQUAL, 0.02).

The bypass block we added was unnecessary - it tried to keep bm2 for hires and
fall back to texmerge for non-hires, but the standard DbgAltTexMerge path ALREADY
keeps bm2 for all overlays. The bypass also loaded textures eagerly (before draw)
which is redundant since ogl_bindbmtex loads on-demand.

## The four cases (all handled by standard DbgAltTexMerge path)

1. Both 64x64: g3_draw_tmap_2 with palette-converted textures (alpha from ogl_filltexbuf)
2. Hires overlay on 64x64 base: ogl_bindbmtex loads KTX2 for overlay, base for bottom
3. 64x64 overlay on hires base: ogl_bindbmtex loads base for overlay, KTX2 for bottom
4. Both hires: ogl_bindbmtex loads KTX2 for both

## Changes

### render.c (d1 + d2)
- Remove the `#ifndef OGL_MERGE` super-transparent bypass block

### convert_d2xxl_textures.ps1
- Use fuzz matching (5%) and zero out RGB for transparent pixels: prevents
  ETC2 color bleeding at block boundaries
- Apply in strip splitting, post-processing, and Read-TGA fallback

## Status
- [x] Remove bypass block (d1 + d2)
- [x] Fix DXA conversion pipeline (3 locations: strip, post-process, Read-TGA)
- [x] Rebuild DXA (d2-hires-128-textures-ktx2.dxa, 31MB)
- [x] Build APK, deploy, push DXA to emulator
- [x] Test: automap test passed, 1418/1608 hires textures loaded (88%, 100% replacement)
- [ ] Visual verification of door35, misc060, rock313, rock346, ceil028 (manual)

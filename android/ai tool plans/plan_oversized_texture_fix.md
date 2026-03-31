# Oversized Texture Fallback and Cache Diagnostics Fix

## Problem
1. "Textures behind the ship at spawn are low-res" - reported by user with 256px mod pack
2. "Hidden door too dark" with 256px pack
3. Cache diagnostics invisible in logcat (con_printf goes to stdout, not logcat)
4. Bogus hires metrics: `replacement_pct=100, max_hires_h=4608` when many textures actually failed
5. After initial fix: garbled menus and very poor framerate due to per-frame PNG retry

## Root Cause Analysis

### Oversized textures (main issue)
- d2x-xl texture pack stores animation frame strips as tall vertical images
  (e.g. 256x2304 = 9 frames of 256x256 stacked vertically)
- GL_MAX_TEXTURE_SIZE = 2048 on Android emulator (SwiftShader)
- `ogl_loadtexture()` correctly returns 1 (failure) for oversized textures
- BUT: `ogl_loadbmtexture_f()` never checked the return value
- Result: `is_png=1` set, `r_hires_loaded++` incremented, `gltexture->handle=0`
- During rendering: `ogl_bindbmtex()` sees `handle<=0`, retries `ogl_loadbmtexture_f()`
  which re-finds the PNG, re-fails, infinite retry per frame
- ~48 oversized sprite strips caused this, inflating metrics to fake 100% replacement

### Hidden door darkness
- Investigated `bm_flags` clobber in `piggy_bitmap_page_out_all()` (sets `=` not `|=`)
- Finding: `piggy_bitmap_page_in()` restores flags from `GameBitmapFlags[i]`, so clobber
  is not permanent
- RGBA PNGs get alpha via `pdata.alpha` regardless of `bm_flags`
- Most likely cause: art difference in d2x-xl texture pack or ETC2 compression artifact
- Status: needs visual comparison of specific hidden door texture to confirm

## Fixes Applied

### Phase 1: Cache diagnostics to logcat [DONE]
- d1/arch/ogl/ogl.c: switched con_printf to __android_log_print in ogl_cache_level_textures
- d2/arch/ogl/ogl.c: same change
- Added `n_png_fail` counter for textures where gltexture was allocated but handle stayed 0
- "Skipping oversized" messages now also logged to logcat (ANDROID_LOG_WARN)

### Phase 2: Fix unchecked ogl_loadtexture return [DONE - revised]
- d1/arch/ogl/ogl.c + d2/arch/ogl/ogl.c:
  - Check ogl_loadtexture() return value in ogl_loadbmtexture_f()
  - On failure: reinit gltexture with bitmap dimensions (not NULL), set is_png=1
  - is_png=1 acts as "PNG was attempted, don't retry" flag
  - Falls through to original bitmap data path
- Added guard at PNG search: skip if `bm->gltexture && bm->gltexture->is_png`
  - Prevents per-frame PNG decompression retry (the main perf killer)
- **Reverted ogl_bindbmtex to original** (no early return guard)
  - Early return caused garbled menus: callers drew geometry with stale textures
  - gltexture is never NULL when ogl_bindbmtex is called (PIGGY_PAGE_IN runs first)

- Flow on oversized PNG:
  1. Cache pass: PNG found, decompressed, upload fails (oversized)
  2. gltexture reinit'd with bitmap dims, is_png=1, handle=0
  3. Falls through to paged-out check, returns (bitmap data not in memory)
  4. Rendering: PIGGY_PAGE_IN pages in data, ogl_bindbmtex calls ogl_loadbmtexture
  5. is_png=1 on gltexture -> PNG search SKIPPED (no I/O, no decompress)
  6. gltexture already allocated (handle=0), bitmap data uploaded -> handle>0
  7. Subsequent frames: handle>0 -> early return, no work

## Test Results
- Build: PASS
- test_mod_loading_256.json5: PASS
- Cache diagnostics visible in logcat:
  - `ogl_cache: starting, 2590 bitmaps, allow_png=1`
  - ~48 "Skipping oversized texture" warnings
  - `ogl_cache: done. hires=418 already=0 bitmap=1191 skipped=933 png_fail=22`
- Introspection metrics accurate:
  - `hires_found=467, hires_uploaded=419, replacement_pct=89, max_hires_h=2048`
- png_fail=22 are oversized sprite strips (gltexture allocated, is_png=1, handle=0)
  - On first render, bitmap data uploaded directly (no PNG retry)

## Future Considerations
- Sprite strip splitting: instead of falling back to 64x64, could split tall strips
  into individual frames at load time to keep hires quality
- OGL_MERGE: could enable now that GLES 3.0 is available, improving dual-texture walls
- bm_flags preservation: `piggy_bitmap_page_out_all` could use `|=` on Android to
  preserve transparency flags, but current behavior is mitigated by RGBA PNGs

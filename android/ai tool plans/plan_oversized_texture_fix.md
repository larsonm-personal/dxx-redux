# Oversized Texture Fallback and Cache Diagnostics Fix

## Problem
1. "Textures behind the ship at spawn are low-res" - reported by user with 256px mod pack
2. "Hidden door too dark" with 256px pack
3. Cache diagnostics invisible in logcat (con_printf goes to stdout, not logcat)
4. Bogus hires metrics: `replacement_pct=100, max_hires_h=4608` when many textures actually failed

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

### Phase 2: Fix unchecked ogl_loadtexture return [DONE]
- d1/arch/ogl/ogl.c: in ogl_loadbmtexture_f(), check ogl_loadtexture() return value
  - If non-zero: `ogl_reset_texture(bm->gltexture); bm->gltexture = NULL;`
  - Free PNG data, fall through to original bitmap data path
- d2/arch/ogl/ogl.c: same change
- Flow on oversized PNG:
  1. Cache pass: PNG found, upload fails, gltexture=NULL, hits paged-out check, returns
  2. Rendering: PIGGY_PAGE_IN pages in data, ogl_bindbmtex retries
  3. PNG fails again, falls through to bitmap data upload (not paged out now)
  4. Original low-res texture created, handle>0, subsequent frames skip

### Phase 3: ogl_bindbmtex NULL safety [DONE]
- d1/arch/ogl/ogl.c: added NULL check after ogl_loadbmtexture call
- d2/arch/ogl/ogl.c: same change
- Prevents crash when texture is unavailable (paged out + oversized PNG)

## Test Results
- Build: PASS
- test_mod_loading_256.json5: PASS
- Cache diagnostics visible in logcat:
  - `ogl_cache: starting, 2590 bitmaps, allow_png=1`
  - ~48 "Skipping oversized texture" warnings
  - `ogl_cache: done. hires=418 already=0 bitmap=1217 skipped=955 png_fail=0`
- Introspection metrics now accurate:
  - Before: `hires_found=1353, hires_uploaded=1353, replacement_pct=100, max_hires_h=4608`
  - After:  `hires_found=467,  hires_uploaded=419,  replacement_pct=89,  max_hires_h=2048`

## Future Considerations
- Sprite strip splitting: instead of falling back to 64x64, could split tall strips
  into individual frames at load time to keep hires quality
- OGL_MERGE: could enable now that GLES 3.0 is available, improving dual-texture walls
- bm_flags preservation: `piggy_bitmap_page_out_all` could use `|=` on Android to
  preserve transparency flags, but current behavior is mitigated by RGBA PNGs

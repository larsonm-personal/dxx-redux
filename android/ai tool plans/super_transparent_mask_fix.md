# Super-Transparent Mask Fix

## Status: Complete (pending visual verification)

## Problem
Two regressions from the initial OGL_MERGE mask loading implementation:

1. **Cockpit low-res**: Using `piggy_bitmap_get_flags()` (`real_flags`) for
   the `needs_alpha` check caused KTX2 textures with `BM_FLAG_TRANSPARENT` in
   `GameBitmapFlags[]` to be skipped when their KTX2 was RGB-only.  During the
   cache pass `bm->bm_flags` is `BM_FLAG_PAGED_OUT` (16), so the old code's
   `needs_alpha = bm->bm_flags & 3` was always 0 and never skipped anything.
   Switching to `real_flags` made the check live and broke textures (like the
   cockpit) whose HiRes versions work fine without alpha.

2. **Door35 see-through not working**: `ogl_load_dxa_mask` uploaded the mask
   PNG's raw multi-channel data directly into a GL_RGBA texture via
   `ogl_filltexbuf` with `data_format = channels`.  This is a format mismatch
   (3 bytes per pixel going into a 4-byte-per-pixel upload), causing garbled
   alpha.  The desktop `ogl_loadpngmask` correctly converts to a single-byte
   mask and uploads via the palette path with `BM_FLAG_TRANSPARENT`, which
   maps 255->alpha=0 (discard) and 0->alpha=1 (keep).

## Fix

### `piggy_bitmap_get_flags()` usage (d1 + d2 ogl.c)
- `real_flags` is ONLY used for `BM_FLAG_SUPER_TRANSPARENT` mask-loading checks
  (2 call sites per file: KTX2 and PNG paths)
- All other flag checks (`needs_alpha`, KTX2 alpha init, PNG alpha init,
  `ogl_loadtexture` flags) remain `bm->bm_flags` (original behavior)

### `ogl_load_dxa_mask()` rewrite (d1 + d2 ogl.c)
- Converts mask PNG to single-byte buffer: `mask[i] = data[i*ch] > 128 ? 255 : 0`
- Uploads via palette path: `ogl_loadtexture(mask, ..., BM_FLAG_TRANSPARENT, 0, ...)`
- In `ogl_filltexbuf`: 255 -> RGBA(0,0,0,0) alpha=0; 0 -> RGBA(pal,255) alpha=1
- This matches the desktop `ogl_loadpngmask` convention
- Shader `c.a * mask.a`: super-transparent areas get alpha=0 -> discard
- Also relaxed `mdata.color` check to just `mdata.depth == 8` to handle
  grayscale mask PNGs

## Files changed
- `d2/arch/ogl/ogl.c` - ogl_load_dxa_mask, ogl_loadbmtexture_f, cache counter
- `d1/arch/ogl/ogl.c` - same
- `d2/main/piggy.c` - added `piggy_bitmap_get_flags()`
- `d1/main/piggy.c` - same
- `d2/main/piggy.h` - declaration
- `d1/main/piggy.h` - declaration

## Verification
- Build: PASS
- Automated test: PASS (37 steps, 13876ms)
- Cache summary: `hires=898 masks=60` (898 hires from KTX2 DXA only)
- Mask count 60 confirms super-transparent textures get mask textures loaded
- Visual verification of cockpit and door35 still needed by user

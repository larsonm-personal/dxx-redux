# Plan: Texture loading diagnostics + 128px regeneration

## Status: IN PROGRESS

## Goals
1. Add diagnostic logging to the texture cache pass to understand why textures
   behind the spawn point appear low-res
2. Investigate the hidden door darkness with 256px textures
3. Re-run the 128px DXA generation (ETC2 encoder was fixed, may affect output)
4. Build and test

## Analysis

### Forward-only hires texture issue
The user reports "textures in the forward view are hires, but if I turn around,
textures behind the ship are low-res."

Code analysis of `ogl_cache_level_textures()` shows it iterates ALL
`GameBitmaps[0..Num_bitmap_files-1]` and calls `ogl_loadbmtexture_f()` for each.
On Android, the function tries PNG/JPG/TGA replacement via PhysFS even for
paged-out bitmaps. This should cover every texture in the level.

Possible causes identified:
1. **bm_flags clobber bug**: `piggy_bitmap_page_out_all()` sets
   `bm_flags = BM_FLAG_PAGED_OUT` which REPLACES all flags. This loses
   `BM_FLAG_TRANSPARENT` / `BM_FLAG_SUPER_TRANSPARENT`. The PNG loading
   path uses `bm->bm_flags & BM_FLAG_TRANSPARENT` to decide RGBA vs RGB.
   For paged-out bitmaps that should be transparent, this causes them to be
   loaded as RGB instead of RGBA. Won't cause low-res, but could cause
   visual artifacts on transparent textures.

2. **Bitmap cache overflow**: During `paging_touch_all()`, if the bitmap
   cache fills up, `piggy_bitmap_page_out_all()` is called to flush
   everything. This can happen repeatedly. After `paging_touch_all()` completes,
   only the last batch of bitmaps may be paged in. However, the PNG loading
   path doesn't depend on bitmap data being in memory (it reads from PhysFS),
   so this shouldn't matter.

3. **Unknown runtime issue**: Need diagnostic logging to confirm whether:
   - The cache pass is finding PNGs for all expected bitmaps
   - Some GL upload is silently failing
   - Textures are getting invalidated after the cache pass

### Plan
- [x] Add `con_printf` in `ogl_cache_level_textures` to report PNG load counts
- [x] Log a few bitmap names where PNG lookup fails to see if it's a naming issue
- [x] Log total time for cache pass (detect if it's suspiciously fast/slow)
- [x] Add telemetry to introspection output for cache pass results
- [ ] Build and test
- [ ] Analyze results and implement fix

### Hidden door darkness
- could be original d2x-xl texture issue (incorrect darkness levels)
- could be transparency flag lost due to bm_flags clobber
- need to identify which bitmap name corresponds to hidden doors

### 128px regeneration
- [ ] re-run convert_all.ps1 for 128px only (both d1 and d2)
- [ ] update game_data_index.txt with new hashes
- [ ] update test_mod_loading_128.json5 with new hash

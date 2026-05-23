# Strip Splitting Fix + tmap2 Debug Labels

## Status: COMPLETE

## Issues

### 1. Strip textures not split in 256/512 packs
- **Root cause**: `convert_all.ps1` line ~131 only passes ImageMagick to
  downscaled (128px) builds: `if ($cfg.Downscaled -and $Magick)`.
  Without ImageMagick, `Split-StripTextures` in `convert_d2xxl_textures.ps1`
  returns 0 immediately
- **Effect**: Strip TGAs like `door53#0.tga` (256x1280, 5 frames) get
  converted as a single tall KTX2. The game loads it for frame #0 and
  displays all 5 frames compressed vertically into one texture slot
- **Fix**: Always pass ImageMagick to the conversion script. Strip splitting
  needs IM even when downscaling is not required

### 2. tmap2 overlay textures missing debug labels
- **Root cause**: Debug texture label accumulation code only exists in
  `g3_draw_tmap()`. `g3_draw_tmap_2()` (which renders wall overlays /
  tmap2 textures like shootable lights) does not accumulate labels
- **Fix**: Add label accumulation for `bmovl` in `g3_draw_tmap_2()`
  after the overlay is drawn (non-OGL_MERGE path)

## Changes

### convert_all.ps1
- [x] Pass `$Magick` to all tex configs, not just downscaled ones
  (strip splitting needs IM even without downscaling)

### d2/arch/ogl/ogl.c + d1/arch/ogl/ogl.c
- [x] Add label accumulation in `g3_draw_tmap_2()` for the overlay bitmap

## Verification
- [x] Rebuild 256 DXA and verify door53#0-#4.ktx2 are split
- [ ] Enable tex_overlay in-game, verify tmap2 labels appear (manual)

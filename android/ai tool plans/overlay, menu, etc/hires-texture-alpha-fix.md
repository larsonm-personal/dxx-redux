# Hires Texture Alpha Fix - Pipeline Changes

## Problem
The d2x-xl TGA texture pack uses two transparency mechanisms:
1. Key color RGB(120,88,128) - same as D2's super-transparent palette index 254
2. Native alpha channel (alphaBits=8 in TGA header) - real per-pixel transparency

The conversion pipeline (`convert_d2xxl_textures.ps1`) was destroying native
alpha data because:
- ImageMagick's `-alpha set` normalizes "Undefined" alpha to 255 (opaque),
  overwriting real alpha data
- ImageMagick cannot do RGB-only color matching when alpha is active  
  (`-opaque`, `-transparent` all include alpha in fuzz distance)
- `Read-TGA` always forced alpha=255 for non-key-color pixels regardless
  of whether the TGA had real alpha data

### Evidence (raw TGA byte analysis)
- misc060#0.tga: 61% alpha=0 (transparent), 37% alpha=255 (opaque), 2% edges.
  NO key color. Alpha IS the transparency data
- door35#0.tga: 34% alpha=0, 64% alpha=255, key color pixels have alpha=90.
  Uses BOTH key color AND native alpha
- rock313.tga: 24bpp RGB, no alpha (base texture, not overlay)

## Solution
Use Read-TGA (raw byte reader) as the universal TGA-to-PNG pre-processor.
It handles both alpha preservation and key color correctly because it reads
bytes directly without ImageMagick's alpha normalization.

### Changes

1. **Read-TGA alpha fix**: Check `alphaBits` from TGA descriptor.
   When `alphaBits > 0`, preserve native alpha. Only force alpha=255 when
   `alphaBits == 0` (truly undefined alpha). Zero RGB for alpha=0 pixels
   (prevents ETC2 color bleeding at block boundaries).

2. **Split-StripTextures rewrite**: Replace IM-based splitting with
   Read-TGA + System.Drawing Bitmap.Clone. Produces individual PNG frames
   with correct alpha + key color handling. Removes IM dependency for splitting.

3. **Main conversion loop**: Add Read-TGA pre-processing step before IM
   Lanczos resize. TGA -> correct-alpha PNG -> IM resize -> etc2tool.
   Remove post-processing key color fix (now handled by Read-TGA).

### Performance
Read-TGA: ~97ms per 512x512 texture. 1600 textures = ~2.5 min additional.
Acceptable given total conversion time of ~15 min.

## Status
- [x] Root cause identified
- [x] Fix approach validated (alpha activate preserves alpha through pipeline)
- [x] Read-TGA fix implemented
- [x] Split-StripTextures rewrite (uses Read-TGA + Bitmap.Clone, outputs PNG)
- [x] Main loop changes (Read-TGA pre-processing for TGAs, PNG from splits)
- [x] DXA rebuild (D2: 1612 textures, D1: 764 textures, 0 errors)
- [x] Test run (PASS, 1419 hires loaded, misc060/door35/ceil028 all RGBA format)

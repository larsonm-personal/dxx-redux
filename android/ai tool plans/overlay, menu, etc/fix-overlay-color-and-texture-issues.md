# Fix overlay color logic and texture issues

## Issue 1: Green/yellow overlay labels incorrect for texmerge'd faces
**Root cause**: The render.c fallback block (fires when `tmap2 != 0 && bm2 == NULL`,
i.e. texmerge was used) looks up the **original** bitmaps and checks their `is_png`
flag. When hires DXA replacements are loaded, the originals have `is_png=1` even
though the actual render used a 64x64 texmerge bitmap. This makes the overlay show
green (hires) when it should show yellow (low-res).

**Fix**: This fallback only fires when texmerge was used, which always produces
64x64 output. Set `is_hires = 0` unconditionally in both d1 and d2 render.c
fallback blocks.

**Files**: d2/main/render.c, d1/main/render.c

## Issue 2: door35#14 has light purple background (should be transparent)

**Root cause**: The d2x-xl source TGA is 32bpp with Alpha=Undefined.

Pipeline trace:
1. `door35#0.tga` is a 512x7680 vertical strip (15 frames of 512x512), 32bpp, BPP=32, ImageType=2 (uncompressed BGRA), Alpha=Undefined
2. `Split-StripTextures` crops frames using: `magick $strip -alpha set -crop 512x512+0+$y +repage $frame.tga`
3. `-alpha set` activates the undefined alpha channel and sets ALL pixels to alpha=255 (fully opaque). This is ImageMagick's behavior for Alpha=Undefined TGAs
4. The resulting frame TGA has 4 channels (RGBA) with alpha=255 everywhere
5. etc2tool reads 4 channels -> `has_alpha=true` -> produces VkFormat=151 (ETC2 RGBA8 EAC)
6. Engine loads RGBA ETC2, sees `edata.format=1` (RGBA), uploads as `GL_COMPRESSED_RGBA8_ETC2_EAC`
7. Engine returns immediately after upload -- **does NOT call `ogl_loadpngmask()`** (that only runs for the PNG path)
8. `gltexture_mask` remains NULL (no super-transparent mask)

Result: The d2x-xl super-transparent key color RGB(120,88,128) -- which makes up 87% of frame 14's pixels -- renders as fully opaque light purple because `ovl.a=1.0` in the overlay shader.

Key shader (`ogl_prog_tex2` in [oglprog.c](d2/arch/ogl/oglprog.c#L67)):
```glsl
vec4 c = vec4(mix(bot.rgb, ovl.rgb, ovl.a), ...);
```
With `ovl.a=1.0`, the overlay completely covers the base texture with its purple background.

## Issue 3: misc060 not centered, rock313 all black

**Root cause**: Same alpha pipeline bug as Issue 2.

misc060 trace:
1. `misc060#0.tga` is a 512x3072 vertical strip (6 frames), 32bpp, Alpha=Undefined
2. Same `-alpha set` strip split -> alpha=255 everywhere
3. etc2tool -> VkFormat=151 (RGBA ETC2) with alpha=255
4. Engine loads RGBA ETC2, overlay shader uses `ovl.a=1.0`
5. 25% of pixels are the super-transparent color (120,88,128), rest is actual indicator content (avg RGB ~(28,32,25), very dark)
6. With alpha=1.0, misc060 completely covers the base texture (rock313)

**rock313 itself is fine**: Source is 24bpp (3 channels), produces VkFormat=147 (RGB ETC2), KTX2 data is 92% non-zero, source TGA mean color is (R=0.33,G=0.06,B=0.04) -- a brownish rock texture. The "all black" appearance is because misc060's opaque overlay (alpha=1.0) hides rock313 entirely, and misc060's non-transparent background is near-black.

**"Not centered"**: misc060's actual indicator content occupies a specific portion of the 512x512 frame. When the super-transparent background is NOT transparent (alpha=1.0), the visible frame boundary changes from "just the indicator" to "the entire 512x512 texture aligned by UV coords." The indicator position within the d2x-xl frame may not match the original game's centering.

## Shared root cause

All three issues trace to the same pipeline bug: 32bpp d2x-xl TGAs with Alpha=Undefined are processed as if they have meaningful alpha, but the alpha is set to 255 (opaque) by `-alpha set`. The d2x-xl super-transparent key color RGB(120,88,128) should be converted to alpha=0 during the pipeline.

## Fix options

### Option A: Fix in conversion pipeline (recommended)
In `Split-StripTextures` and `Convert-GameTextures`, after `-alpha set` or during PNG generation, detect RGB(120,88,128) pixels and set their alpha to 0. This produces RGBA ETC2 with proper alpha transparency. The shader's `mix(bot.rgb, ovl.rgb, ovl.a)` then correctly shows the base texture through transparent regions.

Implementation:
- Add an ImageMagick step after crop: `-fill "rgba(120,88,128,0)" -opaque "rgb(120,88,128)"` or equivalent
- Apply to ALL textures (not just known overlay textures) since the key color is distinctive enough
- Also handle non-strip 32bpp TGAs that go through the Read-TGA path

### Option B: Fix in engine (KTX2 mask generation)
After loading a RGBA ETC2 texture for a bitmap with BM_FLAG_SUPER_TRANSPARENT, decompress the ETC2 data on the CPU, detect (120,88,128) pixels, and generate a mask texture. More complex, slower at load time, and ETC2 decompression on CPU is non-trivial.

### Option C: Hybrid -- strip undefined alpha
For 32bpp TGAs where Alpha=Undefined (all alpha values are identical or junk), strip the alpha channel to produce 24-bit output. This gives RGB ETC2 (format=0). The engine skips RGB-only KTX2 for textures with transparency flags, falling back to the base palette texture which handles (120,88,128) correctly via `ogl_loadpngmask`. Downside: loses the hires replacement for all transparent textures.

## Affected texture scope
Any d2x-xl 32bpp TGA with Alpha=Undefined that uses RGB(120,88,128) as the super-transparent key color. This includes ALL animated textures (strips) since they go through `-alpha set`. Non-strip 32bpp TGAs also affected if they contain the key color.

## Status
- [x] Fix render.c overlay color logic (d1 + d2)
- [x] Investigate door35#14 purple background -- root cause identified
- [x] Investigate misc060/rock313 issues -- root cause identified
- [ ] Implement fix (Option A recommended)
- [ ] Rebuild DXA packs with fix
- [ ] Deploy and test

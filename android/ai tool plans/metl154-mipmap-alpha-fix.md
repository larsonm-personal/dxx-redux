# Fix metl154 overlay disappearing (mipmap alpha degradation)

## Root cause

During the hires texture work (late March 2026), explicit `glGenerateMipmap()`
was added to `ogl_loadtexture()` for all non-font textures on Android. Before
this, the old `GL_GENERATE_MIPMAP` hint was skipped (`#ifndef ANDROID`), so
Android textures had NO mipmaps -- the merge shader always sampled mip level 0
with clean binary alpha. The new `glGenerateMipmap()` creates mipmap chains
using standard box-filter downsampling, which averages transparent (alpha=0)
and opaque (alpha=1) pixels. At steep viewing angles the GPU selects higher mip
levels where the overlay alpha has been diluted toward the area fraction
(~0.2-0.4), causing bars to become semi-transparent or invisible.

This affects both stock 64x64 textures (mipmaps from `glGenerateMipmap()`) and
hires KTX2 512x512 textures (mipmaps pre-baked in the DXA converter). The bug
is identical in both cases because the merge shader's `texture2D(utex2, ...)` 
selects mip levels automatically, and `mix(bot.rgb, ovl.rgb, ovl.a)` reduces
the overlay contribution toward zero when alpha is averaged down.

## Fix

Set `utex2alpha_cutoff` to 0.5 for non-super merge draws. The shader already
supports this:

```glsl
float ovla = ovl.a;
if (utex2alpha_cutoff > 0.0)
    ovla = ovl.a >= utex2alpha_cutoff ? 1.0 : 0.0;
```

This thresholds overlay alpha to binary 0/1 which is correct for all Descent
overlay textures (palette-indexed, always fully opaque or fully transparent).
Eliminates mipmap-induced alpha degradation without requiring DXA rebuild or
shader upgrade.

## Cleanup

Remove the wrap-state experiment code (force-GL_REPEAT in merge draws). The
latest log confirmed all wraps are already GL_REPEAT with zero forced changes
across 15675 lines. The wrap-state hypothesis is definitively ruled out.

## Status
- [x] Create plan
- [x] Set alpha cutoff 0.0 -> 0.5 in d2 ogl.c
- [x] Set alpha cutoff 0.0 -> 0.5 in d1 ogl.c
- [x] Remove wrap-state variables and ogl_force_metl154_repeat_wrap call (d2)
- [x] Remove wrap-state variables and ogl_force_metl154_repeat_wrap call (d1)
- [x] Remove ogl_force_metl154_repeat_wrap function (d2)
- [x] Remove ogl_force_metl154_repeat_wrap function (d1)
- [x] Remove [metl154wrap] logging (d2)
- [x] Remove [metl154wrap] logging (d1)
- [x] Build
- [x] Run code quality linter

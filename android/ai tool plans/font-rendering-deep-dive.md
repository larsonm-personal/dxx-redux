# Font Rendering Deep Dive: Yellow Text + Text Disappearing

## Status: IMPLEMENTED AND TESTED

### Changes made:
1. **Flush exclusion (d1 + d2)**: AF ON flush and TexFilt flush loops now skip
   OGL_FLAG_NOCOLOR textures. Font textures keep their GL handles across
   filter/AF changes. This eliminates the entire class of flush-reload bugs.
2. **Pure-white font foreground (d1 + d2)**: In ogl_loadtexture, after
   ogl_filltexbuf, NOCOLOR GL_RGBA textures get all non-transparent pixels
   forced to (255,255,255,255). This fixes the 6-bit palette scaling producing
   (252,252,252) instead of pure white, ensuring exact vertex color tinting.
3. Build passes, clang-format clean, test_launch_to_automap passes (d2).

## Bug 1: Texture label text not bright yellow in non-hires mode
## Bug 2: Text disappears when TexFilt or AF is turned on (MSAA does NOT cause it)

---

## Root cause analysis summary

After an exhaustive code-level audit of both d1/ and d2/, the OGL_FLAG_NOCOLOR
guards that were added in previous sessions APPEAR correct from a reading
standpoint. All filter-mode, mipmap-generation, and AF-application paths are
guarded. Font texture flags survive flushes. Font atlas pixel data survives
flushes (CPU-side, not freed). Yet the bugs persist.

### What we know works
- Initial font load produces visible text (NOCOLOR flag set, NEAREST filters)
- MSAA changes do NOT cause text disappearing (no texture flush involved)
- g_font_rgb_override correctly sets (1,1,0) for non-hires labels
- Alpha test is enabled (GL_GEQUAL 0.02) -- transparent pixels discarded

### What we know fails
- Text disappears when TexFilt changes (0->1, 0->2, or cycling)
- Text disappears when AF changes (on or off)
- Both involve texture flush-and-reload cycles
- This has survived 4+ fix attempts targeting the NOCOLOR guard

### Key conclusion
The code looks correct from reading. This means one of:
1. There is a subtle runtime interaction that code reading cannot catch
2. The bug is in a path not examined (e.g., a GPU driver quirk, or the GLES3
   shim)
3. There is a race condition or state desynchronization

---

## Traced code paths (full pipeline)

### Font texture creation (ogl_init_font in d2/2d/font.c:585)
1. `oglflags = OGL_FLAG_ALPHA | OGL_FLAG_NOCOLOR` for non-FT_COLOR fonts
2. Parent bitmap created with `BM_FLAG_TRANSPARENT`, data filled with
   palette-indexed pixels (white=gr_find_closest_color(63,63,63), bg=255)
3. `ogl_init_texture()` sets format=GL_RGBA (OGLES ignores NOCOLOR for format),
   stores flags
4. `ogl_loadbmtexture_f()` calls `ogl_loadtexture()` which:
   - NOCOLOR guard prevents GL_LINEAR_MIPMAP_* filters
   - NOCOLOR guard prevents glGenerateMipmap
   - Result: font texture has NEAREST filters, no mipmaps, has_mipmaps=0

### Texture flush (ogl_start_frame in d2/arch/ogl/ogl.c:1685)
TexFilt flush:
  - ALL textures: handle=0, has_mipmaps=0, wrapstate=-1
  - flags, w, h, lw, format, internalformat, is_png all PRESERVED

AF ON flush:
  - Only NON-mipmapped textures: handle=0, wrapstate=-1
  - has_mipmaps NOT cleared (was already 0)
  - flags PRESERVED

### Font texture reload after flush (ogl_bindbmtex -> ogl_loadbmtexture_f)
1. bm->gltexture->handle == 0 -> calls ogl_loadbmtexture(bm)
2. ogl_loadbmtexture_f(bm, GameCfg.TexFilt):
   - AF upgrade: if ogl_aniso_level > 0, texfilt bumped to 2
   - Chase bm_parent to font parent bitmap
   - bm->gltexture != NULL (pointer preserved) -> skip ogl_init_texture
   - bitmapname = piggy_game_bitmap_name(parent) = NULL -> skip PNG/KTX2
   - Falls through to: ogl_loadtexture(buf, 0, 0, tex, bm_flags, 0, texfilt)
3. ogl_loadtexture:
   - NOCOLOR guard: texfilt > 0 BUT flags & OGL_FLAG_NOCOLOR -> else branch
   - Sets GL_NEAREST for MIN and MAG
   - No mipmaps generated, has_mipmaps stays 0
4. ogl_bindbmtex continues:
   - OGL_BINDTEXTURE (handle already bound from upload, likely no-op)
   - if GameCfg.TexFilt > 0: NOCOLOR branch -> GL_NEAREST (if MenuTexFilt=0)

All appears correct.

### Selective filtering in ogl_bindbmtex (d2/arch/ogl/ogl.c:460)
```
if (GameCfg.TexFilt > 0) {
    if (flags & OGL_FLAG_NOCOLOR) {
        // Font: MenuTexFilt controls LINEAR vs NEAREST
    } else if (has_mipmaps) {
        // World/HUD: context-based (MenuTexFilt, HudTexFilt)
    }
    // GAP: non-NOCOLOR, non-mipmap textures get no override
}
```

### Font color path (ogl_ubitmapm_cs in d2/arch/ogl/ogl.c:2934)
- Non-FT_COLOR: c = palette index (from cv_font_fg_color)
- g_font_rgb_override[0] >= 0 -> uses override directly (bypasses palette)
- Vertex color = (R, G, B, 1.0)
- GLES3 shim: fragColor = vColor * texture(uTex, vTexCoord)
- Font foreground ~= (0.988, 0.988, 0.988, 1.0)
- Yellow override result ~= (0.988, 0.988, 0.0, 1.0) -- should be bright

---

## Theories and diagnostic plan

### Theory A: Guards work but something else in GL state breaks
The ogl_start_frame function sets up GL state:
  `glEnable(GL_BLEND)`, `glEnable(GL_ALPHA_TEST)`, `glAlphaFunc(GL_GEQUAL, 0.02)`
The OGL_ENABLE/OGL_DISABLE tracking macros maintain boolean flags.
If any code between flush and text rendering calls raw glDisable(GL_BLEND)
without OGL_DISABLE, the flag desynchronizes and all subsequent OGL_ENABLE(BLEND)
calls are no-ops. Without blending, font background might draw as black.

**Diagnostic**: Log GL_BLEND_enabled and actual glIsEnabled(GL_BLEND) right
before text rendering. Also log alpha test state.

### Theory B: Font texture gets wrong filter despite guard
Maybe ogl_loadtexture's NOCOLOR guard is bypassed in some code path, or
the guard's condition evaluates wrong at runtime.

**Diagnostic**: Add logging inside ogl_loadtexture when OGL_FLAG_NOCOLOR is
set -- log texfilt value, actual filter mode chosen. Also call
glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER) right after upload
to verify the GL state matches what we set.

### Theory C: Font texture is reloaded through a path that doesn't preserve flags
Maybe the font goes through the `bm->gltexture == NULL` path in
ogl_loadbmtexture_f, which creates a NEW texture WITHOUT OGL_FLAG_NOCOLOR.
This would only happen if somehow bm->gltexture becomes NULL during the flush.

**Diagnostic**: Log bm->gltexture pointer and flags value at entry to
ogl_loadbmtexture_f when loading a font texture.

### Theory D: Font texture format (GL_RGBA) + ogl_pal lookup produces bad data
On OGLES, OGL_FLAG_NOCOLOR doesn't affect format -- fonts get GL_RGBA.
The pixel data uses ogl_pal (= gr_palette) for color lookup. If gr_palette's
white entry is black or very dim at reload time, foreground would be invisible.

**Diagnostic**: Log the palette values for the white index during font
texture reload. Print the first few RGBA bytes from the generated texture data.

---

## Fix plan

### Phase 1: Simplify -- exclude font textures from flush entirely
The core insight: font textures NEVER need to be reloaded when TexFilt or AF
changes. They always use GL_NEAREST (or GL_LINEAR if MenuTexFilt is on, but
that's controlled at bind time). Mipmaps are never generated.

Change the flush loops in ogl_start_frame to SKIP OGL_FLAG_NOCOLOR textures:
```c
// TexFilt flush
if (ogl_texture_list[i].handle > 0
    && !(ogl_texture_list[i].flags & OGL_FLAG_NOCOLOR)) {
    // flush
}

// AF ON flush (non-mipmapped textures)
if (ogl_texture_list[i].handle > 0
    && !ogl_texture_list[i].has_mipmaps
    && !(ogl_texture_list[i].flags & OGL_FLAG_NOCOLOR)) {
    // flush
}
```

This eliminates the entire class of bugs related to font texture flush/reload.
Font textures keep their handles, filter state, and pixel data across
TexFilt/AF changes.

The bind-time filter override in ogl_bindbmtex still handles MenuTexFilt
toggling (LINEAR vs NEAREST) without needing a reload.

Apply to both d1/ and d2/.

### Phase 2: Add a MenuTexFilt flush path for completeness
When MenuTexFilt itself changes, font textures may need their bind-time filter
updated. Currently ogl_bindbmtex sets this per-bind. No flush needed -- the
filter is applied on next bind. So no change here.

### Phase 3: Fix yellow text (bug 1)
Need to determine whether this is:
(a) The GL debug_tex_overlay labels in gamerend.c -- uses g_font_rgb_override
(b) Some Kotlin-side overlay text

If (a): Add temporary diagnostic logging to ogl_ubitmapm_cs when
g_font_rgb_override is active -- log the actual color_r, color_g, color_b
values to verify the override is reaching the shader.

If (b): This is a Kotlin/Compose UI issue, not GL code.

One credible theory: the font texture foreground isn't pure white because
gr_find_closest_color(63,63,63) returns a palette index whose entry has been
remapped to a dimmer color (palette changes between menu and game). The
modulated result would be dim yellow instead of bright.

Possible fix: for OGLES builds, bypass the palette lookup entirely and use
hardcoded RGBA(255,255,255,255) for non-FT_COLOR font foreground pixels.
This would guarantee perfect tinting via vertex color modulation.

### Phase 4: Diagnostic logging (to be removed after verification)
Add temporary logging behind #ifdef ANDROID to trace the flush/reload cycle:
1. In ogl_start_frame flush: log count of NOCOLOR textures SKIPPED
2. In ogl_loadtexture: when NOCOLOR, log texfilt and filter mode
3. In ogl_bindbmtex: when NOCOLOR, log the filter applied
4. In ogl_ubitmapm_cs: when g_font_rgb_override active, log color values

### Phase 5: Build, test, verify
1. Build APK
2. Install on emulator
3. Start game, verify text visible
4. Toggle TexFilt via overlay -- verify text stays visible
5. Toggle AF via overlay -- verify text stays visible
6. Check debug_tex_overlay labels -- verify yellow for non-hires

### Phase 6: Clean up diagnostic logging
Remove temporary log lines once bugs are confirmed fixed.

---

## Files to modify
- d2/arch/ogl/ogl.c: flush exclusion (phase 1), diagnostics (phase 4)
- d1/arch/ogl/ogl.c: same changes mirrored
- d2/2d/font.c: optional font foreground fix (phase 3)
- d1/2d/font.c: same

## Code complexity reduction
After phase 1, the following code can be SIMPLIFIED:
- The ogl_loadtexture NOCOLOR guard for filter mode is still useful as defense
  in depth but no longer critical (fonts won't be reloaded by flush)
- The ogl_loadtexture NOCOLOR guard for mipmap generation: same
- The ogl_bindbmtex NOCOLOR branch: STILL NEEDED for MenuTexFilt toggling

The overall accreted complexity from 4+ fix rounds can be trimmed:
- Phase 1 (flush exclusion) is the PRIMARY fix -- simple and complete
- Previous guards become defense-in-depth, not load-bearing

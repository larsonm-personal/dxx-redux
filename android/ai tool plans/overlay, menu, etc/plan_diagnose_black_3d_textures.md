# Plan: Diagnose All-Black 3D Textures

## Symptom
ALL 3D world textures render as black on both real device (GLES 3) and emulator.
Visible: ship HUD/cockpit gauges, crosshairs, overlay text, Kotlin overlays.
Black: every wall, floor, ceiling, object, enemy in the 3D world.

## Key Architecture Facts
- Fragment shader: `c = vColor; if (uTexEnabled != 0) c *= texture(uTex, vTexCoord);`
- 3D polygons: color_array = light_rgb (per-vertex lighting) via g3_draw_tmap
- 2D HUD bitmaps: color = white (1,1,1,1) via ogl_ubitmapm_cs (rendered AFTER ogl_end_frame)
- GLES3 shim wraps ~37 GL1.x calls (matrix stack, client arrays, fixed-function)
- Texture functions (glBindTexture, glGenTextures, glTexImage2D) go to real GLES3 driver
- ogl_pal = gr_palette (populated from game data on level load)
- ogl_max_texture_size queried from GL_MAX_TEXTURE_SIZE, capped at 2048
- BM_FLAG_NO_LIGHTING textures use color (1,1,1,1) instead of light_rgb

## Top Hypotheses (ordered by probability)

### H1: Lighting (light_rgb) is all zeros
If the 3D lighting pipeline produces zero values for all vertices, every non-self-lit
texture would render black (vColor * texture = 0). Self-lit textures (BM_FLAG_NO_LIGHTING)
would still be visible at full brightness.

### H2: Texture data is black/invalid
Palette conversion (ogl_filltexbuf) could produce all-zero RGBA if ogl_pal is corrupt.
Or hires ETC2/PNG paths could produce black textures. Since HUD bitmaps work (same
upload path), this is less likely for base textures but possible for hires ones.

### H3: GL state corruption between ogl_start_frame and 3D draws
Something in the 3D setup path (perspective, depth test, cull face) could be wrong,
though this would typically show as missing geometry rather than black textures.

### H4: Texture completeness (mipmap filter without mipmaps)
If texfilt > 0, min filter is GL_LINEAR_MIPMAP_NEAREST. If glGenerateMipmap fails
silently, textures are "incomplete" in GLES3 and sample as (0,0,0,1) = black.

## Diagnostic Phases

### Phase 1: Force full-bright colors (5 min)
Add `#ifdef ANDROID` block in g3_draw_tmap to force color_array to (1,1,1,1) for
ALL vertices (ignoring light_rgb). This isolates: is the problem in TEXTURE DATA
or LIGHTING?

```c
// Temporary diagnostic: force full-bright to isolate lighting vs texture
#ifdef ANDROID
color_array[index4]   = 1.0;
color_array[index4+1] = 1.0;
color_array[index4+2] = 1.0;
color_array[index4+3] = 1.0;
#else
// ...existing light_rgb code...
#endif
```

**If textures become visible**: H1 confirmed. Investigate lighting pipeline.
**If still black**: Texture data or GL state issue. Proceed to Phase 2.

### Phase 2: Texture data validation (10 min)
Add one-time diagnostic logging in ogl_loadtexture (after ogl_filltexbuf, before
glTexImage2D) to check if texture data is non-zero:

- Log first 16 bytes of texbuf for the first 5 textures loaded
- Log tex->handle after glGenTextures
- Log glGetError() after glTexImage2D
- Log tex->tw, tex->th, tex->format, tex->internalformat

Also add to ogl_bindbmtex: log bm->gltexture->handle for first 5 3D draws.

### Phase 3: Hires vs base texture isolation (5 min)
Temporarily disable hires loading: make ogl_allow_png() return 0, or add
`return 0;` at the top of the HAVE_LIBPNG block in ogl_loadbmtexture_f.

**If base textures work**: ETC2 or PNG pipeline is broken.
**If base textures also black**: Problem is in palette conversion or GL state.

### Phase 4: Texture completeness check (5 min)
Force texfilt = 0 at the top of ogl_loadbmtexture_f to rule out mipmap
incompleteness. Or add glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0)
after every glTexImage2D call.

### Phase 5: Self-lit texture check (observation)
While in-game, look for self-lit surfaces (lava, energy centers, force fields).
These have BM_FLAG_NO_LIGHTING and use color (1,1,1,1). If they render correctly
while normal surfaces are black, it confirms H1 (lighting).

### Phase 6: Git bisect (30 min, last resort)
If none of the above isolates the cause, bisect through the 7 hires texture commits
(ff35d63~1 to HEAD). The known-good state is the commit before hires texture work.

## Implementation Order
1. Phase 1 (force full-bright) -- most information for least effort
2. Phase 5 (observe self-lit) -- zero-code observation
3. Phase 3 (disable hires) -- quick toggle
4. Phase 4 (disable mipmaps) -- quick toggle
5. Phase 2 (texture data logging) -- if above phases don't isolate
6. Phase 6 (bisect) -- last resort

## Status
- [ ] Phase 1: Force full-bright colors
- [ ] Phase 2: Texture data validation
- [ ] Phase 3: Hires vs base isolation
- [ ] Phase 4: Texture completeness check
- [ ] Phase 5: Self-lit texture observation
- [ ] Phase 6: Git bisect

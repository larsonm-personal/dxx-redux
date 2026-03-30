# Plan: Relax Hires Texture Size Limits

## Problem
The Rebirth engine ties texture size limits to the game's render viewport (e.g. 640x360),
with a minimum of 1024. This means texbuf (the upload scratch buffer), ogl_filltexbuf's
safety check, and ogl_loadtexture's guard all reject textures larger than 1024x1024,
even though the GPU can handle much larger.

## Changes (applied identically to d1 and d2)

### 1. Add `ogl_max_texture_size` global
- `ogl.c`: define `int ogl_max_texture_size = 1024;` (safe default)
- `ogl_init.h`: declare `extern int ogl_max_texture_size;`

### 2. Query GL_MAX_TEXTURE_SIZE at init
- `gr.c` `ogl_get_verinfo()`: add `glGetIntegerv(GL_MAX_TEXTURE_SIZE, ...)` OUTSIDE
  the `#ifndef OGLES` block so it runs on desktop and GLES

### 3. Use `ogl_max_texture_size` for texbuf allocation
- `ogl_init_pixel_buffers()`: allocate texbuf to fit `ogl_max_texture_size` instead of
  `max(screen_w, 1024)`. Use `max(dim, ogl_max_texture_size)` for the allocation.

### 4. Use `ogl_max_texture_size` for the size checks
- `ogl_filltexbuf`: replace `max(grd_curscreen->sc_w, 1024)` with `ogl_max_texture_size`
- `ogl_loadtexture`: replace the screen-based guard with `ogl_max_texture_size`

### 5. Make decodebuf dynamic
- Change from `unsigned char decodebuf[1024*1024]` to a dynamic allocation at init
  based on `ogl_max_texture_size * ogl_max_texture_size` (1 byte/pixel for paletted)
  Actually: decodebuf is used for RLE paletted bitmaps which are always small (base game
  textures, max 64x64 or 128x128). Hires replacements are PNGs which bypass decodebuf.
  So 1024*1024 is fine -- leave it alone.

## Files modified
- d1/arch/ogl/ogl.c
- d2/arch/ogl/ogl.c
- d1/arch/ogl/gr.c
- d2/arch/ogl/gr.c
- d1/include/ogl_init.h
- d2/include/ogl_init.h

## Status
- [x] Plan created
- [x] Implementation
- [x] Build
- [x] Test with 256px DXA -- PASS (94 hires textures loaded, max 640x4608, sprite strips > 2048 gracefully skipped)
- [x] Lint -- all checks pass

# OGL_MERGE enablement for super-transparent textures on Android

## Problem
Two-pass non-OGL_MERGE rendering (draw base, blend overlay on top) cannot do
super-transparency: once the base texture writes color+depth, it can't be undone
by the overlay pass. Super-transparent pixels (door centers) need BOTH textures
to be invisible so geometry behind shows through.

## Solution
Enable OGL_MERGE on Android. This uses a single-pass shader that samples base
and overlay simultaneously, with a mask texture (unit 2) to zero output alpha
for super-transparent pixels. The infrastructure already exists for desktop.

For hires textures (ETC2/KTX2), the key color (120,88,128) is destroyed by the
pipeline and lossy compression. Generate mask PNGs at pipeline time (when
key color is still identifiable) and store them alongside the main KTX2 in DXAs.

## Phase 1: Engine -- enable OGL_MERGE [DONE]

### 1a. CMakeLists.txt
- `android/app/src/main/cpp/CMakeLists.txt` line 488: `OPENGLMERGE ON`

### 1b. oglprog.c (d2 + d1, identical files)
- Uncomment `precision mediump float;` in both fragment shaders (required for GLES)
- Add `if (c.a < 0.02) discard;` to both fragment shaders before gl_FragColor
  (OGL_MERGE disables glAlphaFunc; shaders need their own alpha test for depth)

### 1c. ogl.c ogl_start_frame (d2 + d1)
- Change `#ifdef OGL_MERGE` / `#else` to both-execute on OGLES:
  `#ifdef OGL_MERGE` block always runs (shader matrix)
  `#if defined(OGLES) || !defined(OGL_MERGE)` block sets shim/fixed-function matrix
- Alpha test: change `#ifndef OGL_MERGE` to
  `#if defined(OGLES) || !defined(OGL_MERGE)` so shim alpha test stays enabled

### 1d. ogl.c g3_draw_tmap_2 OGL_MERGE path (d2 + d1)
- On Android, use `gles3_shim_use_external(prog)` instead of raw `glUseProgram`
- Allocate a static VBO for GLES 3.0 compliance (no client-side attrib pointers)
- Upload vertex/color/texcoord arrays to VBO before draw
- Call `gles3_shim_use_external(0)` and unbind VBO after draw
- Keep debug texture label overlay code under `#ifdef ANDROID` in the
  non-OGL_MERGE `#ifndef OGL_MERGE` block; duplicate it under `#ifdef ANDROID`
  in the OGL_MERGE `#else` block too

## Phase 2: Engine -- mask loading from DXA [DONE]

### 2a. Add ogl_load_dxa_mask helper (d2 + d1)
- New static function under `#if defined(OGL_MERGE) && defined(ANDROID)`
- Loads `texname_mask.png` from DXA via `read_png` + `ogl_loadtexture`
- Stores in `bm->gltexture_mask`

### 2b. Call from KTX2 loading path
- After successful KTX2 upload, if `BM_FLAG_SUPER_TRANSPARENT`, call helper

### 2c. Call from PNG fallback path
- Replace desktop `ogl_loadpngmask` call with Android mask file loading

## Phase 3: Pipeline -- generate mask PNGs [DONE]

### 3c. Split-StripTextures (added after first rebuild failure)
- Strip splitter now splits mask alongside main frames
- Main loop detects `_mask.png` from strip splitter output
- Filter `_mask.png` from texture file collection to avoid processing as textures

### 3a. convert_d2xxl_textures.ps1 Read-TGA
- Track key color pixel detection in a script-level variable
- When key color found, also create mask bitmap (same dims, alpha=0 for key
  color, alpha=255 elsewhere)

### 3b. Main conversion loop
- After resize, if mask exists, resize mask with same params
- Add mask PNG to DXA as `texname_mask.png`

## Phase 4: Build + Test [DONE]
- APK build: DONE (BUILD SUCCESSFUL)
- DXA rebuild: DONE (D2-128: 1612 textures, 334 masks including 15 door35 frames)
- Test: PASS (test_launch_to_automap, 36 steps, 26s, hires=1419)
- Visual verification of super-transparent doors: pending user testing
- Build APK
- Rebuild D1 + D2 DXAs
- Deploy to emulator, run test_launch_to_automap
- Visual verification of super-transparent doors

## Key facts
- gltexture_mask field: on grs_bitmap under `#ifdef OGL` (always present), gr.h
- ogl_loadpngmask: encodes mask as single-byte array (255=super, 0=normal),
  loaded via ogl_loadtexture as paletted (palette 255 = transparent = mask.a=0)
- ogl_freebmtexture: already frees gltexture_mask (no cleanup changes needed)
- Lores mask generation: already in d1/d2 ogl.c under `#ifdef OGL_MERGE`,
  works automatically once OGL_MERGE is enabled
- Attribute locations: OGL_APOS=0, OGL_ACOLOR=1, OGL_ATEXCOORD=2, OGL_ATEXCOORD2=3
  (gles3_shim uses ATTR_POS=0, ATTR_COLOR=1, ATTR_TEXCOORD=2 -- slots 0-2 match)
- gles3_shim intercepts: glDrawArrays, matrix calls, client-state, enable/disable
  Does NOT intercept: glVertexAttribPointer, glEnableVertexAttribArray, glBindBuffer
- oglprog.c: d1 and d2 are byte-identical (fc.exe confirmed)

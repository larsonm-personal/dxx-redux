# ETC2 emulator black textures fix

## Problem
All in-game textures rendered as 100% black at 7 FPS when ETC2 compressed
textures (.etc2 files from DXA packs) were loaded on the Android emulator.
HUD textures (non-ETC2) rendered correctly.

## Root cause
The Android emulator's "OpenGL ES Translator" (which bridges GLES calls to
the host desktop GL driver) has a broken ETC2 decoder. Even manually crafted
4x4 white ETC2 blocks uploaded via glCompressedTexImage2D decode to all-black.

Key findings during diagnosis:
- GL uploads succeed with no errors (glGetError returns GL_NO_ERROR)
- Uncompressed glTexImage2D uploads work perfectly (solid red test)
- Manually crafted ETC2 blocks (known-correct individual-mode white) also
  render black, confirming the issue is in the emulator's GLES translator,
  not the etc2comp encoder output
- GL_RENDERER: "Android Emulator OpenGL ES Translator (NVIDIA GeForce RTX 5050 Laptop GPU)"
  (not SwiftShader as initially expected)

## Fix (DONE)
Added runtime detection in `ogl_get_verinfo()` (both d1/d2 gr.c):
- Queries `glGetString(GL_RENDERER)` on Android
- If renderer contains "SwiftShader" or "Android Emulator", sets global
  `ogl_etc2_broken = 1`
- ETC2 upload path in `ogl_loadbmtexture_f()` checks this flag and skips
  compressed uploads when set
- Game falls back to base paletted textures on emulator (DXA only has .etc2
  files, no PNGs to fall back to)

Files changed:
- d2/arch/ogl/ogl.c: added `ogl_etc2_broken` global, ETC2 path guard
- d2/arch/ogl/gr.c: SwiftShader/emulator detection in `ogl_get_verinfo()`
- d1/arch/ogl/ogl.c: same changes mirrored
- d1/arch/ogl/gr.c: same changes mirrored

## Notes
- ETC2 compressed textures should work correctly on real hardware (Adreno,
  Mali, etc.) since ETC2 is hardware-decoded on GLES 3.0+ devices
- The emulator's broken ETC2 is a known limitation of the GLES translator
- If hires textures need to be tested on emulator in the future, options:
  1. Create PNG-only DXAs for emulator use
  2. Add a software ETC2 decoder fallback path
  3. Test on physical devices instead

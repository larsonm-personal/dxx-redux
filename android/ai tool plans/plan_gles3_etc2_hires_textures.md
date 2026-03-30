# Plan: Compressed Hires Textures via GLES 3.0 Shim

## Problem
Hires texture replacement loads only 7% on Android emulator. Root causes:
1. Game uses GLES 1.1 -- ETC2 compressed textures require GLES 3.0
2. `ogl_cache_level_textures()` skips BM_FLAG_PAGED_OUT bitmaps
3. Metric reports hires/total instead of hires/available

## Approach
- GLES 3.0 upgrade with compartmentalized fixed-function shim (new files only)
- ETC2 compressed texture uploads (8x GPU memory savings, full RGBA)
- Eager PNG loading for paged-out bitmaps
- Replacement success rate metric

## Phase 1: GLES 3.0 Fixed-Function Shim

- [x] Create `android/app/src/main/cpp/shared/gles3_shim.h` -- redirect macros
- [x] Create `android/app/src/main/cpp/shared/gles3_shim.c` -- CPU matrix stack, shaders, state
- [x] Update `android/app/src/main/cpp/CMakeLists.txt` -- GLESv3 linkage, new sources
- [x] Update EGL context in d1/d2 gr.c -- version 1->3 (#ifdef ANDROID)
- [x] Include shim header from d1/d2 ogl_init.h (#ifdef ANDROID)
- [ ] Verify basic rendering on emulator

## Phase 2: ETC2 Texture Compression

- [x] Create `android/app/src/main/cpp/shared/etc2_compress.c/.h` -- lightweight encoder
- [x] Add GL error checking after texture uploads in d1/d2 ogl.c (#ifdef ANDROID)
- [x] Compressed upload path in `ogl_loadtexture()` -- glCompressedTexImage2D

## Phase 3: Eager PNG Loading

- [x] Remove BM_FLAG_PAGED_OUT filter in `ogl_cache_level_textures()` on Android
- [x] Handle paged-out bitmaps in `ogl_loadbmtexture_f()` -- early return if no PNG

## Phase 4: Metric + Test

- [x] Update hires metric in `game_introspect.cpp` to replacement success rate
- [x] Update `test_mod_loading_128.json5` threshold
- [x] Add r_hires_found / r_hires_loaded counters to d1/d2 ogl.c

## Phase 5: Build, Test, Lint

- [x] Full build (no new warnings from our code)
- [x] Code quality / lint pass (clang-format applied, no new issues)
- [ ] Deploy + emulator test

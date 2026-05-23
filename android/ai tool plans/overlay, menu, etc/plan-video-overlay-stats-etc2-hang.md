# Plan: Video overlay stats expansion + ETC2 hang fix

## Issues
1. Kotlin VideoInfoOverlay missing useful stats from removed C overlay (tex memory, render/display resolution)
2. 128x128 mod hangs at "prepare for descent" -- etc2comp effort level 40 is way too slow for real-time level loading

## Changes

### 1. Extend nativeGetVideoStats JNI
- Add fields [6]-[10]: tex_memory_kb, render_w, render_h, display_w, display_h
- VS_SIZE 6 -> 11
- Use ogl_get_texture_bytes(), grd_curscreen_w/h, android_surface_get_display_width/height

### 2. Update VideoInfoOverlay.kt
- Parse new fields from intarray
- Add rows for texture memory (MB) and render/display resolution
- Update line count 6 -> 8

### 3. Fix ETC2 hang
- Replace ETCCOMP_DEFAULT_EFFORT_LEVEL (40.0f) with low effort (10.0f) in etc2_compress.cpp
- 40 is designed for offline tools; 10 is still decent quality but ~4-10x faster

### 4. Update comment in MainActivity.kt for nativeGetVideoStats

## Status
- [x] JNI extension
- [x] Kotlin overlay update
- [x] ETC2 effort fix
- [x] Build + lint

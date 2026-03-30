# Plan: ImageMagick 128px downscale + per-DXA READMEs

## Status: COMPLETE

## Goals
1. Add get_imagemagick.ps1 dependency fetcher (pinned version)
2. Update 128px DXA generation to use ImageMagick (linear-light Lanczos, micro-sharpen)
3. Source 128px from 512x512 instead of 256x256
4. Customize per-DXA README texts with proper attribution
5. Rebuild all DXA packs

## Changes

### 1. tool_versions.conf
- [x] Add IMAGEMAGICK_VERSION, IMAGEMAGICK_URL, IMAGEMAGICK_DIR_NAME

### 2. android/get_deps/get_imagemagick.ps1
- [x] New script following get_7zip.ps1 / get_fpcalc.ps1 pattern
- [x] Downloads portable Q16-HDRI-x64 7z archive
- [x] Uses 7z to extract; returns path to magick.exe

### 3. game_data/mods/convert_d2xxl_textures.ps1
- [x] Add -Magick parameter
- [x] When Magick provided + MaxDim > 0: use ImageMagick pipeline
  - sRGB -> linear RGB -> Lanczos resize -> micro-sharpen -> sRGB
  - Detect alpha from TGA header to choose PNG vs JPEG output
- [x] Fall back to System.Drawing when Magick not provided

### 4. game_data/mods/convert_all.ps1
- [x] Change 128 config TexSize from 256 to 512
- [x] Add -Magick parameter with auto-locate from dep base
- [x] Multi-line per-pack README texts (not one-liners)

### 5. Rebuild + index
- [x] Run convert_all.ps1
- [x] Regenerate game_data_index.txt

## Credits
- D2: d2x-xl by Aus-RED-5D, DizzyRox, MetalBeast, Novacron, Theftbot
- D1: d2x-xl by DizzyRox, Novacron, Aus-RED-5

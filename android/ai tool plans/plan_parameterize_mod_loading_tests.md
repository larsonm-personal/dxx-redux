# Plan: Parameterize mod loading test for 128/256/512 texture sizes

## Status: IN PROGRESS

## Goal
Create test_mod_loading_256 and test_mod_loading_512 variants alongside the existing
test_mod_loading_128. Each variant pushes the corresponding texture DXA and asserts
hires loading at the appropriate resolution. Run-TestMenu.ps1 auto-discovers test_*.json5
files so no menu changes are needed.

Also: survey the 128px creation pipeline since the user reports "base game textures
look correct, only the 128x128 ones have the odd overlay X pattern" -- the issue may
be baked into the source PNGs rather than a runtime bug.

## Phases

### Phase 1: Create test variants
- [x] Create test_mod_loading_256.json5 (DXA sha: dcb36008..., max_hires_w >= 256)
- [x] Create test_mod_loading_512.json5 (DXA sha: bce93b19..., max_hires_w >= 512)

### Phase 2: Run-TestMenu.ps1
- [x] No changes needed -- auto-discovers test_*.json5

### Phase 3: Survey 128px creation pipeline
- [x] Documented findings below

## 128px Creation Pipeline Survey

### Pipeline overview
1. Source: `game_data/mods/d2x-xl/D2-textures-512x512.7z` (native 512px TGA textures)
2. Script: `convert_all.ps1` calls `convert_d2xxl_textures.ps1 -TexSize 512 -MaxSize 128`
3. For each TGA, the `Convert-WithMagick` function runs:
   ```
   magick input.tga -colorspace RGB -filter Lanczos -resize 128x128 -unsharp 0x0.4 -colorspace sRGB output.{png,jpg}
   ```
4. Alpha detection: if TGA bpp=32, output is PNG (preserves alpha); otherwise JPEG q92
5. Output packed into ZIP as `d2xxl-hires-textures-d2-128.dxa`

### 256px path (different)
- Source: `D2-textures-256x256.7z` (native 256px, NO downscale)
- No ImageMagick involved -- direct TGA->PNG/JPG via System.Drawing

### 512px path
- Source: `D2-textures-512x512.7z` (native 512px, NO downscale)
- Same as 256, just bigger source archive

### Possible X pattern causes
The "X pattern" only appears on 128px textures, not base game or presumably the 256/512
variants. Possible causes in the pipeline:
1. **ImageMagick sRGB<->linear conversion** -- the colorspace round-trip may introduce
   artifacts on textures that are already in linear space or have unusual color profiles
2. **Micro-sharpening (`-unsharp 0x0.4`)** -- could amplify low-contrast noise patterns
   at small resolution, especially on textures with fine detail or alpha edges
3. **TGA alpha interpretation** -- some d2x-xl textures may have a "dirty" alpha channel
   (non-trivial alpha values that are unused in the original game). The pipeline detects
   alpha by bpp only (32-bit = has alpha). If a texture has bpp=32 but meaningless alpha,
   the PNG output preserves that alpha, and the ETC2 compressor or renderer shows it
4. **Lanczos ringing** -- Lanczos filter can produce ringing artifacts at high contrast
   edges when downscaling 4x (512->128), appearing as a halo or X-like pattern on
   textures with strong diagonal features

### Recommended next step
Run the 256 and 512 tests on the emulator. If those look correct, the issue is in the
128px downscale pipeline (ImageMagick). If they also show the X pattern, the issue is
in the runtime ETC2 compressor or renderer. This will narrow down the root cause.

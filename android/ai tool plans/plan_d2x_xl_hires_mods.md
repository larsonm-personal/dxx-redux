# Plan: d2x-xl High-Res Texture and Sound Mod Support

## Status: PHASE 1-2 COMPLETE (textures + sounds conversion, Android PNG loading)

## Completed Work

### Phase 1a: Texture conversion script -- DONE
- Created `game_data/mods/convert_d2xxl_textures.ps1`
- Pure PowerShell + .NET System.Drawing (no external deps like ImageMagick)
- Reads uncompressed type-2 TGA, converts to PNG, packs into .dxa (renamed ZIP)
- Supports optional `-MaxSize` downscaling for mobile
- Tested: D1 279 textures (190.8 MB .dxa), D2 569 textures (358.7 MB .dxa), 0 errors

### Phase 1b: Sound conversion script -- DONE
- Created `game_data/mods/convert_d2xxl_sounds.ps1`
- Pure PowerShell WAV parser + resampler to 8-bit unsigned mono 22050 Hz .r22
- Files placed in `Sounds/` subdirectory inside .dxa
- Tested: D1 113 sounds (1.8 MB .dxa), D2 191 sounds (3.5 MB .dxa), 0 errors

### Phase 2: Android PNG loading via stb_image -- DONE
- Created `android/app/src/main/cpp/shared/pngfile_stb.c`
  - Implements `read_png()` / `write_png()` from `pngfile.h` using stb_image
  - Supports PNG, TGA, JPG, BMP via single header (stb_image.h)
  - `write_png()` is a stub (Android screenshots use TGA fallback)
- CMakeLists.txt changes (`android/app/src/main/cpp/CMakeLists.txt`):
  - Downloads stb_image.h from nothings/stb commit `013ac3beddff3dbffafd5177e7972067cd2b5083`
  - Keeps `PNG OFF` so libpng-dependent `pngfile.c` is NOT compiled
  - Adds `pngfile_stb.c` to both dxx-redux-d1 and dxx-redux-d2 targets
  - Defines `HAVE_LIBPNG` on game targets + arch_ogl targets (activates PNG replacement code in ogl.c/gr.c)
  - Adds `_DECODERS_DIR` to include path for stb_image.h
- Android build verified: BUILD SUCCESSFUL

### DXA format notes
- DXA = renamed ZIP file, auto-mounted by `PHYSFSX_addArchiveContent()` with prepend priority
- PNGs at ZIP root, sounds in `Sounds/` subdirectory
- Engine looks for `{bitmapname}.png` at PhysFS search path root
- Engine looks for `Sounds/{name}.r22` or `Sounds/{name}.raw`

## Remaining Work

## Source Material

Three archives from d2x-xl:

| Archive | Contents | Files | Uncompressed |
|---|---|---|---|
| D2-textures-512x512.7z (193 MB) | D2 hires wall/object/door textures | 569 TGA + 1 JPG | ~1.2 GB |
| D1-textures-512x512.7z (115 MB) | D1 hires wall/object/door textures | 279 TGA | ~748 MB |
| hires-sounds.7z (63 MB) | D1+D2 resampled sound effects | 535 WAV | ~123 MB |

### Texture archive structure

All textures are in `textures/d1/` or `textures/d2/` subdirectories.

Naming convention matches the game's internal bitmap names exactly:
- `ceil002.tga` -- matches bitmap name "ceil002"
- `door01#0.tga` -- matches animated frame "door01#0"
- `box01a#0.tga` -- matches supertransparent frame

File formats:
- Uncompressed TGA (type 2), no RLE
- Mix of RGB (3 bytes/pixel) and RGBA (4 bytes/pixel)
- Most are 512x512, some are 256x256, some doors/objects up to 2048x2048 or 4096x4096
- 1 cockpit JPG (d2 only)

### Sound archive structure

Four subdirectories:
- `sounds/d2/44khz/` -- 191 WAV files, D2 sounds at 44.1 kHz
- `sounds/d2/22khz/` -- 184 WAV files, D2 sounds at 22.05 kHz
- `sounds/d1/` -- 113 WAV files, D1 sounds (appears to be 44.1 kHz)
- `sounds/d2x-xl/` -- 47 WAV files, d2x-xl-specific additions (not in base game)

Sound naming matches the game's 8-char sound names (e.g., "afbr_1", "boss02", "door1c").

---

## Current Engine Support

### Textures: PNG replacement (desktop only)

Redux has an existing hi-res texture replacement system in `ogl_loadbmtexture_f()`:

1. Gets bitmap name via `piggy_game_bitmap_name(bm)` -- returns e.g. "ceil002" or "door01#0"
2. Constructs filename `{name}.png`
3. Opens via PHYSFS (`PHYSFS_openRead`)
4. Loads as PNG via libpng
5. Uploads to GL texture, any resolution (power-of-2 not required on GLES 2+)

Key constraints:
- **format**: PNG only (.png). No TGA, no JPG
- **depth**: 8-bit color depth required (RGB, RGBA, or paletted)
- **guard**: `#ifdef HAVE_LIBPNG` -- must be compiled with libpng
- **multiplayer**: disabled unless `Netgame.AllowCustomModelsTextures` is set
- **name length**: `BitmapFile.name` is 15 chars max (sufficient for all d2x-xl names)
- **filename buffer**: `char filename[64]` in `ogl_loadbmtexture_f` (sufficient)

### Textures: Android status -- BROKEN

**PNG is currently OFF for Android builds** (`android/app/src/main/cpp/CMakeLists.txt` line 453).
`HAVE_LIBPNG` is never defined for Android, so the entire PNG replacement path is compiled out.

libpng is not currently built/linked for Android NDK.

### Sounds: raw PCM only

External sound loading in `ds_load()` (bmread.c):
- Looks for `Sounds/{name}.raw` (11025 Hz) or `Sounds/{name}.r22` (22050 Hz)
- Format: 8-bit unsigned PCM mono, headerless
- Loaded via PHYSFS

Internal format (`digi_sound`): 8-bit, mono, 11025 or 22050 Hz.

SDL_mixer converts from 8-bit mono to output format (44.1/48 kHz stereo) at playback via `SDL_BuildAudioCVT()`.

**No WAV loading support exists** for sound effects. WAV parsing exists only in SDL itself (for music).

---

## Feasibility Assessment

### Textures: HIGH feasibility

The bitmap name mapping is identical -- d2x-xl uses the same internal texture names as the engine. The replacement texture system already exists and works. The gaps are:

1. **Format mismatch**: d2x-xl provides TGA, redux expects PNG
   - **Solution A** (offline conversion, recommended): Convert TGA -> PNG during import. This is a lossless conversion for uncompressed TGA. PNG will be significantly smaller on disk (LZ compression). Can be done with ImageMagick, Python PIL, or a small C tool. No engine changes needed
   - **Solution B** (engine change): Add TGA loading alongside PNG in `ogl_loadbmtexture_f()`. A simple TGA reader for uncompressed type-2 is ~80 lines. The xmodel TGA code exists but uses the C++ CFile abstraction, not PhysFS
   - **Recommendation**: Solution A. Converting offline keeps the engine simple and produces smaller files. On Android where storage matters, PNG is much better than uncompressed TGA (a typical 512x512 RGBA PNG ≈ 200KB vs 1MB TGA)

2. **Android libpng**: PNG replacement is currently disabled on Android
   - libpng needs to be added as an NDK dependency and `PNG` set to `ON`
   - Alternatively, use `SDL_image` (already a common NDK library) for format-agnostic loading -- it supports PNG, TGA, JPG, and more. But this is a bigger change
   - Or use stb_image.h (single-header, no dependencies, supports PNG/TGA/JPG) -- fits the project's "lightweight single-file" philosophy

3. **Memory/GPU concerns on mobile**: 
   - 512x512 RGBA = 1 MB GPU memory per texture. Original textures are 64x64 = 16 KB. That is a 64x increase
   - D2 has ~569 replacement textures. If all loaded: ~569 MB GPU memory (not feasible on mobile)
   - However, the engine loads textures on demand and evicts them (`ogl_smash_textures`). Only visible textures are loaded at any time, which is typically dozens, not hundreds
   - The 2048x2048 and 4096x4096 textures (doors, objects) would be problematic -- 16-64 MB each. These should be downscaled during import, or capped at 1024x1024 for mobile
   - **Recommendation**: During import, cap texture size at 512x512 (or 1024x1024) for mobile. Desktop can use full resolution
   - Compressed GPU formats (ETC2) could reduce GPU memory by 4-8x but reqiure a separate conversion pipeline

4. **Disk space**: 
   - ~1.2 GB uncompressed TGA is too much for mobile. Converting to PNG should bring it down to ~200-300 MB
   - Could offer as optional download from the launcher

### Sounds: MEDIUM feasibility

The sound name mapping is also identical. The gaps are larger:

1. **Format mismatch**: d2x-xl provides WAV, redux expects headerless raw PCM
   - **Solution A** (offline conversion): Strip WAV headers and convert to 8-bit unsigned PCM mono at 22050 Hz -> .r22 files. Loses the quality advantage of 44 kHz and 16-bit depth. The whole point of hires sounds is moot
   - **Solution B** (engine change, recommended): Add WAV file loading in the sound path:
     - Parse WAV header (44 bytes typically) to get sample rate, bit depth, channels
     - Store in `digi_sound` with actual sample rate and bit depth
     - Update `mixdigi_convert_sound()` to handle 16-bit and 44 kHz source data (it already uses `SDL_BuildAudioCVT` which can convert from any format)
     - This is ~60-80 lines of code in piggy.c or a new wav_load.c
     - Search for `Sounds/{name}.wav` before falling back to `.r22`/`.raw`
   - **Solution C**: Convert WAV to .raw but at the actual quality. Add a `.r44` extension convention for 44 kHz. Still loses 16-bit -> 8-bit
   - **Recommendation**: Solution B. WAV parsing is trivial and SDL_BuildAudioCVT already handles the upsampling. This preserves the full quality improvement

2. **Internal format expansion**:
   - `digi_sound.bits` is already an int field -- can hold 16
   - `digi_sound.freq` is already an int field -- can hold 44100
   - `mixdigi_convert_sound()` currently hardcodes `AUDIO_U8` as source format. Change to `AUDIO_S16` when `bits == 16`
   - The `digi_audio.c` raw backend would need similar changes (or just don't support hires there)

3. **Memory**: 44 kHz 16-bit WAV is ~4x larger than 22 kHz 8-bit raw per second
   - Total uncompressed: ~123 MB. Not all loaded at once -- sounds are loaded on demand via `piggy_load_sound_data`
   - Feasible on modern devices

4. **d2x-xl exclusive sounds**: The 47 files in `sounds/d2x-xl/` (airbubbles, ambientlava, etc.) are d2x-xl-specific and have no matching sound IDs in base D1/D2. These cannot be used without adding new sound definitions. Skip these

5. **Naming**: Need to verify that the WAV filenames (sans extension) exactly match the 8-char names in AllSounds[]. The names look right (afbr_1, boss02, door1c, etc.) but some may have longer names -- sound names in the pig file are 8 chars. A few d2x-xl names are longer (e.g., "bossdied" = 8 chars OK, "bb_die-2" = 8 chars OK). Should be verified during import by cross-referencing against the pig file's sound table

---

## Proposed Import Pipeline

### Phase 1: Texture conversion tool (offline, runs on PC)

A script or small tool that:
1. Extracts TGA files from the 7z archive
2. Converts TGA to PNG (preserving alpha channel)
3. Optionally downscales to max 512x512 or 1024x1024 for mobile
4. Outputs flat directory of `{bitmapname}.png` files
5. These can be placed in the PhysFS search path (game data directory)

Could be a PowerShell + ImageMagick script, or a Python script with Pillow.

### Phase 2: Enable PNG loading on Android

1. Add libpng (or stb_image) to the Android NDK build
2. Set `PNG ON` in CMakeLists.txt
3. Add `HAVE_LIBPNG` define
4. Test with a few converted textures

### Phase 3: WAV sound loading (engine change)

1. Add WAV header parsing in the sound loading path
2. Search for `.wav` files in `Sounds/` directory as higher priority than `.raw`/`.r22`
3. Update `mixdigi_convert_sound()` to handle 16-bit source
4. Test with converted sound files

### Phase 4: Sound conversion tool (offline)

1. Extract WAV files from the 7z archive
2. Rename to match game sound names (should already match)
3. Place in `Sounds/` subdirectory in game data path
4. Verify all names match pig file sound table

### Phase 5: Launcher integration

1. Add "Install hires textures" option in launcher
2. Add "Install hires sounds" option
3. Handle 7z extraction and conversion on-device (or ship pre-converted)
4. Show disk space warning

---

## Risks and Open Questions

1. **Licensing**: What is the license on these d2x-xl texture/sound packs? They appear to be community-created replacements. Need to verify redistribution rights before bundling. Might need to point users to download from d2x-xl project page instead
2. **Visual inconsistency**: Not all textures have replacements. A level might show some walls at 512x512 and others at 64x64, creating a jarring visual mismatch. This is inherent to the mod
3. **Cockpit**: The hires-cockpit.jpg is a D2 cockpit overlay at higher resolution. Cockpit rendering may need separate handling from wall textures
4. **GLES 1.1 vs GLES 2.0**: GLES 1.1 requires power-of-2 textures. The 512x512 textures are power-of-2, but if we add non-power-of-2 sizes, we'd need GLES 2.0 or higher
5. **stb_image alternative**: If we go with stb_image.h instead of libpng, it handles PNG, TGA, and JPG in one header with zero dependencies. This would eliminate the need for offline TGA->PNG conversion entirely. However, stb_image PNG loading may be slower than libpng

---

## Recommended Order of Work

1. ~~Write TGA-to-PNG conversion script~~ -- DONE (`game_data/mods/convert_d2xxl_textures.ps1`)
2. ~~Enable PNG on Android (stb_image)~~ -- DONE (`pngfile_stb.c` + CMakeLists changes)
3. Test hires textures on emulator with a handful of converted textures
4. Add WAV sound loading to engine (both d1/ and d2/) -- currently using offline .r22 conversion
5. Test hires sounds on emulator
6. Build launcher UI for importing/managing these mod packs

# Plan: All-in-one DXA conversion, texture test, mod fixes

## Task 1: All-in-one mod conversion script
- Create `game_data/mods/d2x-xl/convert_all.ps1` that calls:
  - convert_d2xxl_textures.ps1 -Game d2 (512x512)
  - convert_d2xxl_textures.ps1 -Game d1 (512x512)
  - convert_d2xxl_textures.ps1 -Game d2 -MaxSize 256 (256x256 source)
  - convert_d2xxl_textures.ps1 -Game d1 -MaxSize 256 (256x256 source)
  - convert_d2xxl_sounds.ps1 -Game both
- Need to make texture script accept an -ArchivePath parameter so we can point at 256x256 archives
- Output filenames need to distinguish 256 vs 512

## Task 2: Texture introspection
- Add `hires_textures` section to game_introspect.cpp
- Count total GameBitmaps with gltexture loaded
- Count those with is_png==1 (hi-res replacements)
- Report max texture size seen among is_png textures
- Sample a known bitmap (e.g. index 0 or first loaded one) to verify

## Task 3: Update test_mod_loading.json5
- Add 256x256 texture DXA as a dep
- Add assertion on hires_textures count > 0
- Keep the sounds assertion

## Task 4: Fix "0 B" size in ModManager
- In load(), if sizeBytes==0 and file exists, read actual file size

## Task 5: Move D1 downloads to mods tab
- Remove d1xr-mac-demo-sounds.dxa and d1xr-hires.dxa from D1_FILES
- Add recommended mods concept to ModsSection
- Show download buttons for recommended mods that aren't installed

## Task 6: Build, lint, test

# Plan: Texture pack naming, 128px DXA, README in DXA, mine exit fix

## Task 1: Rename DXA files with resolution
- [x] Verify d2xxl-hires-textures-d2.dxa is 512px pack -> yes, confirmed
- [x] Rename to d2xxl-hires-textures-d2-512.dxa
- [x] Update convert_all.ps1 output list if needed
- [x] Check any references in SetupActivity.kt, game_data_index.txt etc
- Note: d1xr-hires.dxa is NOT a texture pack (PCX briefing screens/fonts), leave as-is

## Task 2: Create 128x128 downscaled DXAs
- [x] Keep TexSize source-selection model and use -TexSize 256 -MaxSize 128 for downscale
- [x] Add 128 generation to convert_all.ps1 loop
- [x] Run conversion for d1 and d2
- [x] Create README text for each game in the DXA
- [x] Test with game script to get % loaded
- [x] Add hires_pct to introspection, test > X%

## Task 3: Add README.md into DXA archives
- [x] Modify convert_d2xxl_textures.ps1 to accept -ReadmeText parameter
- [x] Add README.md entry to ZIP in Convert-GameTextures function
- [x] Update convert_all.ps1 to pass appropriate readme text per game
- [x] Rebuild all DXAs

## Task 4: Fix D2 mine exit sequence skip
- [x] Research endlevel sequence code in d2/
- [x] Find where the sequence is being skipped or auto-advanced
- [x] Fix the issue while keeping skip button functional

## Status
- Phase complete

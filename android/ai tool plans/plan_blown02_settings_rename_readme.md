# Plan: Blown02 investigation, settings tap fix, dxa rename, readme update

## Task 1: Investigate Blown02.tga [DONE]
- Blown02 exists as .ktx2 in all 6 current texture packs -- conversion succeeded
- No .tga entries found in any rebuilt .dxa file
- The old d2-64 pack has PNGs (not rebuilt this round, predates KTX2 migration)
- Conclusion: no issue to fix

## Task 2: Fix settings overlay single-tap [DONE]
- Bug: animateAdminTray(true) starts animation from 0f to 1f
- First animation frame emits value 0f, triggering `adminTrayOpen = false`
- Fix: guard close condition with `!open` so it only fires on close animation

## Task 3: Rename .dxa files [DONE]
- Old: d2xxl-hires-textures-{game}-{size}.dxa
- New: {game}-hires-{size}-textures-ktx2.dxa (e.g. d2-hires-256-textures-ktx2.dxa)
- Old: d2xxl-hires-sounds-{game}.dxa
- New: {game}-hires-sounds.dxa
- Updated: convert_d2xxl_textures.ps1, convert_all.ps1, convert_d2xxl_sounds.ps1
- Updated: test_mod_loading.json5, game_data/mods/README.md

## Task 4: Update README text for mobile KTX2 [DONE]
- Updated Get-ReadmeText in convert_all.ps1 to note mobile-specific ETC2/KTX2 encoding

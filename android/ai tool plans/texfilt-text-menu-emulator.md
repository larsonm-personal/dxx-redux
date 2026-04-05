# Texfilt Text Fix, Menu Resolution, Emulator Script

## Status: COMPLETE

## Issues
1. Translate run_emulator.sh to PowerShell with auto-rebuild
2. Selective filtering: add text/reticle to menu group (default off)
3. Text disappears when texfilt is cycled (mipmap on font textures)
4. Menu displays at different apparent resolutions (font scaling variance)

## Phase 1: Fix text disappearing (bug 3)
- Root cause: font textures get glGenerateMipmap when texfilt > 0
- Font textures are sparse RGBA -- mipmapping averages glyphs with transparent
  pixels, destroying alpha at lower mip levels -> text invisible
- Fix: add `flags` field to ogl_texture struct, store OGL_FLAG_NOCOLOR
- In ogl_loadtexture, skip mipmap generation for OGL_FLAG_NOCOLOR textures
- Files: d2/include/ogl_init.h, d1/include/ogl_init.h,
         d2/arch/ogl/ogl.c, d1/arch/ogl/ogl.c

## Phase 2: Selective filtering for text (issue 2)
- In ogl_bindbmtex, OGL_FLAG_NOCOLOR textures use MenuTexFilt regardless of
  render context (text is always in the "menu" filtering group)
- Rename launcher label: "Menus / briefings / videos / text / reticle"
- Files: d2/arch/ogl/ogl.c, d1/arch/ogl/ogl.c, GraphicsSettingsPage.kt

## Phase 3: Fix menu resolution variance (bug 4)
- gamefont_choose_game_font uses int scaling when TexFilt=0, float when >0
- Fix: always use integer scaling (consistent, crisp text)
- Files: d2/main/gamefont.c, d1/main/gamefont.c

## Phase 4: Translate run_emulator.sh to PowerShell
- Port all functionality: build, emulator launch, wait for boot, install,
  push game data, launch app, tail logcat
- Add auto-rebuild (check APK freshness like Run-TestMenu.ps1)
- File: android/Run-Emulator.ps1

## Phase 5: Build, lint, test

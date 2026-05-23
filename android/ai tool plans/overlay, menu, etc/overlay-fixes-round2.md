# Overlay Fixes Round 2

## Status: DONE

## Issues
1. Filter-by-context switch spacing too tight -- FIXED
2. Text disappears when texfilt first turned on -- FIXED (bidirectional filter restore)
3. Texture labels still dark in non-hires mode -- code is correct, could not reproduce
4. AF/AA not saving from overlay to launcher -- FIXED (root cause: cross-process SharedPrefs)

## Phase 1: Filter-by-context switch spacing -- DONE
- Added `padding(vertical = 4.dp)` to Switch rows (was 0.dp)
- Header spacer 1.dp -> 2.dp
- File: GraphicsSettingsPage.kt

## Phase 2: Selective filtering bidirectional restore -- DONE
- Root cause: `ogl_bindbmtex` set GL_NEAREST on texture objects but never restored
- Fix: bidirectional filter logic that explicitly sets filters for ALL contexts,
  checking has_mipmaps before setting mipmap filters
- Files: d2/arch/ogl/ogl.c, d1/arch/ogl/ogl.c

## Phase 3: Label colors -- INVESTIGATED, code correct
- g_font_rgb_override path verified correct in ogl_ubitmapm_cs
- GAME_FONT is monochrome (not FT_COLOR), so c >= 0 and override fires
- Color override sets (1,1,0) for yellow, (0,1,0) for green
- Code analysis shows override should produce bright colors
- Could not identify code-level cause of dark labels

## Phase 4: AF/AA save -- DONE (major fix)
- Root cause: AndroidManifest.xml has `android:process=":game"` on MainActivity
- SetupActivity (launcher) and MainActivity (game) run in SEPARATE PROCESSES
- SharedPreferences written by overlay in `:game` process invisible to launcher's cache
- Fix: moved AF/AA persistence from SharedPreferences to descent.cfg
  - Added AnisoLevel and MsaaLevel to Cfg struct (d1+d2 config.h)
  - Added read/write/defaults in config.c (d1+d2)
  - Added runtime sync to ogl_aniso_level/ogl_msaa_samples at end of ReadConfigFile
  - jni_main.c: nativeSetGraphicsOption also sets GameCfg.AnisoLevel/MsaaLevel
  - GraphicsSettingsPage.kt: reads/writes via readConfigValue/updateAllConfigFiles
  - VideoInfoOverlay.kt: removed settingsSaver calls (no longer needed)
  - MainActivity.kt: removed SharedPrefs reads for AF/AA at startup
  - Removed dead settingsSaver field and lambda

## Phase 5: Build, lint, test -- DONE
- Code quality linters pass (pre-existing issues only)
- Android build passes (assembleDebug)
- No compiler warnings in changed files
- Integration test test_launch_to_automap.json5 passes
- descent.cfg verification shows AnisoLevel=0 and MsaaLevel=0 written correctly

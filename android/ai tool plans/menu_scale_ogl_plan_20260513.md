# Android OpenGL Menu Scale Plan

## Goal

Bring the Android touch-friendly menu scale behavior from the software renderer to the OpenGL renderer without changing desktop behavior. Preserve the old working math first, then tune only after device testing.

## Status

Implementation continued May 13 and 14, 2026. First requirement: centralize the 85 percent target so tuning requires changing one spot only. The first OpenGL rendering strategy was rejected after device testing because D2 main menu content was visible twice, the scaled pass was not centered/scaled as intended, and the PCX/menu background was not included with the scaled menu content. The corrected path now builds a `BM_LINEAR` menu-source bitmap with background and contents, crops/scales it through the shared helper, and draws one centered result. Second device pass found remaining issues: first-draw palettes can be wrong, some scaled tap transforms drift in listbox/scroll cases, scaled window backgrounds need scaled borders, and fullscreen PCX-backed main menu should be text-only over an unmodified full-screen background. The latest follow-up switches scaled OGL uploads to the transient indexed blit path so they honor `gr_current_pal`, fixes the abort-game return path to restore menu screen state before `window_close(Game_wind)` longjmps out of the caller, then repairs the remaining startup-menu regressions by targeting scaled OGL blits at the real screen canvas, forcing fullscreen PCX backgrounds to re-upload with the active palette, restoring `MENU_PALETTE` before D2 framed startup menus remap their cached scores background, and explicitly calling `gr_palette_load(gr_palette)` after the startup main-menu `load_palette(MENU_PALETTE, 0, 1)` path so the live OGL upload palette no longer stays pinned to the fullscreen PCX colors. The next follow-up found that saving only `gr_palette` and `gr_fade_table` still was not enough on Android OGL: some indexed gameplay textures had already been uploaded while menu palettes owned `gr_palette`, so they stayed greenish until their GL cache entries were re-uploaded. The same pass also found why `Abort Game` still fell out to the launcher: the game close path assumed `show_menus()` could always resurrect a hidden stack, but some launch paths had no hidden menus left, so closing `Game_wind` left zero native windows. A subsequent regression report showed that smashing the full OGL texture list on game-window activation was too broad and could leave some rendered assets black or transparent. The narrower fix now invalidates only `GameBitmaps` textures and their paired masks when gameplay palette state is restored on Android, clears the merged-wall cache, and still routes game-window close back through a helper that restores hidden menus or recreates the native main menu when none exist. The latest launcher-side follow-up found a separate remaining exit path: the always-visible Android `EXIT` control and the matching admin-tray exit item were still hard-wired to `META_RETURN_TO_LAUNCHER`, so gameplay never reached the engine's own menu path. That control now branches on `nativeIsInGame()`, opening the in-engine game menu during live gameplay and keeping the direct launcher return only for non-game screens.

- [x] Find original software-renderer source changes and nearby plan files
- [x] Study current software math and touch remapping
- [x] Trace why the current code is disabled for OpenGL
- [x] Implement shared menu-scale helpers
- [x] Add OpenGL rendering path
- [x] Mirror coverage in D1 and D2
- [x] Add introspection
- [x] Add integration tests
- [x] Build and format
- [x] Run emulator menu-scale test
- [x] Replace framebuffer-copy/text-only OpenGL pass with an offscreen menu-source bitmap pass
- [x] Re-run D2 emulator visual and introspection checks after the rendering correction
- [x] Fix OGL `BM_LINEAR` offscreen primitive fallbacks needed by source composition
- [x] Fix first-draw palette setup for scaled menu sources
- [x] Fix scaled tap transforms for scrolled/listbox menus
- [x] Restore scaled menu borders on scaled window backgrounds
- [x] Special-case fullscreen PCX-backed menus as scaled text over unmodified background
- [x] Fix remaining first-frame palette split in scaled OGL uploads by using the transient `gr_current_pal` blit path
- [x] Fix the Android abort-game return path by restoring menu state before `window_close(Game_wind)`
- [x] Fix startup-menu OGL destination targeting and palette handoff for D2 main menu and pilot listbox
- [x] Fix D2 startup fullscreen PCX main-menu live palette handoff after `load_palette(MENU_PALETTE, 0, 1)`
- [x] Keep the full bordered menu box inside the shared `.85` target
- [x] Redo newmenu and listbox touch hit bounds to match drawn rows
- [x] Restore Abort Game to the hidden in-game menus instead of the launcher
- [x] Fix the Change Pilots selected-row listbox touch height so lower-half taps still hit the active pilot row
- [x] Fix Android scrolled `newmenu` tap drift by using one stored scroll line spacing for draw, hit, drag, and keyboard placement
- [x] Restore gameplay palette bytes and fade table when menus return to the active game window
- [x] Add a focused pause-menu return automation script for the gameplay or menu reactivation path
- [x] Invalidate cached Android OGL indexed textures when gameplay palette state is restored
- [x] Recreate the native main menu when `Abort Game` closes gameplay without a hidden menu stack to restore
- [x] Narrow Android gameplay palette invalidation to `GameBitmaps` textures instead of smashing the full OGL cache
- [x] Route the Android `EXIT` overlay and admin-tray item to the in-engine game menu during gameplay instead of always returning to the launcher

## Prior Work Found

No committed AI plan file was found for the March 6 software menu-scaling work. The exact source commits are the source of truth:

- `54c3ef85` - `menu scaling so touch works better`
  - Added `g_menu_scale_*` globals and touch remapping in `android/app/src/main/cpp/android_input.c`
  - Added the first Android post-draw menu scale-blit in `d2/main/newmenu.c`
- `6f5c0410` - `more expanded menus`
  - Expanded D2 coverage to listboxes and `kconfig` menus
- `b70c29c8` - `scale main menu (just text)`
  - Added masked scale for fullscreen PCX-backed menus so background art is not scaled
  - Added `gr_bitmap_scale_to_masked` in D2
- `5852384b` - `1280 menu scaling fix`
  - Added the important crop math: keep a fixed 15 px content pad because `BORDERX` and `BORDERY` grow with resolution while bitmap fonts do not
- `12ea45f7` - `start on "shift view when keyboard opens" work`
  - Added `PLAN.KEYBOARD_VIEWPORT_OFFSET.md`, later moved to `android/ai tool plans/networking/PLAN.KEYBOARD_VIEWPORT_OFFSET.md`
  - Documents that keyboard viewport offset depends on active scale-blit remapping
- `849d425d` - `fixes based on testing`
  - `android/ai tool plans/overlay, menu, etc/crash_investigation_menu_close.md` documents stale `g_menu_scale_active` after menu close

## Current Code Map

- `d2/main/newmenu.c`
  - `newmenu_draw` and `listbox_draw` compute shared Android scale rects and draw a single scaled offscreen source bitmap when scaling is active
  - PCX-backed menus draw the full page background first, then draw the scaled menu-region source over it
  - `EVENT_WINDOW_CLOSE` clears `g_menu_scale_active`
- `d2/main/kconfig.c`
  - `kconfig_draw` computes the shared kconfig scale rect and draws a scaled offscreen source bitmap when scaling is active
- `d1/main/newmenu.c`
  - Mirrors the D2 `newmenu` and `listbox` Android scale path
- `d1/main/kconfig.c`
  - Mirrors the D2 kconfig Android scale path
- `d1/2d` and `d2/2d`
  - OGL builds now let `BM_LINEAR` offscreen canvases use software paths for color font text, `gr_bitmap`, and `gr_urect`; this is required for deterministic menu-source bitmap composition
- `android/app/src/main/cpp/android_input.c`
  - Owns `g_menu_scale_active`, source/destination rects, touch remapping, and keyboard-field remapping
- `android/app/src/main/cpp/shared/game_introspect.cpp`
  - Exposes `keyboard_viewport.scale_blit_active` and the `menu_scale` active state, target fill, crop, source rect, destination rect, and computed scale
- `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`
  - Menus render in OpenGL menu context, without `ogl_start_frame` and `ogl_end_frame`
  - `gr_flip` applies keyboard viewport offset for menus after swap/clear so the next menu frame uses the correct viewport
- `d1/arch/ogl/gr.c` and `d2/arch/ogl/gr.c`
  - Android OpenGL screen canvas is `BM_OGL`, so reading `grd_curscreen->sc_canvas.cv_bitmap.bm_data` does not read the rendered menu pixels

## Math To Preserve

Keep the old 0.85 target first. The request says 90 percent or whatever it was, and the recovered code consistently uses 85 percent. Preserve that because the old math was the known-good result.

For `newmenu` and `listbox`:

1. Start from the menu background/source box
2. Compute crop values with fixed pad 15:
   - `crop_l = max(BORDERX - 15, 0)`
   - `crop_t = max(BORDERY - 15, 0)`
   - same for right/bottom
3. Compute cropped content size `cw/ch`
4. `scale = min(0.85f * SWIDTH / cw, 0.85f * SHEIGHT / ch)`
5. Scale only if `scale > 1.05f`
6. Clamp the source box to the screen, then recompute crops and destination rects
7. Publish cropped source rect and scaled destination rect for touch remapping

For `kconfig`, the current D2 software block still uses the earlier uncropped version plus a `2.5f` max-scale clamp. First pass should preserve its current behavior, then consider applying the same crop helper after visual testing.

## Why The Software Code Cannot Just Enable OGL

The existing blocks copy pixels from `grd_curscreen->sc_canvas.cv_bitmap.bm_data`, scale them with `gr_bitmap_scale_to`, and write them back through a sub-canvas. In OpenGL builds the screen canvas is `BM_OGL`; the real rendered menu is in GL state/framebuffers, not in that CPU buffer. Enabling the existing block under OGL would copy stale or meaningless data and would not reliably affect what is on screen.

Avoid a framebuffer-copy implementation as the first choice. It would be tempting for opaque menus, but Android MSAA can leave rendering in the MSAA FBO until `gr_flip`, and copying from that state risks GL errors or ordering bugs. A deterministic offscreen bitmap render followed by an OpenGL textured draw is safer and keeps the same source/destination math as the software path.

## Preferred Design

Add a small Android-only shared helper in `android/app/src/main/cpp/shared/`:

- `android_menu_scale.h`
- `android_menu_scale.c`

Responsibilities:

- Hold the single source of truth for target fill (`0.85f`), crop pad (`15`), scale threshold (`1.05f`), and optional kconfig max scale
- Compute source/crop/destination rectangles from a source box and screen size
- Publish and clear `g_menu_scale_*` state owned by `android_input.c`
- Provide a tiny debug/introspection accessor for active state, source rect, destination rect, and scale

Keep drawing code in the game modules or OGL modules because it depends on `grs_bitmap`, fonts, menu structures, and `ogl_ubitmapm_cs`.

## Implemented Rendering Approach

For software builds:

- Keep the current D2 behavior working
- Optionally replace duplicated math with the shared helper, but do not broaden the source diff unless it makes the OGL work simpler

For OpenGL builds, corrected after device testing:

- Do not use framebuffer-copy as the primary menu source
- Do not draw a normal-size menu and then add a second scaled text-only pass
- Build a `BM_LINEAR` offscreen source bitmap that contains the exact menu region to scale, including the PCX or window background and the menu text/items
- Crop that source with the shared crop math, scale it to the centered destination rectangle, then draw the already-scaled bitmap through the normal OGL bitmap path
- For PCX-backed main menus, draw the full PCX background as the page background, then draw the scaled menu-region bitmap over it so the menu content is visible only once
- Temp bitmap GL textures are freed with `gr_free_bitmap_data`

## Work Plan

## Follow-Up Work Plan

1. Palette correctness
  - Trace palette load/remap order in `nm_draw_background1`, `nm_draw_background`, and scaled bitmap upload
  - Ensure offscreen sources and final scaled bitmap uploads use the menu/PCX palette on the first draw, not a stale game palette

2. Tap mapping correctness
  - Trace Android mouse/touch remap in `android_input.c` against `newmenu_mouse` and `listbox_mouse`
  - Verify listbox scroll offset is not applied twice or omitted after destination-to-source remap
  - Keep publish/clear state tied to the same source rect actually displayed

3. Background/border rendering
  - Build the scaled menu source from the full menu background box, including the border, while preserving the existing crop math for sizing if needed
  - Avoid scaling only the interior crop for normal window-backed menus

4. Fullscreen PCX menu special case
  - Keep the full PCX background drawn unscaled
  - Draw a transparent text/items source over it at the scaled destination, without scaling the background crop
  - Keep `EVENT_NEWMENU_DRAW` callbacks that intentionally draw full-screen page content from being captured into the text-only source

5. Validation
  - Run scoped code quality with `android/run-code-quality.ps1 -Fix`
  - Rebuild Android debug with JDK 21
  - Run the D2 menu-scale automation and capture visual screenshots
  - Add/extend automation coverage for at least one scrolled menu if the existing script does not exercise the bad tap path

## Original Work Plan

1. Shared math and state helper
   - Add `android_menu_scale.h/c`
   - Add both files to D1 and D2 Android targets in `android/app/src/main/cpp/CMakeLists.txt`
   - Keep constants in one place and document that the value is intentionally 0.85 to match the recovered software behavior
   - Add `android_menu_scale_clear()` and use it instead of raw `g_menu_scale_active = 0` where practical

2. D2 OpenGL newmenu path
   - Refactor the existing `newmenu_draw` post-draw block into small Android helpers near the current code
   - Preserve the two cases: opaque menu box and fullscreen PCX background
  - Build a `BM_LINEAR` source bitmap for the menu region, including the PCX/window background and menu contents
   - Under `!defined(OGL)`, keep the existing software copy/scale path or route only the math through the helper
   - Verify touch remapping still uses the cropped source rect, not the uncropped box

3. D2 OpenGL listbox path
   - Apply the same shared rect math to `listbox_draw`
  - Build and scale an offscreen menu-source bitmap for OGL instead of reading screen `bm_data`
   - Publish/clear scale state every draw to avoid stale touch transforms

4. D2 OpenGL kconfig path
   - Start by preserving the current kconfig scale behavior: target 0.85, max clamp 2.5, uncropped source box
  - Build and scale an offscreen menu-source bitmap for OGL instead of reading screen `bm_data`
   - After visual testing, decide whether to switch kconfig to the new crop-pad helper too

5. D1 parity
   - Mirror the D2 Android scale coverage into `d1/main/newmenu.c` and `d1/main/kconfig.c`
   - Use the shared helper for math and state to avoid inventing a second D1 implementation
   - Keep D1/D2 local draw code small and style-matched rather than trying to deduplicate old menu internals

6. Introspection and tests
   - Extend `game_introspect.cpp` to expose `menu_scale`:
     - active
     - src x/y/w/h
     - dst x/y/w/h
     - computed scale or dst/src ratio
   - Add or extend an automation script under `android/game_scripts/` that opens representative menus at a high render resolution:
     - D2 main/new game menu
     - D2 listbox such as load game or mission select
     - D2 controls/kconfig menu
     - D1 main menu and one D1 controls/menu path
   - Assert through introspection that active scaled menus have destination height or width near the 0.85 screen target and that touch selection still maps to the expected item
   - Keep one manual device check for fullscreen PCX-backed menu text because the key risk is visual background distortion

7. Validation
   - Before format/build after any interruption, run `android/stop-stale-formatters.ps1` and kill stale formatters if needed
   - Run focused Android build for D1 and D2 OpenGL targets
   - Run the new integration test script on emulator with logcat cleared and output piped to `temp/`
  - Run `android/run-code-quality.ps1 -Fix` and wait for it to exit

## Validation Notes

- `android/run-code-quality.ps1 --fix` is not accepted by this script on this checkout; use `-Fix`
- 2026-05-13: `android/stop-stale-formatters.ps1` found no stale formatter tasks
- 2026-05-13: `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/cpp/shared/android_menu_scale.c','android/app/src/main/cpp/shared/android_menu_scale.h','android/app/src/main/cpp/shared/game_introspect.cpp','android/app/src/main/cpp/CMakeLists.txt')` passed
- 2026-05-14: `android/gradlew.bat assembleDebug` passed after the stable scroll-line-spacing fix in `d1/main/newmenu.c` and `d2/main/newmenu.c`
- 2026-05-14: `run-windows-build.ps1 -Target both` passed after the same `newmenu` fix
- 2026-05-14: `android/run-code-quality.ps1 -Fix -Paths @('d1/main/newmenu.c','d2/main/newmenu.c')` passed when run from the repo root; running it from `android/` with `../...` paths fell back to repo scope on this checkout
- 2026-05-14: `android/run_test.ps1 test_reticle_options_stage_d2.json5 -Game d2` passed and left D2 staged in the `Reticle Options` scroll box for follow-up touch checks
- 2026-05-14: `android/gradlew.bat assembleDebug` passed after the D1 and D2 gameplay palette save or restore fix in `d1/main/game.c` and `d2/main/game.c`
- 2026-05-14: `run-windows-build.ps1 -Target both` passed after the same gameplay palette handoff fix
- 2026-05-14: `android/run_test.ps1 -ScriptName test_pause_menu_return.json5 -Game d2` passed and returned to gameplay after opening and closing the in-game ESC menu
- 2026-05-14: `android/run-code-quality.ps1 -Fix -Paths @('d1/main/game.c','d2/main/game.c','android/game_scripts/test_pause_menu_return.json5')` passed
- 2026-05-14: `android/gradlew.bat assembleDebug` passed after adding the Android OGL texture-cache smash on gameplay palette restore and the restore-or-create game-menu fallback on close
- 2026-05-14: `run-windows-build.ps1 -Target both` passed after the same D1 or D2 mirrored menu and OGL changes
- 2026-05-14: `android/run_test.ps1 -ScriptName test_abort_game_to_main_menu_d2.json5 -Game d2` exited 0 after the restore-or-create menu fallback fix
- 2026-05-14: `android/run_test.ps1 -ScriptName test_pause_menu_return.json5 -Game d2` exited 0 after adding the Android OGL texture-cache invalidation on gameplay palette restore
- 2026-05-14: `android/run-code-quality.ps1 -Fix -Paths @('d1/main/menu.c','d1/main/menu.h','d1/main/game.c','d1/main/gamecntl.c','d1/include/ogl_init.h','d1/arch/ogl/ogl.c','d2/main/menu.c','d2/main/menu.h','d2/main/game.c','d2/main/gamecntl.c','d2/include/ogl_init.h','d2/arch/ogl/ogl.c')` passed
- 2026-05-14: narrowed the gameplay palette reactivation fix to free only `GameBitmaps` textures plus merged-wall cache entries, after a device report that the full OGL smash could leave some textures black or transparent
- 2026-05-14: `android/gradlew.bat assembleDebug` passed after routing the Android `EXIT` overlay and admin-tray control through a shared gameplay-aware handler in `MainActivity.kt`
- 2026-05-14: `get_errors` reported no Kotlin errors in `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` or `android/app/src/main/java/com/dxxredux/app/ExitButtonView.kt` after the same launcher-side exit change
- 2026-05-14: `android/run_test.ps1 -ScriptName test_pause_menu_return.json5 -Game d2` passed on the updated build, validating the in-engine game-menu path that the gameplay `EXIT` control now targets
- 2026-05-14: `android/gradlew.bat assembleDebug` passed after the narrower `GameBitmaps` invalidation change in `d1/d2/main/game.c` and `d1/d2/arch/ogl/ogl.c`
- 2026-05-14: `android/run_test.ps1 -ScriptName test_pause_menu_return.json5 -Game d2` exited 0 after the narrower invalidation change, and an emulator screenshot captured after the test did not show obvious black or transparent world textures in the basic game-return scene
- 2026-05-14: `run-windows-build.ps1 -Target both` passed after the same narrowed palette reactivation fix
- 2026-05-14: `android/run-code-quality.ps1 -Fix -Paths @('d1/include/ogl_init.h','d1/main/game.c','d1/arch/ogl/ogl.c','d2/include/ogl_init.h','d2/main/game.c','d2/arch/ogl/ogl.c')` passed
- 2026-05-13: `cd android; .\gradlew.bat assembleDebug` passed with `JAVA_HOME=C:\local\jdk-21`
- 2026-05-13: `android/run_test.ps1 test_menu_scale_d2.json5 -Game d2 -Install -TimeoutSeconds 240` passed
- 2026-05-13: corrected OGL path after visual failure by replacing framebuffer/text-only behavior with offscreen menu-source bitmaps
- 2026-05-13: added OGL `BM_LINEAR` fallback fixes in D1/D2 `2d` primitives so source bitmaps receive real text, bitmap, and rectangle pixels
- 2026-05-13: `android/run-code-quality.ps1 -Fix -Paths ...` passed after final correction
- 2026-05-13: `cd android; .\gradlew.bat assembleDebug` passed after final correction with `JAVA_HOME=C:\local\jdk-21`
- 2026-05-13: `android/run_test.ps1 -ScriptName test_menu_scale_d2.json5 -Game d2 -Install -TimeoutSeconds 240` passed after final correction
- 2026-05-13: emulator screenshot `temp/menu_scale_d2_textfixed.png` visually confirmed one centered scaled D2 main menu with scaled menu-region background and no duplicate original-size menu
- 2026-05-13: second device-feedback pass changed `android_menu_scale_compute_cropped()` to keep crop math for sizing but publish/draw the full source box, so borders and touch remap use the same coordinate space
- 2026-05-13: second pass draws PCX-backed menus as scaled transparent text/items over the unscaled fullscreen PCX background
- 2026-05-13: second pass loads the active palette before remapping the cached menu frame PCX, fixing first-use remap against stale palettes
- 2026-05-13: `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/cpp/shared/android_menu_scale.c','d1/main/newmenu.c','d2/main/newmenu.c')` passed
- 2026-05-13: `cd android; .\gradlew.bat assembleDebug` passed with `JAVA_HOME=C:\local\jdk-21`
- 2026-05-13: `android/run_test.ps1 -ScriptName test_menu_scale_d2.json5 -Game d2 -Install -TimeoutSeconds 240` passed; final introspection showed `menu_scale.src` equal to the full menu box and active scale state
- 2026-05-13: emulator screenshot `temp/menu_scale_d2_second_pass.png` visually confirmed enlarged main-menu text over the unmodified fullscreen PCX background
- 2026-05-13: switched `android_menu_scale_blit_bitmap()` to the transient OGL indexed blit path so scaled menu uploads use `gr_current_pal`; `android/run_test.ps1 -ScriptName test_menu_scale_d2.json5 -Game d2 -Install -TimeoutSeconds 240` still passed afterward
- 2026-05-13: added `android/game_scripts/test_abort_game_to_main_menu_d2.json5`; the first run exposed that `Abort Game` left `screen_mode = game` and `current_level_num = 1` after the main menu returned
- 2026-05-13: fixed the D1/D2 `HandleSystemKey(KEY_ESC)` abort branch to call `set_screen_mode(SCREEN_MENU)` and clear the current-level markers before `window_close(Game_wind)`; the abort-game automation then passed and final introspection showed `screen_mode = menu`, `current_level_num = 0`, `player = null`, and `window_count = 1`
- 2026-05-13: `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/cpp/shared/android_menu_scale.c','d2/main/newmenu.c','d1/main/newmenu.c','d2/main/gamecntl.c','d1/main/gamecntl.c','android/game_scripts/test_abort_game_to_main_menu_d2.json5')` passed when rerun from the repo root
- 2026-05-14: fixed the shared scaled OGL blit helper to target `grd_curscreen->sc_canvas` instead of whichever sub-canvas was current during startup menu draws; rebuilt and installed the debug APK, then reran `android/run_test.ps1 -ScriptName test_menu_scale_d2.json5 -Game d2` and visually confirmed the installed-build D2 main menu screenshot in `temp/mainmenu_startup_fix_installed.png`
- 2026-05-14: forced cached fullscreen PCX menu backgrounds to drop their OGL textures before redraw so startup main menus re-upload under the active palette
- 2026-05-14: restored `MENU_PALETTE` before D2 framed startup menus draw `nm_background`; rebuilt and installed the debug APK, then reran a temporary pilot-listbox probe and visually confirmed the installed-build D2 pilot selector screenshot in `temp/pilot_listbox_startup_fix_installed.png`

## Risks And Watchpoints

- Do not read `bm_data` from a `BM_OGL` screen canvas as if it were framebuffer pixels
- Do not scale the fullscreen PCX background art; only scale text/menu content over it
- Keep `g_ogl_render_context` in menu context while drawing scaled menu textures so menu texture filtering preferences apply
- Clear scale state on every close path and on draws that decide not to scale
- Avoid broad D1/D2 menu refactors; only factor small local drawing helpers where the OGL offscreen path needs them
- Watch transient bitmap texture lifetime; `gr_free_bitmap_data` must run after any temp bitmap was drawn through OGL
- Verify keyboard viewport offset and touch remapping together, because `android_get_keyboard_y_offset` remaps field Y through the scale transform

## Open Decisions

- Keep target fill at 0.85 for the first implementation, then tune to 0.90 only if device testing shows the old value is still too small
- Decide whether kconfig should keep its older max-scale clamp or move to the newer crop-pad math after the first OGL visual pass
- Decide whether to leave software renderer code unchanged for minimum risk or to route its math through the new helper during the same tranche
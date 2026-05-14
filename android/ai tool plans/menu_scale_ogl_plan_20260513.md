# Android OpenGL Menu Scale Plan

## Goal

Bring the Android touch-friendly menu scale behavior from the software renderer to the OpenGL renderer without changing desktop behavior. Preserve the old working math first, then tune only after device testing.

## Status

Implementation started May 13, 2026. First requirement: centralize the 85 percent target so tuning requires changing one spot only. Core implementation is in place and build-validated; emulator integration coverage is still pending.

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
  - `newmenu_draw` has the main Android scale-blit block behind `#if defined(ANDROID) && !defined(OGL)`
  - `listbox_draw` has a second Android scale-blit block behind the same guard
  - `EVENT_WINDOW_CLOSE` clears `g_menu_scale_active`
- `d2/main/kconfig.c`
  - `kconfig_draw` has a software-only Android scale-blit block for key, mouse, joystick, and weapon binding menus
- `d1/main/newmenu.c`
  - Has Android keyboard and close-state wiring, but not the current D2 scale-blit blocks
- `d1/main/kconfig.c`
  - Has no scale-blit block
- `android/app/src/main/cpp/android_input.c`
  - Owns `g_menu_scale_active`, source/destination rects, touch remapping, and keyboard-field remapping
- `android/app/src/main/cpp/shared/game_introspect.cpp`
  - Exposes `keyboard_viewport.scale_blit_active`, but not the rects or target scale yet
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

For OpenGL builds:

- Opaque menus/listboxes/kconfig menus use `ogl_copy_screen_region_scaled()` to copy the already-drawn framebuffer region into a temporary GL texture and draw it scaled
- This preserves callback/custom draw output without re-running menu callbacks
- Fullscreen PCX-backed menus keep a temporary `BM_LINEAR` transparent text bitmap and draw it with `ogl_ubitmapm_cs(dx, dy, dw, dh, &tmp, -1, F1_0)`
- Temp bitmap GL textures are freed with `gr_free_bitmap_data`
- For fullscreen PCX-backed menus, keep the existing special rule:
  - Redraw the PCX background at normal size to erase the small text
  - Render only menu text/items to a transparent temp bitmap
  - Mark/use transparency so color 255 stays transparent when uploaded to OGL
  - Draw only the scaled text over the unscaled PCX art
- For opaque-box menus, include the menu box/background in the temp bitmap so the scaled result covers the original unscaled menu

## Work Plan

1. Shared math and state helper
   - Add `android_menu_scale.h/c`
   - Add both files to D1 and D2 Android targets in `android/app/src/main/cpp/CMakeLists.txt`
   - Keep constants in one place and document that the value is intentionally 0.85 to match the recovered software behavior
   - Add `android_menu_scale_clear()` and use it instead of raw `g_menu_scale_active = 0` where practical

2. D2 OpenGL newmenu path
   - Refactor the existing `newmenu_draw` post-draw block into small Android helpers near the current code
   - Preserve the two cases: opaque menu box and fullscreen PCX background
  - Under `defined(OGL)`, use framebuffer copy for opaque menus and transparent offscreen bitmap draw for fullscreen PCX-backed menus
   - Under `!defined(OGL)`, keep the existing software copy/scale path or route only the math through the helper
   - Verify touch remapping still uses the cropped source rect, not the uncropped box

3. D2 OpenGL listbox path
   - Apply the same shared rect math to `listbox_draw`
  - Copy and scale the already-rendered framebuffer region for OGL instead of reading screen `bm_data`
   - Publish/clear scale state every draw to avoid stale touch transforms

4. D2 OpenGL kconfig path
   - Start by preserving the current kconfig scale behavior: target 0.85, max clamp 2.5, uncropped source box
  - Copy and scale the already-rendered framebuffer region for OGL instead of reading screen `bm_data`
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
- 2026-05-13: `cd android; .\gradlew.bat assembleDebug` passed with `JAVA_HOME=C:\local\jdk-21`
- 2026-05-13: `android/run_test.ps1 test_menu_scale_d2.json5 -Game d2 -Install -TimeoutSeconds 240` passed
   - Run the normal CMake/windows build check if the touched files affect host builds outside Android guards

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
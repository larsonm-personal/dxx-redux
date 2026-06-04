# Controls Editor Device Logging Plan

Status: physical Android viewport regression repaired and validated on emulator

## Problem
- On real devices, controls editor pages still show tiny, garbled, unreadable text.
- Emulator automation confirms the enlarged/scrolled kconfig path is active, so the failure likely depends on real device render scale, font scale, bitmap contents, or OpenGL upload/blit behavior.

## Plan
1. Inspect existing kconfig and shared menu-scale diagnostics to see what is already logged. Done.
2. Add targeted device logs for the offscreen text bitmap contents and final OpenGL blit tiles. Done.
3. Prefer concise, bounded logs under Game Logs so they can be exported from the launcher. Done.
4. Run code quality and a debug build after edits. Done.

## Initial Theories
- The phone may be rendering the kconfig page at a much larger logical resolution than the emulator, causing different `FNTScaleX/Y`, source box, and render target sizes.
- The text may be damaged before the final blit, which would show up as low pixel coverage or unexpected color-index distribution in the offscreen bitmap.
- The text may be clean in the offscreen bitmap but damaged during GPU upload/blit, which would show up as large or oddly sliced OpenGL tiles.
- Device logs showed the offscreen bitmap is clean and the tile upload is within limits. The next suspect is GL presentation state: the scaled menu blit can inherit a stale viewport/FBO state, especially when MSAA is active in-level.

## Added Logs
- `[kconfig-scale]`: screen, source/destination/render sizes, scale, base font scale, font dimensions, scroll, item count, and original window canvas.
- `[kconfig-drawstate]`: scaled screen/font state used while drawing into the offscreen bitmap.
- `[kconfig-bitmap]`: visible offscreen bitmap area, FNV hash, counts for expected text/line colors, and top palette colors.
- `[menu-scale-blit]`: final OpenGL blit source/destination geometry, tile count, first tile size, power-of-two upload size, and texture limit.

## Verification
- `android\run-code-quality.ps1 -Fix` passed for the touched C files and this plan.
- `android\gradlew.bat -p android :app:assembleDebug` passed with JDK 21.
- Installed emulator automation `test_kconfig_keyboard_stage_d2.json5` passed and showed the new diagnostics in the introspection console.
- After the overlay-prep fix, reran `android\run-code-quality.ps1 -Fix`, rebuilt `:app:assembleDebug`, and reran installed emulator automation `test_kconfig_keyboard_stage_d2.json5`.
- The final automation pass produced `[menu-scale-prepare] source=menu-scale-region ... viewport=(0,0 960x540)` in the introspection console before `[menu-scale-blit]`, confirming the controls editor path now prepares full-screen OGL state before the region blit.

## Device Log Follow-up
- The phone reported kconfig `render=994x593`, `scaled_fnt=2.97x2.97`, and expected text-color pixels in the offscreen bitmap, so the intermediate text render is not the failing step.
- The final blit uses one 994x459 tile with a 1024x512 texture under the 2048 texture cap, so oversized texture slicing is also unlikely.
- Added an OGL helper that prepares scaled menu overlay blits by resolving MSAA when safe, binding the correct framebuffer, forcing a full-screen 2D viewport/cache, and logging the resulting framebuffer/viewport state.
- The new `[menu-scale-prepare]` log includes previous `last_width/last_height`, MSAA bound/depth state, final framebuffer binding, and viewport.
- Generic scaled menus and kconfig region blits have separate diagnostic counters so earlier option menus do not exhaust the logs before the controls editor appears.

## Second Device Log Follow-up
- The controls editor path is active on device: kconfig reports `screen=1170x540`, `dst=(88,40 994x459)`, `render=994x593`, `scale=1.48`, and `scaled_fnt=2.97x2.97`.
- `[kconfig-bitmap]` reports non-empty text, line, selection, box, and yellow color counts, so the software-side bitmap has expected controls-editor pixels before OpenGL upload.
- `[menu-scale-prepare] source=menu-scale-region` reports framebuffer 0 and viewport `1170x540`, so the controls editor region blit is not obviously using stale MSAA, framebuffer, or viewport state.
- `[menu-scale-blit]` reports the expected `994x459` copy into the `1170x540` render target, using one `1024` tile, so the GL upload geometry also looks sane.
- A touch log briefly reports `screen=1737x802`, matching the temporary scaled-draw screen size. That suggests the offscreen draw's global screen/FNTScale mutation can leak to asynchronous Android input and should be hardened, but it does not by itself explain unchanged visual text.
- The new front-runner is final Android presentation: controls text is still rasterized into a `1170x540` game buffer before the phone scales that buffer to the physical surface.

## Next Work
1. Pass/log the Java `SurfaceView` size into native code so Android OGL setup can compare physical surface size against the game render buffer.
2. If the game buffer is much smaller than the physical surface, prefer a higher Android render size for the EGL buffer so menus are not stretched from `1170x540`.
3. Preserve the existing high-res kconfig offscreen bitmap and region blit changes, then retest with device logs looking for the game render size and surface size to converge.
4. Separately harden the temporary scaled draw state so Android touch/event mapping cannot observe the scaled offscreen screen dimensions.

## Surface/Resolution Implementation
- Added a `nativeSetSurfaceSize(width, height)` bridge from `MainActivity` so native diagnostics can distinguish Java `SurfaceView` size from the `ANativeWindow` buffer size after `setBuffersGeometry`.
- Updated native display-size accessors to prefer the Java view size, so video diagnostics and introspection report physical view size instead of the low-resolution game buffer size.
- Added `[android-egl]` logs for initial EGL surface creation and surface recreation, including game render size, Java view size, native window size before/after geometry, and return code.
- Changed first-launch Android `descent.cfg` defaults from half-screen render resolution to full-screen render resolution.
- Added a guarded migration from the old half-screen default to full-screen when `graphics_settings_generation` is still zero, so explicit render-resolution choices are not repeatedly overridden.
- Tightened the migration so it only fires when the actual config file still contains the old half-screen default, avoiding stale preference values overriding explicit test/user configs.
- Updated the controls-editor staging script to force `1280x720`, preserving coverage for the scaled/scrollable kconfig path now that the app default is full resolution.

## Third Device Log Follow-up
- New phone logs from build `15017` show `game=1170x540 view=2340x1080 win_before=2340x1080 win_after=2340x1080`, so the Java surface-size bridge is working and the physical device view is available.
- The same logs still show `[menu-scale-prepare] ... viewport=(0,0 1170x540) screen=1170x540`, proving the Android OGL viewport is still limited to the low logical render size.
- The controls-editor software bitmap remains healthy (`render=994x593`, `scaled_fnt=2.97x2.97`, expected text pixels), so the next fix should target final GL presentation rather than kconfig text drawing.
- Implement a D1/D2 Android OGL viewport helper that scales `glViewport` and keyboard-gap scissor calls to the physical surface while preserving `last_width/last_height` as logical dimensions for existing coordinate math.
- Size Android MSAA framebuffer objects to the physical surface too, otherwise a physical viewport would be clipped by a logical-sized offscreen framebuffer.
- After validation, expected device logs should show a physical viewport near `2340x1080` with `screen=1170x540`, meaning game/menu coordinates remain logical but rasterization reaches native phone pixels.

## Physical Viewport Implementation
- Replaced the Android `OGL_VIEWPORT` macro body in D1/D2 with an `ogl_android_viewport()` helper.
- The helper keeps `last_width` and `last_height` at logical game/menu dimensions, but scales the actual `glViewport` rectangle to the Java `SurfaceView` size reported by native surface tracking.
- Updated Android keyboard-gap scissor clearing to use the same physical scaling.
- Updated Android MSAA FBO creation and resolve to use the physical surface size, so a physical viewport is not clipped by a logical-sized offscreen framebuffer.
- Updated overlay blit preparation and `gr_flip()` menu viewport setup to call the same helper instead of raw logical `glViewport` calls.

## Physical Viewport Verification
- `android\run-code-quality.ps1 -Fix` passed.
- `.\android\gradlew.bat -p android :app:assembleDebug` passed with JDK 21.
- First emulator test rerun still showed `viewport=(0,0 1280x720)` because the emulator had an old installed APK (`versionCode=15010`, last updated 15:42).
- After installing the freshly built debug APK, `test_kconfig_keyboard_stage_d2.json5` passed.
- The final introspection console showed `[menu-scale-prepare] ... viewport=(0,0 1920x1080) screen=1280x720`, confirming the controls-editor render path now uses the physical surface viewport while retaining logical menu coordinates.

## Physical Viewport Regression
- Device build `15020` regressed the in-level pause menu: the menu is drawn high/right and mostly offscreen.
- Logs confirm the physical viewport patch is active on device: `[menu-scale-prepare] ... viewport=(0,0 2340x1080) screen=1170x540`.
- The affected pause menu is the generic scaled menu path (`source=menu-scale`, not `menu-scale-region`), so applying a physical viewport globally is too broad.
- Touch logs also show `screen=3017x1392` during a tap, matching leaked temporary scaled draw state. That needs hardening separately because async input can observe offscreen draw dimensions.
- Next fix: stop scaling the global Android OGL viewport to physical pixels for ordinary/game menus, restoring centered logical presentation. Keep the kconfig enlarged text and scrolling work intact, and prefer high-resolution fixes inside the controls-editor region/offscreen path rather than changing the whole viewport transform.

## Regression Repair
- Changed the Android OGL viewport helper back to the drawable/game-buffer size instead of the Java `SurfaceView` size.
- Kept the centralized viewport helper and overlay preparation path, but its drawable-size helper now intentionally returns logical game dimensions because `setBuffersGeometry` keeps the EGL drawable at the game resolution.
- MSAA FBO sizing, resolve, framebuffer sampling, keyboard-gap scissor, and overlay preparation now all use that same drawable size again.
- Updated Android touch mapping to use the stable screen canvas bitmap size instead of `grd_curscreen->sc_w/sc_h`, so temporary scaled offscreen draw state cannot make input see impossible screen sizes like `3017x1392`.
- Expected device logs after this repair: generic pause menus should return to `viewport=(0,0 1170x540) screen=1170x540`, while touch logs should also report `screen=1170x540` during scaled-menu taps.

## Regression Repair Verification
- `android\run-code-quality.ps1 -Fix` passed.
- `.\android\gradlew.bat -p android :app:assembleDebug` passed with JDK 21 and generated build info stamped `2026-06-03 18:56 PDT`.
- Added `test_pause_menu_viewport_d2.json5` to cover the generic in-level pause menu path that regressed on device.
- Installed the fresh debug APK on the emulator and reran `test_pause_menu_viewport_d2.json5`; it passed and its final introspection console showed `source=menu-scale` logging `viewport=(0,0 1280x720) screen=1280x720`, confirming the generic pause menu no longer uses the physical `1920x1080` viewport on emulator.
- Reran `test_kconfig_keyboard_stage_d2.json5` against the same fresh install; it passed with controls-editor `source=menu-scale-region` logging `viewport=(0,0 1280x720) screen=1280x720`.

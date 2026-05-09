# Plan: Fix Android menu and loading texture filtering regression

## Goal
Find and fix why main title, loading, and top-level menu textures still render filtered on regular Android when `MenuTexFilt` is off, while the in-game menu path already honors the setting.

## Investigation Status
- [x] Trace the menu and loading draw path to the GL bind point
- [x] Compare the top-level menu path with the already-fixed in-game menu path
- [x] Identify one falsifiable root-cause hypothesis and one cheap discriminating check

## Key Files And Functions
- `d2/arch/ogl/ogl.c` and `d1/arch/ogl/ogl.c`
  - `ogl_bindbmtex()`
  - `ogl_loadtexture()`
  - `ogl_loadbmtexture_f()`
  - `gr_flip()`
- `d2/2d/bitblt.c` and `d1/2d/bitblt.c`
  - `show_fullscr()`
- `d2/main/newmenu.c` and `d1/main/newmenu.c`
  - `nm_draw_background1()`
  - `nm_draw_background()`
  - `EVENT_WINDOW_DRAW` handlers that wrap `newmenu_draw()` and `listbox_draw()`
- `d2/main/gamerend.c` and `d1/main/gamerend.c`
  - `show_boxed_message()`
- `d2/main/titles.c` and `d1/main/titles.c`
  - title screen `EVENT_WINDOW_DRAW` path that calls `show_fullscr()`
- `d2/main/gameseq.c` and `d1/main/gameseq.c`
  - `show_boxed_message(TXT_LOADING, 0)` level-load path

## Current Hypothesis
- `g_ogl_render_context` already reaches menu context for normal `newmenu` draws and loading boxes
- `ogl_bindbmtex()` only applies the Android bind-time filter override to:
  - font textures via `OGL_FLAG_NOCOLOR`
  - non-font textures that already have `has_mipmaps`
- Android ETC2/KTX uploads in `ogl_loadbmtexture_f()` set `GL_LINEAR` or `GL_NEAREST` directly, clamp `GL_TEXTURE_MAX_LEVEL` to 0, and do not set `has_mipmaps`
- Fullscreen title, menu, or loading art that lands on this no-mipmap path can therefore ignore `MenuTexFilt` at bind time and stay linear even in menu context
- Secondary check: `titles.c` draws `show_fullscr()` without its own explicit `g_ogl_render_context = 0` guard, so verify that no pre-flip path can leave it in the wrong context

## Cheap Discriminating Checks
- [x] Static check: confirm `ogl_bindbmtex()` has no non-font, no-mipmap branch for Android selective filtering
- [x] Static check: confirm the Android ETC2 upload path leaves `has_mipmaps == 0` and sets filter state at load time
- [ ] Runtime check: add temporary logging for menu-context binds to capture bitmap name, `has_mipmaps`, chosen filter, and upload source while reproducing the issue on main title, menu, and loading screens

## Phase 1: Prove The Affected Texture Class
- [ ] Add narrow Android-only bind logging in `ogl_bindbmtex()` or the shared texture debug helper for fullscreen menu draws
- [ ] Reproduce on regular Android with `MenuTexFilt=0`
- [ ] Confirm whether the affected assets are ETC2/KTX, PNG, or stock bitmap uploads

## Phase 2: Fix The Bind-Time Override
- [x] Extend `ogl_bindbmtex()` in both D1 and D2 to handle non-font textures without mipmaps
- [x] When menu filtering is off in menu context, force `GL_NEAREST`
- [x] When menu filtering is on in menu context, restore the correct non-mipmap filtered state with `GL_LINEAR`
- [x] Keep the existing HUD and mipmapped-world logic unchanged unless the runtime check disproves the hypothesis

## Phase 3: Add The Title-Screen Guard If Needed
- [ ] If runtime logging shows title or fullscreen paths entering with a non-menu context, wrap the relevant `titles.c` fullscreen draw with a saved and restored menu context in both D1 and D2
- [ ] Keep this as a separate minimal edit so the primary fix remains local to the texture bind path

## Phase 4: Validation
- [x] Run a focused Windows host build for the touched game target(s)
- [ ] Run `android\stop-stale-formatters.ps1` and kill stale tasks if needed
- [x] Run `android\run-code-quality.ps1 --fix`
- [x] Rebuild the Android debug target
- [ ] Verify on device or emulator that:
  - main title screens are crisp with `MenuTexFilt=0`
  - main menus and their backgrounds are crisp with `MenuTexFilt=0`
  - the loading box and background are crisp with `MenuTexFilt=0`
  - the existing in-game menu behavior still matches the current fixed path
  - enabling menu filtering restores linear filtering only for the intended menu group

## Notes
- The likely root owner is `ogl_bindbmtex()`, not the software `android_surface_blit()` path
- The first implementation should stay local to `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c` unless the secondary title-screen context check proves necessary
- Implemented fix: add a non-font, no-mipmap Android branch in `ogl_bindbmtex()` so menu and loading art uploaded without mipmaps can still be forced to `GL_NEAREST` in menu or HUD context and restored to `GL_LINEAR` when filtering is enabled
- Validation: bounded host builds passed for D1 and D2 via `run-windows-build.ps1`, and bounded Android native builds passed for `:app:buildCMakeDebug[arm64-v8a]` and `:app:buildCMakeDebug[arm64-v8a]-2`
- `android\run-code-quality.ps1 -Fix` was rerun under `pwsh` with timeouts after the first host mismatch; the helper currently scopes clang-format to `android/app/src/main/cpp`, so it does not materially format the touched `d1/` and `d2/` engine files
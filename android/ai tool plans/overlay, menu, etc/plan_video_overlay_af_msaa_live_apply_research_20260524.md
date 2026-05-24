# Video Overlay AF/MSAA Live Apply Research - 2026-05-24

## Goal
- Determine why video overlay changes for anisotropic filtering and MSAA no longer apply live while texture filtering and brightness still do
- Review current code paths and historical plan notes for prior fixes and regressions
- Identify the smallest safe implementation or experiments needed to restore live behavior

## Research Steps
- [x] Review historical plan files related to video overlay graphics settings, texture filtering, AF/MSAA persistence, brightness, and texture reloads
- [x] Inspect current Kotlin overlay flow and JNI/native option setter paths for tex_filt, anisotropy, MSAA, and brightness
- [x] Inspect D1/D2 OpenGL apply paths for runtime texture filtering, anisotropy, framebuffer/MSAA setup, and any pending flags
- [x] Compare live-apply behavior across settings and identify the likely regression point
- [x] Define experiments or implementation steps, including texture reload or GL surface recreation if needed

## Findings
- Historical sequence:
	- `graphics-page-msaa-af-persist.md` added AF/MSAA controls and startup application
	- `overlay-launcher-graphics-settings.md` added the live TexFilt plan and recorded the original TexFilt pending-value overwrite bug
	- `overlay-fixes-and-selective-filtering.md` documented the TexFilt resurrection: `g_texfilt_level = GameCfg.TexFilt` had been overwriting the JNI-requested value before the pending block used it
	- `plan_controls_overlay_graphics_followups_20260522.md` identified the pause/menu issue: AF/MSAA pending flags were only handled in `ogl_start_frame()`, while menus can render through `gr_flip()` without a 3D start-frame path
	- Commit `8c9d4398` implemented the follow-up by extracting `ogl_android_apply_pending_runtime_options()` and calling it from both `ogl_start_frame()` and `gr_flip()` in D1 and D2
- Current Kotlin/JNI plumbing is intact:
	- `VideoInfoOverlay.kt` sends `aniso_level`, `msaa_level`, and `tex_filt`
	- `MainActivity.nativeSetGraphicsOption()` routes all of them through `android_graphics_set_option()`
	- `android_graphics_options.c` updates `GameCfg`, updates the live OGL globals, sets pending flags for AF/MSAA/TexFilt, and persists config
	- Brightness differs because `gamma_level` calls `gr_palette_set_gamma()` immediately and does not depend on GL texture or FBO state
- Current renderer behavior explains the split:
	- TexFilt live apply is strong: `android_ogl_apply_texfilt_all()` updates `GL_TEXTURE_MIN_FILTER` and `GL_TEXTURE_MAG_FILTER` on every loaded non-font texture in place
	- AF live apply is weaker: the AF pending block generates mipmaps for loaded non-font textures and applies `GL_TEXTURE_MAX_ANISOTROPY_EXT`, but it does not force mipmapped min filters on already-loaded textures when `GameCfg.TexFilt == 0`
	- `ogl_android_effective_texfilt()` upgrades texture loads to trilinear while AF is on, but it is used at texture creation time, not consistently for already-loaded textures at bind time or AF pending time
	- `ogl_bindbmtex()` only runs its selective filtering restore when `GameCfg.TexFilt > 0`, so with TexFilt off and AF on it can leave an existing world texture at `GL_NEAREST` even after mipmaps and AF parameters exist
	- MSAA live apply is limited by design: the pending block destroys the old MSAA FBO, but the new FBO is only created and bound in the next `ogl_start_frame()` 3D pass. Menu-only paused frames can consume the setting without antialiasing an already-rendered paused frame

## Experiments To Run Next
- Add temporary debug-log or video-stats fields for `g_aniso_pending_apply`, `g_msaa_pending_apply`, `ogl_msaa_samples`, whether the MSAA FBO is allocated, `g_msaa_fbo_bound`, `g_msaa_frame_depth`, and a counter for pending applies serviced from `gr_flip()` vs `ogl_start_frame()`
- On device, pause in-level, open the overlay, change AF and MSAA, and confirm whether pending flags are consumed immediately in `gr_flip()` while still paused
- AF experiment 1: change D1/D2 `ogl_bindbmtex()` to branch on `ogl_android_effective_texfilt(GameCfg.TexFilt) > 0` instead of `GameCfg.TexFilt > 0`, and use that effective level for min-filter selection. This should let AF force mipmapped filtering for world draws even when global TexFilt is off, while still letting menu/HUD contexts force nearest
- AF experiment 2: when AF changes, reuse or extend the TexFilt all-textures helper so already-loaded non-font textures get min/mag filters matching the effective filtering level. If AF is turned off and TexFilt is off, restore nearest. If this is not sufficient, then flush/reload non-font world textures as the heavier fallback
- MSAA experiment 1: add debug proof that changing MSAA while paused only destroys the current FBO and does not create/bind a new one until the next 3D `ogl_start_frame()`
- MSAA experiment 2: if visible paused preview is required, find the least invasive way to force one paused 3D redraw after changing MSAA. Without a new 3D pass, MSAA cannot change pixels that are already in the default framebuffer

## Implementation Before Device Test
- [x] Use `DLOG_GRAPHICS` for exportable AF/MSAA live-apply logs
- [x] Mirror D1/D2 bind-path changes so AF uses effective texture filtering when TexFilt is off
- [x] Add AF pending-apply filter updates for already-loaded textures, including restoring nearest when AF turns off and TexFilt is off
- [x] Add exportable logs for native option requests, AF pending apply, TexFilt pending apply, MSAA pending apply, MSAA FBO destroy/create, and anisotropy parameter counts
- [x] Build/format validation before live device testing

## Implementation Result
- D1/D2 `ogl_bindbmtex()` now uses `ogl_android_effective_texfilt(GameCfg.TexFilt)` for Android selective filtering, so AF can force mipmapped filtering even when the visible TexFilt setting is off
- D1/D2 pending AF apply now updates loaded non-font texture min/mag filters in place and generates missing mipmaps as needed; turning AF off with TexFilt off restores nearest filters
- TexFilt pending apply now reapplies the AF-effective filter state when AF is enabled, avoiding the old path where setting TexFilt off could leave AF-backed textures at nearest
- Exportable graphics logs use `DLOG_GRAPHICS` and include:
	- graphics option request lines from `android_graphics_options.c`
	- `graphics apply[start_frame]` / `graphics apply[gr_flip]` pending consume lines
	- AF effective filter/mipmap counts
	- anisotropy parameter applied/total counts
	- MSAA pending destroy state and FBO create/destroy/incomplete lines

## Validation
- `android\stop-stale-formatters.ps1` -> no stale formatter tasks found
- `android\run-code-quality.ps1 -Fix -Paths @('d1/arch/ogl/ogl.c','d2/arch/ogl/ogl.c','android/app/src/main/cpp/shared/android_graphics_options.c')` -> passed; scoped formatter applied only to the shared Android C file because D1/D2 are excluded
- VS Code diagnostics on touched C files -> no errors
- `cd android; .\gradlew.bat :app:externalNativeBuildDebug --console=plain` -> passed for `arm64-v8a`, `armeabi-v7a`, and `x86_64`
- `cd android; .\gradlew.bat :app:testDebugUnitTest --console=plain` -> passed

## Notes
- Keep D1/D2 changes mirrored if implementation follows
- Prefer engine-side live apply over Kotlin-side duplicated graphics logic
- Validate with Android overlay testing if a code change is made later
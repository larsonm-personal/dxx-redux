# Android GLES Original Graphics Options Reuse Study

## Goal
- Decide whether Android GLES 3.0 graphics controls should reuse, feed, or disable the original in-game graphics options for MSAA, anisotropic filtering, texture filtering, colored lighting, transparency effects, and adjacent advanced options
- Preserve the Android port advantages: richer MSAA/AF/filter presets, mid-level changes, regression coverage, and small d1/d2 diffs

## Research Checklist
- [x] Locate prior Android GLES, texture, MSAA, AF, transparency, and overlay graphics plans
- [x] Map Android launcher/overlay graphics setting flow through Kotlin, JNI, config, and OGL runtime code
- [x] Map original D1/D2 in-game graphics options and their render-side consumers
- [x] Map which kept options are `descent.cfg` settings versus per-pilot player settings
- [x] Map launcher all-game/all-pilot scope versus in-game current-game/current-pilot scope
- [x] Compare option semantics, persistence, runtime mutability, API requirements, and GLES compatibility
- [x] Decide whether to combine, feed, or disable conflicting original menu options on Android
- [x] Plan colored lighting, transparency effects, and other original advanced options for the GLES 3.0 shim/port
- [x] Plan same-store write-through and live runtime updates for every edit surface
- [x] Implement the chosen conflict-resolution changes
- [x] Add validation coverage and run it

## Implementation Status
- 2026-04-25: Started implementation tranche 1. Scope: shared/native live setters, launcher exposure for kept options, pilot visual preference bridge, Android menu conflict cleanup, and focused tests where practical.
- 2026-04-25: Completed implementation tranche 1. Added shared Android graphics setters with config/pilot mirroring, launcher Graphics controls for original visual options, native visual-prefs bridge, Android menu conflict cleanup, config helper tests, code-quality run, focused JVM test, and Android debug build.

## Recommendation
- Do not try to replace the Android GLES MSAA/AF/filter implementation with the original base-game controls. The original controls were useful static desktop OpenGL options, but they do not cover Android's direct EGL path, GLES 3.0 FBO MSAA, selectable AF levels, selective menu/HUD filtering, or mid-level changes.
- Reuse the original data paths where they already match: keep `GameCfg.TexFilt` for 0/1/2 texture filtering, keep `PlayerCfg.DynLightColor`, keep `PlayerCfg.AlphaEffects`, keep D2 `GameCfg.MovieTexFilt`, and keep `GameCfg.ClassicDepth`.
- On Android, disable or hide only the conflicting legacy in-game text-menu controls: `TexFilt=3` / `Anisotropic` and `GameCfg.Multisample` / `4x multisampling`. Those are the misleading ones.
- The all-pilots conflict is real, but it only applies to `PlayerCfg.AlphaEffects` and `PlayerCfg.DynLightColor`. Texture filtering, D2 Movie Filter, and Classic Depth Ordering are `GameCfg` settings in `descent.cfg`, so they are per-game config settings rather than per-pilot settings.
- Keep the Android launcher's existing broad Graphics-tab behavior for config-backed graphics settings: shared settings write root, D1, and D2 configs. For D2-only `MovieTexFilt`, write only D2 plus root fallback. Add game-specific config helpers so D2-only keys are not sprayed into D1's active config.
- For pilot-backed visual effects, do not move the source of truth into Kotlin or `descent.cfg`. Surface them in the Graphics tab through a native C bridge that reads and patches the existing `.plx` `[graphics]` values. Because the launcher has no pilot selector and existing pilot-backed launcher preferences already write all pilots, the first Android implementation should treat these as app-wide visual preferences and write all existing D1 and D2 pilot files.
- Make every edit surface write the same storage and apply the same runtime state. The launcher, overlay, and in-game menu should all go through one native Android graphics apply layer for the running game. That layer should update `GameCfg` or `PlayerCfg`, apply live renderer flags, and persist the changed backing file promptly.
- Feed the remaining in-game `TexFilt` choices through the Android live-apply mechanism and make the overlay setter update `GameCfg.TexFilt` too. Today the overlay's `tex_filt` path changes `g_texfilt_level` but not `GameCfg.TexFilt`, so it is not yet editing the same stored setting.
- Add Android launcher exposure for colored lighting and transparency effects through the native pilot preferences bridge, not by duplicating player-file parsing in Kotlin. Add launcher exposure for D2 Movie Filter and Classic Depth Ordering through config-file editing. Treat Android VSync as a separate clarification item because it currently affects frame limiting more than EGL swap interval.

This is a hybrid of the two proposed paths: feed the part that genuinely shares a setting (`TexFilt` 0/1/2), and disable the legacy controls whose semantics do not match Android (`TexFilt=3` AF and `Multisample`). It keeps the base graphics menu useful without letting it fight the overlay and launcher.

## Prior Plan Trail Found

### GLES 3.0, ETC2, KTX2, and high-res textures
- `plan_gles3_etc2_hires_textures.md`: introduced the GLES 3.0 fixed-function shim, ETC2 uploads, eager PNG loading, and hi-res metrics
- `plan-precompressed-etc2-dxa.md`: moved runtime ETC2 compression to precompressed DXA texture packs
- `plan_etc2_to_ktx2_migration.md`: migrated the custom ETC2 container to KTX2 with KTX-Software v4.4.2
- `plan-etc2-black-texture-diagnostics.md`: kept permanent ETC2 upload/readback diagnostics and removed stale temporary black-texture probes
- `etc2-reenable-launch-button.md`: documents that ETC2 should stay enabled on emulator and that SwiftShader was not the black-texture root cause
- `debug-logging-black-textures.md`, `plan_diagnose_black_3d_textures.md`, `plan_texture_loading_diagnostics.md`, `plan_texture_scan_netstats_debuglog_shader_warnings.md`: related diagnostic plans around texture upload and shader state

### Transparency and merged-wall fixes
- `hires-texture-alpha-fix.md`: fixed TGA alpha preservation and key-color handling in the texture conversion pipeline
- `hires-transparency-perf-graphics.md`: combined super-transparency mask work, performance counters, GPU timer, AF, MSAA, and overlay controls
- `fix-overlay-color-and-texture-issues.md`: identified the alpha/key-color pipeline bug for door35, misc060, and rock313
- `frame-bar-supertrans-graphics-settings.md`: fixed a super-transparency first-bind regression where `super` was computed before texture/mask load
- `super_transparent_mask_fix.md`, `ogl_merge_super_transparent.md`, `supertransparent_threshold_docs_cleanup.md`: earlier super-transparent mask and shader cleanup plans
- `metl154_plain_alpha_fix.md`, `metl154_hires_premerge_fix.md`, `metl154_merge_clip_fix.md`, `metl154_post_merge_clip_followup.md`, `metl154_runtime_diagnostic.md`, `metl154_postfix_cleanup_and_debug_harness.md`, `metl154_tranche1_rename_and_mode_cleanup.md` through `metl154_tranche9_launcher_debug_controls.md`: the main shifted/failed transparency and merged-wall route-debug plan family

### MSAA, AF, texture filtering, and overlay/launcher settings
- `graphics-page-msaa-af-persist.md`: created the Graphics page, added MSAA/AF selectors, and made overlay changes persist
- `overlay-launcher-graphics-settings.md`: added live TexFilt editing, overlay stats, GPU timing, and settings consolidation
- `overlay-fixes-and-selective-filtering.md`: fixed TexFilt cycling, label colors, and selective menu/HUD filtering
- `plan_fix_msaa_missile_glitch.md`: fixed MSAA frame-depth drift and subview color clears
- `font-rendering-deep-dive.md`: documents TexFilt/AF font regressions and the `OGL_FLAG_NOCOLOR` guard decisions
- `d1d2_diff_shrink_study.md` and `d1d2_shrink_phase2_remaining_and_phase3_candidates.md`: classify the GLES3 shim, MSAA, GPU timer, aniso, texfilt, and KTX2 code as Android-only diff that should stay small or move to shared helpers over time

## Current Android Setting Flow

### Launcher and overlay
- `android/app/src/main/java/com/dxxredux/app/GraphicsSettingsPage.kt`
	- `TexFilterSection`: reads/writes `TexFilt` values 0, 1, 2 in config files
	- `MsaaSection`: reads/writes Android-added `MsaaLevel` values 0, 2, 4
	- `AnisoSection`: reads/writes Android-added `AnisoLevel` values 0, 2, 4, 8, 16
	- `SelectiveFilterSection`: reads/writes Android-added `MenuTexFilt` and `HudTexFilt`
	- `ColorDepthSection`: Android EGL surface setting, restart-time only
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
	- `readConfigValue()` checks D2 config, then D1 config, then root fallback. This is fine for shared keys, but it hides per-game differences and needs a game-specific variant before adding D2-only settings.
	- `updateAllConfigFiles()` writes root plus existing D1/D2 `descent.cfg` files. This matches current global launcher semantics for shared graphics config keys.
- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`
	- stats indices 18/19 are AF current/max, 20/21 are MSAA current/max, 27 is TexFilt
	- `cycleAnisotropy()`: 0 -> 2 -> 4 -> 8 -> 16 -> 0, capped by hardware max
	- `cycleMsaa()`: 0 -> 2 -> 4 -> 0, capped by hardware max
	- `cycleTexFilt()`: 0 -> 1 -> 2 -> 0
- `android/app/src/main/java/com/dxxredux/app/EnginePreferencesPage.kt` and `NativePilotPreferences.kt`
	- Existing pilot-backed launcher preferences read one representative pilot and write every matching D1/D2 pilot file through native code.
	- Graphics-tab pilot-backed visual effects should reuse this broad write-all model unless a future pilot selector is added.

### JNI and native state
- `android/app/src/main/cpp/jni_main.c`
	- `nativeSetGraphicsOption("aniso_level")`: updates `ogl_aniso_level`, `GameCfg.AnisoLevel`, and `g_aniso_pending_apply`
	- `nativeSetGraphicsOption("msaa_level")`: updates `ogl_msaa_samples`, `GameCfg.MsaaLevel`, and `g_msaa_pending_apply`
	- `nativeSetGraphicsOption("tex_filt")`: clamps to 0..2, updates `g_texfilt_level`, and sets `g_texfilt_pending_apply`, but currently does not update `GameCfg.TexFilt` or write `descent.cfg`
	- `nativeSetGraphicsOption("menu_tex_filt")` and `"hud_tex_filt"`: update `GameCfg.MenuTexFilt` and `GameCfg.HudTexFilt`
	- `nativeGetVideoStats()`: exposes `ogl_aniso_level`, `ogl_msaa_samples`, `ogl_color_depth`, `g_texfilt_level`, `GameCfg.MenuTexFilt`, and `GameCfg.HudTexFilt`
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
	- `VideoInfoOverlay` calls `nativeSetGraphicsOption()` directly for live runtime controls.
	- `onResume()` reapplies debug/local helper prefs but does not currently reload Graphics-tab config changes made while the game task was paused behind the launcher.

### Render side
- `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`
	- `ogl_aniso_level`, `g_aniso_pending_apply`, `g_texfilt_pending_apply`, `g_texfilt_level`, `ogl_msaa_samples`, and `g_msaa_pending_apply` are the Android runtime controls
	- `ogl_start_frame()` consumes pending AF, TexFilt, and MSAA changes
	- `ogl_bindbmtex()` applies context-sensitive filters for world/menu/HUD textures and keeps font textures under `MenuTexFilt`
	- `ogl_loadbmtexture_f()` upgrades texture filtering to trilinear when AF is on, loads KTX2 before PNG/base data, and applies AF to mipmapped textures
	- `ogl_set_blending()` maps `GR_BLEND_ADDITIVE_A`, `GR_BLEND_ADDITIVE_C`, and `GR_BLEND_NORMAL` to GLES-compatible `glBlendFunc` calls
- `android/app/src/main/cpp/shared/ogl_texture_android.c`
	- `android_ogl_apply_anisotropy_all()` applies AF to all mipmapped textures
	- `android_ogl_apply_texfilt_all()` live-updates texture filters and generates mipmaps when needed
	- `android_ogl_load_dxa_mask()` loads super-transparent DXA mask PNGs
- `android/app/src/main/cpp/shared/ogl_msaa_android.c`
	- shared FBO create/destroy helpers exist, but D1/D2 still carry some local MSAA state and wrapper logic
- `android/app/src/main/cpp/shared/gles3_shim.c`
	- the fixed-function GLES3 shader multiplies vertex color by sampled texture, implements alpha test with discard, and passes `GL_BLEND` through to GLES
	- this is enough for colored lighting and transparency effects; no special new shader route is required

## Original Game Option Map

### In-game graphics menu
- `d1/main/menu.c::graphics_config()` and `d2/main/menu.c::graphics_config()` create the text-based Graphics Options menu
- Shared options:
	- `Texture Filtering`: `GameCfg.TexFilt` radio values 0 None, 1 Bilinear, 2 Trilinear, 3 Anisotropic
	- `Transparency Effects`: `PlayerCfg.AlphaEffects`
	- `Colored Dynamic Light`: `PlayerCfg.DynLightColor`
	- `VSync`: `GameCfg.VSync`
	- `4x multisampling`: `GameCfg.Multisample`
	- `Classic Depth Ordering (SP)`: `GameCfg.ClassicDepth`
	- FPS counter, reticle options, brightness, disable cockpit view
- D2-only option:
	- `Movie Filter`: `GameCfg.MovieTexFilt`

### Config and persistence
- `d1/main/config.h`, `d2/main/config.h`: `GameCfg` contains `TexFilt`, `MenuTexFilt`, `HudTexFilt`, `VSync`, `Multisample`, Android-added `AnisoLevel`, Android-added `MsaaLevel`, `ClassicDepth`, `ColorDepth`, and D2 `MovieTexFilt`
- `d1/main/config.c`, `d2/main/config.c`: read/write the config keys; Android syncs `GameCfg.AnisoLevel` and `GameCfg.MsaaLevel` into `ogl_aniso_level` and `ogl_msaa_samples` after config read
- `d1/main/playsave.h`, `d2/main/playsave.h`: `PlayerCfg` contains `AlphaEffects` and `DynLightColor`
- `d1/main/playsave.c`, `d2/main/playsave.c`: default both player options to 0, read/write them in the `[graphics]` section as `alphaeffects` and `dynlightcolor`
- On Android, PhysFS sets each game's write dir to `filesDir/d1x-redux/` or `filesDir/d2x-redux/`. `WriteConfigFile()` therefore writes the current game's `descent.cfg`, while the launcher helper writes root plus both game configs from Kotlin.
- `write_player_file()` writes the current pilot file and also calls `WriteConfigFile()`. The in-game Graphics Options menu commits values after the dialog returns, and the surrounding Options menu writes the current pilot on close. That is not yet immediate write-through for live Android edits.
- `d1/main/net_udp.c`, `d2/main/net_udp.c`, `d1/main/playsave.c`, `d2/main/playsave.c`: multiplayer has a separate `Netgame.AllowColoredLighting` host option, so player colored lighting can be gated off in multiplayer

### Original render consumers
- Texture filtering:
	- `ogl_loadtexture()` uses `texfilt >= 2` for trilinear mipmaps
	- desktop OpenGL only, not Android/OGLES, uses `texfilt >= 3` to set AF to `ogl_maxanisotropy`
- Original MSAA:
	- `arch/ogl/gr.c::gr_set_attributes()` uses `GameCfg.Multisample` to request 4x SDL GL multisampling under `#ifndef OGLES`
	- Android defines `OGLES` and creates a direct EGL surface, so this path does not drive Android MSAA
- Colored lighting:
	- `main/lighting.c`: `PlayerCfg.DynLightColor` decides whether dynamic lights keep RGB color or collapse to intensity
	- `main/render.c`: `PlayerCfg.DynLightColor` controls red mine-glow tint during control-center destruction
- Transparency effects:
	- `main/endlevel.c`: `PlayerCfg.AlphaEffects` enables additive blending for the big explosion
	- `main/object.c`: `PlayerCfg.AlphaEffects` enables additive/normal blends for fireballs, weapons, powerups, lasers, and D2 markers
	- `main/render.c`: `PlayerCfg.AlphaEffects` desaturates lamp lighting and enables additive blending for fuel centers and D2 force fields
- D2 movie filter:
	- `d2/main/movie.c::show_frame()` passes `GameCfg.MovieTexFilt` to `ogl_ubitblt_i()`
- Classic depth:
	- `arch/ogl/ogl.c`, `main/render.c`, `main/endlevel.c`, and `xmodel/xmodel.cpp` consume `GameCfg.ClassicDepth` to alter depth ordering in single-player paths

## Ownership and Sync Model

### Answer on per-pilot scope
Yes, `Transparency Effects` and `Colored Dynamic Light` are per-pilot today. They live in `PlayerCfg` and persist in each pilot's `.plx` `[graphics]` section as `alphaeffects` and `dynlightcolor`.

No, the other kept graphics-menu options are not per-pilot. `Texture Filtering`, D2 `Movie Filter`, and `Classic Depth Ordering (SP)` are `GameCfg` fields persisted to `descent.cfg`. On Android those configs are effectively per game because each game writes to its own `filesDir/d1x-redux/` or `filesDir/d2x-redux/` directory.

The launcher's current Graphics page is broader than the in-game menu. It writes root, D1, and D2 config files for shared settings. Existing launcher pilot-backed preferences also write all D1/D2 pilot files. The in-game menu naturally edits the running game and current pilot. To make all edit surfaces converge for Android, define the launcher and Android in-game menu behavior as app-wide for these visual graphics preferences: edit the same underlying fields, mirror shared settings across both game configs, and mirror pilot-backed visual effects across all existing D1/D2 pilots.

### Kept-option ownership table
| Option | In-game label | Storage | Current launcher scope | Current in-game scope | Desired Android scope |
| --- | --- | --- | --- | --- | --- |
| Texture filtering 0/1/2 | `Texture Filtering` | `GameCfg.TexFilt` in `descent.cfg` | Root, D1, D2 configs | Running game's config after menu commit | Root, D1, D2 configs, plus live `GameCfg.TexFilt` and texture reload flags in the running game |
| Menu filtering | Launcher-only | `GameCfg.MenuTexFilt` in `descent.cfg` | Root, D1, D2 configs | No in-game control | Root, D1, D2 configs, plus live running-game `GameCfg.MenuTexFilt` |
| HUD filtering | Launcher-only | `GameCfg.HudTexFilt` in `descent.cfg` | Root, D1, D2 configs | No in-game control | Root, D1, D2 configs, plus live running-game `GameCfg.HudTexFilt` |
| Movie filter | D2 `Movie Filter` | D2 `GameCfg.MovieTexFilt` in `descent.cfg` | Not surfaced | D2 running game's config after menu commit | D2 config plus root fallback, plus live D2 `GameCfg.MovieTexFilt` |
| Classic depth | `Classic Depth Ordering (SP)` | `GameCfg.ClassicDepth` in `descent.cfg` | Not surfaced | Running game's config after menu commit | Root, D1, D2 configs, plus live running-game `GameCfg.ClassicDepth` |
| Transparency effects | `Transparency Effects` | `PlayerCfg.AlphaEffects` in `.plx` `[graphics]` | Not surfaced | Current pilot file | All D1/D2 pilot `.plx` files, plus live running-game `PlayerCfg.AlphaEffects` |
| Colored lighting | `Colored Dynamic Light` | `PlayerCfg.DynLightColor` in `.plx` `[graphics]` | Not surfaced | Current pilot file | All D1/D2 pilot `.plx` files, plus live running-game `PlayerCfg.DynLightColor` |

### Current sync gaps to fix
- `nativeSetGraphicsOption("tex_filt")` does not set `GameCfg.TexFilt`, so overlay TexFilt changes can diverge from `descent.cfg` and from the in-game menu.
- `nativeSetGraphicsOption()` does not call `WriteConfigFile()`, so overlay changes are runtime-first and depend on a later config write for persistence.
- `GraphicsSettingsPage` writes config files but cannot currently update a paused/running `MainActivity` until the game is restarted or another runtime path changes the same value.
- The in-game Graphics Options menu applies most values only after `newmenu_do1()` returns. While the menu is opened from an in-level pause menu, toggles should update the active renderer state as soon as the menu value changes.
- `Transparency Effects` and `Colored Dynamic Light` need a native visual-prefs bridge for `.plx`; Kotlin should not parse or patch the pilot text file directly.
- Launcher reads for config-backed settings currently use D2-first fallback. This is fine for global shared controls, but D2-only `MovieTexFilt` and any future per-game display need explicit game-specific read helpers and mixed-state handling.

## Feasibility Findings

### Could Android have reused original texture filtering?
Partly, and it already does. `GameCfg.TexFilt` remains the shared source for 0 None, 1 Bilinear, and 2 Trilinear. The Android additions were still necessary because the original menu update does not set pending flags, does not generate mipmaps for already-loaded textures, does not reset existing linear textures back to nearest, and does not distinguish world/menu/HUD filtering. The Android live-apply and selective filtering code is not wasted duplication.

### Could Android have reused original anisotropic filtering?
Not cleanly. The original implementation is `TexFilt=3`, meaning "use maximum AF if available" during desktop OpenGL texture load. It has no 2x/4x/8x/16x presets, no mid-level updates, and is compiled out of the Android/OGLES upload path. Android's `AnisoLevel` plus `ogl_aniso_level` model is the right implementation for the port.

### Could Android have reused original MSAA?
No. The original `Multisample` option asks SDL for a 4x multisample default framebuffer before context creation and warns that restart may be needed. Android bypasses SDL video mode, uses direct EGL, and needs a runtime FBO resolve path. Android's `MsaaLevel` plus `ogl_msaa_samples` model is the right implementation.

### Can Android reuse colored lighting?
Yes. It is already base-game render logic, not a GLES wrapper feature. The GLES3 shim and OGL_MERGE shaders pass vertex colors through, so the existing `PlayerCfg.DynLightColor` code should work. In multiplayer, it still depends on `Netgame.AllowColoredLighting`, which should remain a game/network option.

### Can Android reuse transparency effects?
Yes, with one naming caveat. `PlayerCfg.AlphaEffects` controls additive blend effects for objects, explosions, fuel centers, and similar visuals. It is unrelated to the high-res KTX2/DXA wall texture transparency and super-transparent mask pipeline. The GLES3 shim already supports the required blend funcs and alpha test. Do not reimplement this in Android texture code.

## Conflict Decision

### Keep and feed on Android
- `TexFilt` values 0, 1, and 2
	- Launcher already writes the same config key
	- Overlay already updates the live runtime state, but must be fixed to update `GameCfg.TexFilt` and persist too
	- In-game menu should call the same pending-apply and write-through path as the overlay after it changes this value
- `ClassicDepth`, D2 `MovieTexFilt`, `PlayerCfg.AlphaEffects`, and `PlayerCfg.DynLightColor`
	- These are genuine base-game options and should be surfaced in the launcher while keeping the original storage fields
	- All Android edit surfaces should update the running game immediately and persist to the same backing store

### Hide or disable on Android
- `Texture Filtering: Anisotropic` / `TexFilt=3`
	- Conflicts with Android `AnisoLevel`
	- Has no preset level
	- Appears as Trilinear in the overlay and cycles strangely from 3 to 1
- `4x multisampling` / `GameCfg.Multisample`
	- Conflicts with Android `MsaaLevel`
	- Does not affect Android's direct EGL/FBO MSAA path
	- Misleadingly says restart is required even though Android MSAA is live-toggleable

### Keep as-is in the text menu
- `Transparency Effects` and `Colored Dynamic Light`
	- They are real base-game player options and should continue to work
- `Movie Filter` in D2
	- It has a real movie rendering consumer and no Android conflict
- `Classic Depth Ordering (SP)`
	- It has real render-side consumers and no Android conflict
- Brightness, reticle, FPS counter, and disable cockpit
	- Not part of this conflict

### Clarify before surfacing in launcher
- `VSync`
	- On Android, `SDL_GL_SWAP_CONTROL` is not used because the `OGLES` path creates direct EGL surfaces
	- `GameCfg.VSync` still influences frame limiting in `game.c`, `automap.c`, and `arch/sdl/timer.c`
	- Do not add it to the Android Graphics page as true swap-interval control unless an `eglSwapInterval` decision is made

## Implementation Plan

### Phase 1: Android menu conflict cleanup
- [x] Add a shared Android helper, for example `android_graphics_options.h/.c`, that can be called from JNI and D1/D2 menu code to apply config-backed graphics settings through one path
- [x] In that helper, implement `android_graphics_set_texfilt(value, persist)` to clamp to 0..2, set `GameCfg.TexFilt`, set `g_texfilt_level`, set `g_texfilt_pending_apply`, and optionally call `WriteConfigFile()`
- [x] Move `nativeSetGraphicsOption("tex_filt")`, `"menu_tex_filt"`, `"hud_tex_filt"`, `"aniso_level"`, and `"msaa_level"` onto the shared helper so overlay edits update both runtime state and stored `GameCfg` fields consistently
- [x] Add helper setters for `classic_depth` and D2 `movie_tex_filt` that update `GameCfg.ClassicDepth` / `GameCfg.MovieTexFilt`, persist config, and rely on the next render/movie frame to consume the value
- [x] In both `d1/main/menu.c` and `d2/main/menu.c`, under `#ifdef ANDROID`, remove or disable the `Anisotropic` texture filtering radio item from `graphics_config()`
- [x] In both menus, under `#ifdef ANDROID`, remove or disable the `4x multisampling` checkbox and the restart warning tied to `GameCfg.Multisample`
- [x] In `graphics_config_menuset()` for both games, on `EVENT_NEWMENU_CHANGED`, update Android runtime state immediately for TexFilt, ClassicDepth, D2 MovieTexFilt, AlphaEffects, and DynLightColor instead of waiting until the menu closes
- [x] After the menu commits a 0/1/2 TexFilt change on Android, route the final value through the same helper instead of only changing `GameCfg.TexFilt`
- [x] Clamp Android `TexFilt` config values above 2 during config read. If preserving intent is desired before the first release, map `TexFilt=3` to `TexFilt=2` plus a default `AnisoLevel` such as 16, which is later capped by hardware. Otherwise simply clamp to 2

### Phase 2: Launcher exposure for original visual options
- [x] Add `readConfigValueForGame(filesDir, game, key)` and `updateConfigFilesForGame(filesDir, game, settings)` Kotlin helpers. Keep `updateAllConfigFiles()` for settings that are intentionally global across D1/D2.
- [x] Update `GraphicsSettingsPage` to accept `gameVariant` so it can read the selected game for D2-only display while still applying shared settings globally when appropriate
- [x] Add `ClassicDepth` to the launcher Graphics page as a shared config-backed toggle that writes root, D1, and D2 configs
- [x] Add D2-only `MovieTexFilt` to the launcher Graphics page. Read from D2 config, write D2 config and root fallback, and hide or disable it when D1 is the selected game unless the UI has a D2-specific subsection
- [x] Extend `ConfigImportExport.EXPORTED_CFG_KEYS` with `ClassicDepth` and D2 `MovieTexFilt`, making import write D2-only keys through the game-specific helper
- [x] Extend the native pilot preferences bridge (`android_pilot_prefs.cpp`, `NativePilotPreferences.kt`, and the needed `playsave.c/.h` helpers) with a visual-prefs struct for `AlphaEffects` and `DynLightColor`, using `playsave.c` as the source of truth for reading/writing player files
- [x] For D1 and D2, add C helpers that read and patch `.plx` `[graphics]` keys without duplicating parser details in Kotlin. Both games already persist these two fields in the text `.plx` file.
- [ ] Add a compact launcher Graphics page section for pilot visual effects. Read all existing D1/D2 pilot files, show a mixed state if values differ, and when toggled write all existing D1/D2 pilot files to match the launcher's current app-wide behavior.
- [x] If no pilot files exist, show defaults from the native helper and leave writes disabled until a pilot exists, matching the existing engine preferences page behavior
- [x] Do not add `VSync` to the launcher Graphics page until Android swap-interval semantics are either implemented with `eglSwapInterval` or deliberately renamed as a frame limiter

### Phase 3: Launcher-to-running-game live apply
- [x] Add a lightweight launcher write generation, for example SharedPreferences key `graphics_settings_generation`, incremented by `GraphicsSettingsPage` whenever it writes a graphics config or pilot visual preference
- [x] In `MainActivity.onResume()`, compare that generation with the last applied value. If it changed, reload the selected game's config-backed graphics values and current visual-prefs values, then call native setters for TexFilt, MenuTexFilt, HudTexFilt, AnisoLevel, MsaaLevel, ClassicDepth, D2 MovieTexFilt, AlphaEffects, and DynLightColor
- [x] Add native setters for `alpha_effects` and `dynlight_color` that update the active `PlayerCfg` fields without requiring a pilot reload. Persistence is handled by the launcher bridge for launcher edits and by `write_player_file()` / visual-prefs patching for in-game edits.
- [x] Keep `ColorDepth` restart-time only. The launcher should write config, but `MainActivity.onResume()` should not attempt to recreate the EGL surface for it in this phase.
- [x] Decide whether overlay edits should call `WriteConfigFile()` immediately or use a short debounce. Prefer immediate writes for discrete buttons because AF/MSAA/TexFilt are low-frequency controls and the user's request is live write-back.

### Phase 4: In-game menu write-through and global Android mirroring
- [x] For Android, after `graphics_config_menuset()` changes a discrete kept option, update the active in-memory field and persist immediately through the shared helper. Avoid writing repeatedly for brightness slider motion in this phase.
- [x] For shared config-backed values changed from the in-game menu, write the running game's `descent.cfg` through `WriteConfigFile()` and mirror the changed key to root plus the other game's config with an Android file patch helper. This makes launcher and in-game menu edits converge on the same app-wide graphics defaults.
- [x] For D2 `MovieTexFilt`, persist only D2 plus root fallback.
- [x] For `AlphaEffects` and `DynLightColor`, update active `PlayerCfg` immediately, write the current pilot, and patch all existing D1/D2 `.plx` files through the same visual-prefs helper used by the launcher. This is an intentional Android app-wide behavior so the launcher and in-game menu do not drift.
- [ ] Add debug-log lines under an existing Android graphics/game category when these helpers mirror settings, but keep them concise and gated if noisy.

### Phase 5: Optional overlay stats and live controls
- [ ] Expose `PlayerCfg.AlphaEffects`, `PlayerCfg.DynLightColor`, D2 `MovieTexFilt`, and `GameCfg.ClassicDepth` through `nativeGetVideoStats()` or introspection so automated tests can verify current runtime values without screenshots
- [ ] Do not add overlay tap controls for these until the launcher page exists; the overlay is already dense and these options are not performance-tuning controls like AF/MSAA/TexFilt

### Phase 6: Validation
- [ ] Add or extend a host-side unit/integration test for the native pilot prefs bridge to confirm `AlphaEffects` and `DynLightColor` round-trip through C-backed player file access
- [x] Add Kotlin unit coverage for the config helpers: shared keys write root/D1/D2, D2-only `MovieTexFilt` writes D2 plus root fallback, and read helpers do not hide per-game differences unintentionally
- [ ] Add a JVM or native test fixture with multiple D1/D2 `.plx` files containing mixed visual prefs, then verify launcher write-all normalizes them and preserves unrelated pilot fields
- [ ] Add an Android automation script that opens the original in-game Graphics Options menu and verifies the Android build does not show legacy `Anisotropic` and `4x multisampling` controls, while still showing `Transparency Effects` and `Colored Dynamic Light`
- [ ] Extend an existing video-settings regression to cycle TexFilt from the text menu, introspect/stats-read `GameCfg.TexFilt`/`g_texfilt_level`, and confirm loaded textures update without needing a restart
- [ ] Add a simple render-state or introspection check for `AlphaEffects` and `DynLightColor` values after toggling them in the menu, without relying on screenshots
- [ ] Add a launcher-to-game resume test: change TexFilt, ClassicDepth, and visual prefs in the launcher Graphics tab while the game task exists, resume the game, then introspect the runtime values
- [ ] Add a write-through test for overlay TexFilt proving `GameCfg.TexFilt`, `g_texfilt_level`, and `descent.cfg` agree after a tap
- [x] Run `android\stop-stale-formatters.ps1`, then `android\run-code-quality.ps1 --fix`, then the Android debug build and affected automation tests

## Risks and Notes
- Do not confuse `Transparency Effects` with high-res texture transparency. The former is `PlayerCfg.AlphaEffects` additive blending. The latter is the KTX2/DXA mask and alpha pipeline.
- Avoid creating another Kotlin parser for player files. The native pilot prefs bridge exists specifically to keep player-file details in C.
- Be explicit that Android is choosing app-wide launcher semantics for these visual preferences. If true per-pilot editing is wanted later, add a pilot selector before exposing conflicting per-pilot states as ordinary toggles.
- Do not let D2-first config reads mask a D1/D2 split after in-game edits. Either mirror shared settings across both game configs or show a mixed/per-game state in the launcher.
- Overlay setters must update both runtime globals and `GameCfg` or `PlayerCfg`; runtime-only changes recreate the original conflict under a different UI.
- Avoid making Android text-menu controls more feature-complete than the launcher. Adding text-menu AF radio groups for 0/2/4/8/16 and MSAA radio groups for 0/2/4 would add d1/d2 menu churn for a path the Android overlay and launcher already cover better.
- Keep D1 and D2 mirrored. The menu, config, AlphaEffects, DynLightColor, ClassicDepth, and render consumers exist in both games, with D2 adding only `MovieTexFilt` and a few extra AlphaEffects render sites.
# Controls, overlay, and graphics follow-up planning - 2026-05-22

## Goals
- Plan controller analog exponential support alongside existing deadband controls
- Plan cleanup of unbound-action detection and action labels
- Plan overlay settings visibility cleanup by game context
- Plan live graphics setting application for AF/MSAA while paused
- Plan investigation for AF missing on merged textures

## Plan
1. [done] Map existing touch analog curve, controller axis configuration, unbound-action menu, and overlay settings owners
2. [done] Map AF/MSAA setting flow from Kotlin to native renderer and identify where live update is missing
3. [done] Map merged texture upload/filtering path and identify why AF may not apply there
4. [done] Record per-issue implementation plan, risks, tests, and suggested order

## Notes
- This is an initial planning pass only. No code changes for the seven follow-up issues have been made here.
- Main Kotlin owners:
	- `TouchOverlayView.kt`: remaining/unbound action menu, admin tray action list, labels, held activation, and drawing
	- `ControllerConfigPage.kt`: controller config UI, `controller_config.json` save/load, thresholds/deadzones
	- `MainActivity.kt`: controller JSON runtime loading, physical controller event dispatch, overlay callbacks, graphics JNI calls
	- `VideoInfoOverlay.kt`: in-game graphics controls for texture filtering, anisotropy, and MSAA
	- `HumanReadableConfig.kt` and `ConfigImportExport.kt`: config import/export paths that must follow any controller JSON schema change
- Main native owners:
	- `android_gamepad_config.cpp`: applies `controller_config.json` to `PlayerCfg`, `joy_axis_button_deadzone[]`, and `PlayerCfg.JoystickDead[]`
	- `android_input.c`: JNI virtual joystick event injection
	- `d1/arch/sdl/joy.c` and `d2/arch/sdl/joy.c`: virtual joystick axis/button events and axis-as-button deadzones
	- `d1/main/kconfig.c` and `d2/main/kconfig.c`: gameplay axis deadzone and scaled `Controls.joy_axis[]`
	- `android_graphics_options.c`: JNI graphics setters and pending apply flags
	- `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`: pending graphics apply, MSAA FBO, texture upload, cached merged textures
	- `android/app/src/main/cpp/shared/merged_wall_debug.c`: cached merged texture setup/final filter state

## Per-issue planning

### 1. Controller analog exponential sliders
- Existing touch code already has the target behavior and range: `ResponseCurve.EXPONENTIAL`, `applyResponseCurve()`, and `TouchBindings.MIN_EXPONENT/MAX_EXPONENT/DEFAULT_EXPONENT` of `1.0/4.0/2.0`.
- Add controller axis exponent storage alongside controller `thresholds`, likely `axis_exponents` or `exponents` keyed by the same axis IDs: `LS_X`, `LS_Y`, `RS_X`, `RS_Y`, `LT`, `RT`. Use float values clamped to `1.0..4.0` and default `1.0` if the intended default is unchanged linear controller behavior, or `2.0` if the desired feature default is exponential. I recommend default `1.0` for existing configs and a visible slider default of `2.0` only when the user opts in.
- UI work belongs in `ControllerConfigPage.kt`:
	- Extend `LoadedConfig`, `saveConfig()`, `loadConfig()`, state maps, reload handling, and `saveCurrent()` calls to include exponent values.
	- Add an exponent slider to `ButtonFunctionPickerDialog` only for analog single-axis functions, not for axis-as-button mode where it would confuse threshold activation.
	- Add X/Y exponent sliders to `StickPickerDialog` when each axis is in analog mode. Keep them near the existing dead-zone sliders so the mental model is one axis response section per axis.
- Runtime application should happen before events reach native gameplay deadzone logic, not in the axis-as-button path. The simplest Android-only path is in `MainActivity.onGenericMotionEvent()`: load exponent values from `controller_config.json`, transform only the `ctrl` physical axis values with the existing `applyResponseCurve()`, then feed the transformed values to `InputMixer`. This leaves touch and gyro behavior unchanged and avoids touching D1/D2 first.
- If physical controller and touch are mixed on the same axis, applying the curve before `InputMixer` keeps the controller curve controller-specific. Applying it inside `InputMixer` would affect touch and gyro unless source-aware logic is added.
- Native gameplay deadzones still come from `android_gamepad_config.cpp` and `PlayerCfg.JoystickDead[]`. Do not try to encode exponent in `.plr`; keep it Android runtime/config only.
- Tests:
	- JVM tests for save/load JSON round trip and clamping in controller config helpers.
	- JVM tests for controller axis transform helper: `1.0` is pass-through, `2.0` maps `0.5` to `0.25`, signs are preserved, and out-of-range config values clamp.
	- Manual or automation smoke test with controller input: right stick small motions should be visibly softened at exponent `2..4`, while axis-as-button threshold behavior remains unchanged.
- Risk: controller config versioning. `CONTROLLER_CONFIG_VERSION` should be bumped if defaults need to be written into fresh configs, but old configs must still load with default exponent values.

### 2. Unbound actions should consider controller buttons
- Current remaining actions only call `touchLayoutBoundActionBindings(layout)`, so controller bindings in `controller_config.json` do not hide entries such as Rewind.
- Add a small helper that converts controller function labels to touch binding IDs using `TouchBindings.nameToBinding(label)` or the existing reverse label maps. This should include standard button labels and meta labels, including labels stored in `bindings` for face buttons, d-pad controls, and axis-as-button controls.
- Feed this set into `remainingKeyTouchActions()` as an optional `extraBoundBindings: Set<Int> = emptySet()` or a more specific `controllerBoundBindings` parameter.
- In `TouchOverlayView`, add a provider or cached set populated by `MainActivity.loadMetaBindings()` or a nearby controller-config loader. Avoid parsing the JSON every draw; parse on startup/config reload and when returning from controller settings.
- Keep this as UI filtering only. The actual controller dispatch already works through `buttonMetaBindings`, `dpadMetaBindings`, `mixerButtonMap`, and native joystick buttons.
- Tests:
	- Extend `RemainingKeyTouchActionsTest` so `META_REWIND` disappears when provided through extra/controller-bound bindings.
	- Add a controller binding helper test for labels like `Rewind`, `Headlight`, and `Energy->Shield`.
- Risk: controller labels currently include both standard and extra/meta labels. Prefer `TouchBindings.nameToBinding()` so future labels stay centralized.

### 3. Quick Save and Quick Load labels should become Save and Load
- The admin tray labels are in `TouchOverlayView.adminTrayLabel()` as `Quick Save` and `Quick Load`.
- Change only the user-visible strings to `Save` and `Load`. Keep `ADMIN_QUICK_SAVE` and `ADMIN_QUICK_LOAD` internal names unless a later cleanup wants a wider rename. Behavior should remain `openSaveLoadMenu(openSave = true/false)`, not instant save/load.
- Optional follow-up: decide whether `TouchBindings.META_BUTTON_LABELS` should also relabel `META_QUICK_SAVE` and `META_QUICK_LOAD`. The request appears to target the in-game overlay menu, so I would start with admin tray text only.
- Tests:
	- Add or extend a small label helper test if `adminTrayLabel()` is exposed. If not, this can be covered by a lightweight refactor to make labels testable or by existing screenshot/manual check.
- Risk: none significant. This is low blast radius.

### 4. Headlight should not be in overlay settings
- `adminTrayVisibleActions()` appends `ADMIN_HEADLIGHT` in gamepad-only mode.
- Remove `ADMIN_HEADLIGHT` from the admin/settings tray list. Do not remove `BTN_HEADLIGHT` from `remainingBaseActionBindings`; headlight should still appear in the remaining/unbound actions menu when not bound elsewhere.
- Keep `ADMIN_HEADLIGHT` constant and callback for now if removing them creates extra churn. Once the UI no longer lists it, dead constant cleanup can wait.
- Tests:
	- Update `AdminTrayUiTest.gamepadOnlyModeIncludesMigratedControllerActions()` to exclude headlight.
	- Add or confirm a `RemainingKeyTouchActionsTest` assertion that D2 empty layouts still include `BTN_HEADLIGHT`.
- Risk: if a Shield user has no physical headlight binding, they still need the unbound action menu path. That makes issue 2 more important, because once headlight is controller-bound it should disappear from unbound actions too.

### 5. Hide warp, accept join, net stats, and net events in single-player
- Today `adminTrayVisibleActions()` always includes `ADMIN_NET_STATS` and `ADMIN_NET_EVENTS`, and appends `ADMIN_WARP` and `ADMIN_ACCEPT_JOIN` in gamepad-only mode. `MainActivity` can disable net controls in single-player, but the request is to hide them.
- Add context to `adminTrayVisibleActions()`, likely:
	- `isMultiplayerGame: Boolean`
	- `hasPendingLaunchInfo: Boolean` if net events/stats should remain visible during a pending launch handoff before `isMultiplayerGame` flips true
- Use the existing `shouldEnableNetStatsControl()` and `shouldEnableNetEventsControl()` policy as the source of truth for whether net stats/events are relevant. For warp and accept join, hide unless `isMultiplayerGame` is true or, for accept, unless the accept label/provider says there is a pending request. Initial implementation can simply hide both in non-multiplayer gamepad-only mode.
- `TouchOverlayView.currentAdminTrayActions()` already has `isMultiplayerGameProvider`; add a pending-launch or admin visibility provider only if needed. `MainActivity` owns `isMultiplayerGame` and can derive pending launch info from `MatchmakingStateHolder.state.value.gameLaunchInfo`.
- Tests:
	- Extend `AdminTrayUiTest` with single-player and multiplayer cases. In single-player, assert no net stats/events/warp/accept. In multiplayer, assert they appear in the same relative order.
	- Keep the automap behavior tests intact.
- Risk: pending multiplayer launch state. If stats/events are useful before the game enters network mode, preserve them while `gameLaunchInfo != null` by mirroring the existing enable helper.

### 6. AF/MSAA should update live while paused
- Kotlin already calls `VideoInfoOverlay.graphicsOptionSetter`, which calls `MainActivity.nativeSetGraphicsOption()`, which calls `android_graphics_set_aniso_level()` and `android_graphics_set_msaa_level()`. Those set `g_aniso_pending_apply` and `g_msaa_pending_apply`.
- The native pending flags are currently consumed in `ogl_start_frame()`. Menus and pause/settings overlays can render through `event_process()` to `newmenu_draw()` to `gr_flip()` without entering a 3D `ogl_start_frame()` path, so AF/MSAA changes can wait until gameplay resumes.
- Plan:
	- Extract the pending apply block from `ogl_start_frame()` into a small Android-only helper in both `d1/arch/ogl/ogl.c` and `d2/arch/ogl/ogl.c`, for example `ogl_apply_android_graphics_pending()`.
	- Call it at the start of `ogl_start_frame()` and also near the start of `gr_flip()` before framebuffer resolve/readback/swap.
	- For AF: same logic as current code, including mipmap generation for loaded non-font textures and `ogl_apply_anisotropy_all()`.
	- For MSAA: when applied during a menu-only frame, destroying the FBO is safe, but creation still only needs to happen when the next 3D frame binds MSAA. If we want a visible paused-game preview behind the menu, the change only becomes visible if the paused game frame is redrawn; that is acceptable as long as the setting is applied before the next frame needing it.
	- Keep texture filtering in the same helper too, so all pending graphics options are serviced consistently on 3D and menu frames.
- Tests:
	- Native build validation for both D1 and D2.
	- On-device/manual test: pause, open Video Info, cycle AF/MSAA, confirm debug log line for pending apply happens immediately while still paused. For MSAA, verify no FBO crash and that the next rendered frame uses the selected sample count.
	- Consider adding introspection/debug counters for `aniso_pending`, `msaa_samples`, and `msaa_fbo_bound` if manual verification is painful.
- Risk: `glGenerateMipmap()` and FBO destruction must run on the GL thread. Calling from `gr_flip()` preserves that. Do not apply directly in the JNI setter.

### 7. AF not applying to merged textures
- The cached merged wall texture path is Android OGL merge code:
	- `ogl_android_get_cached_plain_texmerge_bitmap()` allocates an `ogl_texture`, calls `android_merged_wall_cached_texmerge_setup_output_texture()`, then `android_merged_wall_cached_texmerge_finalize_entry()`.
	- `android_merged_wall_cached_texmerge_finalize_filters()` only generates mipmaps and applies `GL_TEXTURE_MAX_ANISOTROPY_EXT` when `texfilt_level > 0`.
- Likely root cause: normal bitmap loading upgrades `texfilt` when AF is enabled (`if (ogl_aniso_level > 0 && texfilt < 2) texfilt = 2` in `ogl_loadbmtexture_f()`), but the cached merged texture path passes raw `GameCfg.TexFilt`. With AF on and texture filtering off, newly-created cached merged textures get no mipmaps and no AF parameter.
- Plan:
	- In `ogl_android_get_cached_plain_texmerge_bitmap()` in both D1 and D2, compute a local `load_texfilt` the same way as `ogl_loadbmtexture_f()` before calling setup/finalize: if `ogl_aniso_level > 0 && load_texfilt < 2`, use `2`.
	- Pass `load_texfilt` to `android_merged_wall_cached_texmerge_setup_output_texture()` and `android_merged_wall_cached_texmerge_finalize_entry()`.
	- Keep cache keys unchanged at first. Existing pending texfilt/aniso apply paths can update already-created texture objects; the creation path is the missing part for textures created after AF is already enabled.
	- If testing shows already-cached merged textures do not update after AF changes, extend `ogl_apply_anisotropy_all()` or the shared helper to include cached texture entries explicitly, or clear the cached texmerge cache when AF changes. Prefer reapplying parameters over clearing cache if the texture objects are in `ogl_texture_list`.
- Tests:
	- Native build validation.
	- Add temporary debug logging or use existing merged-wall debug output to verify cached merged texture creation sees `texfilt=2` and `aniso>1` when AF is on with global TexFilt off.
	- On-device visual/manual check on a known merged transparent wall case, comparing AF off vs 4x/8x AF while TexFilt is 0.
- Risk: changing `texfilt` for cached merged textures when AF is on may increase mipmap generation work. This matches the existing non-merged texture policy and should be acceptable.

## Suggested order
1. Quick label and admin-tray visibility cleanup: issues 3, 4, and 5. These are small Kotlin changes with strong unit coverage.
2. Controller-bound unbound-action filtering: issue 2. It builds on the overlay helper tests and makes the headlight removal less likely to strand actions.
3. Controller analog exponent UI/storage/runtime: issue 1. This is larger and touches config import/export plus runtime input behavior.
4. Graphics pending apply while paused: issue 6. This is native GL-thread work but mechanically scoped.
5. Merged texture AF creation fix: issue 7. Do after issue 6 or alongside it, because both touch AF behavior and should be validated together.

## Validation checklist for implementation phase
- Run focused JVM tests after Kotlin changes, at minimum `AdminTrayUiTest`, `RemainingKeyTouchActionsTest`, and any new controller config tests.
- Run `android\run-code-quality.ps1 --fix` after Kotlin/native edits, first checking stale formatter tasks if this follows a timeout or interrupted run.
- Run `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:externalNativeBuildDebug --console=plain` after native renderer/input changes.
- For on-device graphics behavior, use debug logs or introspection rather than screenshots unless visual confirmation is specifically needed.
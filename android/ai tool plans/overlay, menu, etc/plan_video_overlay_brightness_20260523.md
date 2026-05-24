# Admin Tray Brightness Control + Video Overlay Focus - 2026-05-23

## Status
- [x] Inspect the existing video overlay controls, admin tray controls, controller focus route, and engine brightness storage
- [x] Implement the brightness control
- [ ] Validate on JVM, native build, and device or emulator

## Goals
- Add a brightness control to the in-game admin tray, not the video info overlay
- Edit the existing engine brightness value, `GammaLevel`, through `gr_palette_set_gamma()` and `gr_palette_get_gamma()`
- Reflect brightness changes live while the game is rendering the 3D environment
- Save immediately whenever brightness changes, including touch drag and controller edits
- For controller use, keep D-pad left/right as tray navigation until the brightness control is activated with `A` / D-pad center
- While brightness edit mode is active, use D-pad left/right to adjust the value, and a second `A` / D-pad center press to deselect the slider
- Show a light green tint across the full brightness rectangle while it is active in controller edit mode
- Keep adding the shared green controller navigation outline to the existing video overlay Texture Filtering, AF, and MSAA buttons

## Current Anchors
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` draws the admin tray, routes touch events, tracks `adminTraySelectedIndex`, and handles tray controller navigation in `handleAdminTrayGamepadKey()`
- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt` draws the custom Canvas video overlay and owns Texture Filtering, AF, and MSAA touch/controller handling
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` wires admin tray callbacks, `VideoInfoOverlay.graphicsOptionSetter`, `nativeSetGraphicsOption()`, and D-pad routing through `dispatchDpad()` / `handleControllerMenuKey()`
- `android/app/src/main/cpp/shared/android_graphics_options.c` centralizes Android graphics option updates and config mirroring for `TexFilt`, `AnisoLevel`, and `MsaaLevel`
- `android/app/src/main/cpp/jni_main.c` exposes `nativeGetVideoStats()` and `nativeSetGraphicsOption()`
- `d1/main/config.c` and `d2/main/config.c` already persist `GammaLevel=` and call `gr_palette_set_gamma()` when loading config
- `d1/main/menu.c` and `d2/main/menu.c` already expose the native Graphics Options brightness slider with range `0..16`
- `d1/include/palette.h` and `d2/include/palette.h` declare `gr_palette_set_gamma()` and `gr_palette_get_gamma()`
- Existing admin tray tests live in `AdminTrayUiTest` and controller-menu routing tests live in `ControllerMenuCycleTest`

## Implementation Plan

### Phase 1: Native brightness bridge
- [x] Add `android_graphics_set_gamma_level(int value, int persist)` to `android_graphics_options.c/.h`
- [x] Clamp to `0..16`, call `gr_palette_set_gamma(value)`, then assign `GameCfg.GammaLevel = gr_palette_get_gamma()`
- [x] Persist with `persist_config_if_needed(persist, "GammaLevel", GameCfg.GammaLevel, 1, 1)` so root, D1, and D2 config mirrors stay aligned like the other shared graphics settings
- [x] Add a `"gamma_level"` option name to `android_graphics_set_option()` and keep Kotlin callers using that Android bridge name rather than editing config directly

### Phase 2: Live value plumbing for Kotlin
- [x] Add a way for Kotlin to read the current live brightness, preferably through a small native getter or by extending `nativeGetVideoStats()` if that keeps code smaller
- [x] Return `gr_palette_get_gamma()` instead of only `GameCfg.GammaLevel`, so the tray follows the same live value if the native Graphics Options menu changes it
- [x] Expose `TouchOverlayView` providers/callbacks for admin-tray brightness: current value provider and setter callback
- [x] Wire those providers in `MainActivity.kt` to `nativeSetGraphicsOption("gamma_level", value)` and the native getter/stats value

### Phase 3: Admin tray brightness slider
- [x] Add `ADMIN_BRIGHTNESS` to `TouchOverlayView` action constants and include it in `adminTrayVisibleActions()` near `ADMIN_VIDEO_INFO`
- [x] Treat brightness as a custom slider cell, not a checkbox or one-shot action
- [x] Update admin tray layout helpers so a brightness slider can draw inside its whole grid rectangle without breaking current panel sizing
- [x] Draw label, value, track, and thumb inside the brightness rectangle using the existing `0..16` engine range
- [x] Use a light green full-rectangle background tint only while the slider is active from controller edit mode
- [x] Keep the existing green navigation stroke for ordinary tray focus around the current cell

### Phase 4: Admin tray touch interaction
- [x] In `handleAdminTrayTouch()`, detect touches inside the brightness rectangle before normal button activation
- [x] Map touch x-position to integer brightness `0..16`, clamp, apply live, and save immediately
- [x] Support `ACTION_DOWN` as a direct set and `ACTION_MOVE` as drag updates
- [x] Only call the setter when the integer value changes to avoid repeated config writes while the finger is still inside one step
- [x] Clear touch tracking on `ACTION_UP`, `ACTION_POINTER_UP`, and `ACTION_CANCEL` without invoking a separate action callback

### Phase 5: Admin tray controller interaction
- [x] Add `adminTrayBrightnessActive` state, cleared when the tray closes or when focus leaves the brightness action
- [x] When the brightness cell is focused and `A` / D-pad center is pressed, toggle `adminTrayBrightnessActive`
- [x] While active, D-pad left/right decrement/increment brightness and save immediately; D-pad left/right must not move tray focus
- [x] A second `A` / D-pad center press deactivates the brightness slider and returns left/right to normal tray navigation
- [x] Back / B / tray dismissal should close the tray and clear `adminTrayBrightnessActive`; no deferred save is needed because every move already saved
- [x] Perform haptic feedback when entering/exiting edit mode and when brightness changes through the controller

### Phase 6: Video overlay controller focus polish
- [x] Add a Canvas focus-stroke paint that matches the shared `tvFocusBorderColor` green (`0xFF00E676`)
- [ ] Prefer centralizing the ARGB constant in `DpadFocusUtils.kt` and using that from both Compose and `VideoInfoOverlay.kt`
- [x] Draw the green outline around selected Texture Filtering, AF, and MSAA controls while preserving pressed fill feedback
- [x] Do not add a brightness action to `VideoInfoOverlay.kt`; brightness now belongs to the admin tray
- [x] Keep existing video overlay button activation behavior unchanged

### Phase 7: Startup and cross-page consistency
- [x] Add `GammaLevel` to `MainActivity.applyGraphicsSettingsPrefs()` so any config-side brightness edit can be applied to a running game through the same native bridge
- [ ] Do not add a launcher brightness UI in this tranche unless requested; this plan is scoped to the in-game admin tray plus video overlay focus polish
- [x] Do not create a separate SharedPreferences brightness value; the source of truth stays in the engine config and palette state

### Phase 8: Tests and validation
- [x] Update `AdminTrayUiTest` expectations for the new `ADMIN_BRIGHTNESS` action and its visible ordering
- [x] Add focused JVM coverage for brightness action classification: not checkbox, does not close tray after controller activation, and uses slider/edit-mode semantics
- [x] Add focused JVM coverage for brightness clamp/step helpers if helper functions are extracted from `TouchOverlayView.kt`
- [x] Keep or update `SettingsChildOverlayControllerTest` so the video overlay controller actions remain Texture Filtering, AF, MSAA, and debug-only actions with no Brightness entry
- [x] Run the focused JVM tests, at minimum `AdminTrayUiTest`, `ControllerMenuCycleTest`, and `SettingsChildOverlayControllerTest`
- [x] Run `android/stop-stale-formatters.ps1` before formatting, then `android/run-code-quality.ps1 --fix` on touched files
- [x] Run an Android debug build or the relevant Gradle native build for both D1 and D2 variants
- [ ] On emulator or device, open the admin tray in a level, touch the brightness slider, drag it, and confirm the 3D view changes live and saves immediately
- [ ] With a controller, focus Brightness in the admin tray, press `A` to activate it, confirm the full rectangle gets the light green tint, and confirm D-pad left/right edits live
- [ ] Press `A` again and confirm the tint clears and D-pad left/right returns to tray navigation
- [ ] Dismiss the tray after editing and confirm `GammaLevel=` has already persisted, then restart and confirm the value is retained
- [ ] Open the video overlay and confirm Texture Filtering, AF, and MSAA show the green navigation outline when focused with controller navigation

## Notes
- The earlier version of this plan put brightness in `VideoInfoOverlay.kt`; that is superseded. Brightness belongs in `TouchOverlayView.kt` admin tray now
- `nativeSetGraphicsOption()` currently persists graphics options on every call. Since brightness is discrete `0..16`, touch dragging should only call native code when the integer level changes
- Controller brightness edit mode is necessary because D-pad left/right already navigate the admin tray grid
- No D1/D2 source edits should be needed beyond using the existing `palette.h` declarations from shared Android code. If include paths force a change, keep it minimal and mirrored for both games
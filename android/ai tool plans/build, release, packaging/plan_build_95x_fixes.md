# Plan: Build 95x Fixes

9 targeted fixes across touch controls, analog triggers, UI overlays, and permissions.

## Phase 1: Touch Controls Editor -- Mouse Mode & Stacking

### 1A. Hide stick in mouse mode; entire zone is selectable
- TouchEditorPage.kt (hitTest, canvas drawing): When stick has `mouseMode=true`,
  don't draw the stick circle/thumb. Draw only the floating zone rectangle.
  hitTest should match the floating zone rect instead of the circle.
- TouchOverlayView.kt (runtime): Already correct -- mouse mode only draws
  the floating zone rect. Verify no regression.

### 1B. Default mouse sensitivity 10x higher
- TouchOverlayView.kt (mouse drag processing): Add hidden
  `MOUSE_SENSITIVITY_MULTIPLIER = 10.0f`. Apply in formula:
  `scale = (sensitivity * MULTIPLIER) / MOUSE_REFERENCE_DISTANCE`.
  Slider range stays 0.1-5.0; effective range becomes 1.0-50.0.

### 1C. Double-tap within mouse/joystick zone fires a button
- TouchControl.kt: Add `doubleTapBinding: Int = -1` to AnalogStickControl.
- TouchOverlayView.kt: Track per-stick lastTapTime. On ACTION_DOWN within
  zone, if now - lastTapTime < 300ms and doubleTapBinding >= 0, inject the
  bound button press. Otherwise set lastTapTime = now.
- TouchEditorPage.kt (StickPropertiesPanel): Add "Double-Tap Action" dropdown.
- Serialization: include in TouchLayoutRepository save/load and
  HumanReadableConfig export.

### 1D. Stacked controls cycle on repeated taps
- TouchEditorPage.kt hitTest(): Collect ALL matches at tap point into an
  ordered list (buttons > radials > sliders > sticks). If the currently-selected
  control is in the list, return the NEXT one (wrapping). Track lastHitCycle
  and lastHitIndex; reset when tap position changes significantly.

### 1E. Autoselect ordering in settings export
- ConfigImportExport.kt exportAll(): Also read autoselect via
  NativeAutoselectPatcher.readAutoselect() for d1/d2 and include as
  "autoselect_d1"/"autoselect_d2" arrays.
- importCombined(): If those keys exist, call writeAutoselect() to restore.

## Phase 2: Analog Triggers -- Half-Axis Binding

Root cause: User binds LT/RT to "Slide Up"/"Slide Down" (button functions,
kc indices 8/9). But LT/RT are axes, and buildJoyPairs() has no code path
for "axis control -> button function via half-axis". Binding is silently dropped.

Fix:
1. ControllerConfigPage.kt buildJoyPairs(): Add BUTTON_TO_HALF_AXIS map:
   "Slide Up" -> ("Slide U/D", positive), etc. When trigger axis is bound
   to a button function with a half-axis counterpart, emit the axis binding.
2. android_input.c: Add half-axis combiner. When two triggers are bound to
   the same kconfig axis (opposite signs), pre-combine their values:
   virtual_value = LT_raw - RT_raw.
3. kconfig already handles "Slide U/D" axis (index 19) -- no changes needed.

## Phase 3: LAN Permission Fix

Confirmed root cause: NEARBY_WIFI_DEVICES not declared in AndroidManifest.xml.
Android silently denies the runtime request.

Fix: Add `<uses-permission android:name="android.permission.NEARBY_WIFI_DEVICES" />`

## Phase 4: Settings Button Transparency

TouchOverlayView.kt drawAdminTrayPanel(): Remove
`canvas.drawColor((dimAlpha shl 24))` which draws full-screen black at
~53% opacity. Panel has its own dark background already.

## Phase 5: Save/Load Menu Back Button

1. android_input.c: Add `volatile int g_saveload_menu_active = 0;`
2. d2/main/state.c and d1/main/state.c: Set flag around newmenu_do2() call
   in state_get_savegame_filename().
3. android_input.c: Add nativeIsSaveLoadMenuActive() JNI query.
4. MainActivity.kt polling loop: Query flag, show skipButton with label
   "BACK" when active.

## Key Files

- TouchEditorPage.kt -- editor hitTest, canvas, properties panel
- TouchOverlayView.kt -- runtime overlay, mouse mode, admin tray
- TouchControl.kt -- data models (AnalogStickControl)
- TouchBindings.kt -- button/axis constants
- ControllerConfigPage.kt -- buildJoyPairs, trigger binding
- ConfigImportExport.kt -- export/import
- NativeAutoselectPatcher.kt -- autoselect JNI
- LanDiscoveryTab.kt -- LAN permission
- AndroidManifest.xml -- missing permission
- android_input.c -- JNI flags, axis handler
- d2/main/state.c, d1/main/state.c -- save/load menu
- d2/arch/sdl/joy.c, d1/arch/sdl/joy.c -- axis button handler
- SkipButtonView.kt -- overlay button
- MainActivity.kt -- overlay polling

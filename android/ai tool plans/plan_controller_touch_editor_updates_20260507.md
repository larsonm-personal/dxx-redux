# Controller and touch editor updates - research plan

Status legend: `[ ]` not started, `[~]` in progress, `[x]` done.

## Scope

- Add repeat scrolling/navigation in Android controller editing dialogs for held D-pad and stick-driven navigation.
- Move deadband controls up in single-axis and stick picker dialogs.
- Allow a 0% minimum for single-axis deadband where real trigger hardware rests at 0%.
- Add touch-editor edge selection and edge dragging for mouse-mode stick regions and single-axis regions.
- Add a forced touch control for remaining key touch items, initially headlight and pause, shown only for actions not already bound to touch controls.

## Research summary

- [x] Controller picker dialogs are all in `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt`.
  - `ButtonFunctionPickerDialog` is the single-axis trigger picker. It renders `AxisThresholdBar` at the top when a trigger threshold is present, but the threshold slider is currently at the bottom of the scroll content.
  - `StickPickerDialog` owns the left/right stick picker. The Y-axis live bar is near the top of the Y section, while its dead zone slider is lower after the binding radio group.
  - `DpadFunctionPickerDialog` owns D-pad assignment.
  - `verticalDpadFocusEscape` consumes up/down key events one step at a time.
  - Each picker has a local `ScrollState` and calls `ScrollArrows` only for visual indicators.
- [x] Stick axis-mode deadzone sliders already use `valueRange = 0f..95f`, but trigger/single-axis `ButtonFunctionPickerDialog` currently uses `5f..95f` and labels the control as `Threshold` regardless of axis function vs button function.
- [x] SetupActivity already synthesizes D-pad key transitions from HAT and stick motion while controller config dialogs are open.
  - `SetupActivity.handleControllerMotion` updates `controllerAxes`, `dpadAxes`, `axisGeneration`, and calls `synthesizeDpadTransition` for dialog navigation.
  - `dispatchKeyEvent` lets D-pad and A through to the dialog while picker dialogs are open.
- [x] Touch editor selection and drag logic is centralized in `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt`.
  - `hitTestAll` returns ordered `(type, index)` hits and already supports repeated taps cycling through stacked controls.
  - Current priority order is buttons, radials, sliders, sticks, diagnostics, axis regions.
  - `moveControl` moves whole controls; it does not resize region edges.
  - Mouse-mode stick regions use `AnalogStickControl.floatingZone`; single-axis regions use `AxisRegionControl.zone`.
- [x] Runtime touch dispatch is centralized in `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`.
  - `dispatchTouchButton` sends standard bindings through `InputMixer` and meta actions through `metaActionCallback`.
  - `pressLayoutButtonBinding` and `releaseLayoutButtonBinding` are the reusable standard button path.
  - Settings menu behavior is a `DiagnosticType.SETTINGS` special case plus a fallback default admin tray tab when no settings diagnostic exists.
- [x] Headlight and pause are already available as bindings.
  - Headlight is `TouchBindings.BTN_HEADLIGHT` and is D2-only.
  - Pause is `TouchBindings.META_PAUSE`, listed in `META_BUTTON_LABELS`, saved through controller `meta_bindings`, loaded by `MainActivity.loadMetaBindings`, and dispatched by `android_meta_actions.c` as `SDLK_PAUSE`.
  - No D1/D2 source changes appear necessary for pause binding.

## Recommended implementation plan

### Phase 1: General repeat navigation for controller editor dialogs

- [x] Add a reusable repeat-scroll/navigation helper for Compose pickers, probably in `DpadFocusUtils.kt` if it can stay generic, or as private helpers in `ControllerConfigPage.kt` if it needs picker-local state.
- [x] Prefer Android key repeat defaults where available: check `android.view.ViewConfiguration.getKeyRepeatTimeout()` for initial delay and `getKeyRepeatDelay()` for repeat interval. If compile/runtime support is awkward, fall back to requested constants: 500 ms initial delay and 125 ms interval.
- [x] Make the helper work for both physical D-pad key events and synthesized stick/HAT navigation events delivered to picker dialogs.
- [x] Apply it to `ButtonFunctionPickerDialog`, `StickPickerDialog`, and `DpadFunctionPickerDialog` around their scrollable content.
- [x] Keep normal focus movement intact for radio buttons, sliders, and footer buttons. Held up/down should scroll when focus movement reaches a scroll boundary or when the dialog content itself is the active navigation target.
- [ ] Consider a small pure unit test for repeat timing state if the repeat logic is factored into a non-Compose helper.

### Phase 2: Deadband control placement and 0% single-axis minimum

- [x] In `ButtonFunctionPickerDialog`, move the trigger threshold/deadband control to immediately below `AxisThresholdBar`.
- [x] Rename the label dynamically: use `Dead zone` when the current selection is a single-direction axis option from `HALF_AXIS_MAP`; keep `Threshold` for button-function bindings.
- [x] Let single-axis dead zone reach 0%. Recommended rule: slider min is 0 for axis-function selections and 5 for ordinary button threshold selections. If the selected function is not yet known, using 0 for trigger pickers is acceptable because real triggers can rest at 0%.
- [x] In `StickPickerDialog`, move the Y-axis dead zone slider directly below the Y-axis `AxisThresholdBar` before the Y binding radio group. Use a small local axis-section helper if it reduces duplicated X/Y layout.
- [x] Preserve existing analog stick default behavior from `DEFAULT_STICK_DEAD_ZONE = 10` and button-mode default behavior from `DEFAULT_AXIS_THRESHOLD = 30`.

### Phase 3: Touch editor region edge selection and dragging

- [x] Add edge encoding helpers in `TouchEditorPage.kt`, such as `encodeRegionEdge(index, edge)` and `decodeRegionEdge(value)`, with edges left, top, right, bottom.
- [x] Add edge hit testing for:
  - mouse-mode stick `floatingZone` edges from `layout.sticks` where `stick.mouseMode` is true;
  - single-axis `AxisRegionControl.zone` edges from `layout.axisRegions`.
- [x] Insert edge hits in `hitTestAll` immediately after button hits and before radial/slider/stick/body hits so repeated tap cycling sees buttons first and edges next.
- [x] Use the same hit oversize policy as buttons. The button hit multiplier is currently `1.3f`; for edges, compute a line hit slop from the default button radius and the extra 30% oversize, with a sane minimum for thin screens.
- [x] Add selected-edge drawing in `drawAllControls`, using the selected color on just the edge line so it is clear which side will move.
- [x] Extend drag handling so selected edge types resize the zone instead of moving the whole control.
- [x] Clamp edges so zones cannot invert or collapse. Use a small minimum width/height, for example 2% of the screen, unless an existing touch-layout constraint is preferred.
- [x] Keep the current whole-region selection and movement behavior for taps inside the region body.

### Phase 4: Forced remaining-actions touch control

- [x] Add a small action definition list for forced remaining key touch items, initially:
  - D2 only: `TouchBindings.BTN_HEADLIGHT` with label `Headlight`.
  - D1 and D2: `TouchBindings.META_PAUSE` with label `Pause`.
  - Leave the list easy to extend for future controls.
- [x] Add a helper that gathers all touch-bound actions from `TouchLayout`:
  - `buttons.binding`, `buttons.longPressBinding` when enabled;
  - `sticks` button-mode direction bindings and `doubleTapBinding`;
  - `radialMenus` segment bindings and center binding where action-based;
  - `dpads` directional bindings.
- [x] Compute remaining forced actions by subtracting the touch-bound set from the forced action list and filtering D1/D2-only actions.
- [x] Recommended data model: add a small `ActionListControl` or similar to `TouchControl.kt`/`TouchLayout` if editor positioning is required. Lower-line-count alternative: extend the settings-diagnostic pattern with a runtime fallback control and optional explicit editor control.
- [x] Reuse the settings-admin tray pattern in `TouchOverlayView`: closed transparent button, open overlay state, tap-outside closes, selection dispatches action and closes.
- [x] Draw the open menu as a vertical transparent list anchored where the button was. Each item should dispatch a quick press/release through the existing `dispatchTouchButton` or `metaActionCallback` path.
- [x] Do not show the forced control when the remaining forced action list is empty.
- [x] Avoid duplicating controller bindings in the touch-bound calculation. The requirement is based on other touch items, not physical controller assignments.

### Phase 5: Tests and validation

- [x] Add unit tests under `android/app/src/test/java/com/dxxredux/app/` for remaining-action filtering and D1/D2 action visibility.
- [x] Add unit tests for edge encode/decode and zone resize clamping if those helpers are pure functions.
- [ ] Extend existing admin tray or touch overlay tests if the forced action menu uses the settings/admin tray pattern.
- [x] Run Android Kotlin/unit validation:
  - `android\gradlew.bat :app:compileDebugKotlin :app:testDebugUnitTest`
- [x] Run code quality after implementation:
  - first check stale formatters with `android\stop-stale-formatters.ps1`
  - kill stale formatters if needed with `android\stop-stale-formatters.ps1 -Kill`
  - run `android\run-code-quality.ps1 -Fix`
- [x] Run the project-required build/test pass appropriate for the final implementation. If native files are untouched, Android compile plus unit tests may be enough for this tranche, but full Android build verification is safer because touch/controller code is user-facing.

## Result

- Implemented controller picker hold-repeat navigation in `ControllerConfigPage.kt` using Android key-repeat defaults when available, with a 500 ms / 125 ms fallback.
- Moved single-axis and stick deadband controls next to their live bars, and allowed 0% dead zone for trigger single-axis assignments.
- Added edge hit-testing, edge highlighting, and drag-resize for mouse-mode stick zones and axis regions in `TouchEditorPage.kt`.
- Added a runtime fallback remaining-actions touch control in `TouchOverlayView.kt` that shows a transparent `More` button and vertical popup list for unbound headlight and pause actions.
- Added unit coverage in `RemainingKeyTouchActionsTest.kt` and `TouchEditorZoneEdgeTest.kt`.
- Validation passed:
  - `android\gradlew.bat :app:compileDebugKotlin :app:testDebugUnitTest`
  - `android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt','android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt','android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt','android/app/src/test/java/com/dxxredux/app/TouchEditorZoneEdgeTest.kt','android/app/src/test/java/com/dxxredux/app/RemainingKeyTouchActionsTest.kt')`
  - `run-windows-build.ps1 -Target both -Compiler vs2022-community`

## Resolved choices

- The forced remaining-actions control was implemented as a runtime fallback rather than a persisted layout item.
- Single-axis trigger controls now allow 0% when the current trigger assignment is a single-direction axis/dead-zone mode, while button-threshold assignments keep the 5% floor.
- Repeat navigation uses Android key-repeat defaults when available and falls back to the requested 500 ms initial delay with 125 ms repeat intervals.
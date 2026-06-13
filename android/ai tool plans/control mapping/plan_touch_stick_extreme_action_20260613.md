# Touch Stick Extreme Action Design

## Goal
- Add a configurable special case for touch sticks: when a stick is dragged beyond normal full scale in a configured direction, trigger an extra action
- Initial use: on the throttle stick's positive throttle direction, hold afterburner while deflection is greater than 1.5x normal full scale
- Keep the design general enough for future stick directions, controls, and either held or tapped extra actions

## Phase 1: Study
- [done] Locate touch stick layout, serialization, editor UI, and runtime input emission
- [done] Locate action/button binding paths for afterburner and general game actions
- [done] Identify test seams for pure Kotlin behavior and on-device/runtime smoke verification

## Phase 2: Design
- [done] Define data model additions with conservative defaults
- [done] Define runtime behavior for threshold, hysteresis, held/tapped modes, and cancellation
- [done] Define editor UI placement and wording
- [done] Define migration/compatibility expectations before first Android release
- [done] Define focused verification plan

## Phase 3: Implementation Follow-Up
- [pending] Implement after the design is accepted
- [pending] Run scoped code quality and relevant unit tests
- [pending] Run an Android debug build

## Existing Code Shape
- Touch layouts are modeled in `TouchControl.kt`
  - `AnalogStickControl` owns axes, invert flags, response curve, button-mode direction bindings, and double-tap action/mode
  - `TouchLayout.toJson()` persists raw numeric IDs; `HumanReadableConfig` exports/imports readable axis and binding names for bundled presets and config import/export
- Runtime stick handling lives in `TouchOverlayView.kt`
  - `updateStickFromTouch()` computes `dx`, `dy`, clamps the rendered thumb to `radius`, emits normal axes through `axisCallback`, and dispatches button-mode directions through `dispatchTouchButton()`
  - The unclamped `dist`, `dx`, and `dy` are available before normal axis clamping, which is the right place to detect "1.5x full scale"
  - `dispatchTouchButton()` already routes standard game buttons through `InputMixer` and meta actions through `metaActionCallback`
- Button IDs are centralized in `TouchBindings.kt`
  - `BTN_AFTERBURNER = 27`
  - D2-only bindings are identified by `D2_ONLY_BUTTONS`
- The default movement stick maps `axisY = Left Stick Y`, `invertY = false`
  - Physical stick-up emits negative `Left Stick Y`
  - D1/D2 `kconfig.c` subtracts the throttle joystick axis when throttle is not inverted, so negative `Left Stick Y` means positive forward thrust in the current default setup

## Recommended Design

### Data Model
Add a small reusable config object and attach a list of them to `AnalogStickControl`.

```kotlin
enum class StickExtremeAxis { X, Y }
enum class StickExtremeDirection { NEGATIVE, POSITIVE }
enum class StickExtremeActionMode { HOLD, PULSE_ON_ENTER }

data class StickExtremeAction(
    val enabled: Boolean = false,
    val axis: StickExtremeAxis = StickExtremeAxis.Y,
    val direction: StickExtremeDirection = StickExtremeDirection.NEGATIVE,
    val threshold: Float = 1.5f,
    val releaseThreshold: Float = 1.35f,
    val binding: Int = TouchBindings.BTN_AFTERBURNER,
    val mode: StickExtremeActionMode = StickExtremeActionMode.HOLD,
)
```

Initial `AnalogStickControl` field:

```kotlin
val extremeActions: List<StickExtremeAction> = emptyList()
```

Notes:
- Use a list, even though the editor initially creates at most one. This makes later "positive X triggers slide-on", "negative Y taps reverse camera", or multiple direction specials possible without reshaping the schema.
- Defaults are off and harmless. Existing saved layouts parse to `emptyList()`.
- For a lower-line-count initial implementation, the editor can expose only the first/list-singleton action while the runtime supports all list entries.

### JSON Shape
Raw runtime JSON:

```json
"extremeActions": [
  {
    "enabled": true,
    "axis": "Y",
    "direction": "NEGATIVE",
    "threshold": 1.5,
    "releaseThreshold": 1.35,
    "binding": 27,
    "mode": "HOLD"
  }
]
```

Human-readable JSON should use binding names:

```json
"extremeActions": [
  {
    "enabled": true,
    "axis": "Y",
    "direction": "NEGATIVE",
    "threshold": 1.5,
    "releaseThreshold": 1.35,
    "binding": "Afterburner",
    "mode": "HOLD"
  }
]
```

Implementation locations:
- `TouchControl.kt`
  - Add enums/data class
  - Add `extremeActions` to `AnalogStickControl`
  - Serialize only non-empty `extremeActions`
  - Parse absent array as empty
  - Clamp `threshold` to a conservative range, for example `1.01f..2.5f`
  - Clamp `releaseThreshold` below `threshold`, for example `min(raw, threshold - 0.05f)` and at least `1.0f`
- `HumanReadableConfig.kt`
  - In `stickToHuman()`, convert each action `binding` with `TouchBindings.bindingToName()`
  - In `parseStick()`, resolve action `binding` with `resolveBinding()`

### Runtime Behavior
Keep normal axis emission unchanged. The axis still tops out at `-1..1`; the extreme action is a second output generated from the same touch drag.

Detection should use the pre-clamp directional component:

1. In `updateStickFromTouch()`, compute `unclampedX = (px - cx) / s.radius` and `unclampedY = (py - cy) / s.radius` before the current circle-edge clamp
2. Apply the stick's `invertX` or `invertY` to the corresponding unclamped component, matching emitted axis-space
3. For each enabled `StickExtremeAction`, choose the signed component by axis and direction
4. Enter when component magnitude crosses `threshold`
5. Exit when component falls below `releaseThreshold`, when the finger lifts, when the overlay deactivates, when automap/game variant filtering hides the binding, or when pointer stealing resets the stick

This means:
- Default move-stick forward afterburner should be `axis = Y`, `direction = NEGATIVE`, `binding = Afterburner`, `mode = HOLD`, `threshold = 1.5`, `releaseThreshold = 1.35`
- If the user inverts that touch stick, the same negative emitted-axis direction continues to mean "forward" for the game because detection happens after the touch-level invert
- Sensitivity, deadzone, and response curve should not affect the threshold. The threshold is physical drag distance measured in stick radii

### Runtime State
Extend `StickState` with one runtime state bit per configured action.

```kotlin
val extremePressed = BooleanArray(control.extremeActions.size)
```

If the list length can change during editor live update, rebuild states already recreates `StickState`, so this stays simple.

Dispatch tags should be unique per stick and action:

```kotlin
"touch:extreme${stickIndex}:$actionIndex"
```

For `HOLD`:
- On enter, call `dispatchTouchButton(binding, true, tag)`
- On exit/reset, call `dispatchTouchButton(binding, false, tag)`
- Because `InputMixer` OR-mixes buttons, this does not interfere with a separate visible afterburner button, controller button, or another touch source

For `PULSE_ON_ENTER`:
- On enter, reuse the existing delayed pulse behavior from double-tap or extract a generic `fireTouchPulse(binding, tag)`
- Do not fire repeatedly while still beyond the threshold
- Re-arm only after falling below `releaseThreshold`

### Visibility And Game Variant Filtering
Add a helper similar to `stickBindingVisibleInCurrentMode()`:

```kotlin
private fun touchBindingAllowedInCurrentMode(binding: Int): Boolean {
    if (gameVariant == "d1" && binding in TouchBindings.D2_ONLY_BUTTONS) return false
    return !automapActive || automapTouchButtonVisible(binding)
}
```

Use it for extreme actions. If it becomes false while held, release the action immediately.

This prevents a D2-only afterburner action from leaking into D1 and prevents hidden/automap-inappropriate bindings from staying pressed.

### Editor UI
Add an "Extreme Action" block to `StickPropertiesPanel`, below the normal axis/button-mode settings and before double-tap.

Initial UI:
- Toggle: `Extreme Action`
- Binding picker: default `Afterburner`
- Axis picker: `X` / `Y`
- Direction picker: `Negative` / `Positive`
- Mode picker: `Hold` / `Tap once`
- Threshold slider: `1.1..2.5`, default `1.5`

To keep the first version approachable:
- When enabling on a stick whose `axisY` label is `Fwd/Back`, default to `Y negative`
- When enabling on other sticks, still default to `Y negative`, but let the user change it
- Hide `releaseThreshold` from UI initially and derive it as `threshold - 0.15`, clamped to at least `1.0`

Potential label text:
- Section label: `Extreme Action`
- Toggle: `Enabled`
- Mode labels: `Hold while pushed past edge`, `Tap once past edge`

### Bundled Presets
Do not enable the action in all bundled presets by default without play testing. The first implementation should make it easy to turn on from the editor.

Optional follow-up once the behavior feels right:
- Add the disabled/default action object to `simple.json`, `advanced.json`, and `claw.json` for discoverability
- Or enable it only in the Advanced preset's `move` stick for D2-oriented testing

I recommend leaving bundled presets behavior-neutral for the first patch and relying on editor defaults when the user turns the option on.

### Future Generalization
This model can later be reused by:
- `SliderControl`: compare slider travel beyond its normal track if the UI is allowed to drag past the end
- `AxisRegionControl`: compare drag distance beyond a configured region/reference origin
- Controller axes: the same `ExtremeAxisAction` concept could attach to controller config, though physical controllers usually cannot exceed 1.0, so their threshold model would need to mean "near end" rather than "past end"

If extending beyond sticks, consider renaming the data class to `AxisExtremeAction` and putting it in a shared control behavior section. For the first implementation, stick-local is simpler and avoids speculative plumbing.

## Verification Plan
- Unit tests:
  - `TouchControl.fromJson()` parses missing `extremeActions` as empty
  - Raw `TouchLayout.toJson()` round-trips an extreme action
  - `HumanReadableConfig.touchLayoutToHumanJson()` writes `"Afterburner"` and `humanJsonToTouchLayout()` resolves it back to `BTN_AFTERBURNER`
  - A small pure helper reports enter/held/release with threshold `1.5` and release `1.35`
  - D1 filtering rejects `BTN_AFTERBURNER` for extreme action dispatch
- Runtime/on-device smoke:
  - Configure `move` stick with `Y negative`, `Afterburner`, `Hold`
  - Drag to normal full forward: throttle axis reaches full scale, afterburner stays off
  - Drag past `1.5x`: afterburner button state turns on
  - Drag back below `1.35x`: afterburner turns off while throttle remains controlled
  - Lift/cancel/deactivate overlay: afterburner is released
  - Run with a separate afterburner button also held to confirm `InputMixer` OR behavior releases only after both sources release

## Open Decisions
- Whether the first shipped preset should leave this off, or enable it by default on the movement stick for D2
- Whether "tap once" should be included in the first implementation or staged after the held-afterburner path
- Whether the editor should expose a visual outer ring at `1.5x` around sticks with an enabled extreme action. This is useful feedback, but not required for the initial behavior

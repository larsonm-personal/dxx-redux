# Plan: Touch Interface Editor & Advanced Touch Controls

## TL;DR
Build a customizable touch control system for the Descent Android port, modeled after mobile shooters (CoD Mobile, PUBG Mobile, Apex Legends Mobile). Replace the hardcoded touch layout in TouchOverlayView with a data-driven system of positioned/sized widgets defined in a JSON layout file. Add a visual drag-and-drop editor, dual-stick support, gyro aiming, radial menus (guidebot, weapons), and user-customizable presets. Continue emulating a joystick/controller through the existing JNI bridge — no game engine changes needed.

## Phase Status

| Phase | Status | Key Files |
|-------|--------|-----------|
| 1: Data Model & Layout Persistence | ✅ Complete | TouchBindings.kt (96 lines), TouchControl.kt (~520 lines), TouchLayoutRepository.kt (~170 lines) |
| 2: Refactor TouchOverlayView | ✅ Complete | TouchOverlayView.kt (~840 lines, fully data-driven) |
| 3: Gyro Input | ✅ Complete | GyroInputManager.kt (~180 lines), wired in TouchOverlayView + MainActivity |
| 4: Touch Layout Editor | ✅ Complete | TouchEditorPage.kt (~790 lines), wired in SetupActivity |
| 5: Radial Menus (Guidebot + Weapons) | ✅ Complete | TouchOverlayView.kt, TouchControl.kt, TouchLayoutRepository.kt, TouchEditorPage.kt, MainActivity.kt |
| 6: Polish & Presets | ⬜ Not Started | — |

## Current State (post Phase 3)

### Implemented files
- **TouchBindings.kt** (96 lines): Constants for 23 button indices (BTN_FIRE_PRIMARY=0 through BTN_TOGGLE_BOMB=54), 6 axis indices (AXIS_LEFT_X=0 through AXIS_RTRIGGER=5), label maps, and layout constraint constants (size/opacity/sensitivity/deadzone/exponent limits). Indices mirror `kc_joystick[]` in C.
- **TouchControl.kt** (~520 lines): 5 enums (ResponseCurve, DPadMode, ButtonShape, SliderOrientation, GyroActivation). `applyResponseCurve()` helper. 9 data classes with full JSON round-trip serialization (FloatingZone, AnalogStickControl, ButtonControl, SliderControl, RadialSegment, RadialMenuControl, DPadControl, GyroConfig, TouchLayout). All use percentage-based positioning (0-100).
- **TouchLayoutRepository.kt** (~170 lines): `load()`/`save()` to `touch_layout.json`. 3 presets: Simple (1 stick, 2 buttons, map), Advanced (2 sticks with exponential curves, 6 buttons, throttle slider), Claw (2 floating sticks, 5 buttons, gyro enabled with TOUCH_STICK activation).
- **TouchOverlayView.kt** (~840 lines): Fully data-driven. StickState/ButtonState inner classes. `setLayout()`/`getLayout()` API. Computes geometry from layout percentages in `computeGeometry()`. Dynamic hit-testing with floating stick support, toggle buttons, haptic feedback. `updateStickFromTouch()` applies deadzone → response curve → sensitivity → inversion. `updateGyroStickActive()` bridges to GyroInputManager. Music controls and automap overlay remain hardcoded (not layout-driven).
- **GyroInputManager.kt** (~180 lines): SensorEventListener using TYPE_GAME_ROTATION_VECTOR. Quaternion-based rotation delta (refR^T × curR matrix). Extracts yaw/pitch, applies deadzone/sensitivity/inversion. Three activation modes: ALWAYS, TOUCH_STICK (via `rightStickActive` set by TouchOverlayView), ADS_ONLY (via `toggledOn`). Lifecycle-managed `resume()`/`pause()`. `calibrate()` resets reference orientation.
- **MainActivity.kt**: Creates GyroInputManager when `layout.gyro.enabled`, wires `axisCallback` to `nativeJoystickAxis`, calls `resume()`/`pause()` in lifecycle, passes gyro to `touchOverlay.gyroManager`.

### Existing infrastructure (unchanged)
- **android_input.c**: JNI bridge with `nativeJoystickAxis(axis, value)` (6 axes) and `nativeJoystickButton(button, pressed)`. Also has automap-specific input. Game supports up to 128 axes and 128 buttons per joystick.
- **ControllerConfigPage.kt**: Existing Compose-based gamepad config UI, accessed from SetupActivity's ControllerSection. Saves to `controller_config.json`.
- **kconfig system (d2)**: 56-entry `kc_joystick[]` array. 6 analog axes. Per-axis deadzone/sensitivity at engine level.
- **SDL 1.2.15**: No sensor/gyro API. Gyro done at Android layer.

### Implementation notes discovered during build
- **Gyro + stick are independent writers**: Both call `nativeJoystickAxis()` separately. The engine sees the last value written per axis per frame. This is functionally correct because gyro fires at sensor rate (~100Hz) while touch fires on ACTION_MOVE events — they naturally interleave. True additive combining would require tracking per-axis stick values in the overlay, which adds complexity for marginal benefit.
- **Floating stick zone** is computed in pixels from percentage in `computeGeometry()` and checked in `onTouchEvent` — stick appears at first touch position within the zone.
- **Toggle buttons** maintain `toggled` state across pointer up/down cycles. Visual feedback: brighter fill when toggled on.
- **Haptic feedback** calls `performHapticFeedback(VIRTUAL_KEY)` per control's `hapticFeedback` flag.
- **Passthrough taps**: touches that don't hit any control are tracked; on release, `tapPassthroughCallback` fires (for "press any key" screens).
- **Button hit area**: 1.3× radius for generous touch targets.
- **Automap overlay**: Separate code path in `onTouchEvent` when `automapActive=true`. 1-finger drag → heading/pitch, 2-finger pinch → zoom/rotate/translate. Automap buttons (center, markers, MAP) rendered at top.

### Phase 5 implementation notes (Radial Menus)
- **Data model**: `RadialMenuControl` extended with `centerLabel: String` and `centerBinding: Int` fields for the center zone action (e.g., guidebot "Clear Goal"). Both serialize to JSON with backward-compatible `optString`/`optInt` deserialization.
- **`RadialSegment.binding`**: Stores Android `KeyEvent.KEYCODE_*` values (KEYCODE_0=7 through KEYCODE_9=16). Fired via `keyCallback` → `nativeKeyEvent()` in JNI — no new C code needed. Unicode chars derived from keycodes via `keycodeToUnicode()`.
- **Shared drawing code**: Single `drawRadialMenu()` method handles both guidebot (9 segments) and weapon wheel (10 segments), parameterized by `segments.size`. Draws either trigger icon (when closed) or full pie wheel (when open).
- **Trigger icon**: Small circle (button-sized, `base * 0.05f * sizeMult`) with 4-char label. Tapping opens the radial wheel.
- **Open wheel**: Pie slices drawn via `android.graphics.Path` (moveTo center → arcTo → close). Wheel radius = `base * 0.18f * sizeMult`. Selected segment expands to 1.15× radius and uses darker fill color (0x88334455 vs 0x44888888). Segments start from top (-90°).
- **Center zone**: Circle at `0.22× radius`. When `centerLabel` is set, fires `centerBinding` on release. When empty, center = cancel (no action). Guidebot wheel has center "Clear" → KEY_0 (clear escort goal). Weapon wheel has no center action.
- **Touch interaction**: Press-hold on trigger opens wheel → drag to select segment (computed by angle from center, haptic feedback on segment transition) → release fires key down+up via `keyCallback`. Releasing on center fires center action or cancels. Releasing outside wheel = no action.
- **Segment selection**: `updateRadialSelection()` uses `atan2` angle + 90° rotation to map finger direction to segment index. Center detected by distance < 0.22× radius. Haptic feedback on every segment change.
- **Draw order**: Closed triggers drawn before open wheels so open wheels appear on top of everything.
- **Presets**: Guidebot wheel (9 segments + center "Clear") and weapon wheel (10 segments: 5 primary + 5 secondary) added to Advanced and Claw presets at non-overlapping positions.
- **Editor support**: `TouchEditorPage.kt` updated with radial menu canvas drawing (ghost wheel showing segment layout), hit testing, drag-to-move, properties panel (ID, center label, size, opacity, haptic, segment list display), and "Add Radial Menu" option.
- **keyCallback**: New `(Int, Int, Int) -> Unit` callback on `TouchOverlayView` (action, androidKeyCode, unicodeChar). Wired in `MainActivity` to `nativeKeyEvent()`. Clean separation: radial menus use keyCallback, sticks use axisCallback, buttons use buttonCallback.

## Mobile Shooter Control Research & Strategy

### Common Mobile FPS Control Schemes (CoD Mobile, PUBG Mobile, Fortnite, Apex Legends Mobile)
1. **Dual-stick**: Left stick = move (forward/back/strafe), right stick = aim (pitch/yaw). Universal standard.
2. **Gyro aiming**: Fine-tune aim via phone tilt. Usually supplements the right stick. Three activation modes: always-on, while-touching-aim-stick, toggle-button.
3. **Floating sticks**: Stick appears wherever you first touch within a zone (rather than fixed position). Reduces thumb fatigue. Most games offer both fixed and floating per-stick.
4. **Non-linear response curves**: Most mobile shooters apply exponential or S-curve scaling to stick output so the center has fine control and edges have fast gross movement. CoD Mobile calls this "ADS sensitivity curve." Apex uses a tunable exponent. Critical for aiming precision.
5. **Per-stick sensitivity**: Each stick has its own sensitivity multiplier (separate from the engine's per-axis sensitivity). The touch sensitivity controls how much of the axis range the player covers with their thumb movement. The engine sensitivity controls how the axis value maps to game movement.
6. **Button clusters**: Fire buttons near right thumb, secondary actions arranged around the right stick. Usually 3-6 buttons.
7. **Radial menus**: Weapon/item wheels activated by press-and-hold, slide to select. Good for many options in small space.
8. **Opacity customization**: Per-control opacity (20-100%) PLUS a global opacity multiplier that dims everything uniformly. Most games have both.
9. **Size customization**: Per-control size (50-200% of default). Buttons have min-size limits so they stay tappable.
10. **Multi-finger support**: Competitive "claw grip" players use 4-6 fingers. Controls must be independently trackable with no conflicts.
11. **Presets**: 2-3 built-in layouts (Simple, Advanced, Claw) plus custom.
12. **Haptic feedback**: Short vibration on button tap. One line of Android API per event. Standard UX.

### Descent-Specific Challenges (6DOF vs 3DOF shooter)
Descent has 6 degrees of freedom vs a typical shooter's 3. Extra axes:
- **Vertical slide** (up/down thrust) — D-pad or buttons
- **Bank/roll** — buttons or dedicated axis
- **Forward/reverse throttle** — left stick Y or separate slider

### Recommended Default Layout
- **Left stick**: Forward/reverse (Y) + strafe L/R (X), with exponential response curve
- **Right stick**: Pitch (Y) + yaw/heading (X), with exponential response curve, floating mode
- **Gyro** (optional): Pitch + yaw fine adjustment, layered on right stick
- **Bank buttons**: Two buttons (bank left, bank right)
- **Slide D-pad**: 4-way pad in "swipe" mode for vertical+lateral slide thrust
- **Fire primary**: Large button, right side
- **Fire secondary**: Button near fire primary
- **Afterburner**: Button
- **MAP button**: Top area
- **Flare/bomb**: Buttons
- **Weapon cycle**: Two small buttons or radial menu
- **Guidebot wheel**: Radial menu (press-hold, slide to select from 9 commands)
- **Rear view**: Hold button

## Control Widget Types

### 1. AnalogStick
- Circular touch zone, renders base ring + thumb indicator
- Outputs 2 axes (configurable: which kconfig axis pair)
- Properties:
  - `fixedCenter` (bool): Fixed position vs floating (appears at first touch within zone)
  - `floatingZone` (optional rect): For floating sticks, the screen region where first-touch activates the stick. Default: half the screen on that stick's side.
  - `deadzone` (int 0-50%): Inner dead zone where small movements produce zero output. Applied before response curve.
  - `sensitivityX`, `sensitivityY` (float 0.2-3.0, default 1.0): Multiplier on axis output after response curve. Independent per axis so players can have different horizontal/vertical sensitivity.
  - `responseCurve` (enum): `linear`, `exponential`, `s_curve`. Controls how the raw displacement maps to axis output.
  - `responseCurveExponent` (float 1.0-4.0, default 2.0): When `responseCurve` is `exponential`, this is the power. 1.0 = linear, 2.0 = quadratic (fine center, fast edges), 3.0 = cubic (very fine center). Only used when curve is `exponential`.
  - `axisXBinding`, `axisYBinding` (int): Which JNI axis index (0-5+)
  - `invertX`, `invertY` (bool)
- **Response curve math** (applied in Kotlin before sending to JNI):
  - `linear`: `output = input` (current behavior)
  - `exponential`: `output = sign(input) * |input|^exponent` where `input` is the deadzone-adjusted normalized value (-1 to 1)
  - `s_curve`: `output = sign(input) * (3*|input|^2 - 2*|input|^3)` (smoothstep — soft start, fast middle, soft end)
  - After curve: `output *= sensitivity`; clamp to -1..1

### 2. Button
- Circular or rounded-rect touch zone
- Outputs a joystick button press (configurable button index)
- Properties:
  - `label` (string): Display text or icon glyph
  - `buttonBinding` (int): kconfig button index
  - `isToggle` (bool): Hold mode (default) vs toggle mode (for slide-on, bank-on)
  - `shape` (enum): `circle`, `rounded_rect`. Rounded rect is better for labels like "MAP", "REAR"
  - `haptic` (bool, default true): Vibrate on press
- Min size: 36dp equivalent (Android touch target guideline)

### 3. Slider
- Rectangular vertical/horizontal drag zone
- Outputs 1 axis value proportional to drag distance
- Properties:
  - `orientation` (enum): `vertical`, `horizontal`
  - `axisBinding` (int): Which JNI axis index
  - `invert` (bool)
  - `sensitivity` (float 0.2-3.0): Multiplier on output
  - `responseCurve` (enum): `linear`, `exponential` — same options as stick
  - `responseCurveExponent` (float 1.0-4.0)
  - `springBack` (bool, default true): Returns to center when released. False = stays at last position (for throttle that holds speed)
- Use case: throttle control, fine bank axis

### 4. RadialMenu
- Press-and-hold activates; renders a wheel of N segments around the touch point
- User slides to segment, releases to select
- Properties:
  - `segments[]`: Array of `{label, action}` entries
  - `action` types: `"key:SDLK_1"` (inject key), `"button:3"` (joystick button), `"guidebot_N"` (shorthand for guidebot command N)
  - `cancelOnCenter` (bool, default true): Releasing without sliding to a segment cancels
  - `haptic` (bool, default true): Vibrate on segment change during slide
- Special instances:
  - **Guidebot wheel**: 9 segments + center cancel. Injects KEY_1-KEY_9 / KEY_0 per `set_escort_special_goal()` in escort.c. Only shown when guidebot present in level (queryable via JNI flag).
  - **Weapon select**: Primary (5 segments) and secondary (5 segments), or combined wheel

### 5. DPad
- Multi-modal directional control with three operating modes:
- **Properties common to all modes**:
  - `upBinding`, `downBinding`, `leftBinding`, `rightBinding` (int): Button indices for each direction
  - `mode` (enum): `individual_buttons`, `connected_buttons`, `swipe_stick`
  - `diagonals` (bool, default false): Allow two simultaneous directions. When true, touching the diagonal area between two directions activates both.
  - `haptic` (bool, default true)

- **Mode: `individual_buttons`**
  - Four separate circular buttons arranged in a cross pattern
  - Each button is an independent touch target
  - Can be touched simultaneously (two fingers for diagonal movement)
  - Visual: Four separate circles with direction arrows/labels

- **Mode: `connected_buttons`**
  - Traditional D-pad appearance: four connected directional zones in a single touch target
  - Single finger slides between directions
  - Touching the boundary between two adjacent zones activates both (diagonal)
  - Touch point mapped to quadrant(s) based on angle from center
  - With `diagonals: true`: 8 zones (N, NE, E, SE, S, SW, W, NW). Each zone is 45 degrees. Cardinal zones activate one button, diagonal zones activate two adjacent buttons.
  - With `diagonals: false`: 4 zones (N, E, S, W). Each zone is 90 degrees.
  - Visual: Single diamond/cross shape with direction indicators

- **Mode: `swipe_stick`**
  - Looks and feels like an analog stick but outputs discrete button presses instead of continuous axis values
  - Dragging past a configurable threshold (% of radius) in a direction activates that direction's button
  - `swipeThreshold` (float 0.1-0.9, default 0.3): How far from center (as fraction of control radius) before a direction activates
  - Returns to center on release (all buttons released)
  - Can activate diagonals (two adjacent buttons) when dragged to a diagonal angle past threshold
  - Visual: Same ring+thumb as an analog stick, but with direction indicators. Thumb snaps to the active direction rather than following the finger smoothly.
  - Good for: slide up/down/left/right, where analog precision isn't needed but quick directional input is

### 6. GyroControl (non-visual, settings-driven input source)
- Uses Android SensorManager `TYPE_GAME_ROTATION_VECTOR` (fused gyro+accel, no magnetometer drift)
- Maps phone tilt deltas to configurable axes (usually pitch + yaw)
- Properties:
  - `enabled` (bool)
  - `sensitivityX`, `sensitivityY` (float 0.1-5.0): Per-axis multiplier
  - `axisXBinding`, `axisYBinding` (int): Which JNI axis indices
  - `activationMode` (enum): `always`, `touch_right_stick` (only gyro when right stick is touched), `toggle` (a dedicated button toggles gyro)
  - `responseCurve` (enum): `linear`, `exponential` — same as sticks. Exponential gyro gives fine control for small tilts.
  - `responseCurveExponent` (float 1.0-4.0)
  - `invertX`, `invertY` (bool)
- Output: Kotlin sums stick value + gyro delta before calling `nativeJoystickAxis()`. No new JNI function needed.
- Lifecycle: Register/unregister sensors on app pause/resume.
- Calibrate button in UI: captures current orientation as the neutral point.

## Global Layout Settings

In addition to per-control properties, the `TouchLayout` has global settings:

- `globalOpacity` (float 0.2-1.0, default 0.7): Multiplied with each control's individual opacity. Effective opacity = `globalOpacity * control.opacity`. So a control at 0.8 with global at 0.5 renders at 0.4. Lets users dim all controls at once without changing individual settings.
- `hapticEnabled` (bool, default true): Master switch for all haptic feedback. When false, no control vibrates regardless of its individual `haptic` setting. When true, individual `haptic` settings are honored.

## Data Structure: TouchLayout (JSON)

```json
{
  "version": 2,
  "name": "Default 6DOF",
  "globalOpacity": 0.7,
  "hapticEnabled": true,
  "controls": [
    {
      "id": "left_stick",
      "type": "analog_stick",
      "x": 0.12, "y": 0.75,
      "size": 1.0,
      "opacity": 0.6,
      "floating": false,
      "axisX": 2, "axisY": 5,
      "invertX": false, "invertY": false,
      "deadzone": 15,
      "sensitivityX": 1.0, "sensitivityY": 1.0,
      "responseCurve": "exponential",
      "responseCurveExponent": 2.0
    },
    {
      "id": "right_stick",
      "type": "analog_stick",
      "x": 0.85, "y": 0.65,
      "size": 1.0,
      "opacity": 0.5,
      "floating": true,
      "floatingZone": {"left": 0.5, "top": 0.0, "right": 1.0, "bottom": 1.0},
      "axisX": 0, "axisY": 1,
      "invertX": false, "invertY": true,
      "deadzone": 10,
      "sensitivityX": 1.0, "sensitivityY": 0.8,
      "responseCurve": "exponential",
      "responseCurveExponent": 2.2
    },
    {
      "id": "fire_primary",
      "type": "button",
      "x": 0.90, "y": 0.40,
      "size": 1.2,
      "opacity": 0.6,
      "label": "FIRE",
      "buttonIndex": 0,
      "toggle": false,
      "shape": "circle",
      "haptic": true
    },
    {
      "id": "slide_dpad",
      "type": "dpad",
      "x": 0.12, "y": 0.35,
      "size": 0.8,
      "opacity": 0.5,
      "mode": "swipe_stick",
      "swipeThreshold": 0.3,
      "diagonals": true,
      "upBinding": 5,
      "downBinding": 6,
      "leftBinding": 3,
      "rightBinding": 4,
      "haptic": true
    },
    {
      "id": "throttle",
      "type": "slider",
      "x": 0.04, "y": 0.55,
      "size": 1.0,
      "opacity": 0.4,
      "orientation": "vertical",
      "axisBinding": 5,
      "invert": false,
      "sensitivity": 1.0,
      "responseCurve": "linear",
      "springBack": false
    },
    {
      "id": "guidebot_wheel",
      "type": "radial_menu",
      "x": 0.05, "y": 0.30,
      "size": 1.0,
      "opacity": 0.5,
      "segments": [
        {"label": "Energy",    "action": "key:49"},
        {"label": "EnrgyCtr",  "action": "key:50"},
        {"label": "Shield",    "action": "key:51"},
        {"label": "Powerup",   "action": "key:52"},
        {"label": "Robot",     "action": "key:53"},
        {"label": "Hostage",   "action": "key:54"},
        {"label": "Scram",     "action": "key:55"},
        {"label": "Spew",      "action": "key:56"},
        {"label": "Exit",      "action": "key:57"}
      ],
      "cancelOnCenter": true,
      "haptic": true
    }
  ],
  "gyro": {
    "enabled": false,
    "sensitivityX": 1.0,
    "sensitivityY": 1.0,
    "axisX": 0,
    "axisY": 1,
    "activation": "touch_right_stick",
    "responseCurve": "exponential",
    "responseCurveExponent": 1.5,
    "invertX": false,
    "invertY": false
  }
}
```

- **Positions** (`x`, `y`): Normalized 0.0-1.0 (fraction of screen width/height), center of widget
- **Size**: Multiplier on default pixel size (0.5-2.0, clamped)
- **Opacity**: 0.2-1.0 per-control, multiplied by `globalOpacity`

## Response Curve Detail

The response curve is implemented in the Kotlin touch layer, shaping the signal before it reaches JNI. This is separate from the engine's kconfig sensitivity system (which further scales the signal). The two layers serve different purposes:

| Layer | Purpose | Where configured |
|-------|---------|-----------------|
| Touch response curve | Shape feel of thumb-on-glass: precision in center, speed at edges | Per-stick in touch_layout.json |
| Touch sensitivity | Scale magnitude of touch output | Per-axis per-stick in touch_layout.json |
| Engine deadzone | Ignore small axis values (joystick noise) | Per-axis in playsave/kconfig |
| Engine sensitivity | Scale axis contribution to movement rate | Per-axis in playsave/kconfig |

### Implementation (in Kotlin, per stick update):

```kotlin
fun applyResponseCurve(rawInput: Float, deadzone: Float, curve: ResponseCurve,
                       exponent: Float, sensitivity: Float): Float {
    // 1. Apply deadzone: remap so that (deadzone..1.0) becomes (0.0..1.0)
    val abs = abs(rawInput)
    if (abs < deadzone) return 0f
    val adjusted = (abs - deadzone) / (1f - deadzone)

    // 2. Apply response curve
    val curved = when (curve) {
        LINEAR -> adjusted
        EXPONENTIAL -> adjusted.pow(exponent)  // 2.0 = quadratic, fine center
        S_CURVE -> 3f * adjusted * adjusted - 2f * adjusted * adjusted * adjusted
    }

    // 3. Apply sensitivity and restore sign
    return (sign(rawInput) * curved * sensitivity).coerceIn(-1f, 1f)
}
```

### Visualization for the editor
When editing a stick's response curve properties, show a small preview graph (128x128 px) of input-vs-output. X axis = raw displacement, Y axis = output. Updates live as the user adjusts exponent/sensitivity sliders. This makes the abstract numbers tangible.

## Steps

### Phase 1: Data Model & Layout Persistence (foundation)

1. **Create `TouchControl.kt`** — Data classes for the control model:
   - Sealed class hierarchy: `AnalogStickControl`, `ButtonControl`, `SliderControl`, `RadialMenuControl`, `DPadControl`
   - Each has `id`, `type`, `x`, `y`, `size`, `opacity`, plus type-specific properties as listed above
   - `GyroConfig` data class (separate from controls, since it has no position)
   - `TouchLayout` top-level class with `version`, `name`, `globalOpacity`, `hapticEnabled`, `controls: List<TouchControl>`, `gyro: GyroConfig`
   - `ResponseCurve` enum: `LINEAR`, `EXPONENTIAL`, `S_CURVE`
   - `DPadMode` enum: `INDIVIDUAL_BUTTONS`, `CONNECTED_BUTTONS`, `SWIPE_STICK`
   - File: `android/app/src/main/java/com/dxxredux/app/TouchControl.kt`

2. **Create `TouchLayoutRepository.kt`** — Load/save/manage layouts:
   - Read/write `touch_layout.json` from `context.filesDir` (same pattern as `controller_config.json`)
   - `loadLayout(): TouchLayout` (returns default layout if file missing)
   - `saveLayout(layout: TouchLayout)`
   - `getDefaultLayout(): TouchLayout` — hardcoded default matching current controls + additions (dual stick, exponential curves, D-pad)
   - `getPresets(): List<TouchLayout>` — 3 built-in presets (Simple, Advanced, Claw)
   - Version migration: if loaded version < current, add missing fields with defaults
   - File: `android/app/src/main/java/com/dxxredux/app/TouchLayoutRepository.kt`

3. **Define shared constants** for axis/button indices:
   - Constants object mapping readable names to kconfig indices
   - Document that these mirror `kc_joystick[]` in `d2/main/kconfig.c`
   - Include response curve defaults, size limits, opacity limits as constants
   - File: `android/app/src/main/java/com/dxxredux/app/TouchBindings.kt`

### Phase 2: Refactor TouchOverlayView to be data-driven (*depends on Phase 1*)

4. **Refactor `TouchOverlayView.kt`** to consume `TouchLayout`:
   - Replace hardcoded position/size calculations with layout-driven geometry
   - Add `setLayout(layout: TouchLayout)` method
   - Each control type gets a renderer and hit-test method
   - Multi-touch pointer tracking: `Map<Int, String>` of `pointerId → controlId`
   - Apply `globalOpacity * control.opacity` to all paint alpha values
   - Apply response curves + per-axis sensitivity in `updateStickFromTouch()` using the formula above
   - Apply `globalHapticEnabled && control.haptic` for haptic decisions
   - Backward compatibility: if no layout file exists, `getDefaultLayout()` produces the same controls as today (with linear response, sensitivity 1.0 — matching current behavior)
   - Keep automap mode handling (not user-editable for now)

5. **Add new control renderers/handlers** for each widget type:
   - **Second analog stick**: Same code as first stick, parameterized by control data
   - **DPad renderer** (3 modes):
     - `individual_buttons`: Draw 4 separate circles in cross layout, independent hit-test each
     - `connected_buttons`: Draw single cross shape, map touch angle to quadrant(s), activate 1-2 directions
     - `swipe_stick`: Draw ring+thumb (like analog stick), compute displacement direction + magnitude, activate direction button(s) when past `swipeThreshold`
   - **RadialMenu renderer**: Draw circle segments on press-hold, highlight segment under finger, select on release
   - **Slider renderer**: Draw track + thumb, handle drag along axis, apply response curve + sensitivity
   - **Haptic feedback**: Call `view.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)` on button press, D-pad direction activation, radial menu segment selection, and slider snap-to-center

6. **Wire radial menu actions to game**:
   - Guidebot commands: inject KEY_1-KEY_9 via `nativeKeyEvent()` JNI bridge (already exists for key injection)
   - If `nativeKeyEvent()` doesn't support arbitrary SDL keycodes, add `nativeInjectKey(int sdlk_code)` to android_input.c
   - Weapon selection: inject appropriate key events

### Phase 3: Gyro Input (*parallel with Phase 2*)

7. **Create `GyroInputManager.kt`**:
   - Register for `TYPE_GAME_ROTATION_VECTOR` sensor via Android SensorManager
   - Calculate rotation deltas (yaw, pitch) between consecutive samples using quaternion difference
   - Apply per-axis sensitivity, response curve (same enum/logic as sticks), and inversion
   - Three activation modes:
     - `always`: gyro applies every frame
     - `touch_right_stick`: gyro only applies while right stick pointer is active
     - `toggle`: a dedicated button (configurable) toggles gyro on/off
   - Output: Kotlin holds `stickValue + gyroDelta`, sends combined result via `nativeJoystickAxis()`
   - Lifecycle: register sensor on resume, unregister on pause. Clear accumulated state on pause.
   - Calibrate: capture current rotation as neutral. Store calibration in gyro config.
   - File: `android/app/src/main/java/com/dxxredux/app/GyroInputManager.kt`

8. **Gyro settings in editor** (Phase 4) or ControllerSection:
   - Enable/disable toggle
   - which axex does it map to? Note that it's allowed to double bind with an axis since the common case will be for fine aim in conjunciton with gross aim on the touch screen
   - Sensitivity sliders (X and Y, independent)
   - Response curve picker + exponent slider
   - Activation mode picker (always / touch-right-stick / toggle)
   - Invert toggles per axis
   - Calibrate/reset button

### Phase 4: Touch Layout Editor (*depends on Phase 1 & 2*)

9. **Create `TouchEditorActivity.kt`** (or Composable page within SetupActivity):
   - Full-screen display of all controls at their positions/sizes, rendered on a mock game-screen background
   - **Select mode**: Tap a control to select it (blue highlight border)
   - **Move mode**: Drag selected control to reposition. Live update `x`, `y`. Constrain to screen bounds.
   - **Resize**: Pinch on selected control or drag corner handles. Clamped 0.5-2.0. Min size enforced (36dp for buttons, larger for sticks).
   - **Properties panel**: Bottom sheet showing selected control's settings:
     - **All controls**: opacity slider, size fine-tune slider, delete button
     - **Sticks**: deadzone slider, sensitivity X/Y sliders, response curve picker (dropdown: Linear / Exponential / S-Curve), exponent slider (when exponential), floating toggle, invert X/Y toggles, axis binding pickers, response curve preview graph
     - **Buttons**: label editor, button binding picker, hold/toggle mode, shape picker, haptic toggle
     - **Sliders**: axis binding picker, orientation picker, sensitivity slider, response curve, spring-back toggle
     - **DPad**: mode picker (Individual / Connected / Swipe Stick), direction binding pickers, diagonals toggle, swipe threshold slider (swipe_stick mode only)
     - **RadialMenu**: segment editor (add/remove/reorder segments, edit labels and actions)
   - **Global settings panel** (accessible from toolbar):
     - Global opacity slider (0.2-1.0)
     - Haptic master toggle
     - Gyro settings (enable, sensitivity, response curve, activation mode, calibrate)
   - **Add control**: FAB or menu → pick control type → place at screen center
   - **Preset picker**: Switch between built-in presets (confirms overwrite of current layout)
   - **Reset to default** button
   - **Save** button: writes `touch_layout.json`
   - **Overlap warning**: Yellow tint on controls that overlap more than 30% with another
   - File: `android/app/src/main/java/com/dxxredux/app/TouchEditorActivity.kt`

10. **Add "Touch Controls" button** to SetupActivity `ControllerSection()`:
    - Place next to existing "Define Controls" button
    - Same `OutlinedButton` style
    - Opens TouchEditorActivity
    - File: modify `ControllerSection()` in SetupActivity.kt (~L2553)

11. **Wire editor output to game**:
    - On save, `TouchLayoutRepository.saveLayout()` writes JSON
    - On game launch, `MainActivity` reads layout and calls `TouchOverlayView.setLayout()`
    - Hot-reload: returning from editor to game reloads the layout

### Phase 5: Guidebot Command Wheel & Special Controls (*depends on Phase 2*)

12. **Guidebot radial menu**:
    - 9 segments: Energy, Energy Center, Shield, Powerup, Robot, Hostage, Scram, Player Spew, Exit
    - Center/cancel: release without selecting = clear goal (KEY_0)
    - Activation: press-hold guidebot button → wheel appears → slide → release = select
    - Inject KEY_1-KEY_9 / KEY_0 matching `set_escort_special_goal()` in d2/main/escort.c
    - D2 only (D1 has no guidebot). JNI `nativeIsGuidebotPresent()` flag to conditionally show.

13. **Weapon select wheel** (lower priority):
    - Primary: Laser, Vulcan, Spreadfire, Plasma, Fusion (5 segments)
    - Secondary: Concussion, Homing, Proximity, Smart, Mega (5 segments)
    - Two separate wheels or one with sub-menus. Inject weapon select key events.

### Phase 6: Polish & Presets (*depends on all above*)

14. **Create 3 default presets**:
    - **Simple**: Left stick (linear curve) + A/B fire buttons + MAP. Matches current hardcoded layout.
    - **Advanced**: Dual sticks (exponential curves) + fire buttons + bank buttons + slide D-pad (swipe_stick mode) + guidebot wheel + throttle slider + weapon cycle. Global opacity 0.5.
    - **Claw (4-finger)**: Same as Advanced but fire buttons positioned at top corners for index fingers. Sticks at bottom. Lower opacity (0.4) since claw players look through controls more.

15. **Per-control opacity rendering completed** (started in Phase 2)

16. **Collision detection** in editor: Warn when controls overlap >30%

17. **Version migration**: `touch_layout.json` version field; on load, fill missing fields with defaults for any newer version

18. **Haptic feedback tuning**: Different haptic patterns for button press, D-pad direction change, radial menu segment change, stick reaching edge

## Relevant Files

### Existing files to modify
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` — Refactor from hardcoded to data-driven
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` — Load layout on game start, pass to overlay
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` — Add "Touch Controls" button (~L2553)
- `android/app/src/main/cpp/android_input.c` — May need `nativeInjectKey()` for radial menus

### New files to create
- `android/app/src/main/java/com/dxxredux/app/TouchControl.kt` — Data model classes
- `android/app/src/main/java/com/dxxredux/app/TouchLayoutRepository.kt` — Load/save/defaults/presets
- `android/app/src/main/java/com/dxxredux/app/TouchBindings.kt` — Shared constants
- `android/app/src/main/java/com/dxxredux/app/GyroInputManager.kt` — Sensor input
- `android/app/src/main/java/com/dxxredux/app/TouchEditorActivity.kt` — Visual editor

### Reference files (read only)
- `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt` — Editor UI patterns, shared constants scheme
- `d2/main/kconfig.c` — Source of truth for `kc_joystick[]` axis/button index meanings
- `d2/main/escort.c` — Guidebot command mapping (KEY_1-9 → escort goals)

## Verification

1. **Unit tests** for `TouchLayoutRepository`: serialize/deserialize round-trip, default layout validity, version migration, preset generation
2. **Unit tests** for response curve math: verify linear/exponential/s_curve outputs at 0, 0.5, 1.0, edge cases, deadzone interaction
3. **Unit tests** for `DPadControl` hit-testing: all 3 modes, diagonal activation, swipe threshold behavior
4. **Unit tests** for floating stick zone activation and position tracking
5. **Integration test**: default layout renders controls at positions matching current hardcoded layout
6. **Manual test**: Open touch editor, move controls, change response curve, save, launch game, verify
7. **Gyro test**: Enable gyro, tilt phone, verify smooth aim. Disable, verify no drift. Test all 3 activation modes.
8. **Multi-touch test**: Hold left stick + right stick + fire simultaneously, verify no pointer conflicts
9. **D-pad mode test**: Test all 3 modes (individual, connected, swipe) with single and diagonal input
10. **Global opacity test**: Set global to 0.3, per-control to 0.5, verify effective opacity ~0.15
11. **Haptic test**: Enable/disable master haptic, per-control haptic, verify behavior
12. **Guidebot wheel test**: In-game, select "Find Exit", verify buddy message
13. **Preset test**: Switch presets, verify layouts
14. **Regression**: Touch overlay enable/disable checkbox. Windows/Linux cmake builds unaffected.

## Decisions

- **No game engine changes**: All touch → SDL joystick events via existing JNI bridge. Radial menus inject key events.
- **Two layers of sensitivity/deadzone**: Touch layer (response curve + sensitivity in Kotlin) shapes the feel of the glass. Engine layer (kconfig deadzone + sensitivity in C) controls how axis values map to game movement. Both matter; they serve different purposes. Touch layer is tuned for thumb precision, engine layer for game feel.
- **Gyro via Android SensorManager**: SDL 1.2 has no sensor API. `TYPE_GAME_ROTATION_VECTOR` (fused gyro+accel) avoids drift.
- **Gyro+stick combined in Kotlin**: Sum values before `nativeJoystickAxis()`. No new JNI function needed unless axis count needs expanding.
- **Separate `touch_layout.json`**: Different concern from physical gamepad config.
- **Automap controls hardcoded for now**: Multi-finger pinch/rotate doesn't fit the standard control model.
- **D1 compatibility**: Same layout system. Guidebot wheel hidden in D1. Afterburner button functional only if D2.
- **Global opacity multiplier**: Multiplied with per-control opacity. Simple and consistent.
- **D-pad as multi-modal**: Three modes cover the spectrum from "I want four separate buttons" to "I want to swipe in a direction." The swipe_stick mode is novel and particularly good for Descent's slide controls.

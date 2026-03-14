# Plan: Build 919 Fixes

## Status: ALL PHASES COMPLETE

## Issues

| # | Bug | Root Cause | Fix Location | Status |
|---|-----|-----------|-------------|--------|
| 1 | Touch aim up->turn right, aim down->turn left | axisX/axisY swapped in look stick configs | advanced.json, claw.json | DONE |
| 2 | Afterburner not selectable in controller button editor | Not a bug -- correctly in D2 list, filtered for D1 | N/A | WONTFIX |
| 3 | D2 controller Automap/CyclePrimary/CycleSecondary not working | No default bindings | android_gamepad_config.cpp, kconfig.c (d1+d2), default.json | DONE |
| 4 | D-pad/stick not navigating menus | No JOYSTICK_BUTTON_DOWN for btns 22-25, no axis->key | newmenu.c (d1+d2), newmenu + listbox handlers | DONE |
| 5 | D1/D2 pilot cross-contamination | PHYSFS_getPrefDir returns same dir for both on Android | physfsx.c (d1+d2) | DONE |
| 6 | Gyro slide uses rate, not absolute angle | GyroInputManager always updates ref each frame | GyroInputManager.kt, GyroConfig in TouchControl.kt | DONE |
| 7 | Gyro uses wrong sensor type | TYPE_GAME_ROTATION_VECTOR has no gravity ref | GyroInputManager.kt | DONE |

---

## Virtual Button/Axis Mapping Architecture

The Android port uses a two-layer mapping model -- physical inputs are
translated to **virtual button/axis IDs** in the Kotlin/JNI layer, and
the game engine maps those virtual IDs to game actions via kconfig.

```
Physical Android Input (KeyEvent / MotionEvent / touch / gyro)
    |
    v
Kotlin layer (MainActivity.kt, TouchOverlayView)
    gamepadButtonIndex()        -> virtual button 0-9
    dpadKeyCodeToJoyButton()    -> virtual button 22-25
    onGenericMotionEvent()      -> virtual axis 0-5 + HAT->dpad
    touch overlay               -> button kcIndex+128, axis 0-7
    GyroInputManager            -> virtual axis 6-7 (BK, SU)
    |
    v  JNI: nativeJoystickButton(btn, pressed)
    |       nativeJoystickAxis(axis, value)
    v
joy.c (C, virtual gamepad layer)
    joy_button_handler()        -> button_map[] lookup (bypass if >=128)
    joy_axis_handler()          -> axis_map[] lookup, EVENT_JOYSTICK_MOVED
    joy_axisbutton_handler()    -> axis_button_map[] for trigger->button
    |
    v  fires EVENT_JOYSTICK_BUTTON_DOWN/UP, EVENT_JOYSTICK_MOVED
    |
kconfig.c (game config layer)
    PlayerCfg.KeySettings[1][action_index] = virtual_button_or_axis_id
    kc_joystick[] maps action_index -> Controls.xxx_state
    |
    v
Game Action (fire, accelerate, turn, slide, etc.)
```

### Virtual button IDs (registered in joy_init, joy.c)

| Range   | Count | What                 | Source                        |
|---------|-------|----------------------|-------------------------------|
| 0-9     | 10    | Face/shoulder buttons| A,B,X,Y,L1,R1,Sel,Sta,L3,R3  |
| 10-21   | 12    | Axis-buttons         | 2 per axis (neg/pos), axes 0-5|
| 22-25   | 4     | D-pad                | DUp,DDown,DLeft,DRight        |
| 128+    | var   | Touch overlay         | kcIndex+128, bypasses button_map|

Axis-button numbering:
  10=-LX, 11=+LX, 12=-LY, 13=+LY, 14=-RX, 15=+RX,
  16=-RY, 17=+RY, 18=-LT, 19=+LT, 20=-RT, 21=+RT

### Virtual axis IDs (registered in joy_init, joy.c)

| ID | Name | Android Source                   | Game binding       |
|----|------|----------------------------------|--------------------|
| 0  | LX   | MotionEvent.AXIS_X               | Slide L/R          |
| 1  | LY   | MotionEvent.AXIS_Y               | Throttle           |
| 2  | RX   | MotionEvent.AXIS_Z               | Turn L/R           |
| 3  | RY   | MotionEvent.AXIS_RZ              | Pitch U/D          |
| 4  | LT   | MotionEvent.AXIS_LTRIGGER        | (axis-buttons only)|
| 5  | RT   | MotionEvent.AXIS_RTRIGGER        | (axis-buttons only)|
| 6  | BK   | Virtual (gyro/touch)             | Bank L/R           |
| 7  | SU   | Virtual (gyro/touch)             | Slide U/D          |

### kconfig action indices (kc_joystick[], D2)

Key axis actions:
  [13]=Pitch U/D (default: axis 3/RY)
  [15]=Turn L/R  (default: axis 2/RX)
  [17]=Slide L/R (default: axis 0/LX)
  [19]=Slide U/D (default: axis 7/SU, virtual)
  [21]=Bank L/R  (default: axis 6/BK, virtual)
  [23]=Throttle  (default: axis 1/LY)

Key button actions:
  [0]=Fire Primary   (default: btn 0/A)
  [1]=Fire Secondary (default: btn 1/B)
  [2]=Accelerate     (default: btn 21/+RT)
  [3]=Reverse        (default: btn 19/+LT)
  [4]=Fire Flare     (default: btn 2/X)
  [6/7]=Slide L/R    (default: btn 24/25, DLeft/DRight)
  [8/9]=Slide U/D    (default: btn 22/23, DUp/DDown)
  [27]=Afterburner   (default: btn 3/Y, D2 only)
  [28]=Cycle Primary (default: btn 4/L1, D2 only)
  [29]=Cycle Second. (default: btn 5/R1, D2 only)
  [50]=Automap       (default: btn 6/Select, D2 only)

D1 differences: [27]=Automap(btn 6), [44]=CyclePri(btn 4), [45]=CycleSec(btn 5)

---

## Changes Made

### Phase 1: Touch axis swap
- Swapped axisX/axisY for look stick in advanced.json and claw.json

### Phase 2: Controller button defaults
- Added defaults for Fire Flare(X), Afterburner(Y), Cycle Primary(L1),
  Cycle Secondary(R1), Automap(Select) to:
  - android_gamepad_config.cpp (with #ifdef DXX_BUILD_DESCENT_II)
  - d2/main/kconfig.c kconfig_get_default_settings()
  - d1/main/kconfig.c kconfig_get_default_settings()
  - default.json

### Phase 4: D-pad and stick menu navigation
- Added D-pad buttons 22-25 -> KEY_UP/DOWN/LEFT/RIGHT in:
  - newmenu_handler() in d2/main/newmenu.c and d1/main/newmenu.c
  - listbox_handler() in d2/main/newmenu.c and d1/main/newmenu.c
- Added EVENT_JOYSTICK_MOVED axis->key conversion with deadzone (64/128)
  and edge-triggered debouncing for both sticks

### Phase 5: Pilot cross-contamination
- Modified PHYSFSX_init in d2/misc/physfsx.c and d1/misc/physfsx.c to
  create game-specific subdirs (d2x-redux/, d1x-redux/) under files/
- PHYSFS writeDir set to game-specific subdir
- Root files/ dir kept in search path for launcher-written files
- Verified on emulator: files/d2x-redux/Players/player.plr created

### Phase 6: Gyro absolute angle mode
- Added GyroMode enum (RATE, ABSOLUTE) to TouchControl.kt
- Added mode and maxAngle fields to GyroConfig with serialization
- Modified GyroInputManager to skip reference update in ABSOLUTE mode
- In ABSOLUTE mode, angle is mapped proportionally within maxAngle range

### Phase 7: Gyro sensor type
- Switched GyroInputManager from TYPE_GAME_ROTATION_VECTOR to
  TYPE_ROTATION_VECTOR (gravity-referenced) for stable absolute tilt

### Phase 8: D1/D2 controller config byte array mismatch
- Bug: controller_config.json had only D2-format key_settings_joystick.
  When D1 loaded it, kconfig indices were wrong (D2 index 27=Afterburner
  became D1 index 27=Automap mapped to Y button instead of Select)
- Fix: saveConfig() now generates both key_settings_joystick_d1 and
  key_settings_joystick_d2. C code reads game-specific key via #ifdef.
- Files: ControllerConfigPage.kt, android_gamepad_config.cpp, SetupActivity.kt

### Phase 9: Touch overlay cross-contamination
- Bug: kconfig_fill_joy_settings() (always D2 build via NativePilotPatcher)
  applied D2's touch overlay col_map to D1 byte arrays, overwriting D1's
  Cycle Primary (index 44) and Cycle Secondary (index 45) with bad values
- Fix: Removed touch overlay from kconfig_fill_joy_settings() -- redundant
  with kc_set_controls() which applies it at game runtime
- File: d2/main/kconfig.c

### Phase 10: Test extensions
- Added dot-path navigation to assert system (game_automate.cpp) for
  paths like joystick_controls.items[27].value
- Added Phase 3 to test_dpad_triggers.json5 with D1/D2-specific button
  default assertions (Automap, Cycle Primary/Secondary, Afterburner, Flare)

### Phase 11: D2-only control visibility in pickers
- Problem: ButtonFunctionPickerDialog and DpadFunctionPickerDialog filter
  out D2-only functions when gameVariant=="d1", silently hiding them
- Fix: Show all functions always; mark D2-only ones with dimmed/italic
  style and "(D2 only)" suffix. They remain selectable.
- Also update unassigned function computation to exclude D2-only functions
  from the "unassigned" warning when D1 is active
- Files: ControllerConfigPage.kt

### Phase 12: Per-axis activation threshold
- Problem: Axis-to-button threshold is hardcoded to 38/128 (~30%) in
  joy_axisbutton_handler() in both d1/d2 joy.c
- Fix: Add per-axis configurable threshold (5-95%, step 5, default 30%)
  - JSON: "thresholds": {"LT": 30, "RT": 30, ...} in controller_config.json
  - Kotlin UI: Slider + live AxisThresholdBar in picker dialogs for
    LT/RT and sticks in button mode
  - C: joy_axis_button_deadzone[] array read from JSON at startup,
    used in joy_axisbutton_handler() instead of hardcoded 38
- Files: ControllerConfigPage.kt, default.json, joy.c (d1+d2),
  android_gamepad_config.cpp, ConfigImportExport.kt, HumanReadableConfig.kt

## Build verification (after Phase 12)
- MSVC D1: OK
- MSVC D2: OK
- Android APK: OK
- Code quality (clang-format + ktlint): OK

## Full test results after Phase 12 (all EXIT 0)
| Test                    | D1   | D2   |
|-------------------------|------|------|
| test_launch_to_automap  | PASS | PASS |
| test_keyboard_defaults  | PASS | PASS |
| test_joystick_menu      | PASS | PASS |
| test_dpad_triggers      | PASS | PASS |
| test_controller_compare | PASS | PASS |
| test_axis_mapping       | PASS | PASS |
| test_death              | PASS | PASS |

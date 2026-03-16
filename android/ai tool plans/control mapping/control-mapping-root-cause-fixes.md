# Plan: Fix All Control Mapping Bugs (Build 916+)

## TL;DR

Five root-cause bugs explain all three user-reported symptoms. The core issues are:
(1) D-pad buttons missing from button_map[] so all D-pad presses become button 0,
(2) joy_axisbutton_handler() sends the wrong button for positive/negative axis directions,
(3) trigger defaults used wrong button values,
(4) saveConfig() hardcodes "d2" breaking D1 pilot patching.
Fix the handler under #ifdef ANDROID, populate D-pad button_map, update trigger defaults,
fix the hardcoded game variant.

## Root Cause Analysis

| User Report | Root Cause |
|---|---|
| D-pad slide/roll does nothing (fresh D2) | BUG 1: button_map[22-25] uninitialized -- all D-pad presses become button 0 |
| Triggers do nothing (launcher-configured) | BUG 2: joy_axisbutton_handler backwards + BUG 3: trigger defaults wrong |
| Afterburner in D1 opens automap | BUG 4: saveConfig() hardcodes "d2" in nativePatchPilotFiles() |

### Bug Details

BUG 1 - D-pad button_map missing: In joy_init(), D-pad buttons 22-25 are registered in
joybutton_text[] but NOT in SDL_Joysticks[0].button_map[]. When nativeJoystickButton(22, 1)
arrives, joy_button_handler() does button = button_map[22] which is 0 (memset). Every D-pad
press becomes Fire Primary.

BUG 2 - Axis button handler backwards: joy_axisbutton_handler() sends button (base) when
axis goes positive, and button+1 when negative. For triggers (0..1 only positive), the base
button always fires. ControllerConfigPage correctly maps LT=19(+LT) and RT=21(+RT), but the
handler sends 18/20 instead. Fix under #ifdef ANDROID per user instruction.

BUG 3 - Trigger defaults compensate for BUG 2: Hardcoded defaults use values 18/20 (base
buttons) with misleading comments. After fixing BUG 2, these need to change to 19/21.

BUG 4 - Hardcoded "d2" in saveConfig: ControllerConfigPage.saveConfig() calls
nativePatchPilotFiles(..., "d2") instead of gameVariant. D1 pilot files get D2 kc_joystick
indices.

BUG 5 - Misleading comments: Several comments say "+RT"/"+LT" but values are base buttons.

## Steps

### Phase 1: Fix joy_axisbutton_handler (d1/d2, #ifdef ANDROID)
1. d2/arch/sdl/joy.c: add #ifdef ANDROID block with correct neg_btn/pos_btn assignment
2. d1/arch/sdl/joy.c: same change

### Phase 2: Fix D-pad button_map (d1/d2)
3. d2/arch/sdl/joy.c: add button_map identity entries in D-pad registration
4. d1/arch/sdl/joy.c: same change

### Phase 3: Fix trigger defaults (depends on Phase 1)
5. android_gamepad_config.cpp: change 20->21, 18->19
6. d2/main/kconfig.c kconfig_get_default_settings(): same changes
7. d1/main/kconfig.c kconfig_get_default_settings(): same changes

### Phase 4: Fix saveConfig hardcoded "d2"
8. ControllerConfigPage.kt line ~424: replace "d2" with gameVariant

### Phase 5: Build verification
9. cmake build D1+D2 native
10. Build APK with gradle
11. Run code quality linter

### Phase 6: Testing
12. Run test_axis_mapping.json5 for D1 and D2 -- PASS
13. Run test_keyboard_defaults.json5 for D1 and D2 -- PASS
14. Create and run test_dpad_triggers.json5 for D1 and D2 -- PASS
    - Added send_button automation action with held/pressed modes to game_automate.cpp
    - Test verifies D-pad buttons 22-25 map to slide controls
    - Test verifies trigger axes 4/5 map to throttle controls

## Status: COMPLETE -- all phases done, all tests passing

## Relevant Files
- d2/arch/sdl/joy.c -- joy_axisbutton_handler() line 165, D-pad registration line 248
- d1/arch/sdl/joy.c -- identical code
- android/app/src/main/cpp/android_gamepad_config.cpp -- android_apply_gamepad_defaults()
- d2/main/kconfig.c -- kconfig_get_default_settings() line 2090
- d1/main/kconfig.c -- kconfig_get_default_settings() line 1983
- android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt -- saveConfig() line 424

## Decisions
- Handler fix is #ifdef ANDROID only; desktop behavior preserved
- After handler fix, BUTTON_CONTROLS values (LT=19, RT=21) already correct -- no Kotlin
  data changes needed
- Not fixing joybutton_text[] naming since engine config menu unused on Android
- No backwards compat for existing controller_config.json (pre-release)

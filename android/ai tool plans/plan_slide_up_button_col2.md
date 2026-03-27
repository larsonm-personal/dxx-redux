# Fix: Button-bound Slide Up not working when D-pad also bound

## Problem
When both a face button AND d-pad are bound to the same function (e.g., Slide Up),
buildJoyPairs emits two (kcIndex, value) pairs with the same kcIndex. kconfig_fill_joy_settings
applies them sequentially, so the last one (d-pad) overwrites the first (button).

Col2 (secondary binding slots) could hold the second binding, but touch overlay code in
kc_set_controls unconditionally overwrites col2 with touch button values (col1_index + 128).

## Root cause
Touch overlay uses col2 slots in kc_joystick[] to store offset button values
(col1_kcIndex + 128). This prevents col2 from being used for secondary gamepad bindings.

## Fix: implicit touch matching + col2 for secondary bindings

### 1. kconfig.c button matching (D1+D2): implicit touch match
Add #ifdef ANDROID match: `i + 128 == btn` alongside the existing value match.
This lets touch buttons match at the col1 entry's index without needing a stored
value in col2. Touch sends kcIndex + 128 as the SDL button id.

Affects 4 sites (2 games x 2 handlers):
- [x] D2 kconfig_read_controls EVENT_JOYSTICK_BUTTON_DOWN/UP (line ~1390)
- [x] D2 newmenu_handler/kconfig_handler EVENT_JOYSTICK_BUTTON_DOWN (line ~1270)
- [x] D1 kconfig_read_controls EVENT_JOYSTICK_BUTTON_DOWN/UP (line ~1290)
- [x] D1 newmenu_handler EVENT_JOYSTICK_BUTTON_DOWN (line ~1175)

### 2. kc_set_controls (D1+D2): remove touch overlay col_map
Remove the #ifdef ANDROID block that writes col1_index + 128 into col2 slots.
Col2 will now hold whatever is in KeySettings (secondary gamepad binding or 0xFF).
- [x] D2 kc_set_controls
- [x] D1 kc_set_controls

### 3. kconfig_fill_joy_settings (D1): remove touch overlay col_map
D1's kconfig_fill_joy_settings also applies the touch overlay col_map. Remove it.
- [x] D1 kconfig_fill_joy_settings

### 4. kconfig_get_default_settings (D2): remove touch overlay col_map
D2's kconfig_get_default_settings applies the touch overlay col_map to defaults.
Remove it (defaults in col2 should be 0xFF like any unbound slot).
- [x] D2 kconfig_get_default_settings

### 5. buildJoyPairs (Kotlin): secondary binding to col2
When buildJoyPairs emits a duplicate kc index, redirect to the col2 index.
Add COL2_MAP constants for D1 and D2.
- [x] ControllerConfigPage.kt buildJoyPairs -- col2 fallback
- [x] ControllerConfigPage.kt -- COL2_MAP constant

### 6. Build + test
- [x] Android build succeeds (EXIT: 0, no warnings)
- [x] Windows MSVC: all changes are #ifdef ANDROID, no impact on Windows builds
  - vcpkg/SDL not configured locally, so can't run MSVC build, but changes are guarded
- [ ] Test with emulator

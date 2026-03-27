# Fix Controller Mapping Bugs and Exit Button

## Bug 1: LT bound to Slide Down triggers spurious actions

### Root cause
Virtual combiner axes (8, 9, 10) registered in joy_init() do not have their
`axis_button_map[]` entries set to -1. The memset zero leaves them at 0.
When joy_axisbutton_handler sees axis 8/9 with axis_button_map=0, it
generates button 0/1 events (A/B face buttons), causing spurious Fire
Primary / Fire Secondary / other actions mapped to those buttons.

### Fix
In `d2/arch/sdl/joy.c` and `d1/arch/sdl/joy.c` joy_init(), set
`axis_button_map[8..10] = -1` after registering the virtual combiner axes.

## Bug 2: android_apply_gamepad_defaults clobbers user axis config

### Root cause
`android_apply_gamepad_defaults()` unconditionally overwrites
`KeySettings[1][19] = 7` (Slide U/D = gyro axis) and
`KeySettings[1][21] = 6` (Bank L/R = gyro axis) AFTER loading the user's
controller_config.json. This destroys any half-axis combiner assignment
the user configured for those slots.

### Fix
Only set gyro axis defaults when the JSON config doesn't already specify
values for those slots (i.e. when they're still 0xFF after loading).

## Bug 3: LT as button for Slide Up has no effect

### Root cause
Same as bug 2. The half-axis combiner correctly writes a virtual axis
to kc_joystick[19], but android_apply_gamepad_defaults overwrites it to 7.
The combiner output goes to the orphaned virtual axis, so Slide U/D gets
no input.

## Bug 4: Exit button in-game has no effect

### Root cause
META_RETURN_TO_LAUNCHER injects ESC + SDL_QUIT. During gameplay, ESC opens
the "Abort Game?" dialog in standard_handler (via nm_messagebox). This is a
blocking modal. Quitting then closes it, but another dialog appears because
Game_wind is now front and standard_handler shows ANOTHER "Abort Game?"
confirmation (which resets Quitting=0 until user picks Yes). The net effect
is that the user sees a dialog flash and either needs to interact with it
or sees confusing behavior.

### Fix
Add a `force_quit` flag in android_meta_actions. When META_RETURN_TO_LAUNCHER
fires, set this flag. In standard_handler (inferno.c), when Quitting and
Game_wind is front and force_quit is set, skip the confirmation dialog and
close Game_wind directly.

## Files to modify

### Both d1/ and d2/:
- [x] `arch/sdl/joy.c` -- set axis_button_map to -1 for combiner axes
- [x] `main/inferno.c` -- check force_quit flag in standard_handler

### Android-specific:
- [x] `android/app/src/main/cpp/android_gamepad_config.cpp` -- conditional gyro overwrite
- [x] `android/app/src/main/cpp/shared/android_meta_actions.c` -- set force_quit flag
- [x] `android/app/src/main/cpp/shared/android_meta_actions.h` -- declare force_quit

### Build scripts (separate issue):
- [x] `game_data/extract_all_cds.ps1` -- source test_env.ps1 for cmake path
- [x] `game_data/extract_all_gog.ps1` -- source test_env.ps1 for cmake path

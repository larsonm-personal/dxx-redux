# Plan: D1/D2 Touch Bindings and Integration Fixes

Six bugs spanning player file isolation, D1 virtual gamepad initialization, axis
mapping misalignment, touch overlay game-awareness, afterburner toggle mode, and
D1 automap touch controls.

The deepest issue is that D1 is missing the entire Android virtual gamepad setup
that D2 has in joy.c, causing axis-to-button mapping corruption. Most other
issues stem from the touch overlay being game-unaware (same buttons/mappings for
both D1 and D2 despite different kc_joystick layouts).

---

## Phase 1: D1 Virtual Gamepad Init (Bug #3 root cause -- COMPLETE)

### Problem
d1/arch/sdl/joy.c lacks the `#ifdef ANDROID` block that d2/arch/sdl/joy.c has
(lines 209-246). Without it:
- joy_init() calls SDL_Init() which fails on Android, returns early
- axis_button_map[] stays all zeros (static global)
- joy_axisbutton_handler() maps axis 0 -> button 0 (Fire Primary),
  axis 1 -> button 1 (Fire Secondary)
- Moving either stick fires weapons

### Fix
Copy D2's `#ifdef ANDROID` virtual gamepad registration block into D1's
joy_init(). This registers 6 axes with properly initialized axis_button_map
values (10, 12, 14...) that don't collide with game control buttons.

### Files
- d1/arch/sdl/joy.c -- add Android virtual gamepad block
  (reference d2/arch/sdl/joy.c lines 209-246)

---

## Phase 2: Left Stick Y Axis Mapping (Bugs #3 partial, #4 -- COMPLETE)

### Problem
Both games default `joy_out[19] = 1`, mapping axis 1 (Left Y) to Slide U/D.
TouchBindings.kt labels this axis as "Fwd/Back". The correct target is Throttle
(kc_joystick index 23), not Slide U/D (index 19).

### Fix
Change `joy_out[19] = 1` to `joy_out[23] = 1` in three locations:
1. d2/main/kconfig.c  kconfig_get_default_settings
2. d1/main/kconfig.c  kconfig_get_default_settings
3. android/app/src/main/cpp/android_gamepad_config.cpp  android_apply_gamepad_defaults

Update test_axis_mapping.json5 expected values if needed.

### Files
- d2/main/kconfig.c
- d1/main/kconfig.c
- android/app/src/main/cpp/android_gamepad_config.cpp
- android/game_scripts/test_axis_mapping.json5

---

## Phase 3: D1 Touch Button Remapping (Bug #3 continued -- COMPLETE)

### Problem
The touch overlay shows identical buttons for D1 and D2, but D1's kc_joystick
array has a different layout (48 entries vs D2's 56). Key mismatches:

| Touch Button     | D2 kc_joystick[N] | D2 Action       | D1 kc_joystick[N] | D1 Action    |
|------------------|--------------------|------------------|--------------------|--------------|
| 27 (Afterburner) | [46]               | Afterburner      | [28]               | Automap(!)   |
| 50 (Automap)     | [51]               | Automap          | N/A (>48)          | Out of bounds|

D1 has no afterburner. D1 has no headlight, energy-to-shield, or toggle bomb
joystick bindings.

D1's col_map `{28,27}` maps touch afterburner (btn 27) to kc_joystick[28]
which is Automap in D1 -- that's actually correct for getting an automap button,
but the touch overlay LABELS it "Afterburner" which is confusing and shows a
button that doesn't exist in D1.

### Fix
1. In TouchOverlayView.kt: when gameVariant == "d1", hide D2-only buttons
   (Afterburner, Headlight, Energy->Shield, Toggle Bomb) during drawing
2. Fix D1's col_map in all three locations to use proper D1-specific mappings:
   - Remove {28,27} (afterburner -> automap confusion)
   - Add a proper automap mapping so touch BTN_AUTOMAP button (50) can
     work in D1. D1 automap is at kc_joystick[27] (col1) and [28] (col2),
     so add {28,50} to route touch automap button 50 -> kc_joystick[28]
   - Fix Cycle Primary/Secondary: D1 uses 44/45 (col1) and 46/47 (col2).
     Touch sends 44 and 45 for these. Result: col_map {46,44},{47,45}
     is already correct for D1.

### Files
- d1/main/kconfig.c -- three col_map arrays (kconfig_set_controls,
  kconfig_fill_joy_settings, kconfig_get_default_settings)
- android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt --
  hide D2-only buttons when gameVariant == "d1"

---

## Phase 4: Player File Segregation (Bug #1) -- COMPLETE

NativePilotPatcher was scanning the shared filesDir root, missing .plr files in
game-specific subdirectories. Fixed patch_all_plr_files() in
android_gamepad_config.cpp to scan d2x-redux/, d1x-redux/, and their Players/
subdirectories (4 dirs instead of 2).

## Phase 5: Afterburner Toggle Mode (Bug #5) -- COMPLETE

The toggle infrastructure already existed in ButtonControl and TouchOverlayView.
Set toggle=true on the afterburner button in presetClaw() in TouchLayoutRepository.kt.

## Phase 6: D1 Automap Touch Controls (Bug #6) -- COMPLETE

Copied D2's #ifdef ANDROID touch accumulator block into d1/main/automap.c
automap_apply_input(). The block reads volatile globals written by
nativeAutomapInput() for heading, pitch, thrust, bank, and slide controls.
D1 actually has AutomapFreeFlight, so the block was copied verbatim.

---

## Bug #2: D1 Touch Menu Navigation -- LIKELY NON-ISSUE

Investigation shows D1 and D2 have identical mouse/touch menu support. Both use
the same touch-to-mouse conversion. D1's newmenu.c has 8 #ifdef ANDROID sections.
May be a side effect of Phase 1's joy_init failure. Verify after Phase 1.

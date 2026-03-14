# Plan: Controller Bugs, Audio Fix, Menu Button Support

**STATUS: ALL PHASES COMPLETE** -- D1+D2 build clean, code quality passes.

## TL;DR
Fix ~6 bugs across D1/D2 controller mapping, D1 audio, and add controller button support for menus/briefings. Root causes: hardcoded "d2" in reset, wrong axis-button indices for triggers, D-pad routed through keyboard instead of joystick, D1 audio sample rate hardcoded to 44100Hz, and ControllerConfigPage using D2-only kc_joystick indices for both games. New feature: A/B buttons act as select/back in menus and advance in briefings, scoped so in-level gameplay is unaffected.

---

## Phase 1: D1 Reset Controls Bug (Critical -- unblocks all D1 controller testing)

### Bug
SetupActivity.kt hardcodes "d2" in both reset call sites (~L123 command handler and ~L3000 UI button) instead of using the selected game variable. D1 pilots are never reset.

### Fix
Change both "d2" literals to the actual selected game variable.

### Files
- android/app/src/main/java/com/dxxredux/app/SetupActivity.kt

---

## Phase 2: D1 BUTTON_KC_INDEX Mismatch

### Bug
ControllerConfigPage.kt BUTTON_KC_INDEX uses D2 indices unconditionally. D1 has 48 joystick controls (Automap at index 27), D2 has 56 (Automap at index 50). D2-only functions (Afterburner, Cycle Primary/Secondary, Headlight, Energy-to-Shield, Toggle Bomb) don't exist in D1.

### Fix
Create a D1-specific BUTTON_KC_INDEX variant, select based on gameVariant parameter. Also fix d1/main/kconfig.c col_map entry {28,50} which uses D2's automap index.

### Files
- android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt
- d1/main/kconfig.c ~L1991 col_map

---

## Phase 3: Trigger Axis-Button Index Fix (D1 and D2)

### Bug
joy_axisbutton_handler() fires base button (N) for positive axis, N+1 for negative. Triggers go 0-to-1 (positive only), so LT fires button 18, RT fires button 20. But kconfig_get_default_settings sets joy_out[2]=21 (Accelerate) and joy_out[3]=19 (Reverse) -- off by one. The wrong button indices mean triggers do nothing.

### Fix
Change defaults to joy_out[2]=20 (RT base=positive), joy_out[3]=18 (LT base=positive) in both d2/main/kconfig.c and d1/main/kconfig.c. Same fix in android_apply_gamepad_defaults(). Add LT/RT to AXIS_BUTTON_SDL in ControllerConfigPage.kt.

### Files
- d2/main/kconfig.c ~L2092
- d1/main/kconfig.c ~L1985
- android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt

---

## Phase 4: D-Pad Routing Fix (D1 and D2)

### Bug
D-pad HAT axes are intercepted in MainActivity.kt onGenericMotionEvent() and converted to keyboard arrow keys via nativeKeyEvent(). Arrow keys map to pitch/turn in keyboard defaults. The "DUp": "Slide Up" bindings in default.json are never applied because D-pad events bypass the joystick system entirely.

### Fix
Register 4 D-pad virtual joystick buttons (22-25) in joy_init() in d2/arch/sdl/joy.c and d1/arch/sdl/joy.c. Change dispatchDpad() to call nativeJoystickButton() with these indices when no meta binding is active. Update kconfig defaults to map D-pad buttons to Slide Up/Down/Left/Right. Add D-pad entries to BUTTON_KC_INDEX.

### Files
- d2/arch/sdl/joy.c ~L220
- d1/arch/sdl/joy.c
- android/app/src/main/java/com/dxxredux/app/MainActivity.kt ~L954
- android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt
- d2/main/kconfig.c -- update defaults for D-pad buttons
- d1/main/kconfig.c -- same

---

## Phase 5: D1 Audio Sample Rate Fix

### Bug
d1/arch/sdl/digi_mixer.c hardcodes SAMPLE_RATE_44K (44100Hz). D2 queries g_android_native_sample_rate (typically 48000Hz on Android). Mismatch causes resampling artifacts in redbook/CD audio.

### Fix
Port D2's audio init pattern to D1: extern g_android_native_sample_rate, DIGI_MIXER_OUTPUT_RATE macro, 4096 buffer size for Android, Mix_QuerySpec() in mixdigi_convert_sound().

### Files
- d1/arch/sdl/digi_mixer.c

---

## Phase 6: Menu Controller Button Support (New Feature)

### Feature
A button = select, B button = back in menus; both advance briefing screens.

### Scoping (critical)
These mappings must ONLY be active in menus and briefings, NOT during in-level gameplay. The window event system handles this naturally: the front window's handler gets events first. When a newmenu window is front, it consumes button 0/1 events. When the game window is front (in-level), it never sees this menu logic -- buttons 0/1 go through kconfig_read_controls as fire primary/secondary. No special "am I in a menu?" check is needed, just handle EVENT_JOYSTICK_BUTTON_DOWN in the menu/briefing handlers and return 1 (consumed).

### Steps
1. Add EVENT_JOYSTICK_BUTTON_DOWN handling in newmenu_handler() in both d2/main/newmenu.c and d1/main/newmenu.c
2. Button 0 (A) acts like KEY_ENTER (select), button 1 (B) acts like KEY_ESC (back)
3. Same in listbox_handler for list-style menus
4. In briefing_handler() in both d2/main/titles.c and d1/main/titles.c: either button advances (not skip entire briefing)
5. Add "menu_select_button": 0, "menu_back_button": 1 to default.json for future customization
6. Use named constants (MENU_JOY_BUTTON_SELECT, MENU_JOY_BUTTON_BACK) in C code

### Files
- d2/main/newmenu.c -- newmenu_handler, listbox_handler
- d1/main/newmenu.c -- same
- d2/main/titles.c -- briefing_handler
- d1/main/titles.c -- same
- android/app/src/main/assets/configs/controller/default.json

---

## Phase 7: Back Button in Config Screens

### Feature
Android back button exits touch config / controller config screens to the launcher.

### Fix
Add BackHandler { onBack() } composable in ControllerConfigPage.kt and TouchEditorPage.kt.

### Files
- android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt
- android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt

---

## Phase 8: Build and Verify

1. Build D1+D2 for Android, confirm no new warnings
2. Run android/run-code-quality.ps1 --fix
3. Run existing regression tests
4. Manual verification of all fixes

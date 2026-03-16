# Plan: Extra Controller/Touch Button Options

## TL;DR
Add "extra" (meta) game actions to both the controller config and touch editor in the launcher. These are actions like quicksave, ESC, guide bot commands, etc. that aren't in the standard kc_joystick[] array. Uses a C-side dispatch table with JNI entry point. Also add "axis as two buttons" mode for both controller and touch sticks. UI gets an "Extra" toggle to switch between standard and extra action lists.

## Architecture

### Meta Actions System
- C dispatch table maps action IDs to SDL key sequences (C = source of truth)
- JNI function `nativeMetaAction(actionId, pressed)` injects appropriate key events
- Kotlin defines action IDs (shared constants) and labels
- For controller: Java intercepts button events before nativeJoystickButton if mapped to meta action
- For touch: button callback calls nativeMetaAction instead of nativeJoystickButton
- D1/D2 filtering via a D2_ONLY set (same pattern as existing D2_ONLY_BUTTONS)

### Axis-as-Buttons
- Controller: stick picker dialog gets "Use as two buttons" toggle per axis
- Touch: AnalogStickControl gets buttonMode with negative/positive action bindings
- Uses existing joy_axisbutton_handler C code (axes already generate button events)
- For touch, Kotlin generates button press/release based on stick deflection + threshold

## Phases

### Phase 1: Data Structures & Constants

1. **Define meta action IDs and labels in TouchBindings.kt**
   - Add `META_ACTION_OFFSET = 1000` to distinguish from kc_joystick indices
   - Add constants: META_QUICK_SAVE (1000), META_QUICK_LOAD (1001), META_GAME_MENU (1002), META_GUIDE_BOT_MENU (1003), META_GUIDE_FIND_ENERGY (1004) through META_GUIDE_CLEAR_GOAL (1013), META_MULTIPLAYER_HUD (1014), META_DROP_FLAG (1015), META_DROP_MARKER (1016), META_WEAPON_1..10 (1020..1029), META_PAUSE (1030)
   - Add `META_BUTTON_LABELS: Map<Int, String>` with display names
   - Add `D2_ONLY_META_ACTIONS: Set<Int>` (guide bot, drop flag, drop marker)

2. **Create android_meta_actions.h** -- shared C header
   - Action ID constants (matching Kotlin values)
   - Flag constants (META_FLAG_INSTANT for one-shot actions)
   - Function declarations

3. **Create android_meta_actions.c** -- C dispatch table + JNI
   - Struct: `{ int action_id; int keys[]; int key_count; int flags; }`
   - Table for all meta actions with SDL key sequences + modifier handling
   - Function: `meta_action_dispatch(action_id, pressed)` injects SDL events
   - "Instant" actions inject full down+up on press, ignore release
   - JNI export: `Java_com_dxxredux_app_NativeMetaActions_nativeMetaAction()`

4. **Create NativeMetaActions.kt** -- JNI bridge
   - `external fun nativeMetaAction(actionId: Int, pressed: Int): Int`
   - Load via same native library as NativePilotPatcher

5. **Add android_meta_actions.c to build system** (CMakeLists.txt)

### Phase 2: UI - "Extra" Toggle in Pickers

6. **Controller config button picker** (ControllerConfigPage.kt ~L1431)
   - Add "Extra" toggle at top of ButtonFunctionPickerDialog
   - Default state shows existing BUTTON_KC_INDEX functions (standard controls)
   - When "Extra" toggled on, show META_BUTTON_LABELS instead
   - Filter D2_ONLY_META_ACTIONS when gameVariant == "d1"
   - Selected meta action stored as binding value >= 1000

7. **Touch editor button binding picker** (TouchEditorPage.kt ~L1399)
   - Same "Extra" toggle in ButtonBindingPicker dialog
   - Same filtered meta action list

### Phase 3: Controller Config Persistence & Dispatch

8. **Extend controller_config.json schema**
   - Add `"meta_bindings"` section: `{ "A": 1000, "L1": 1012 }`
   - When meta action assigned, remove from regular "bindings", add to "meta_bindings"
   - Update saveConfig() and loadConfig()

9. **Java-side controller interception** (MainActivity.kt)
   - Before nativeJoystickButton(button, state): check meta_bindings lookup table
   - If meta action: call NativeMetaActions.nativeMetaAction() instead
   - Load meta_bindings from controller_config.json at startup

### Phase 4: Touch Overlay Meta Action Support

10. **Touch button dispatch** (TouchOverlayView.kt)
    - If binding >= META_ACTION_OFFSET: call metaActionCallback instead of buttonCallback
    - Add metaActionCallback to TouchOverlayView
    - Wire in MainActivity.kt

11. **Touch radial menu meta actions**
    - Allow meta action IDs as RadialSegment bindings
    - In fireRadialSelection(), detect and dispatch via metaActionCallback

### Phase 5: Axis-as-Two-Buttons (Controller)

12. **Stick picker dialog extension** (ControllerConfigPage.kt ~L1499)
    - Per-axis "Use as two buttons" toggle
    - When enabled, show two button pickers (negative/positive direction)
    - SDL axis-button values: LS_X=(10,11), LS_Y=(12,13), RS_X=(14,15), RS_Y=(16,17)

13. **Controller config persistence for axis-buttons**
    - Add "axis_as_buttons" section to controller_config.json
    - When building kc_joystick settings, set axis slot to 255 (unbound), assign axis-button SDL values to chosen function slots

14. **Java-side axis-button interception**
    - Axis-buttons fire as SDL joystick buttons via C joy_axisbutton_handler
    - For standard actions: kc_joystick[] handles them (no Java intercept needed)
    - For meta actions: extend meta_bindings table to include axis-button SDL values

### Phase 6: Axis-as-Two-Buttons (Touch)

15. **AnalogStickControl button mode** (TouchControl.kt)
    - Add buttonMode, negativeXBinding, positiveXBinding, negativeYBinding, positiveYBinding fields
    - When buttonMode is true, stick generates button press/release instead of axis values
    - Threshold = existing deadzone value

16. **Touch overlay stick-as-buttons dispatch** (TouchOverlayView.kt)
    - If stick.control.buttonMode, track directions "pressed" past deadzone
    - On state change, fire buttonCallback or metaActionCallback
    - All four directions independent

17. **Touch editor stick properties panel** (TouchEditorPage.kt ~L1016)
    - "Button Mode" toggle
    - When enabled, show 4 binding pickers instead of axis pickers

18. **Touch layout persistence** (TouchLayoutRepository.kt)
    - Serialize new AnalogStickControl fields
    - Default buttonMode=false for backward compat

### Phase 7: D1/D2 Filtering

19. **Controller config D1 filtering** (ControllerConfigPage.kt)
    - Add gameVariant parameter (from SetupActivity)
    - Filter D2_ONLY_BUTTONS and D2_ONLY_META_ACTIONS from pickers when D1
    - ControllerConfigPage currently does NOT filter D2-only standard functions

20. **Touch editor D1 filtering** (TouchEditorPage.kt)
    - Filter META_BUTTON_LABELS by D2_ONLY_META_ACTIONS
    - Existing D2_ONLY_BUTTONS filtering in TouchOverlayView.kt handles standard buttons

## Relevant Files

### Kotlin (modify)
- `android/app/src/main/java/com/dxxredux/app/TouchBindings.kt` -- meta action constants, labels, D2-only set
- `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt` -- Extra toggle, axis-as-buttons, persistence
- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` -- Extra toggle, button mode panel
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` -- metaActionCallback, stick button-mode dispatch
- `android/app/src/main/java/com/dxxredux/app/TouchControl.kt` -- buttonMode fields
- `android/app/src/main/java/com/dxxredux/app/TouchLayoutRepository.kt` -- new field serialization
- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` -- callback wiring, controller interception

### Kotlin (create)
- `android/app/src/main/java/com/dxxredux/app/NativeMetaActions.kt` -- JNI bridge

### C (create)
- `android/app/src/main/cpp/shared/android_meta_actions.c` -- dispatch table + JNI
- `android/app/src/main/cpp/shared/android_meta_actions.h` -- shared action ID constants

### C (modify)
- `android/app/src/main/cpp/CMakeLists.txt` or equivalent -- add to build

### C (reference only)
- `d2/main/kconfig.c` -- kc_joystick[] layout, kconfig_fill_joy_settings()
- `d1/main/kconfig.c` -- D1 kc_joystick[] layout (48 entries vs 56)
- `d2/main/gamecntl.c` -- key handlers for ESC, F7, ALT+F2, ALT+F3, etc.
- `d2/main/escort.c` -- guide bot commands (SHIFT+1..0, SHIFT+F4)
- `d2/arch/sdl/joy.c` -- joy_axisbutton_handler(), axis_button_map layout
- `android/app/src/main/cpp/android_input.c` -- android_to_sdlk(), SDL_PushEvent pattern

## Meta Action Table

| ID | Name | Key Sequence | D1 | D2 |
|---|---|---|---|---|
| 1000 | Quick Save | ALT+F2 | Y | Y |
| 1001 | Quick Load | ALT+F3 | Y | Y |
| 1002 | Game Menu | ESC | Y | Y |
| 1003 | Guide Bot Menu | SHIFT+F4 | - | Y |
| 1004 | GB: Find Energy | SHIFT+1 | - | Y |
| 1005 | GB: Find Reactor | SHIFT+2 | - | Y |
| 1006 | GB: Find Shield | SHIFT+3 | - | Y |
| 1007 | GB: Find Powerup | SHIFT+4 | - | Y |
| 1008 | GB: Find Robot | SHIFT+5 | - | Y |
| 1009 | GB: Find Hostage | SHIFT+6 | - | Y |
| 1010 | GB: Scram | SHIFT+7 | - | Y |
| 1011 | GB: Find Items | SHIFT+8 | - | Y |
| 1012 | GB: Find Exit | SHIFT+9 | - | Y |
| 1013 | GB: Clear Goal | SHIFT+0 | - | Y |
| 1014 | Multiplayer HUD | F7 | Y | Y |
| 1015 | Drop Flag | ALT+0 | - | Y |
| 1016 | Drop Marker | F4 | - | Y |
| 1020-1029 | Weapon 1-10 | 1-0 keys | Y | Y |
| 1030 | Pause | PAUSE | Y | Y |

## Verification

1. Build APK: `.\gradlew clean assembleDebug` -- no compile errors
2. Code quality: `android\run-code-quality.ps1 --fix`
3. Manual: assign controller button to Quick Save, verify save file created
4. Manual: assign controller button to Game Menu (ESC), verify it opens the menu
5. Manual: touch button with Quick Save binding works in-game
6. Manual: LS_X as two buttons with two different actions
7. Manual: touch stick in button mode fires discrete presses
8. Manual: switch to D1, verify guide bot/drop flag/drop marker hidden
9. Introspection: verify state changes
10. Integration tests for C dispatch table
11. add a meta button to a test touch overlay and verify it by adding to this existing script: android\game_scripts\test_axis_mapping.json5

## Decisions

- Meta action IDs start at 1000 (META_ACTION_OFFSET) to cleanly separate from kc_joystick indices (0-55)
- C-side dispatch table is source of truth for action-to-key mappings (per project principles)
- Java intercepts controller buttons mapped to meta actions before sending to JNI (simpler than C-side intercept, no kconfig changes needed)
- Axis-as-buttons for both controller (uses existing joy_axisbutton_handler) and touch (Kotlin-side threshold logic)
- "Extra" toggle in pickers rather than a separate screen -- keeps UI compact
- controller_config.json gets new sections (meta_bindings, axis_as_buttons) rather than modifying existing bindings structure -- backward compatible

## Further Considerations

1. Some meta actions are "instant" (quicksave = inject full press+release on button press) while others should track press/release (pause, ESC). The dispatch table has a META_FLAG_INSTANT flag for this.
2. Guide bot commands require PlayerCfg.EscortHotKeys enabled. Recommend defaulting it on for Android or using the direct SHIFT+digit approach since it's simpler.
3. Weapon selection keys (1-0) already have joystick button bindings in kc_d2x[]. Key injection is simpler than exposing those slots and avoids kconfig layout changes.

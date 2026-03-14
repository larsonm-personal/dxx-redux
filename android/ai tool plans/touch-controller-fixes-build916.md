# Touch + Controller Fixes (Build 916)

7 user-reported bugs, 3 additional requirements, 8 implementation phases.

Root causes:
- dual D-pad dispatch (keyboard arrows + joystick buttons simultaneously)
- kconfig_fill_kb_settings zeroing all keyboard bindings via memset(0xFF)
- new-player vs reset-all using different default sources
- wrong aim/gyro defaults in bundled layout JSONs
- guide bot radial using raw keycodes instead of META_GUIDE actions

## Phase 1: D-pad dual dispatch fix

**Bug**: D-pad acts as "select" in menus and fires primary weapon in-game even when
bound to slide.

**Root cause**: `onKeyDown`/`onKeyUp` in MainActivity.kt don't intercept DPAD keycodes.
DPAD_UP/DOWN/LEFT/RIGHT fall through to `nativeKeyEvent`, injecting SDLK arrow keys.
Meanwhile, the HAT axis path in `onGenericMotionEvent` already sends joystick buttons
22-25 via `dispatchDpad()`. Both paths fire simultaneously.

**Fix**: In `onKeyDown` and `onKeyUp`, check for KEYCODE_DPAD_UP/DOWN/LEFT/RIGHT before
the `nativeKeyEvent` fallthrough. Route them through `dispatchDpad(keyCode, action)` and
return true.

**Files**: MainActivity.kt (onKeyDown ~L995, onKeyUp ~L1020)

## Phase 2: Keyboard defaults restored

**Bug**: In-game keyboard bindings are completely empty. MAP button doesn't open automap.

**Root cause**: `kconfig_fill_kb_settings()` in kconfig.c uses `memset(out, 0xFF, MAX_CONTROLS)`
as the base. `saveConfig()` in ControllerConfigPage.kt writes empty keyboard indices/values arrays
to controller_config.json. `load_config_into_playercfg()` reads them back, feeding zero entries to
`kconfig_fill_kb_settings`, which produces all-0xFF (unbound) keyboard settings.

**Fix**: Change `memset(out, 0xFF, MAX_CONTROLS)` to `memcpy(out, DefaultKeySettings[0], MAX_CONTROLS)`
in both D1 and D2 kconfig.c. This starts from correct defaults (including TAB=automap at index 44),
then overlays any explicit overrides from the JSON.

**Files**: d2/main/kconfig.c L2069, d1/main/kconfig.c L1963

## Phase 3: Reset-all unification + dialog text

**Bug**: New player and reset-all produce different bindings.

**Root cause**: `nativeResetToDefaults` uses `kconfig_get_default_settings()` (hardcoded C defaults).
But it doesn't delete controller_config.json, so the next `android_apply_gamepad_defaults()` reads
stale JSON overrides. New player reads the same JSON but through a different code path.

**Fix**:
1. In `nativeResetToDefaults` (android_gamepad_config.cpp ~L172): add `remove(CONFIG_PATH)`.
   SetupActivity already re-creates it from bundled defaults.
2. Update reset dialog text in SetupActivity.kt: change "In-game joystick settings for every pilot"
   to "In-game keyboard, joystick, and mouse settings for every pilot".

**Files**: android_gamepad_config.cpp, SetupActivity.kt

## Phase 4: Touch aim defaults (invertY + axis swap)

**Bug**: Touch aiming is inverted (swipe up = look down) and X/Y axes are swapped.

**Root cause**: Look stick defaults in advanced.json and claw.json have invertY=false and
axisX="Right Stick X" (turn L/R), axisY="Right Stick Y" (pitch U/D). The mapping is backwards.

**Fix**: In both layout JSONs, for look sticks: set `invertY: true`, swap `axisX` and `axisY`
values so axisX="Right Stick Y" (pitch), axisY="Right Stick X" (turn).

**Files**: advanced.json, claw.json (look stick sections)

## Phase 5: Gyro sensitivity + "Roll + Slide U/D" axis mode

**5a. Sensitivity defaults**: Change GyroConfig default sensitivityX/Y from 1f to 3f.
Update claw.json gyro section to match.

**5b. Register virtual axes 6/7**: joy.c Android init registers 6 axes. Add 2 more:
axis 6 (Bank L/R) and axis 7 (Slide U/D). These are virtual -- only the gyro "Roll" mode
uses them, no physical controller sends them.
- joy.c (D1+D2): expand axis_names[] to 8 entries, update loop counts
- kconfig_get_default_settings (D1+D2): add joy_out[21]=6 (Bank=axis6), joy_out[19]=7 (SlideUD=axis7)
- android_apply_gamepad_defaults: add matching defaults

**5c. Gyro UI**: Replace binary isAim toggle with tri-state axis mode selector.
- TouchBindings.kt: add AXIS_BANK=6, AXIS_SLIDE_UD=7 + labels + names
- GyroSettingsDialog: detect 3 modes (AIM: axes 2,3; SLIDE: axes 0,1; ROLL: axes 6,7).
  Add third radio button "Roll + slide up/down".

**Files**: TouchControl.kt, TouchBindings.kt, TouchEditorPage.kt, joy.c (d1+d2),
kconfig.c (d1+d2), android_gamepad_config.cpp, claw.json, advanced.json

## Phase 6: MAP button -- fixed by Phase 2

No separate code changes. Once keyboard defaults are restored, TAB injection from
`toggleAutomap()` matches the automap binding (DefaultKeySettings[0][44] = 0xF = TAB).
Verify during testing.

## Phase 7: Guide bot radial commands

**Bug**: All guide bot wheel segments open the guide bot menu instead of sending
individual commands.

**Root cause**: Radial segments use `KEYCODE_1`..`KEYCODE_9` with a special case in
`fireRadialSelection` that first sends Shift+F4 (opens escort menu), then a bare digit
key. The digit key lands on the escort menu window, but the timing/dispatch doesn't work.

**Fix**:
1. In advanced.json and claw.json: replace guide radial KEYCODE_* with META_GUIDE_* names
   (e.g. "Meta: GB: Find Energy"). Center binding: "Meta: GB: Clear Goal".
2. Remove the `rm.control.id == "Guide"` special case from fireRadialSelection in
   TouchOverlayView.kt. META actions route through the generic `isMetaAction(binding)` check,
   which dispatches via `metaActionCallback`. The C-side android_meta_actions.c injects the
   correct Shift+digit key combo for each META_GUIDE_* action.

**Files**: advanced.json, claw.json, TouchOverlayView.kt

## Phase 8: Build, lint, verify

1. Build D1+D2 native (cmake/ninja) -- no new warnings
2. Build APK (assembleDebug)
3. Code quality: android\run-code-quality.ps1 --fix
4. Emulator test: D-pad in menus, MAP button, guide bot wheel, keyboard bindings

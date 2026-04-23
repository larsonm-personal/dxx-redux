# Shield / TV launcher follow-ups

## scope

These are the remaining issues called out after the D1 render crash fix:

1. MIDI preview slider still skips when navigating away with Up/Down
2. MIDI preview source selector traps the gamepad D-pad
3. Android TV on-screen keyboard does not navigate correctly with controller input and does not seem to trigger the same game-view shift as phone IMEs
4. Controller label shows `virtual-search` instead of a useful Bluetooth controller name

## recommended order

### 1. Android TV keyboard input routing

Status:
- Implemented in `MainActivity.kt`
- Kotlin build validation passed before and after scoped code quality
- Controller-originated IME navigation now preserves device and timing metadata instead of using anonymous synthetic key events
- Controller-originated IME navigation now also accepts left-stick movement in addition to HAT navigation
- `AKEYCODE_DPAD_CENTER` now maps to engine Enter in `android/app/src/main/cpp/android_input.c`
- Initially selected Android `NM_TYPE_INPUT_MENU` items now enter edit mode during menu creation in both `d1/main/newmenu.c` and `d2/main/newmenu.c`
- Shield/phone on-device verification is still pending

Why first:
- Highest leverage user-facing blocker
- Strongest local hypothesis from current code
- Likely affects every controller-driven text-entry path, not just one screen

Current anchors:
- `showKeyboard()` and IME height reporting already exist in `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `dispatchKeyEvent()` and `onGenericMotionEvent()` in the same file now normalize controller navigation into IME-facing DPAD/BACK/CENTER events while `keyboardActive` is true
- `onKeyDown()` and `onKeyUp()` still act as the backstop that swallows controller events if the IME path does not consume them
- `android/app/src/main/cpp/android_input.c` owns Android-to-engine key translation for TV remote/gamepad select behavior
- `d1/main/newmenu.c` and `d2/main/newmenu.c` own the Android input-menu auto-edit state that decides whether the soft keyboard opens immediately

Current hypothesis:
- The remaining controller-only failure was likely split between missing directional synthesis for stick-driven navigation and a missing `DPAD_CENTER` native key translation for remote select
- The missing slide-up behavior may be secondary: the IME might not be reporting a non-zero inset on TV, or the keyboard could be on-screen without focusable navigation because the activity keeps eating controller events

Next steps:
- Re-test on both phone keyboard and Shield TV keyboard to confirm DPAD, HAT, left stick, and gamepad A/B navigation all reach the IME
- Re-test TV remote select in in-game menus now that `DPAD_CENTER` maps to Enter on the native side
- Confirm that menus opening with an initially selected text field now show the keyboard without a second confirm press
- Verify whether Shield now reports a non-zero IME inset and shifts the game view the same way as phone keyboards
- Add temporary `DXX-Keyboard` logs only if the on-device retest still shows missing navigation or missing height updates

### 2. MIDI preview slider Up/Down regression

Status:
- Implemented in `MusicPickerPage.kt`
- Kotlin build validation passed before and after scoped code quality
- Manual or scripted launcher verification is still pending

Why second:
- Very local surface in one file
- User reports the previous fix was ineffective, and the current code suggests why

Current anchors:
- `verticalDpadFocusEscape()` in `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt`
- MIDI preview `Slider(...)` in the same file

Current hypothesis:
- The current fix only intercepts `KeyEventType.KeyDown`
- The slider may still react on the matching KeyUp or internal slider handling path, producing the observed one-step skip when focus leaves vertically

Next steps:
- Verify on device that Up/Down now changes focus without seeking on the MIDI preview slider
- Confirm the same shared helper still behaves correctly on the CD preview sliders that reuse it
- Add one short launcher-side automation or manual adb verification script if this regression is likely to recur

### 3. MIDI source selector D-pad trap

Status:
- Replaced the exposed dropdown with a plain button-triggered `DropdownMenu` in `MusicPickerPage.kt`
- Kotlin build validation passed before and after scoped code quality
- Controller-vs-remote on-device verification is still pending

Why third:
- Same page as the slider issue, so it is efficient to investigate in the same tranche
- The likely fix surface is still local to `MusicPickerPage.kt`

Current anchors:
- `MidiSection()` source picker in `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt`
- The current source selector uses `ExposedDropdownMenuBox` + `OutlinedTextField(menuAnchor())`
- SetupActivity synthesizes HAT-axis transitions into DPAD key events for controller navigation

Current hypothesis:
- The Material exposed dropdown anchor does not cooperate well with gamepad-synthesized D-pad input on Android TV
- The TV remote works better because it emits direct DPAD key events, while joystick HAT events take a different path or focus timing

Next steps:
- Verify on Shield that both the TV remote and gamepad can move past the selector into the track list
- Only add remote-vs-controller event logs if the simplified selector still traps focus on device
- Keep any follow-up local to `MusicPickerPage.kt` unless a shared TV selector helper becomes clearly worthwhile

### 4. Controller display name detection

Why fourth:
- Lower functional severity than the keyboard and preview-navigation issues
- Likely needs device-property inspection before a safe implementation choice exists

Current anchors:
- Setup screen controller header in `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
- Controller config page detection block in `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt`
- Both currently show `gamepads.first().name` directly

Current hypothesis:
- On Shield, Android exposes `InputDevice.name == "virtual-search"` for the active Bluetooth controller path, so the current UI is faithfully rendering a poor platform-provided label
- A better human-readable label may be recoverable from other `InputDevice` properties or a Bluetooth-device lookup, but that needs on-device inspection first

Next steps:
- Add temporary logging or a debug dump of `id`, `name`, `descriptor`, `vendorId`, `productId`, `sources`, `keyboardType`, `isExternal`, and any API-available Bluetooth metadata for connected controllers
- Compare Shield output against a device where the correct controller name is already shown
- Then centralize controller display-name formatting in one launcher helper instead of repeating `gamepads.first().name` in multiple screens

## suggested tranche shape

### tranche A

- Completed code changes for TV keyboard controller routing
- Completed code changes for the MIDI preview slider Up/Down regression
- Pending on-device verification for both fixes

### tranche B

- Completed code changes for the MIDI source selector
- Pending on-device verification and any lightweight regression script or checklist

### tranche C

- Instrument controller identity on Shield and one known-good device
- Implement a shared controller display-name helper once the best source is known
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
- Shared debug logging now reuses the active log file across the launcher/game process handoff in `DebugLog.kt`, so a single run no longer starts a fresh `debuglog_*.txt` just because `MainActivity` is in the `:game` process
- Kotlin build validation passed before and after scoped code quality
- Controller-originated IME navigation now preserves device and timing metadata instead of using anonymous synthetic key events
- Controller-originated IME navigation now also accepts left-stick movement in addition to HAT navigation
- `AKEYCODE_DPAD_CENTER` now maps to engine Enter in `android/app/src/main/cpp/android_input.c`
- Initially selected Android `NM_TYPE_INPUT_MENU` items now enter edit mode during menu creation in both `d1/main/newmenu.c` and `d2/main/newmenu.c`
- Temporary keyboard-debug logging is now in place in `MainActivity.kt`, `android/app/src/main/cpp/android_input.c`, `d1/main/newmenu.c`, and `d2/main/newmenu.c`
- Exported TV logs confirmed that the keyboard demand/show path and DPAD reroute path are working, while `nativeSetKeyboardHeight` still stayed at `height=0` on TV
- `MainActivity.kt` now falls back from `Type.ime()` to `getInsetsIgnoringVisibility(Type.ime())`, then bottom occlusion from `getWindowVisibleDisplayFrame()`, and finally a TV-only heuristic height when every platform-reported value stays at zero
- `MainActivity.kt` now shows the IME through a hidden `KeyboardInputView` instead of the `SurfaceView`, giving Android TV keyboards a standard editor target for focus and D-pad routing
- New 2026-04-22 TV logs confirmed that the keyboard shift is now working, but also showed the rerouted DPAD events still entering `onKeyDown()` as plain controller keys, which meant the custom `KeyEvent.flags` marker was not surviving the framework dispatch path
- `MainActivity.kt` now uses a local IME-navigation dispatch-depth guard instead of a custom `KeyEvent.flags` bit, so rerouted DPAD/CENTER/BACK events bypass the controller swallow path in `onKeyDown()` / `onKeyUp()` while `super.dispatchKeyEvent()` is feeding the IME
- Newer 2026-04-22 Shield logs confirmed a second split in behavior: the only reliably working controller actions were confirm-style inputs, while synthetic controller DPAD/HAT events still returned `handled=false`, and controller `B` mapped to synthetic `BACK` and escaped the activity back to the launcher
- `MainActivity.kt` now synthesizes controller TV-keyboard navigation with a keyboard-style DPAD source to better match the working TV remote path, keeps controller `A` and `Select` as confirm actions, and closes the keyboard locally on controller `B` instead of remapping it to activity-level back behavior
- The latest supplied `debuglog_20260422_213918.txt` and `debuglog_20260422_213956.txt` were different builds (`11910` and `11920`), so they did not show a fresh single-run split-log regression by themselves
- `MainActivity.kt` now stops consuming joystick HAT/left-stick motion while the TV keyboard is open and lets the raw controller navigation path fall through to the system IME, while TV-remote `BACK` now closes the keyboard locally through the same `hideKeyboard()` path as controller `B`
- `d1/main/menu.c` now sets `Newmenu_allowed_chars = "09"` for the start-level selector, matching the existing D2 path so D1 level select requests the numeric keyboard instead of the letter keyboard
- Kotlin compile validation passed after the dispatch-depth reroute fix and again after scoped code quality
- Kotlin compile validation also passed before and after scoped code quality for the revised source-alignment and controller-button fix
- `:app:externalNativeBuildDebug` was later revalidated successfully during the interrupted-build follow-up, so the remaining risk here is on-device behavior, not compile health
- Kotlin compile validation, `:app:externalNativeBuildDebug`, `android\\run-code-quality.ps1 -Fix`, and `run-windows-build.ps1 -Target d1` all passed after the raw-controller passthrough and D1 numeric-keyboard fixes
- Shield/phone on-device verification is still pending

Why first:
- Highest leverage user-facing blocker
- Strongest local hypothesis from current code
- Likely affects every controller-driven text-entry path, not just one screen

Current anchors:
- `showKeyboard()` and IME height reporting already exist in `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`
- `dispatchKeyEvent()` and `onGenericMotionEvent()` in the same file now leave directional DPAD and joystick navigation on the raw system path while `keyboardActive` is true, but still map controller `A` / `Select` to confirm and controller `B` / remote `BACK` to a local keyboard close
- `onKeyDown()` and `onKeyUp()` still act as the backstop that swallows controller events if the IME path does not consume them
- `android/app/src/main/cpp/android_input.c` owns Android-to-engine key translation for TV remote/gamepad select behavior
- `d1/main/menu.c` and `d2/main/menu.c` own the start-level selector that decides whether Android should request numeric-only input
- `d1/main/newmenu.c` and `d2/main/newmenu.c` own the Android input-menu auto-edit state that decides whether the soft keyboard opens immediately

Current hypothesis:
- The remaining controller-only failure was likely split between missing directional synthesis for stick-driven navigation and a missing `DPAD_CENTER` native key translation for remote select
- The exported TV logs now show the slide-up problem more directly: the keyboard is shown and the IME reroute path reports `handled=true`, but the TV build kept reporting zero IME height, so the menu-shift logic never received a non-zero keyboard height
- The newest exported TV log tightened the remaining input root cause further: the shift path is fixed, but the rerouted controller and remote select events were still being swallowed by `MainActivity` because a custom `KeyEvent.flags` marker did not survive the framework dispatch path
- The latest local fixes now cover all three observed failure modes directly: the shift path no longer depends solely on visible IME insets, the IME no longer targets the `SurfaceView`, and rerouted IME navigation no longer depends on custom `KeyEvent.flags`
- The latest exported Shield log refined the remaining controller-only failure further: the reliable difference is not that remote input reports `handled=true`, but that the working remote path behaves like keyboard-style DPAD input while synthetic controller navigation was using a different synthetic source and then depending on a reroute helper with no original-event fallback
- The newest local fix narrows the remaining controller hypothesis further: if Bluetooth controller d-pad and left-stick navigation were failing because `MainActivity` was consuming the raw joystick event before the TV IME could see it, then stopping that interception should let the system keyboard handle controller navigation the same way it already handles the working TV remote path
- The D1 numeric-keyboard root cause was local and concrete: the start-level menu never set `Newmenu_allowed_chars = "09"`, so Android had no signal to request a number keyboard there

Next steps:
- Re-test on both phone keyboard and Shield TV keyboard to confirm Bluetooth controller d-pad, HAT, and left stick now navigate the TV keyboard when `MainActivity` leaves raw joystick motion on the system path
- Re-test TV remote select in in-game menus now that `DPAD_CENTER` maps to Enter on the native side
- Re-test TV remote select and remote `BACK` inside the on-screen keyboard now that `BACK` follows the same local-close path as controller `B`
- Re-test Bluetooth controller `A`, `B`, and `Select` while the keyboard is open to confirm `A` and `Select` confirm the highlighted character and `B` closes the keyboard
- Re-test D1 start-level selection to confirm it now requests a numeric keyboard instead of the letter keyboard
- Confirm that menus opening with an initially selected text field now show the keyboard without a second confirm press
- Verify whether Shield now reports a non-zero fallback-derived keyboard height and shifts the game view the same way as phone keyboards
- Confirm that a single launcher-to-game run now appends to one `debuglog_*.txt` file instead of splitting into separate launcher and game logs
- If the issue still reproduces, export the debug logs and look for `[KB] ime raw nav`, `[KB] closeImeButton`, `[AKEY]`, and `[KBMENU]` markers to locate which handoff is still failing

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
- Completed code changes for raw controller passthrough and D1 numeric start-level keyboard selection
- Completed temporary logging instrumentation for the TV keyboard path in Kotlin, JNI, and D1/D2 menu code
- Completed code changes for the MIDI preview slider Up/Down regression
- Pending on-device verification and log capture for the keyboard path
- Pending on-device verification for the MIDI preview slider fix

### tranche B

- Completed code changes for the MIDI source selector
- Pending on-device verification and any lightweight regression script or checklist

### tranche C

- Instrument controller identity on Shield and one known-good device
- Implement a shared controller display-name helper once the best source is known
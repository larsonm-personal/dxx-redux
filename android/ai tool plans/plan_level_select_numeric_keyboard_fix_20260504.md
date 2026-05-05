# Level Select Numeric Keyboard Fix Plan

Source bug: [android/outstanding_bugs.md](android/outstanding_bugs.md)

## 1. [ready to test] Trace regular-Android numeric IME input on the level-select prompt

### Scope
- Regular Android in-game level-select prompt opened from New Game
- Soft keyboard is requested in numeric mode
- Backspace sometimes works, number taps do not populate the field

### Code anchors
- [android/app/src/main/java/com/dxxredux/app/MainActivity.kt](android/app/src/main/java/com/dxxredux/app/MainActivity.kt) `showKeyboard`, `KeyboardInputView`, `GameInputConnection`
- [android/app/src/main/cpp/android_input.c](android/app/src/main/cpp/android_input.c) `android_show_keyboard`
- Repo memory: `/memories/repo/android-tv-keyboard-final-routing.md`

### Local hypothesis
- The numeric IME is taking a different delivery path than the full text keyboard. `GameInputConnection` handles `commitText` and `deleteSurroundingText`, but it does not currently bridge `sendKeyEvent`, which some numeric keypads use for digit taps.
- A secondary risk is the unconditional text-only flags added to numeric `inputType`, which can confuse certain IMEs.

### Cheap discriminating checks
- Inspect the current `GameInputConnection` overrides and confirm `sendKeyEvent` is not handled.
- Keep the first code change narrow enough that a focused Kotlin compile can validate it immediately.

### Planned change
- First pass: add explicit `sendKeyEvent` bridging for printable digit keys plus backspace/enter in `GameInputConnection`.
- If needed, narrow the `inputType` flags so numeric mode does not include text-only variation flags.

### Current status
- Implemented the narrower `inputType` fix in `MainActivity.kt`: numeric keyboard mode now keeps `TYPE_CLASS_NUMBER` untouched instead of OR-ing in text-only flags.
- Added `MainActivityInputTypeTest` to lock the regression down with a unit test for numeric vs text editor attributes.
- Focused validation passed with `:app:testDebugUnitTest --tests com.dxxredux.app.MainActivityInputTypeTest`.
- Scoped formatter/lint validation passed with `android\run-code-quality.ps1 -Fix -Paths android\app\src\main\java\com\dxxredux\app\MainActivity.kt`.
- No on-device verification yet, so the bug stays at ready-to-test instead of fixed.

### Validation plan
- Run a focused Kotlin compile for the Android app after the first edit.
- If compile passes, decide whether a second local change is needed before broader validation.
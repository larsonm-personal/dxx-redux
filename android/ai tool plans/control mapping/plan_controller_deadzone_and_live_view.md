# Controller deadzone + live-view fixes

Status legend: `[ ]` not started, `[~]` in progress, `[x]` done.

## Scope

- Fix Android launcher controller deadzone settings so analog stick deadzones affect in-game movement, not just axis-as-button activation.
- Fix the controller binding dialogs so the live deadzone/threshold preview updates while the dialog is open.
- Keep the change scoped to the Android launcher and Android-specific native controller config bridge.

## Plan

- [x] Trace the controller threshold path from `ControllerConfigPage.kt` into the native loader and verify which runtime variables consume it.
- [x] Apply launcher thresholds to both `joy_axis_button_deadzone[]` and the analog gameplay deadzone path (`PlayerCfg.JoystickDead[]`) when a physical axis is bound as an analog gameplay axis.
- [x] Update the controller picker dialogs to observe live axis-generation updates while open.
- [x] Normalize stick dialog defaults so analog stick deadzones reopen at the 10% stick default instead of inheriting the 30% button threshold default.
- [x] Validate with Android Kotlin/native compile, Android unit tests, Android code-quality pass, and the Windows host build helper.

## Result

- `android_gamepad_config.cpp` now maps launcher threshold percentages onto `PlayerCfg.JoystickDead[]` for analog gameplay bindings while preserving the existing `joy_axis_button_deadzone[]` behavior for axis-as-button mappings.
- `ControllerConfigPage.kt` now passes a live update token into the picker dialogs and uses mode-aware threshold normalization for stick dialogs.
- Validation passed:
  - `android\gradlew.bat :app:compileDebugKotlin :app:externalNativeBuildDebug`
  - `android\gradlew.bat :app:testDebugUnitTest`
  - `android\run-code-quality.ps1 -Fix`
  - `run-windows-build.ps1 -Target both -Compiler vs2022-community`

## Remaining manual check

- By-hand Android verification still needed with a Bluetooth controller: confirm small stick motion below the configured deadzone no longer moves the ship, and confirm the picker dialog bars move live while the dialog is open.
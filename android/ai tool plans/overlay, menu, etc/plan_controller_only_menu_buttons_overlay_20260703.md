# Controller-only menu buttons overlay

## Goal
Scope and, if straightforward, implement the controller-only default touch overlay behavior:

- add a built-in touch preset containing only the two menu buttons
- select that preset by default on a new install when no touch interface is detected
- keep the overlay menu option active when hidden, but hide the menu buttons until the controller menu-selection switch activates the menus

## Plan
- [x] Trace the current touch preset definitions, default config loading, and touch-interface detection paths
- [x] Identify what is already implemented and what still needs changes
- [x] Make focused code changes if the implementation surface is clear
- [x] Run scoped formatting and the most relevant available tests or compile checks
- [x] Update this plan with completed work and validation notes

## Findings

- Touch presets are bundled JSON assets loaded by `TouchLayoutRepository`.
- The active layout is not written on first launch; missing or corrupt active layout files fall back to `defaultLayout(context)`.
- The previous default preset was just the first asset alphabetically, which was `Advanced`.
- The overlay preference default was based only on whether a controller was currently detected, so no-touch/controller devices defaulted the overlay off.
- Controller menu cycling was already present through `META_MENU_CYCLE`, `cycleControllerMenu()`, and overlay visibility policy that keeps the overlay view alive while controller menus are open.
- The remaining missing behavior was selecting a no-touch default layout and hiding the two menu affordances while their controller-owned menus are closed.

## Changes

- Added a bundled `Controller Menus` touch preset with no gameplay controls, a Settings diagnostic button, and the existing More-actions control.
- Added no-touch default preset selection using `PackageManager.FEATURE_TOUCHSCREEN`.
- Added a shared default for `touch_overlay_enabled`: enabled on touchless devices, enabled on touch devices without a controller, disabled on touch devices with a controller.
- Added preset-specific draw and hit-test gates so `Controller Menus` hides More and Settings while closed, but shows them while the controller menu cycle has opened their menus.
- Kept existing touch layouts unchanged.

## Validation

- `android\run-code-quality.ps1 -Fix -Paths ...` passed for the touched files.
- `gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ControllerMenuTouchOverlayDefaultTest --tests com.dxxredux.app.OverlayVisibilityPolicyTest --tests com.dxxredux.app.ControllerMenuCycleTest` passed.
- `gradlew.bat :app:assembleDebug` passed, including Android CMake builds for arm64-v8a, armeabi-v7a, and x86_64.

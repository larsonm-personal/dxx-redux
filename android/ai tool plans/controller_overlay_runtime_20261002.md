# Controller overlay runtime behavior

## Scope

- Use the bundled Controller Menus layout in memory when a controller is detected and the launcher touch overlay is disabled
- Preserve the saved touch layout and gyro configuration
- Ignore disabled touch controls, including radial menus, when calculating extra actions
- Keep menus opened through controller cycling open after outside taps and action activation; retain explicit dismissal and screen transitions

## Work

- [x] Implement layout selection, action availability, and menu opening-mode tracking
- [x] Add unit and device-level regression coverage with a reusable runner
- [x] Run scoped code quality, relevant unit tests, Android CMake/APK build, and device checks
- [x] Review changes and record validation

## Implementation

- MainActivity selects the effective display layout on resume and before controller menu cycling, allowing a newly connected controller to use the controller preset
- The saved layout continues to supply gyro configuration and extra-action candidates, while the launcher toggle decides whether its touch bindings and wheels count as available
- Controller-opened extra actions and settings retain their opening mode across touch and controller activation; outside taps are consumed, and held-action release/cancellation keeps the extra-actions panel open
- Explicit B/Back/Menu dismissal, the existing menu-cycle timing policy, and actions that open another screen retain their closing behavior
- Replacing a display layout releases inputs before rebuilding control state
- Added test_controller_overlay.ps1 and registered it in the input_preferences coverage family

## Validation

- Scoped mixed-language code quality passed; subsequent edits to the device fixture also passed scoped checks
- 95 targeted unit tests passed across RemainingKeyTouchActionsTest, ControllerMenuTouchOverlayDefaultTest, ControllerMenuCycleTest, AdminTrayUiTest, OverlayVisibilityPolicyTest, GyroToggleConfigTest, and SettingsChildOverlayControllerTest
- :app:assembleDebug and :app:assembleDebugAndroidTest passed, including both engines' CMake build tasks for arm64-v8a, armeabi-v7a, and x86_64; no new source compiler warnings
- android/tests/test_controller_overlay.ps1 -Serial emulator-5556 passed: D1/D2 overlay cases, saved gyro/layout preservation, disabled touch action filtering, repeatable touch/controller activation, held release/cancellation, explicit dismissal, Automap transition, and ordinary touch-menu dismissal
- Coverage-family registration verified directly
- Full-suite catalog discovery is blocked by existing metadata in test_fov_demo_compatibility.jsonc: _standalone=false requires _owner; the unchanged HEAD file has the same missing owner, so the new regression was run directly
- git diff --check passed

Build and device output: android/temp/controller_overlay_build.log, android/temp/controller_overlay_test_build.log, android/temp/controller_overlay_device.log

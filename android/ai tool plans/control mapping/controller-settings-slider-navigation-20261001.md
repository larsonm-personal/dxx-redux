# Controller settings slider navigation

- [x] Reproduce FOV focus skipping with Android introspection and targeted diagnostics
- [x] Audit other menu sliders and custom interactive controls for the same focus issue
- [x] Fix the shared cause and add a reusable controller navigation regression
- [x] Run scoped code quality, relevant Android build, and emulator checks
- [x] Record final validation evidence

Preserve existing workspace edits and desktop/native menu behavior

## Findings and changes

- Wide FOV sliders lose geometric focus searches to the narrow texture-filter radio buttons. The previous APK fails the wide-layout regression with `FOV expected 100, got 0`; diagnostics show the first texture-filter radio focused instead. Explicit up/down links now connect the last resolution option, FOV, and first texture-filter option
- Touch editor properties and gyro settings allowed slider up/down keys to adjust values, and geometric searches could skip wide sliders when returning from small buttons. These forms now use field traversal order for up/down, including controls beside each other. Left/right remains available for slider adjustment
- Touch-stick deadzone used a continuous 0..50 slider and truncated values to integers. A right-arrow increment of 0.5 disappeared. The control now uses 51 integer positions
- Audited controller assignment sliders, audio/CD/MIDI preview sliders, native D1/D2 menu item types and slider navigation, custom controller/touch canvases, and focus exclusions. Controller assignment and preview dialogs already route vertical navigation; preview footers explicitly return focus to the slider. Native menus select sliders and skip noninteractive text. Controller canvas actions have controller selection paths
- Temporary FOV-specific diagnostics were removed. Reusable diagnostics remain in the instrumentation test
- The reported unchecked FOV entry is absent from the on-disk `android/outstanding_bugs.md`; preserve the user's existing checklist edits

## Regression runner

Build and install the debug app and Android test APK, then run `android/tests/test_slider_navigation.ps1`

The test dispatches controller key events through SetupActivity, exercises the production graphics page for D1 and D2 in portrait/landscape with normal and wide layouts, checks persistence and FOV bounds, and exercises touch slider traversal and integer deadzone adjustment. It restores config files and requested orientation

## Validation

- Scoped mixed-language code quality passed for the changed production Kotlin and PowerShell runner
- `:app:assembleDebug :app:assembleDebugAndroidTest` passed, including the relevant CMake tasks for both engines and all configured ABIs; no new source compiler warnings
- `android/tests/test_slider_navigation.ps1` passed on emulator-5554: all eight graphics combinations, bidirectional FOV boundary navigation, first texture-filter activation, FOV bounds, and touch float/integer slider navigation and adjustment
- `git diff --check` passed for the tracked implementation changes
- Evidence: `temp/slider-baseline-anchored.log` and its Android focus diagnostics show the previous APK failure; `temp/slider-order-build.log` and `temp/slider-order-test.log` record the final successful build and test

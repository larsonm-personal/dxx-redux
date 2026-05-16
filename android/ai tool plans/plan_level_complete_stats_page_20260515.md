# Level Complete Stats Page Plan

- [x] Trace the level-complete page code path and confirm the Android menu scale path touches it
- [x] Fix the OGL multi-line text spacing bug affecting the title lines
- [x] Exempt the level-complete page from Android menu scale magnification
- [x] Add delayed tap-anywhere advance using the shared cutscene tap suppress window
- [x] Run focused validation for the touched Android and D1/D2 slices

## Validation notes

- Android follow-up: `test_levelcomplete_touch_skip.json5` now uses a direct `trigger_levelcomplete` automation hook instead of stepping through the mine-exit sequence
- Root cause of the remaining emulator failure was in `android_test_inject_touch_tap()`: it injected raw SDL touch events and bypassed the `nativeTouchEvent()` delayed-ESC path that real Android taps use while `g_levelcomplete_active` is set
- `android\gradlew.bat :app:assembleDebug` passed after the native automation changes
- `android\run_test.ps1 -ScriptName test_levelcomplete_touch_skip.json5 -Game d2 -TimeoutSeconds 180` now passes on the emulator (`automation_result.json`: PASS, elapsed_ms=8923)
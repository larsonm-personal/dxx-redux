# Plan: test_dpad_triggers follow-up from report 20260522_232545

- [x] Create tranche plan file and capture the failing report context
- [x] Inspect the failing `test_dpad_triggers` report section and local automation script
- [x] Trace the owning D2 joystick-control index/binding path for item 50
- [x] Form one local hypothesis for why D2 reports `255` instead of `150`
- [x] Apply the smallest fix in the owning D2 binding/introspection path
- [x] Run focused validation for `test_dpad_triggers`
- [x] Update this note with the outcome and any remaining blockers

## Notes

- Report under review: `C:\local\dxx-redux\temp\test_reports\report_20260522_232545.md`
- Failing assertion in D2: `joystick_controls.items[50].value == 150` but introspection reported `255`
- Root cause: `saveConfig()` was serializing joystick settings through `NativePilotPatcher.nativeBuildJoySettings()`, which produced only 50 entries in the launcher path. D2 button slots `50-55` never made it into `controller_config.json`, so the live game reloaded them as `255`
- Fix: build joystick settings arrays in Kotlin from `buildJoyPairs()` and size them per variant (`50` for D1, `56` for D2 live joystick slots), then keep using the same arrays for JSON serialization and pilot patching
- Also bumped `CONTROLLER_CONFIG_VERSION` to `4` so stale truncated configs regenerate on first launch
- Added a focused JVM regression test in `android/app/src/test/java/com/dxxredux/app/ControllerConfigSerializationTest.kt` covering D2 indices `50`, `52`, and `54`, plus D1 bounds
- Validation:
	- `gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ControllerConfigSerializationTest`
	- `android/run_test.ps1 -ScriptName test_dpad_triggers.json5 -Game d2`
	- `android/run_test.ps1 -ScriptName test_dpad_triggers.json5 -Game d1`
- Outcome: D2 now writes default controller config version `4` and final game introspection reports `items[50]=150`, `items[52]=152`, `items[54]=154`; both D2 and D1 `test_dpad_triggers` runs passed

## Goal

Set up a no-UI Android TV performance test configuration so Shield or TV users can run a clean graphics test without opening the in-game video overlay or changing debug options manually.

## Status

- [x] Trace the startup prefs and overlay application path
- [x] Force a clean TV debug perf-test preference set at startup
- [x] Auto-show the video overlay for TV debug perf sessions
- [x] Run a focused Android build
- [x] Update this file with results

## Planned test shape

- Debug build only
- Android TV or gamepad-only devices only
- Show the video info overlay automatically on launch
- Keep merged-wall experiment at default
- Disable graphics and texture debug logging so the test is not dominated by debug-log overhead
- Leave core render settings such as filtering or MSAA unchanged for a cleaner baseline

## Result

- `MainActivity` now seeds a TV-only debug perf-test preference set before `DebugLog.init()` reads category prefs
- That startup override forces:
	- video info debug controls off
	- legacy merged-wall experiment off
	- graphics log category off
	- texture log category off
- `MainActivity.applyGraphicsDebugPrefs()` now auto-shows the video info overlay for debug TV sessions and explicitly resets merged-wall mode to default
- Core graphics settings are left alone so the run remains a usable baseline instead of a different rendering configuration

## Validation

- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:compileDebugKotlin --console=plain`: passed
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug --console=plain`: passed
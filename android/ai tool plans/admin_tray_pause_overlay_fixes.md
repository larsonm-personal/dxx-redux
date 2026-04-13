## Admin tray overlay and pause fixes

### Goals
- [x] Keep net stats and video info toggles enabled after activation instead of immediately reverting
- [x] Move the three overlay toggles into the requested right-side grid positions, swapping existing actions as needed
- [x] Keep cycle view from closing the settings tray while preserving pause ownership
- [x] Route quick save and quick load through pause-menu save/load flows instead of direct save/load actions if needed
- [x] Add targeted regression coverage for tray close behavior and slot ordering
- [x] Run Android code quality, targeted JVM tests, and a debug APK build

### Notes
- Likely touch points: `TouchOverlayView.kt`, `MainActivity.kt`, and Android/native pause-menu helpers
- Preserve existing working behavior for net events and auto-level toggle
- Quick save/load handoff stayed in Kotlin by queueing pause-close before ALT+F2/F3, so no new native hook was needed
- No existing emulator automation covered the admin tray path, so this pass used JVM regression tests plus `:app:testDebugUnitTest` and `:app:assembleDebug`
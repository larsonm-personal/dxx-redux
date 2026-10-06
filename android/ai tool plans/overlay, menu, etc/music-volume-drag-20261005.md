# Music overlay volume dragging

- [x] Trace touch handling and the delayed native state refresh
- [x] Reproduce missing immediate feedback with real MotionEvents and diagnostic logging
- [x] Update the thumb immediately, suppress duplicate commands, and align touch coordinates with the drawn track
- [x] Verify tapping, sustained dragging, release and cancellation on the emulator; run scoped quality checks and the Android build

The panel currently queues a native volume command on every touch event without updating its local state or invalidating. Each command restarts the 120 ms state refresh delay in MainActivity. The thumb therefore cannot follow a continuous drag. Drawing also uses an inset track while touch mapping uses the outer rectangle.

Keep the existing large volume hit area and discrete native 0-8 range. Preserve unrelated working tree changes.

Verification:

- Before the fix, emulator instrumentation logged `queued=[4] displayed=8` and failed on immediate touch-down feedback
- After the fix, the same test logged `queued=[4] displayed=4` and passed in landscape and portrait, including 100 stationary MOVE events, sideways drift, endpoint clamping, release coordinates, cancellation, and rejected commands
- Coverage extends the registered `test_controller_overlay.ps1` runner with production View touch dispatch and a recording callback in place of the native command queue
- Scoped code quality, both automation catalog checks, focused MusicOverlay JVM tests, and debug APK/instrumentation builds passed; the Android build includes the D1 and D2 x86_64 CMake targets
- Initial verification encountered a concurrent build file lock and a catalog timeout; serial retries passed
- Logs are under `android/temp/music-volume-20261005/`

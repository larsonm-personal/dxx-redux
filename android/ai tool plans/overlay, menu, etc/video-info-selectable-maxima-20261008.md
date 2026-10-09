# Video Info selectable graphics maxima

1. Inspect the Retroid capability report and trace AF/MSAA labels through JNI and Kotlin
2. Derive displayed maxima and availability from the same supported choices used by the controls; audit launcher and first-run explanations
3. Cover the Retroid report and partial/unsupported capabilities, run scoped quality checks, Kotlin tests and the Android build

Evidence: Retroid Mali-G77 MC9 reports color/depth sample counts [16, 8, 4], msaa_max 16, msaa_2 4, msaa_4 4 and aniso_max 16. JNI indices are correct. The overlay incorrectly presents the raw driver maximum as a selectable maximum

Status: implemented and verified

- Overlay AF/MSAA maxima and availability now use the supported selectable choices, preserving effective-sample explanations
- Launcher AF help uses the selectable ceiling; launcher MSAA and first-run explanations already handle rounded samples correctly
- Added exact label introspection and regression coverage for the Retroid report, higher driver limits, partial support and unavailable features
- Scoped formatting/lint and whitespace checks passed
- Nine focused Kotlin tests passed; the arm64 debug APK and both native game targets built successfully
- Read the Retroid reports through ADB; did not replace its running installation

Validation logs: `android/temp/video-maxima-quality.log` and `android/temp/video-maxima-build.log`

Follow-up: expose the existing native `msaa_max` in Kotlin and explain driver limits above the game's selectable ceiling in launcher MSAA help. Keep higher sample counts disabled pending rendering and performance validation; native settings normalization and preview validation also currently enforce 4x

Follow-up validation: nine focused Kotlin tests passed, instrumentation fixtures compiled, arm64 APK and both native game targets built, and scoped formatting/lint passed. Logs: `android/temp/msaa-driver-note-build.log` and `android/temp/msaa-driver-note-quality.log`

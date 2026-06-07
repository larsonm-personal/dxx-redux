# Plan: automap marker quick menu

- [x] Trace the Android automap marker quick menu and native marker command flow
- [x] Change quick menus so jump shows placed markers and drop/set show free slots
- [x] Add numbered marker placement for Android touch while keeping text naming available separately
- [x] Draw marker number labels over automap marker objects in D2
- [x] Add or update focused tests for quick menu policy
- [x] Run scoped formatting/tests and record results

## Verification

- Passed: `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\java\com\dxxredux\app\AutomapTouchPolicy.kt','android\app\src\test\java\com\dxxredux\app\AutomapTouchPolicyTest.kt','android\app\src\main\java\com\dxxredux\app\MainActivity.kt','android\app\src\main\java\com\dxxredux\app\TouchOverlayView.kt','android\app\src\main\cpp\android_input.c','android\app\src\main\cpp\jni_main.c','d2\main\automap.c')`
- Passed: `.\android\gradlew.bat -p android :app:testDebugUnitTest --tests com.dxxredux.app.AutomapTouchPolicyTest`
- Passed: `.\android\gradlew.bat -p android :app:assembleDebug`

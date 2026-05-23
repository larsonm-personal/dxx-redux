# Overlay menu follow-ups - 2026-05-22

## Goals
- Add the response exponent control to the single-axis editor in the same style as the stick editor
- Remove automap from the settings/admin tray so it only appears through unbound controls when needed
- Make settings-button cycling close the unbound menu after an action or after the menu has been left open for more than 2.5 seconds
- Make the game-menu action close the overlay settings stack and immediately open the native game menu

## Plan
1. [done] Inspect the current overlay/controller menu cycle and single-axis editor code paths
2. [done] Patch `TouchOverlayView.kt` to track unbound-menu open/action state and update the menu cycle rules
3. [done] Patch `MainActivity.kt` so the settings `Game Menu` action closes overlay UI before opening ESC menu
4. [done] Patch `ControllerConfigPage.kt` so the single-axis editor shows the exponent control consistently
5. [done] Extend focused JVM tests for admin tray visibility and controller menu cycle rules, then rerun focused tests

## Validation
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.AdminTrayUiTest --tests com.dxxredux.app.ControllerMenuCycleTest --tests com.dxxredux.app.RemainingKeyTouchActionsTest --tests com.dxxredux.app.ControllerAxisExponentTest --console=plain`
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt','android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt','android/app/src/main/java/com/dxxredux/app/MainActivity.kt','android/app/src/test/java/com/dxxredux/app/AdminTrayUiTest.kt','android/app/src/test/java/com/dxxredux/app/ControllerMenuCycleTest.kt')`
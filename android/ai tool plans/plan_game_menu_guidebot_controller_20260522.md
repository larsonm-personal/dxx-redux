# Overlay Game Menu and Guidebot Controller Menu Plan

## Goals
- Make the overlay settings `Game Menu` action hide the overlay settings UI and open the native ESC game menu from the paused state
- Design and implement a controller-friendly GuideBot menu with d-pad selection, A to select, B to close, and second GuideBot button press to close
- Keep multiplayer behavior from pausing the game while the GuideBot controller menu is open

## Plan
1. [done] Inspect current overlay game-menu and GuideBot menu paths
2. [done] Patch the overlay game-menu action so native pause can transition to ESC game menu reliably
3. [done] Add native GuideBot controller-menu state, drawing highlight, and d-pad/A/B input handling
4. [done] Wire Android controller GuideBot button presses to toggle the controller menu
5. [done] Run focused Kotlin/native validation and code-quality checks

## Validation
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:compileDebugKotlin :app:externalNativeBuildDebug --console=plain`
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.AdminTrayUiTest --tests com.dxxredux.app.ControllerMenuCycleTest --tests com.dxxredux.app.RemainingKeyTouchActionsTest --console=plain`
- `cd repo-root; $env:JAVA_HOME='c:\local\jdk-21'; .\android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/MainActivity.kt','android/app/src/main/cpp/android_input.c')`
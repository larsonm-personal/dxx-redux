# Launcher Touch Highlight Flash Plan

## Problem
- On phones using the touch launcher, a green button focus highlight can briefly appear when returning from a submenu to the main launcher page
- The same focus artifact may affect other launcher menus

## Goals
- Find the focus or pressed-state path that causes controller-style highlight to show during touch navigation
- Keep controller and Android TV navigation intact
- Add or update a focused test if practical
- Run the relevant validation commands

## Plan
- [x] Inspect launcher Compose focus/highlight code and recent menu navigation paths
- [x] Identify why returning to a page can briefly request or retain focus in touch mode
- [x] Patch the root cause with a narrow Android launcher change
- [x] Add or update focused JVM tests if the behavior can be covered outside instrumentation
- [x] Run code quality/test validation for changed files

## Notes
- Do not edit android/outstanding_bugs.md
- The main launcher was forcing keyboard input mode and focusing Define Controls whenever returning from a subpage
- Touch input now clears controller-navigation focus mode; Android TV and controller navigation still seed focus
- Validation: android\run-code-quality.ps1 -Fix
- Validation: .\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.LauncherFocusPolicyTest

# Robot and Hostage HUD Counts

## Goal
Add a saved Game Preferences option to show robot kill counts and hostage counts in the upper-right HUD below the score line, using the same native score-line rendering style.

## Checklist
- [x] Find score-line rendering, player preference save/load, and existing robot/hostage tracking in D1 and D2
- [x] Add a player-file backed preference and launcher toggle for the HUD counts
- [x] Track hostages lost when the player's ship is destroyed
- [x] Draw robot and hostage count lines under the score in D1 and D2
- [x] Validate with focused source checks, code quality, and build where practical

## Validation
- `android/run-code-quality.ps1 -Fix` passed after formatting touched Android C/C++ and Kotlin files
- `run-windows-build.ps1` passed for D1 and D2 Windows builds
- `android/gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain --no-daemon` passed with JDK 21 after adding the missing shared helper declarations
- Final `android/run-code-quality.ps1` passed

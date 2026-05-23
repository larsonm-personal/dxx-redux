# Mod detail path and hash display plan

## Goal
- Show the full local path to a mod file as the first line in each individual mod detail popup
- Show expected and actual SHA-256 values for base file mismatches in the detail popup problems section
- Keep the change scoped to launcher detail/reporting UI and focused tests

## Tasks
- [x] Inspect current mod details data and dialog rendering
- [x] Add full-path detail data and render it first
- [x] Add expected/actual SHA-256 mismatch text in problems output
- [x] Update focused tests
- [x] Run focused validation and formatting checks

## Validation
- `cd android; .\gradlew.bat ':app:testDebugUnitTest' --tests 'com.dxxredux.app.ModManagerDetailsTest'`
- `android\run-code-quality.ps1 -Fix -Paths android\app\src\main\java\com\dxxredux\app\ModManager.kt,android\app\src\main\java\com\dxxredux\app\SetupActivity.kt,android\app\src\test\java\com\dxxredux\app\ModManagerDetailsTest.kt`
- `c:\local\jdk-21\bin\java.exe -jar c:\local\ktlint-1.8.0\ktlint.jar --format android\app\src\test\java\com\dxxredux\app\ModManagerDetailsTest.kt`
- `git diff --check -- android/app/src/main/java/com/dxxredux/app/ModManager.kt android/app/src/main/java/com/dxxredux/app/SetupActivity.kt android/app/src/test/java/com/dxxredux/app/ModManagerDetailsTest.kt`

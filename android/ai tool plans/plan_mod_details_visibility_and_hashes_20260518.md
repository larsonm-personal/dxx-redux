# Mod details visibility and hashes plan

## Goal
- Fix patch conflict problem text when there is no other mod name to show
- Show base file hashes in the Base Files section: expected and actual on mismatch, expected on match
- Let Notes text wrap fully instead of being cut off
- Show up to the first three example files per Files category with an ellipsis when more examples exist, without cutting off text

## Tasks
- [x] Inspect current details model, conflict text, file category summaries, and popup rendering
- [x] Fix conflict problem wording and wrapping in Problems
- [x] Add Base Files hash display
- [x] Adjust Files examples to first-three-plus-ellipsis and full wrapping
- [x] Adjust Notes wrapping
- [x] Update focused tests
- [x] Run formatting and focused validation

## Validation
- `cd android; .\gradlew.bat ':app:testDebugUnitTest' --tests 'com.dxxredux.app.ModManagerDetailsTest'`
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix -Paths android\app\src\main\java\com\dxxredux\app\ModManager.kt,android\app\src\main\java\com\dxxredux\app\SetupActivity.kt,android\app\src\test\java\com\dxxredux\app\ModManagerDetailsTest.kt`
- `java -jar c:\local\ktlint-1.8.0\ktlint.jar --format android\app\src\test\java\com\dxxredux\app\ModManagerDetailsTest.kt`
- Editor diagnostics on touched Kotlin files
- `git diff --check -- android/app/src/main/java/com/dxxredux/app/ModManager.kt android/app/src/main/java/com/dxxredux/app/SetupActivity.kt android/app/src/test/java/com/dxxredux/app/ModManagerDetailsTest.kt "android/ai tool plans/plan_mod_details_visibility_and_hashes_20260518.md"`

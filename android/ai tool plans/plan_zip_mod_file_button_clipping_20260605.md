# Zip Mod File Button Clipping Plan

## Goal
- [x] Fix the file buttons in zip mod info popups so their borders are visible and their labels are not clipped by the rounded left edge.

## Steps
- [x] Locate the zip mod file list UI in `SetupSections.kt`.
- [x] Replace the plain text-style file row button with a bordered button.
- [x] Add explicit horizontal padding so file names and metadata sit clearly inside the border.
- [x] Run focused code quality on the touched Kotlin file.

## Verification
- `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/SetupSections.kt')` passed.
- `android/gradlew.bat :app:compileDebugKotlin` passed.

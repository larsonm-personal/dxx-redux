# Save Explorer Recent Dedup

## Goal
Fix the Save Explorer "Ten Recent" tab so repeated autosave entries do not crowd out distinct recent saves.

## Plan
- [x] Read the save explorer UI and native bridge paths.
- [x] Add a focused recent-list dedupe key in the Kotlin explorer UI.
- [x] Run scoped formatting or compilation checks for the touched Kotlin file.
- [x] Mark this plan complete with validation notes.

## Validation
- `.\android\run-code-quality.ps1 -Fix -Paths android\app\src\main\java\com\dxxredux\app\SetupSaveExplorer.kt` passed.
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.SaveExplorerTest` passed from `android\` with JDK 21.

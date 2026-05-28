# Resume save thumbnail fullscreen plan

## Goal
Add tappable save thumbnails in launcher resume flows, with a fullscreen-width preview overlay that dismisses on any tap

## Phases
- [x] Locate resume recent save and choose save Compose UI, thumbnail model fields, and existing tests
- [x] Add shared thumbnail preview UI behavior while preserving absent-thumbnail detection
- [x] Add or update launcher tests for thumbnail presence and fullscreen dismissal behavior
- [x] Run code quality and targeted tests, then record results

## Notes
- Keep this in launcher Kotlin unless existing UI already delegates elsewhere
- Return from choose-save thumbnail preview to the choose-save window
- Do not alter native save thumbnail format unless the UI path exposes a gap

## Results
- `android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/SetupResumePanel.kt','android/app/src/test/java/com/dxxredux/app/ResumeSavePanelTest.kt')` passed
- Direct ktlint format was run on `android/app/src/test/java/com/dxxredux/app/ResumeSavePanelTest.kt` because the wrapper only scans main Kotlin sources
- `android\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ResumeSavePanelTest --no-daemon` passed

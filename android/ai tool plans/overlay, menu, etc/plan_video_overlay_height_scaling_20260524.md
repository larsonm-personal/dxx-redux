# Video overlay height scaling plan

## Goal
Scale the video info overlay by height so every visible row fits on short displays, with touch hit regions matching the scaled button rows.

## Requirements
- Use view height as the fit constraint
- Shrink non-touch info text first so buttons fit below it
- Scale touch button drawing and hit rectangles together
- Keep existing controller and touch behavior intact
- Validate with focused unit coverage plus Kotlin formatting/tests

## Steps
- [x] Inspect current overlay layout and nearby tests
- [x] Add a height-only layout calculator for info rows and action rows
- [x] Apply separate info and button metrics in drawing and hit rectangles
- [x] Add unit tests for normal, debug, and very short heights
- [x] Run code quality and focused Android unit tests

## Validation
- `android\stop-stale-formatters.ps1`: no stale formatter tasks
- `android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt','android/app/src/test/java/com/dxxredux/app/VideoInfoOverlayLayoutTest.kt')`: passed
- `Push-Location android; .\gradlew.bat :app:testDebugUnitTest --tests VideoInfoOverlayLayoutTest`: passed
- VS Code diagnostics: no errors in touched Kotlin files

## Git ignore follow-up
- Narrowed `.gitignore` from unanchored `app/` to root-only `/app/` so new files under `android/app/` appear in status
- Verified `android/app/src/test/java/com/dxxredux/app/VideoInfoOverlayLayoutTest.kt` is no longer ignored
- Verified generated `android/app/build/.../BuildConfig.java` remains ignored by the existing `build*/` rule

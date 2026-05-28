# Launcher Save Preview Thumbnail 2x Plan

## Goal
Increase the launcher save preview thumbnail resolution by 2x while leaving in-game save thumbnails unchanged where possible. Old or mismatched launcher metadata thumbnails should not be displayed.

## Steps
- [x] Locate launcher preview thumbnail metadata generation and display paths
- [x] Update Android launcher metadata thumbnail dimensions and validation
- [x] Keep in-game thumbnail display behavior unchanged unless shared constants require a guarded split
- [x] Add or update focused tests for correct-size and wrong-size launcher thumbnails
- [x] Run targeted tests and code quality checks

## Notes
- No launcher backwards compatibility is required before first release
- D1 and D2 save paths may both need equivalent updates because metadata generation is duplicated

## Validation
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --tests com.dxxredux.app.ResumeSavePanelTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `buildd1\maths\test_android_save_meta.exe`
- `buildd2\maths\test_android_save_meta.exe`
- `android\run-code-quality.ps1 -Fix`

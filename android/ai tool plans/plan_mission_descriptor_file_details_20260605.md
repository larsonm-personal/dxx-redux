# Mission Descriptor File Details Plan

## Goal
- [x] Show parsed mission descriptor details for standalone `.mn2` and `.msn` files in the existing file info popup.

## Steps
- [x] Locate the existing file info popup and mission descriptor parser.
- [x] Reuse the mission descriptor parser for standalone `.mn2` and `.msn` file details.
- [x] Render title, type, author, editor, level count, and level names when present.
- [x] Add focused unit coverage for descriptor parsing if needed.
- [x] Run code quality and focused build/test checks.

## Verification
- `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/SetupSections.kt','android/app/src/main/java/com/dxxredux/app/SetupActivity.kt','android/app/src/test/java/com/dxxredux/app/MissionDescriptorFileDetailsTest.kt')` passed.
- `android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.MissionDescriptorFileDetailsTest --tests com.dxxredux.app.SectorgameMissionZipTest` passed.
- `android/gradlew.bat :app:assembleDebug` passed.

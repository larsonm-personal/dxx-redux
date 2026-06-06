# Mission Zip Readme View Plan

## Goal
- [ ] Show text documentation inside mission zip details with predictable readme selection and text file ordering.

## Steps
- [x] Inspect mission zip metadata, details UI, and existing tests.
- [x] Add readme/text-file metadata helpers and persistence if needed.
- [x] Move `.txt` constituents to the top of mission zip file lists.
- [x] Add file-details View action and a launcher text viewer.
- [x] Add top-level View readme action for the chosen readme.
- [x] Add focused JVM tests for ordering and readme selection.
- [x] Run formatting and focused tests.

## Verification
- [x] `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/MissionZip.kt','android/app/src/main/java/com/dxxredux/app/SetupSections.kt','android/app/src/test/java/com/dxxredux/app/MissionZipTest.kt','android/ai tool plans/plan_mission_zip_readme_view_20260606.md')`
- [x] `android/gradlew.bat -p android :app:testDebugUnitTest --tests com.dxxredux.app.MissionZipTest`
- [x] `android/gradlew.bat -p android :app:testDebugUnitTest --tests com.dxxredux.app.ModManagerMissionZipTest`

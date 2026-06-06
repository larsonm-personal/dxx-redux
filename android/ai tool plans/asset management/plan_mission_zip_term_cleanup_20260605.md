# Mission Zip Term Cleanup Plan

## Goal
- [x] Remove the old sectorgame naming from code, filenames, comments, tests, and non-planning docs so full text search only finds it in planning docs.

## Work Items
- [x] Rename `SectorgameMissionZip.kt` and its test to mission-zip names.
- [x] Rename Kotlin symbols and references from `SectorgameMissionZip` to `MissionZip`.
- [x] Replace non-planning comments, temp-file prefixes, and docs that still use the old term.
- [x] Verify full text search only reports planning docs.
- [x] Run focused unit tests and debug assemble.

## Verification
- [x] `rg -n -i "sectorgame" .` only reports files under `android/ai tool plans`.
- [x] `rg --files | rg -i "sectorgame"` only reports a planning-doc filename.
- [x] `android/run-code-quality.ps1 -Fix -Paths ...` passed for changed Kotlin files.
- [x] `android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.MissionZipTest --tests com.dxxredux.app.ModManagerMissionZipTest --tests com.dxxredux.app.MissionDescriptorFileDetailsTest --tests com.dxxredux.app.GameFileMetadataTest` passed.
- [x] `android/gradlew.bat :app:assembleDebug` passed.

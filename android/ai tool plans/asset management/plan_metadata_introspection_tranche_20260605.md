# Metadata Introspection Tranche Plan

## Goal
- [x] Add the first reviewed metadata improvements while keeping long file/entry lists bounded in the launcher UI.

## Scope
- [x] Extend mission descriptor parsing with secret level count/list and common asset reference keys.
- [x] Add HOG total embedded byte details.
- [x] Add PIG animated bitmap group hints.
- [x] Add POG header/count/override summaries.
- [x] Make metadata example lists explicitly capped and scrollable with limited height when many rows are shown.
- [x] Add focused unit tests and run formatter/tests/build.

## Verification
- [x] `android/run-code-quality.ps1 -Fix -Paths ...` passed for changed Kotlin files.
- [x] `android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.GameFileFormatsTest --tests com.dxxredux.app.GameFileMetadataTest --tests com.dxxredux.app.MissionDescriptorFileDetailsTest --tests com.dxxredux.app.MissionZipTest --tests com.dxxredux.app.ModManagerMissionZipTest` passed.
- [x] `android/gradlew.bat :app:assembleDebug` passed.

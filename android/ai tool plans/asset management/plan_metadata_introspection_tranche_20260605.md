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

## Follow-up UI Cleanup
- [x] Replace the visible "Examples" metadata section with a bounded scrollable "Full contents" text box.
- [x] `android/run-code-quality.ps1 -Fix -Paths ...` passed for the UI cleanup.
- [x] `android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.GameFileMetadataTest --tests com.dxxredux.app.MissionDescriptorFileDetailsTest --tests com.dxxredux.app.MissionZipTest --tests com.dxxredux.app.ModManagerMissionZipTest` passed.
- [x] `android/gradlew.bat :app:assembleDebug` passed after the UI cleanup.

## Follow-up Contents Explorer
- [x] Replace inline full contents boxes with a five-row preview plus a contents explorer dialog.
- [x] Expose full metadata contents from the Kotlin metadata model instead of truncating at parse time.
- [x] Add contents sorting by name ascending, name descending, size ascending, size descending, and type.
- [x] Run formatter, focused metadata tests, and assembleDebug.

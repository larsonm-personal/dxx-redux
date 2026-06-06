# Uneasy Zip Hog Real Path Fix Plan

## Goal
- [x] Fix the remaining `Can't open file <missions/Uneasy4.rl2>` crash for staged sectorgame mission zips.

## Findings
- The mission descriptor is visible in the mission picker.
- The level file remains missing because the mission HOG is not mounted.
- `PHYSFSX_addRelToSearchPath("missions/Uneasy4.hog")` uses `PHYSFSX_getRealPath`, which appends the logical path to the real directory returned by PhysFS.
- Mounting `stageDir` at the `missions` mount point makes that helper look for `stageDir/missions/Uneasy4.hog`, but the previous staging layout wrote `stageDir/Uneasy4.hog`.

## Steps
- [x] Change mission zip staging to mirror the logical `missions/` directory on disk.
- [x] Mount the generated staging root normally instead of mounting it at the `missions` mount point.
- [x] Keep bundled `.dxa` files mountable from the generated `missions/` directory.
- [x] Update launch handoff regression coverage.
- [x] Run code quality, focused tests, and Android debug build.

## Verification
- `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/ModManager.kt','android/app/src/test/java/com/dxxredux/app/ModManagerMissionZipTest.kt')` passed.
- `android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ModManagerMissionZipTest --tests com.dxxredux.app.SectorgameMissionZipTest` passed.
- `android/gradlew.bat :app:assembleDebug` passed.

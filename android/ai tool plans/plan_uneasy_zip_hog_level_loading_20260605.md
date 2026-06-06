# Uneasy Zip Hog Level Loading Plan

## Goal
- [x] Fix sectorgame mission zips that show in the mission list but fail to load levels from their bundled HOG files.

## Findings
- `Uneasy4.mn2` is visible after mounting the outer zip at `missions`.
- The crash `Can't open file <missions/Uneasy4.rl2>` means the mission HOG was not mounted or otherwise visible when the level loader looked for the RL2.

## Steps
- [x] Trace mission HOG mounting and Android launch handoff.
- [x] Choose a minimal launcher/native handoff that exposes `.mn2` plus bundled `.hog` contents.
- [x] Implement the fix in launcher handoff for D1 and D2.
- [x] Add focused regression coverage for the launch handoff.
- [x] Run code quality and focused build/test checks.

## Implementation Notes
- Enabled mission zips are staged into `<game>x-redux/.generated_mission_zips/<zip-name>/` before launch.
- The generated directory is mounted at the PhysFS `missions` mount point so `.mn2` and `.hog` files are visible as real mission files.
- Bundled `.dxa` files are also written into `.active_mod_paths` as standalone archive paths after staging.
- The staging directory is regenerated on each launch handoff and is private launcher storage, not a user-facing extraction.

## Verification
- `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/ModManager.kt','android/app/src/test/java/com/dxxredux/app/ModManagerMissionZipTest.kt')` passed.
- `android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.SectorgameMissionZipTest --tests com.dxxredux.app.ModManagerMissionZipTest` passed.
- `android/gradlew.bat :app:assembleDebug` passed.

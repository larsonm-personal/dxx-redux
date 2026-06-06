# Uneasy Mission Zip Ogg Music Plan

## Goal
- [x] Investigate why `uneasy4.ogg` from `Uneasy4.zip` does not play and determine whether the mission zip import can make it work as looping level music.

## Steps
- [x] Trace D2 music selection code for OGG, song lists, CD audio, and mission-specific song loading.
- [x] Inspect `Uneasy4.zip` and bundled archive contents for `descent.sng`, `dxx-r.sng`, `.ogg`, and `.s22`.
- [x] Determine whether the OGG is inaccessible, lacks a song-list reference, or is overridden by launcher music mode.
- [x] Implement the smallest launcher/game handoff fix if feasible.
- [x] Add focused regression coverage and run code quality/build checks.

## Findings
- The outer `Uneasy4.zip` contains `Uneasy4.mn2`, `Uneasy4.hog`, and `Uneasy4.dxa`.
- `Uneasy4.dxa` contains `descent.sng`, `descent2.s22`, and `Uneasy4.ogg`.
- `descent.sng` lists the normal title/briefing/endgame slots followed by `Uneasy4.ogg` as the first level song.
- The engine only uses this song list in `MUSIC_TYPE_BUILTIN`.
- Launcher CD mode writes `MusicType=2`, which takes the Redbook branch and ignores the mission song list.

## Implementation Notes
- `ModManager.hasEnabledMissionZipBuiltinMusic(game)` scans enabled mission zip DXA files for `descent.sng` or `dxx-r.sng` entries that reference OGG, MP3, FLAC, or MID music.
- `writeMusicConfigForLaunch(game)` now writes `MusicType=1` for a launch when the user's selected mode is CD and an enabled mission zip supplies built-in/add-on mixer music.
- The stored launcher preference remains CD; the override is launch-time config only.
- Custom file music mode is left alone.

## Verification
- `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/java/com/dxxredux/app/ModManager.kt','android/app/src/main/java/com/dxxredux/app/SetupActivity.kt','android/app/src/main/java/com/dxxredux/app/SetupConfigFiles.kt','android/app/src/test/java/com/dxxredux/app/ModManagerMissionZipTest.kt')` passed.
- `android/gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ModManagerMissionZipTest --tests com.dxxredux.app.MissionZipTest` passed.
- `android/gradlew.bat :app:assembleDebug` passed.

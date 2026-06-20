# RAR import support

## Goal
- Add RAR archive support for mission import and music scan tooling, starting with Reetus archives

## Plan
- [x] Locate archive-extension handling in Android importer and mission music scripts
- [x] Add `.rar` to supported archive detection and extraction paths
- [x] Verify extraction support against Reetus archives
- [x] Run scoped code quality and update plan results

## Results
- Added Android RAR archive support through the mission archive facade, using pinned 7-Zip-JBinding for device builds and host `tar` as a JVM-test fallback
- Added `.rar` to picker, directory scan, mission archive import, setup archive extraction, and mission music fingerprint tooling
- Verified `reetus.rar`, `reetus2.rar`, and `FFYL.rar` with `fingerprint_mission_zip_music.ps1`; all extract successfully and contain no OGG/MP3/FLAC tracks
- Added a Reetus-backed mission import/staging unit test guarded by `Assume` when the local fixture is absent
- Passed scoped code quality, mission archive unit tests, and `:app:assembleDebug`

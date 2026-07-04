# Multiplayer preferences persistence plan

## Goal
Make multiplayer use the most recently used normal pilot preferences as its defaults, and preserve player graphics/preferences changes across app restarts.

## Tasks
- [x] Trace player config loading for singleplayer and multiplayer entry
- [x] Identify which preferences are overwritten or skipped for multiplayer
- [x] Patch the shared preference source and persistence path with minimal d1/d2 changes
- [x] Add or update focused regression coverage where practical
- [x] Run scoped formatting and relevant build/test checks

## Notes
- Keep d1 and d2 behavior aligned
- Prefer existing playsave/config helpers over duplicating player file parsing
- No new narrow regression was added; the behavior crosses PhysFS pilot files and Android multiplayer launch state, so verification used the Android NDK build and debug unit suite
- Verified with `android\run-code-quality.ps1 -Fix -Paths ...`, `gradlew.bat :app:assembleDebug`, and `gradlew.bat :app:testDebugUnitTest`

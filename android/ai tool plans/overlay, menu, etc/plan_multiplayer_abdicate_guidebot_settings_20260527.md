# Multiplayer abdicate guidebot settings action plan

## Goal
- Add a multiplayer-only guidebot abdication action to the Android in-game settings/admin tray menu that already contains save/load actions
- Invoke the existing native guidebot ownership transfer mechanism rather than duplicating ownership rules in Kotlin
- Keep single-player menus unchanged

## Steps
1. Trace the existing guidebot ownership and release/transfer code in D1/D2 and Android JNI helpers
2. Add an Android settings/admin tray item visible only during multiplayer
3. Route the new item through a native helper that gives the guidebot to the next player in line
4. Add focused JVM or native coverage where practical
5. Run formatting, targeted tests, and an Android debug build, then record results here

## Status
- Created plan before implementation
- Traced existing Android `META_GUIDE_RELEASE_CONTROL` path and D2 escort ownership transfer
- Added `Abdicate Guidebot` to the Android admin tray for supported D2 multiplayer games
- Wired the action through the existing meta action dispatcher and disabled it unless the local player owns a released guidebot
- Changed D2 guidebot release ownership transfer from random recipient selection to the next connected player slot after the current owner
- Added admin tray JVM coverage for multiplayer visibility, supported-game gating, and close-after-activate behavior
- Validation complete:
	- `android\helpers\stop-stale-formatters.ps1`
	- `android\run-code-quality.ps1 -Fix`
	- `android\gradlew.bat testDebugUnitTest --tests com.dxxredux.app.AdminTrayUiTest`
	- `android\gradlew.bat assembleDebug`
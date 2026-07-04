# Multiplayer Callsign Selection Plan

## Goal
Change the Android multiplayer callsign flow from free text by default to selecting an existing pilot callsign, with a nearby explicit create-new action.

## Steps
- [x] Find the launcher UI that collects the multiplayer callsign and the native bridge that passes it to auto-net
- [x] Replace the default callsign text entry with an existing-callsign selector and an explicit new-callsign affordance
- [x] Preserve native pilot-file creation only for the explicit new-callsign path
- [x] Add or update focused tests where practical
- [x] Run scoped formatting and the relevant build/test checks

## Verification
- `android\run-code-quality.ps1 -Fix` scoped to touched Kotlin, C, and plan files passed
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.multiplayer.MultiplayerCallsignsTest` passed
- `.\gradlew.bat :app:assembleDebug` passed

## Notes
- Keep the player file format source of truth in existing C code
- Avoid broad d1/d2 edits unless the launcher bridge requires them

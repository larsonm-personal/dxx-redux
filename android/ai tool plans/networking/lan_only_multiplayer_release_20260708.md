# LAN-only multiplayer release plan

## Goal
Hide matchmaking/server connection UI for the first release path and send the launcher Multiplayer entry directly to the LAN/direct IP experience.

## Steps
- [x] Trace the existing multiplayer launcher, LAN tab, callsign selection, and resume offer code paths.
- [x] Update the launcher flow so Multiplayer opens the LAN/direct IP screen only.
- [x] Add callsign selection and resume offer handling to the LAN-only view.
- [x] Adjust focused tests or add coverage for the simplified release path.
- [x] Run scoped formatting and relevant verification.

## Verification
- [x] `android\run-code-quality.ps1 -Fix` on the touched Kotlin and plan files passed.
- [x] `android\gradlew.bat :app:compileDebugKotlin` passed.
- [ ] `android\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.multiplayer.MultiplayerControllerFocusPolicyTest --tests com.dxxredux.app.SaveExplorerTest` is blocked by the pre-existing `SettingsChildOverlayControllerTest` reference to missing `VideoInfoControllerAction.MERGED_WALL_EXPERIMENT`.

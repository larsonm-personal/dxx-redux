# Controller-only multiplayer action access plan

## Goal
- When multiplayer-only overlay actions such as warp-to-player and accept-join are available, controller-only users should see the on-screen status button and be able to invoke the action from the unbound actions menu
- Keep phone and touch-overlay behavior unchanged

## Steps
1. Trace how admin tray actions, touch overlay visibility, and the unbound actions menu share action metadata
2. Confirm whether warp and accept status buttons are already shown when the regular touch overlay is hidden
3. Add controller-only unbound menu entries for live warp and accept actions, inserted where initial menu focus lands
4. Add or extend a focused JVM test for action list contents and ordering
5. Run code quality or targeted test/build checks and update this plan with results

## Status
- Created plan before implementation
- Implemented controller-only remaining-actions entries for live warp and accept requests
- Standalone warp and accept overlays are now attached on gamepad-only devices too
- Fixed accept overlay visibility to follow the live join callsign
- Added focused JVM coverage for controller-only insertion and first-selection ordering
- Validation passed:
	- `android\helpers\stop-stale-formatters.ps1`
	- `android\run-code-quality.ps1 -Fix`
	- `android\gradlew.bat testDebugUnitTest --tests com.dxxredux.app.RemainingKeyTouchActionsTest`
	- `android\gradlew.bat assembleDebug`
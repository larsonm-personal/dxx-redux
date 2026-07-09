# Save explorer pop-open plan

## Goal
- Keep a collapsed one-line save explorer affordance visible when the recent-save area is hidden.
- Persist the open or closed startup state when the user opens or closes it.
- Move the current Save Explorer button into the expanded area so it stays at the top.
- Hide the older on/off setting from game preferences or the advanced tab.

## Steps
- [x] Locate the launcher UI and preference state for resume recent save and save explorer.
- [x] Update the collapsed and expanded save explorer UI behavior.
- [x] Persist open/closed state from the UI and remove the old visible setting.
- [x] Add or update focused tests if the launcher test structure supports this area.
- [x] Run scoped formatting and available verification.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths ...` passed for the touched files.
- `.\gradlew.bat :app:compileDebugKotlin` passed.
- `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.ResumeSavePanelTest` was blocked while compiling unrelated unit-test source `SettingsChildOverlayControllerTest.kt`, which currently references unresolved `MERGED_WALL_EXPERIMENT`.
